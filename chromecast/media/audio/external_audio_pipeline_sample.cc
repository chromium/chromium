// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/external_audio_pipeline_shlib.h"
#include "chromecast/public/media/mixer_output_stream.h"

using ExternalMediaMetadataChangeObserver = chromecast::media::
    ExternalAudioPipelineShlib::ExternalMediaMetadataChangeObserver;

namespace chromecast {
namespace media {

class TestLoopBack {
 public:
  void OnData(const float* data,
              int data_size,
              MixerOutputStream* stream,
              int channels) {
    auto delay = stream->GetRenderingDelay();
    int64_t delay_ms = delay.timestamp_microseconds + delay.delay_microseconds;
    for (auto* observer : observers_) {
      observer->OnLoopbackAudio(
          delay_ms, kSampleFormatF32, stream->GetSampleRate(), channels,
          reinterpret_cast<uint8_t*>(const_cast<float*>(data)),
          data_size * sizeof(float));
    }
  }

  void AddObserver(
      ExternalAudioPipelineShlib::LoopbackAudioObserver* observer) {
    observers_.push_back(observer);
  }

  void RemoveObserver(
      ExternalAudioPipelineShlib::LoopbackAudioObserver* observer) {
    auto it = base::ranges::find(observers_, observer);
    if (it != observers_.end()) {
      observers_.erase(it);
    }
    observer->OnRemoved();
  }

 private:
  std::vector<ExternalAudioPipelineShlib::LoopbackAudioObserver*> observers_;
};

TestLoopBack g_test_loop_back;

class MixerOutputStreamTest : public MixerOutputStream {
 public:
  explicit MixerOutputStreamTest(TestLoopBack* test_loop_back)
      : stream_(MixerOutputStream::Create()), test_loop_back_(test_loop_back) {
    DCHECK(test_loop_back_);
  }

  bool Start(int requested_sample_rate, int channels) override {
    channels_ = channels;
    return stream_->Start(requested_sample_rate, channels);
  }

  int GetSampleRate() override { return stream_->GetSampleRate(); }

  MediaPipelineBackend::AudioDecoder::RenderingDelay GetRenderingDelay()
      override {
    return stream_->GetRenderingDelay();
  }

  int OptimalWriteFramesCount() override {
    return stream_->OptimalWriteFramesCount();
  }

  bool Write(const float* data,
             int data_size,
             bool* out_playback_interrupted) override {
    test_loop_back_->OnData(data, data_size, this, channels_);
    return stream_->Write(data, data_size, out_playback_interrupted);
  }

  void Stop() override { return stream_->Stop(); }

 private:
  std::unique_ptr<MixerOutputStream> stream_;
  TestLoopBack* test_loop_back_;
  int channels_ = 0;
};

bool ExternalAudioPipelineShlib::IsSupported() {
  return true;
}

void ExternalAudioPipelineShlib::AddExternalMediaVolumeChangeRequestObserver(
    ExternalMediaVolumeChangeRequestObserver* observer) {}

void ExternalAudioPipelineShlib::RemoveExternalMediaVolumeChangeRequestObserver(
    ExternalMediaVolumeChangeRequestObserver* observer) {}

void ExternalAudioPipelineShlib::SetExternalMediaVolume(float level) {}

void ExternalAudioPipelineShlib::SetExternalMediaMuted(bool muted) {}

void ExternalAudioPipelineShlib::AddExternalLoopbackAudioObserver(
    LoopbackAudioObserver* observer) {
  g_test_loop_back.AddObserver(observer);
}

void ExternalAudioPipelineShlib::RemoveExternalLoopbackAudioObserver(
    LoopbackAudioObserver* observer) {
  g_test_loop_back.RemoveObserver(observer);
}

void ExternalAudioPipelineShlib::AddExternalMediaMetadataChangeObserver(
    ExternalMediaMetadataChangeObserver* observer) {}

void ExternalAudioPipelineShlib::RemoveExternalMediaMetadataChangeObserver(
    ExternalMediaMetadataChangeObserver* observer) {}

std::unique_ptr<MixerOutputStream>
ExternalAudioPipelineShlib::CreateMixerOutputStream() {
  return std::make_unique<MixerOutputStreamTest>(&g_test_loop_back);
}

}  // namespace media
}  // namespace chromecast
