// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_AUDIO_OUTPUT_REDIRECTOR_INPUT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_AUDIO_OUTPUT_REDIRECTOR_INPUT_H_

namespace media {
class AudioBus;
}  // namespace media

namespace chromecast {
namespace media {

// Interface used by matching MixerInputs to pass audio data to an output
// redirector.
class AudioOutputRedirectorInput {
 public:
  // Returns the relative order of the output redirector (used if there are
  // multiple output redirectors that match a given MixerInput).
  virtual int Order() = 0;

  // Returns any extra delay that the output redirector will add. Used for A/V
  // sync.
  virtual int64_t GetDelayMicroseconds() = 0;

  // Called to handle audio from a single input stream. Note that all audio
  // output redirectors will receive this data, even if they are not first in
  // the queue of redirectors; this is to allow smooth fading in/out when
  // redirectors are added or removed.
  virtual void Redirect(
      ::media::AudioBus* const buffer,
      int num_frames,
      MediaPipelineBackend::AudioDecoder::RenderingDelay rendering_delay,
      bool redirected) = 0;

 protected:
  virtual ~AudioOutputRedirectorInput() = default;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_AUDIO_OUTPUT_REDIRECTOR_INPUT_H_
