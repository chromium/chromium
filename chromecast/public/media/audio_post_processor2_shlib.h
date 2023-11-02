// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_AUDIO_POST_PROCESSOR2_SHLIB_H_
#define CHROMECAST_PUBLIC_MEDIA_AUDIO_POST_PROCESSOR2_SHLIB_H_

#include <string>

#include "volume_control.h"

// Plugin interface for audio DSP modules.
// This is applicable only to audio CMA backends (Alsa, Fuscia).
//
// Please refer to
// chromecast/media/cma/backend/post_processors/governor_shlib.cc
// as an example for new code, but OEM's implementations should not have any
// Chromium dependencies.
//
// Please refer to
// chromecast/media/cma/backend/post_processors/post_processor_wrapper.h for an
// example of how to port an existing AudioPostProcessor to AudioPostProcessor2
//
// Notes on PostProcessors that have a different number of in/out channels:
//  * PostProcessor authors are free to define their channel order; Cast will
//    simply pass this data to subsequent PostProcessors and MixerOutputStream.
//  * Channel selection for stereo pairs will occur after the "mix" group, so
//    devices that support stereo pairs should only change the number of
//    in the "linearize" group of cast_audio.json.

namespace chromecast {
namespace media {

// Interface for AudioPostProcessors used for applying DSP in StreamMixer.
class AudioPostProcessor2 {
 public:
  struct Config {
    int output_sample_rate;  // The output sample rate for this processor.
    int system_output_sample_rate;  // The system (hardware) output sample rate.
    // The number of output frames expected from ProcessFrames().
    int output_frames_per_write;
  };

  struct Metadata {
    // The maximum volume multiplier applied to the current buffer, in dBFS.
    float volume_dbfs;

    // The (max) current target volume multiplier that we are slewing towards.
    float target_volume_dbfs;

    // The system volume applied to the stream (normalized to 0-1). Equivalent
    // to DbFSToVolume(volume_dbfs).
    float system_volume;
  };

  struct Status {
    int input_sample_rate = -1;
    int output_channels = -1;

    // The group delay, measured in frames at the input sample rate. See
    // chromecast/media/cma/backend/post_processors/post_processor_unittest.cc
    // for how this is tested.
    int rendering_delay_frames = 0;

    // The number of input frames of silence it will take for the processor to
    // come to rest after playing out audio.
    // In the case of an FIR filter, this is the length of the FIR kernel.
    // In the case of IIR filters, this should be calculated as the number of
    // frames for the output to decay to 10% (5 time constants).
    // When inputs are paused, at least |GetRingingTimeInFrames()| of
    // silence will be passed through the processor.
    int ringing_time_frames = 0;

    // The data buffer in which the last output from ProcessFrames() was stored.
    // This will never be called before ProcessFrames().
    // This data location should be valid until ProcessFrames() is called
    // again.
    // The data returned by GetOutputBuffer() should not be modified by this
    // instance until the next call to ProcessFrames().
    // If |channels_in| >= |channels_out|, this may be |data| from the
    // last call to ProcessFrames().
    // If |channels_in| < |channels_out|, this PostProcessor is responsible for
    // allocating an output buffer.
    // If PostProcessor owns |output_buffer|, it must ensure that the memory
    // is valid until the next call to ProcessFrames() or destruction.
    float* output_buffer = nullptr;
  };

  virtual ~AudioPostProcessor2() = default;

  // Sets the Config of the processor.
  // Returns |false| if the processor cannot support |config|
  virtual bool SetConfig(const Config& config) = 0;

  // Returns the processor's generated settings. Post processors should keep
  // an up-to-date Status as a member variable.
  virtual const Status& GetStatus() = 0;

  // Processes audio frames from |data|.
  // This will never be called before SetOutputSampleRate().
  // ProcessFrames may overwrite |data|, in which case GetOutputBuffer() should
  // return |data|.
  // |data| will be 32-bit interleaved float with |channels_in| channels.
  // ProcessFrames must produce an equal duration of audio as was input,
  // allowing for sample rate / channel count changes. If the output data
  // will take up equal or less space than the input data, ProcessFrames()
  // should overwrite the input data and store a pointer to |data| in |Status|.
  // Otherwise, the Processor should allocate and own its own output buffer.
  virtual void ProcessFrames(float* data, int frames, Metadata* metadata) = 0;

  // Sends a message to the PostProcessor. Implementations are responsible
  // for the format and parsing of messages.
  // Returns |true| if the message was accepted or |false| if the message could
  // not be applied (i.e. invalid parameter, format error, parameter out of
  // range, etc).
  // If the PostProcessor can/will not be updated at runtime, this can be
  // implemented as "return false;"
  virtual bool UpdateParameters(const std::string& message) = 0;

  // Sets content type to the PostProcessor so it could change processing
  // settings accordingly.
  virtual void SetContentType(AudioContentType content_type) {}

  // Called when device is playing as part of a stereo pair.
  // |channel| is the playout channel on this device (0 for left, 1 for right).
  // or -1 if the device is not part of a stereo pair.
  virtual void SetPlayoutChannel(int channel) {}
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_AUDIO_POST_PROCESSOR2_SHLIB_H_
