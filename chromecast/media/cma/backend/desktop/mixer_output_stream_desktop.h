// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_DESKTOP_MIXER_OUTPUT_STREAM_DESKTOP_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_DESKTOP_MIXER_OUTPUT_STREAM_DESKTOP_H_

#include "chromecast/public/media/mixer_output_stream.h"

namespace chromecast {
namespace media {

// MixerOutputStream implementation for Desktop.
class MixerOutputStreamDesktop : public MixerOutputStream {
 public:
  MixerOutputStreamDesktop() = default;
  ~MixerOutputStreamDesktop() override = default;
  MixerOutputStreamDesktop(const MixerOutputStreamDesktop&) = delete;

  // MixerOutputStream implementation:
  bool Start(int requested_sample_rate, int channels) override;
  int GetNumChannels() override;
  int GetSampleRate() override;
  MediaPipelineBackend::AudioDecoder::RenderingDelay GetRenderingDelay()
      override;
  int OptimalWriteFramesCount() override;
  bool Write(const float* data,
             int data_size,
             bool* out_playback_interrupted) override;
  void Stop() override;

 private:
  int sample_rate_ = 0;
  int channels_ = 0;

  // Value returned by OptimalWriteFramesCount().
  int target_packet_size_ = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_DESKTOP_MIXER_OUTPUT_STREAM_DESKTOP_H_
