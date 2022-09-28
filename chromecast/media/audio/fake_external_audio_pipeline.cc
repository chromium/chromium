// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "chromecast/media/audio/fake_external_audio_pipeline_support.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/external_audio_pipeline_shlib.h"
#include "chromecast/public/media/mixer_output_stream.h"

namespace chromecast {
namespace media {
namespace {

// Library implementation for media volume/mute testing part. It stores
// observers for sending volume/mute change requests and stores volume/muted.
class TestMediaVolumeMute {
 public:
  TestMediaVolumeMute() = default;

  TestMediaVolumeMute(const TestMediaVolumeMute&) = delete;
  TestMediaVolumeMute& operator=(const TestMediaVolumeMute&) = delete;

  // Called by library.
  void AddExternalMediaVolumeChangeRequestObserver(
      ExternalAudioPipelineShlib::ExternalMediaVolumeChangeRequestObserver*
          observer) {
    volume_change_request_observer_ = observer;
  }
  void RemoveExternalMediaVolumeChangeRequestObserver(
      ExternalAudioPipelineShlib::ExternalMediaVolumeChangeRequestObserver*
          observer) {
    volume_change_request_observer_ = nullptr;
  }

  void SetVolume(float volume) { volume_ = volume; }
  void SetMuted(bool muted) { muted_ = muted; }

 protected:
  // Used by derived class for FakeExternalAudioPipelineSupport.
  ExternalAudioPipelineShlib::ExternalMediaVolumeChangeRequestObserver*
      volume_change_request_observer_ = nullptr;
  float volume_ = 0;
  bool muted_ = false;
};

// Library implementation for loopback data testing part. It stores
// CastMediaShlib observers and does loopback audio data to it.
class TestLoopBack {
 public:
  TestLoopBack() = default;

  TestLoopBack(const TestLoopBack&) = delete;
  TestLoopBack& operator=(const TestLoopBack&) = delete;

  // Called from FakeMixerOutputStream.
  void OnData(const float* data,
              int data_size,
              MixerOutputStream* stream,
              int channels) {
    auto delay = stream->GetRenderingDelay();
    int64_t delay_microseconds =
        delay.timestamp_microseconds + delay.delay_microseconds;
    for (auto* observer : observers_) {
      observer->OnLoopbackAudio(
          delay_microseconds, kSampleFormatF32, stream->GetSampleRate(),
          channels, reinterpret_cast<uint8_t*>(const_cast<float*>(data)),
          data_size * sizeof(float));
    }
  }
  // Called from library.
  void AddExternalLoopbackAudioObserver(
      ExternalAudioPipelineShlib::LoopbackAudioObserver* observer) {
    observers_.push_back(observer);
  }

  void RemoveExternalLoopbackAudioObserver(
      ExternalAudioPipelineShlib::LoopbackAudioObserver* observer) {
    auto it = base::ranges::find(observers_, observer);
    if (it != observers_.end()) {
      observers_.erase(it);
    }
    observer->OnRemoved();
  }

 protected:
  // Used by derived class for FakeExternalAudioPipelineSupport.
  std::vector<ExternalAudioPipelineShlib::LoopbackAudioObserver*> observers_;
};

class TestMediaMetadata {
 public:
  TestMediaMetadata() = default;

  TestMediaMetadata(const TestMediaMetadata&) = delete;
  TestMediaMetadata& operator=(const TestMediaMetadata&) = delete;

  // Called from library.
  void AddExternalMediaMetadataChangeObserver(
      ExternalAudioPipelineShlib::ExternalMediaMetadataChangeObserver*
          observer) {
    media_metadata_change_observer_ = observer;
  }
  void RemoveExternalMediaMetadataChangeObserver(
      ExternalAudioPipelineShlib::ExternalMediaMetadataChangeObserver*) {
    media_metadata_change_observer_ = nullptr;
  }

 protected:
  ExternalAudioPipelineShlib::ExternalMediaMetadataChangeObserver*
      media_metadata_change_observer_ = nullptr;
};

// Final class includes library implementation for testing media volume/mute
// + loopback and FakeExternalAudioPipelineSupport implementation.
class TestMedia : public TestMediaVolumeMute,
                  public TestLoopBack,
                  public TestMediaMetadata,
                  public testing::FakeExternalAudioPipelineSupport {
 public:
  TestMedia() = default;

  TestMedia(const TestMedia&) = delete;
  TestMedia& operator=(const TestMedia&) = delete;

  bool supported() const { return supported_; }

  // FakeExternalAudioPipelineSupport implementation:
  void SetSupported() override { supported_ = true; }

  void Reset() override {
    supported_ = false;
    volume_change_request_observer_ = nullptr;
    volume_ = 0;
    muted_ = false;
    observers_.clear();
  }

  float GetVolume() const override { return volume_; }
  bool IsMuted() const override { return muted_; }

  void OnVolumeChangeRequest(float level) override {
    CHECK(volume_change_request_observer_);
    volume_change_request_observer_->OnVolumeChangeRequest(level);
  }
  void OnMuteChangeRequest(bool muted) override {
    CHECK(volume_change_request_observer_);
    volume_change_request_observer_->OnMuteChangeRequest(muted);
  }

  void UpdateExternalMediaMetadata(
      const ExternalAudioPipelineShlib::ExternalMediaMetadata& metadata)
      override {
    if (media_metadata_change_observer_) {
      media_metadata_change_observer_->OnExternalMediaMetadataChanged(metadata);
    }
  }

 private:
  bool supported_ = false;
};

TestMedia* GetTestMedia() {
  static base::NoDestructor<TestMedia> g_test_media;
  return g_test_media.get();
}

// MixerOutputStream implementation, it will be created by
// ExternalAudioPipelineShlib::CreateMixerOutputStream.
class FakeMixerOutputStream : public MixerOutputStream {
 public:
  FakeMixerOutputStream() : test_loop_back_(GetTestMedia()) {}

  FakeMixerOutputStream(const FakeMixerOutputStream&) = delete;
  FakeMixerOutputStream& operator=(const FakeMixerOutputStream&) = delete;

  // MixerOutputStream implementation:
  bool Start(int requested_sample_rate, int channels) override {
    sample_rate_ = requested_sample_rate;
    channels_ = channels;
    return true;
  }

  void Stop() override {}

  int GetNumChannels() override { return channels_; }
  int GetSampleRate() override { return sample_rate_; }

  MediaPipelineBackend::AudioDecoder::RenderingDelay GetRenderingDelay()
      override {
    return MediaPipelineBackend::AudioDecoder::RenderingDelay();
  }

  int OptimalWriteFramesCount() override { return 256; }

  bool Write(const float* data,
             int data_size,
             bool* out_playback_interrupted) override {
    // To check OnLoopbackInterrupted.
    *out_playback_interrupted = true;
    // Loopback data.
    test_loop_back_->OnData(data, data_size, this, channels_);
    return true;
  }

 private:
  int sample_rate_ = 0;
  int channels_ = 0;
  TestLoopBack* const test_loop_back_;
};

}  // namespace

namespace testing {
// Get the interface for interaction with the library from unittests.
FakeExternalAudioPipelineSupport* GetFakeExternalAudioPipelineSupport() {
  return GetTestMedia();
}

}  // namespace testing

// Library implementation.
bool ExternalAudioPipelineShlib::IsSupported() {
  return GetTestMedia()->supported();
}

void ExternalAudioPipelineShlib::AddExternalMediaVolumeChangeRequestObserver(
    ExternalMediaVolumeChangeRequestObserver* observer) {
  GetTestMedia()->AddExternalMediaVolumeChangeRequestObserver(observer);
}

void ExternalAudioPipelineShlib::RemoveExternalMediaVolumeChangeRequestObserver(
    ExternalMediaVolumeChangeRequestObserver* observer) {
  GetTestMedia()->RemoveExternalMediaVolumeChangeRequestObserver(observer);
}

void ExternalAudioPipelineShlib::SetExternalMediaVolume(float level) {
  GetTestMedia()->SetVolume(level);
}

void ExternalAudioPipelineShlib::SetExternalMediaMuted(bool muted) {
  GetTestMedia()->SetMuted(muted);
}

void ExternalAudioPipelineShlib::AddExternalLoopbackAudioObserver(
    LoopbackAudioObserver* observer) {
  GetTestMedia()->AddExternalLoopbackAudioObserver(observer);
}

void ExternalAudioPipelineShlib::RemoveExternalLoopbackAudioObserver(
    LoopbackAudioObserver* observer) {
  GetTestMedia()->RemoveExternalLoopbackAudioObserver(observer);
}

void ExternalAudioPipelineShlib::AddExternalMediaMetadataChangeObserver(
    ExternalMediaMetadataChangeObserver* observer) {
  GetTestMedia()->AddExternalMediaMetadataChangeObserver(observer);
}

void ExternalAudioPipelineShlib::RemoveExternalMediaMetadataChangeObserver(
    ExternalMediaMetadataChangeObserver* observer) {
  GetTestMedia()->RemoveExternalMediaMetadataChangeObserver(observer);
}

std::unique_ptr<MixerOutputStream>
ExternalAudioPipelineShlib::CreateMixerOutputStream() {
  return std::make_unique<FakeMixerOutputStream>();
}

}  // namespace media
}  // namespace chromecast
