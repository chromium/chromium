// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_AUDIO_POST_PROCESSOR_SHLIB_H_
#define CHROMECAST_PUBLIC_MEDIA_AUDIO_POST_PROCESSOR_SHLIB_H_

#include <string>
#include <vector>

#include "chromecast_export.h"
#include "volume_control.h"

namespace chromecast {
namespace media {
class AudioPostProcessor;
}  // namespace media
}  // namespace chromecast

// Creates an AudioPostProcessor.
// This is applicable only to Alsa CMA backend.
// Please refer to
// chromecast/media/cma/backend/post_processors/governor_shlib.cc
// as an example, but OEM's implementations should not have any
// Chromium dependencies.
// Called from StreamMixerAlsa when shared objects are listed in
// /etc/cast_audio.json
// AudioPostProcessors are created on startup and only destroyed/reset
// if the output sample rate changes.
extern "C" CHROMECAST_EXPORT chromecast::media::AudioPostProcessor*
AudioPostProcessorShlib_Create(const std::string& config, int channels);

namespace chromecast {
namespace media {

// The maximum amount of data that will ever be processed in one call.
const int kMaxAudioWriteTimeMilliseconds = 20;

// Interface for AudioPostProcessors used for applying DSP in StreamMixerAlsa.
class AudioPostProcessor {
 public:
  // Updates the sample rate of the processor.
  // Returns |false| if the processor cannot support |sample_rate|
  // Returning false will result in crashing cast_shell.
  virtual bool SetSampleRate(int sample_rate) = 0;

  // Processes audio frames from |data|, overwriting contents.
  // |data| will always be 32-bit interleaved float.
  // |frames| is the number of audio frames in data and is
  // always non-zero and less than or equal to 20ms of audio.
  // AudioPostProcessor must always provide |frames| frames of data back
  // (may output 0’s)
  // |system_volume| is the Cast Volume applied to the stream
  // (normalized to 0-1). It is the same as the cast volume set via alsa.
  // |volume_dbfs| is the actual attenuation in dBFS (-inf to 0), equivalent to
  // VolumeMap::VolumeToDbFS(|volume|).
  // AudioPostProcessor should assume that volume has already been applied.
  // Returns the current rendering delay of the filter in frames,
  // or negative if an error occurred during processing.
  // If an error occurred during processing, |data| should be unchanged.
  virtual int ProcessFrames(float* data,
                            int frames,
                            float system_volume,
                            float volume_dbfs) = 0;

  // Returns the number of frames of silence it will take for the
  // processor to come to rest.
  // This may be the actual number of frames stored,
  // or may be calculated from internal resonators or similar.
  // When inputs are paused, at least this |GetRingingTimeInFrames()| of
  // silence will be passed through the processor.
  // This is not expected to be real-time;
  // It should only change when SetSampleRate is called.
  virtual int GetRingingTimeInFrames() = 0;

  // Sends a message to the PostProcessor. Implementations are responsible
  // for the format and parsing of messages.
  // OEM's do not need to implement this method.
  virtual void UpdateParameters(const std::string& message) {}

  // Set content type to the PostProcessor so it could change processing
  // settings accordingly.
  virtual void SetContentType(AudioContentType content_type) {}

  // Called when device is playing as part of a stereo pair.
  // |channel| is the playout channel on this device (0 for left, 1 for right).
  // or -1 if the device is not part of a stereo pair.
  virtual void SetPlayoutChannel(int channel) {}

  virtual ~AudioPostProcessor() = default;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_AUDIO_POST_PROCESSOR_SHLIB_H_
