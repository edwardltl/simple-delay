/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DelayAudioProcessor::DelayAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", AudioChannelSet::stereo(), true)
                     #endif
                       ), parameters(*this, nullptr, Identifier("savedParams"),
                                      {
                                          // add list of parameters that plugin will have and their values
                                          std::make_unique<AudioParameterFloat>("wet", // parameter ID
                                                                                "Wet", // parameter name
                                                                                0.0f, // min value
                                                                                100.0f, // max value
                                                                                50.0f // default value
                                                                                ),
                                          std::make_unique<AudioParameterFloat>("time", // parameter ID
                                                                                "Time", // parameter name
                                                                                1.0f, // min value
                                                                                2000.0f, // max value
                                                                                500.0f // default value
                                                                                ),
                                          
                                          std::make_unique<AudioParameterFloat>("feedback", // parameter ID
                                                                                "Feedback", // parameter name
                                                                                0.0f, // min value
                                                                                100.0f, // max value
                                                                                40.0f // default value
                                                                                )
                                      }
                                      )

#endif
{
    // store pointers to our parameters
    wetParameter = parameters.getRawParameterValue("wet");
    timeParameter = parameters.getRawParameterValue("time");
    feedbackParameter = parameters.getRawParameterValue("feedback");
}

DelayAudioProcessor::~DelayAudioProcessor()
{
}

//==============================================================================
const String DelayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DelayAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DelayAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DelayAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DelayAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DelayAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int DelayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DelayAudioProcessor::setCurrentProgram (int index)
{
}

const String DelayAudioProcessor::getProgramName (int index)
{
    return {};
}

void DelayAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void DelayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    
    const int numInputChannels = getTotalNumInputChannels();
    
    // sample buffer for 2 seconds (with some extra headroom)
    const int delayBufferSize = 2* sampleRate + samplesPerBlock;
    mDelayBuffer.setSize(numInputChannels, delayBufferSize);
}

void DelayAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void DelayAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    const int bufferLength = buffer.getNumSamples();
    const int delayBufferLength = mDelayBuffer.getNumSamples();
    
    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const float* bufferData = buffer.getReadPointer(channel);
        
        // copy of the original buffer with extra time
        const float* delayBufferData = mDelayBuffer.getReadPointer(channel);
        
        float* dryBuffer = buffer.getWritePointer(channel);
        
        fillDelayBuffer(channel, bufferLength, delayBufferLength, bufferData, delayBufferData);
        getFromDelayBuffer(buffer, channel, bufferLength, delayBufferLength, bufferData, delayBufferData); 
        feedbackDelay(channel, bufferLength, delayBufferLength, dryBuffer);
    }
    
    mWritePosition += bufferLength;
    mWritePosition %= delayBufferLength;

}

void DelayAudioProcessor::fillDelayBuffer (int channel, const int bufferLength, const int delayBufferLength, const float* bufferData, const float* delayBufferData)
{
    // smoothing
    float wetRatio = *wetParameter / 100.0f;
    wetValue.reset(mSampleRate, 0.05f);
    wetValue.setTargetValue(wetRatio);
    
    //copy data from the main buffer to the delay buffer
    if (delayBufferLength > bufferLength + mWritePosition)
    {
        mDelayBuffer.copyFromWithRamp(channel, mWritePosition, bufferData, bufferLength, wetValue.getNextValue(), wetValue.getNextValue());
    }
    else {
        const int bufferRemaining = delayBufferLength - mWritePosition;
        //copy remaining samples to the end of delay buffer and repeating
        mDelayBuffer.copyFromWithRamp(channel, mWritePosition, bufferData, bufferRemaining, wetValue.getNextValue(), wetValue.getNextValue());
        mDelayBuffer.copyFromWithRamp(channel, 0, bufferData, bufferLength - bufferRemaining, wetValue.getNextValue(), wetValue.getNextValue());
    }
}

void DelayAudioProcessor::getFromDelayBuffer (AudioBuffer<float>& buffer, int channel, const int bufferLength, const int delayBufferLength, const float* bufferData, const float* delayBufferData)
{
    // adding smoothing
    float delayTime = *timeParameter;
    timeValue.reset(mSampleRate, 0.05f);
    timeValue.setTargetValue(delayTime);
    
    float wetRatio = *wetParameter / 100.0f;
    wetValue.reset(mSampleRate, 0.05f);
    wetValue.setTargetValue(wetRatio);
    float dryRatio = 1.0f-wetRatio;
    
    const int readPosition = static_cast<int> (delayBufferLength + mWritePosition - (mSampleRate * delayTime /1000)) % delayBufferLength;
    
    
    if (delayBufferLength > bufferLength + readPosition)
    {
        buffer.addFrom(channel, 0, delayBufferData + readPosition, bufferLength, wetRatio);
    }
    else {
        const int bufferRemaining = delayBufferLength - readPosition;
        buffer.copyFrom(channel, 0, delayBufferData + readPosition, bufferRemaining, dryRatio);
        buffer.copyFrom(channel, bufferRemaining, delayBufferData, bufferLength - bufferRemaining);
    }
}

void DelayAudioProcessor::feedbackDelay (int channel, const int bufferLength, const int delayBufferLength, float* dryBuffer)
{
    float feedback = *feedbackParameter / 100.0f;
    // adding smoothing
    feedbackValue.reset(mSampleRate, 0.05f);
    feedbackValue.setTargetValue(feedback);
    
    if (delayBufferLength > bufferLength + mWritePosition)
    {
        mDelayBuffer.addFromWithRamp(channel, mWritePosition, dryBuffer, bufferLength, feedbackValue.getNextValue(), feedbackValue.getNextValue());
    }
    else
    {
        const int bufferRemaining = delayBufferLength - mWritePosition;
        
        mDelayBuffer.addFromWithRamp(channel, bufferRemaining, dryBuffer, bufferRemaining, feedbackValue.getNextValue(), feedbackValue.getNextValue());
        mDelayBuffer.addFromWithRamp(channel, 0, dryBuffer, bufferLength - bufferRemaining, feedbackValue.getNextValue(), feedbackValue.getNextValue());
    }
}

//==============================================================================
bool DelayAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* DelayAudioProcessor::createEditor()
{
    return new DelayAudioProcessorEditor (*this, parameters);
}

//==============================================================================
void DelayAudioProcessor::getStateInformation (MemoryBlock& destData)

{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    
    // creates a variable to store last settings
    auto state = parameters.copyState();
    // creating an xml to store those settings
    std::unique_ptr<XmlElement> xml (state.createXml());
    // copying the settings to the xml
    copyXmlToBinary(*xml, destData);
}

void DelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    
    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary(data, sizeInBytes));
    
    // recalls data from the xml
    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(parameters.state.getType()))
        {
            parameters.state = (ValueTree::fromXml(*xmlState));
        }
    }
}


//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DelayAudioProcessor();
}
