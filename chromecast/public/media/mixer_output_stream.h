// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_MIXER_OUTPUT_STREAM_H_
#define CHROMECAST_PUBLIC_MEDIA_MIXER_OUTPUT_STREAM_H_

#include <memory>

#include "media_pipeline_backend.h"

namespace chromecast {
namespace media {

// Interface for output audio stream. Used by the mixer to play mixed stream.
class MixerOutputStream {
 public:
  static constexpr int kInvalidSampleRate = 0;

  // Creates a default MixerOutputStream.
  static std::unique_ptr<MixerOutputStream> Create();

  virtual ~MixerOutputStream() {}

  // Start the stream. Caller must call GetSampleRate() to get the actual sample
  // rate selected for the stream. It may be different from
  // |requested_sample_rate|, e.g. if IsFixedSampleRate() is true, or the device
  // doesn't support |requested_sample_rate|.
  virtual bool Start(int requested_sample_rate, int channels) = 0;

  // Returns number of channels expected to be passed to Write().
  virtual int GetNumChannels() = 0;

  // Returns current sample rate.
  virtual int GetSampleRate() = 0;

  // Returns current rendering delay for the stream.
  virtual MediaPipelineBackend::AudioDecoder::RenderingDelay
  GetRenderingDelay() = 0;

  // Returns the optimal number of frames to pass to Write(). For ALSA, this is
  // the period size.
  virtual int OptimalWriteFramesCount() = 0;

  // |data_size| is size of |data|. Should be divided by number of channels
  // to get number of frames.
  // The current implementation of the mixer relies on ALSA specific behavior
  // where Write blocks if there is not enough space in the buffer. Until
  // this is changed other implementation must follow the same behavior.
  virtual bool Write(const float* data,
                     int data_size,
                     bool* out_playback_interrupted) = 0;

  // Stops the stream. After being stopped the stream can be restarted by
  // calling Start().
  virtual void Stop() = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_MIXER_OUTPUT_STREAM_H_
