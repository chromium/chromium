// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "chromecast/public/media/mixer_output_stream.h"

namespace chromecast {
namespace media {

// Dummy MixerOutputStream implementation.
class MixerOutputStreamDummy : public MixerOutputStream {
 public:
  MixerOutputStreamDummy() = default;

  // MixerOutputStream implementation:
  bool Start(int requested_sample_rate, int channels) override { return true; }

  int GetNumChannels() override { return 2; }

  int GetSampleRate() override { return 48000; }

  MediaPipelineBackend::AudioDecoder::RenderingDelay GetRenderingDelay()
      override {
    return MediaPipelineBackend::AudioDecoder::RenderingDelay();
  }

  int OptimalWriteFramesCount() override { return 256; }

  bool Write(const float* data,
             int data_size,
             bool* out_playback_interrupted) override {
    return true;
  }

  void Stop() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MixerOutputStreamDummy);
};

// static
std::unique_ptr<MixerOutputStream> MixerOutputStream::Create() {
  return std::make_unique<MixerOutputStreamDummy>();
}

}  // namespace media
}  // namespace chromecast
