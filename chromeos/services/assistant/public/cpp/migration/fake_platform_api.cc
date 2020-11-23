// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/notreached.h"
#include "chromeos/services/assistant//public/cpp/migration/fake_platform_api.h"
#include "libassistant/shared/public/platform_audio_output.h"

namespace chromeos {
namespace assistant {

namespace {

class FakeVolumeControl : public assistant_client::VolumeControl {
 public:
  // assistant_client::VolumeControl implementation:
  void SetAudioFocus(assistant_client::OutputStreamType) override {}
  float GetSystemVolume() override { return 0.5; }
  void SetSystemVolume(float new_volume, bool user_initiated) override {}
  float GetAlarmVolume() override { return 0.5; }
  void SetAlarmVolume(float new_volume, bool user_initiated) override {}
  bool IsSystemMuted() override { return false; }
  void SetSystemMuted(bool muted) override {}
};

class FakeAudioOutputProvider : public assistant_client::AudioOutputProvider {
 public:
  FakeAudioOutputProvider() = default;
  FakeAudioOutputProvider(FakeAudioOutputProvider&) = delete;
  FakeAudioOutputProvider& operator=(FakeAudioOutputProvider&) = delete;
  ~FakeAudioOutputProvider() override = default;

  // assistant_client::AudioOutputProvider implementation:
  assistant_client::AudioOutput* CreateAudioOutput(
      assistant_client::OutputStreamType type,
      const assistant_client::OutputStreamFormat& stream_format) override {
    NOTIMPLEMENTED();
    abort();
  }

  std::vector<assistant_client::OutputStreamEncoding>
  GetSupportedStreamEncodings() override {
    return {};
  }

  assistant_client::AudioInput* GetReferenceInput() override {
    NOTIMPLEMENTED();
    abort();
  }

  bool SupportsPlaybackTimestamp() const override { return false; }

  assistant_client::VolumeControl& GetVolumeControl() override {
    return volume_control_;
  }

  void RegisterAudioEmittingStateCallback(
      AudioEmittingStateCallback callback) override {
    NOTIMPLEMENTED();
  }

 private:
  FakeVolumeControl volume_control_;
};

}  // namespace

FakePlatformApi::FakePlatformApi()
    : audio_output_provider_(std::make_unique<FakeAudioOutputProvider>()) {}

FakePlatformApi::~FakePlatformApi() = default;

assistant_client::AudioInputProvider& FakePlatformApi::GetAudioInputProvider() {
  NOTIMPLEMENTED();
  abort();
}

assistant_client::AudioOutputProvider&
FakePlatformApi::GetAudioOutputProvider() {
  return *audio_output_provider_.get();
}

assistant_client::AuthProvider& FakePlatformApi::GetAuthProvider() {
  NOTIMPLEMENTED();
  abort();
}

assistant_client::FileProvider& FakePlatformApi::GetFileProvider() {
  NOTIMPLEMENTED();
  abort();
}

assistant_client::NetworkProvider& FakePlatformApi::GetNetworkProvider() {
  NOTIMPLEMENTED();
  abort();
}

assistant_client::SystemProvider& FakePlatformApi::GetSystemProvider() {
  NOTIMPLEMENTED();
  abort();
}

}  // namespace assistant
}  // namespace chromeos
