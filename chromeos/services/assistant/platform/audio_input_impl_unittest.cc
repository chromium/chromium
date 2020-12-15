// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_input_impl.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/test_support/scoped_assistant_client.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

namespace {

class ScopedFakeAssistantClient : public ScopedAssistantClient {
 public:
  ScopedFakeAssistantClient() = default;
  ~ScopedFakeAssistantClient() override = default;

  // ScopedAssistantClient overrides:
  void RequestAudioStreamFactory(
      mojo::PendingReceiver<audio::mojom::StreamFactory> receiver) override {
    if (!fake_stream_factory_.receiver_.is_bound())
      fake_stream_factory_.receiver_.Bind(std::move(receiver));
  }

 private:
  audio::FakeStreamFactory fake_stream_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFakeAssistantClient);
};

}  // namespace

class AudioInputImplTest : public testing::Test,
                           public assistant_client::AudioInput::Observer {
 public:
  AudioInputImplTest() {
    // Enable DSP feature flag.
    scoped_feature_list_.InitAndEnableFeature(features::kEnableDspHotword);

    PowerManagerClient::InitializeFake();
    CrasAudioHandler::InitializeForTesting();

    audio_input_impl_ = std::make_unique<AudioInputImpl>(
        FakePowerManagerClient::Get(), CrasAudioHandler::Get(),
        "fake-device-id");

    audio_input_impl_->AddObserver(this);
  }

  ~AudioInputImplTest() override {
    audio_input_impl_->RemoveObserver(this);
    audio_input_impl_.reset();
    CrasAudioHandler::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

  bool GetRecordingStatus() const {
    return audio_input_impl_->IsRecordingForTesting();
  }

  bool IsUsingHotwordDevice() const {
    return audio_input_impl_->IsUsingHotwordDeviceForTesting();
  }

  AudioInputImpl* audio_input_impl() { return audio_input_impl_.get(); }

  // assistant_client::AudioInput::Observer overrides:
  void OnAudioBufferAvailable(const assistant_client::AudioBuffer& buffer,
                              int64_t timestamp) override {}
  void OnAudioError(assistant_client::AudioInput::Error error) override {}
  void OnAudioStopped() override {}

 protected:
  void ReportLidEvent(chromeos::PowerManagerClient::LidState state) {
    FakePowerManagerClient::Get()->SetLidState(state,
                                               base::TimeTicks::UnixEpoch());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedFakeAssistantClient fake_assistant_client_;
  std::unique_ptr<AudioInputImpl> audio_input_impl_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputImplTest);
};

TEST_F(AudioInputImplTest, StopRecordingWhenLidClosed) {
  // Trigger a lid open event.
  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);
  EXPECT_TRUE(GetRecordingStatus());

  // Trigger a lid closed event.
  ReportLidEvent(chromeos::PowerManagerClient::LidState::CLOSED);
  EXPECT_FALSE(GetRecordingStatus());

  // Trigger a lid open event again.
  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);
  EXPECT_TRUE(GetRecordingStatus());
}

TEST_F(AudioInputImplTest, StopRecordingWithNoPreferredDevice) {
  // Start as recording.
  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);
  EXPECT_TRUE(GetRecordingStatus());

  // Preferred input device is lost.
  audio_input_impl()->SetDeviceId(std::string());
  EXPECT_FALSE(GetRecordingStatus());

  // Preferred input device is set again.
  audio_input_impl()->SetDeviceId("fake-device_id");
  EXPECT_TRUE(GetRecordingStatus());
}

TEST_F(AudioInputImplTest, StopRecordingWhenDisableHotword) {
  // Start as recording.
  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);
  EXPECT_TRUE(GetRecordingStatus());

  // Hotword disabled should stop recording.
  audio_input_impl()->OnHotwordEnabled(false);
  EXPECT_FALSE(GetRecordingStatus());

  // Hotword enabled again should start recording.
  audio_input_impl()->OnHotwordEnabled(true);
  EXPECT_TRUE(GetRecordingStatus());
}

TEST_F(AudioInputImplTest, StartRecordingWhenDisableHotwordAndForceOpenMic) {
  // Start as recording.
  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);
  EXPECT_TRUE(GetRecordingStatus());

  // Hotword disabled should stop recording.
  audio_input_impl()->OnHotwordEnabled(false);
  EXPECT_FALSE(GetRecordingStatus());

  // Force open mic should start recording.
  audio_input_impl()->SetMicState(true);
  EXPECT_TRUE(GetRecordingStatus());

  // Stop force open mic should stop recording.
  audio_input_impl()->SetMicState(false);
  EXPECT_FALSE(GetRecordingStatus());
}

TEST_F(AudioInputImplTest, SettingHotwordDeviceDoesNotAffectRecordingState) {
  // Start as recording.
  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);
  EXPECT_TRUE(GetRecordingStatus());

  // Hotword device does not change recording state.
  audio_input_impl()->SetHotwordDeviceId(std::string());
  EXPECT_TRUE(GetRecordingStatus());

  audio_input_impl()->SetHotwordDeviceId("fake-hotword-device");
  EXPECT_TRUE(GetRecordingStatus());
}

TEST_F(AudioInputImplTest, SettingHotwordDeviceUsesHotwordDeviceForRecording) {
  // Start as recording.
  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);
  EXPECT_TRUE(GetRecordingStatus());

  // Hotword device does not change recording state.
  audio_input_impl()->SetHotwordDeviceId(std::string());
  EXPECT_TRUE(GetRecordingStatus());
  EXPECT_FALSE(IsUsingHotwordDevice());

  audio_input_impl()->SetHotwordDeviceId("fake-hotword-device");
  EXPECT_TRUE(GetRecordingStatus());
  EXPECT_TRUE(IsUsingHotwordDevice());
}

}  // namespace assistant
}  // namespace chromeos
