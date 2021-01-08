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
#include "chromeos/services/assistant/platform/audio_input_host.h"
#include "chromeos/services/assistant/platform/audio_stream_factory_delegate.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/test_support/scoped_assistant_client.h"
#include "media/audio/audio_device_description.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

namespace {

using LidState = chromeos::PowerManagerClient::LidState;
using ::testing::_;

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

class ScopedCrasAudioHandler {
 public:
  ScopedCrasAudioHandler() { CrasAudioHandler::InitializeForTesting(); }
  ScopedCrasAudioHandler(const ScopedCrasAudioHandler&) = delete;
  ScopedCrasAudioHandler& operator=(const ScopedCrasAudioHandler&) = delete;
  ~ScopedCrasAudioHandler() { CrasAudioHandler::Shutdown(); }

  CrasAudioHandler* Get() { return CrasAudioHandler::Get(); }
};

}  // namespace

class AudioInputImplTest : public testing::Test,
                           public assistant_client::AudioInput::Observer {
 public:
  AudioInputImplTest() {
    // Enable DSP feature flag.
    scoped_feature_list_.InitAndEnableFeature(features::kEnableDspHotword);

    PowerManagerClient::InitializeFake();

    CreateNewAudioInputImpl();
  }

  ~AudioInputImplTest() override {
    audio_input_impl_->RemoveObserver(this);
    audio_input_impl_.reset();
    // |audio_input_host_| uses the fake power manager client, so must be
    // destroyed before the power manager client.
    audio_input_host_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

  bool GetRecordingStatus() const {
    return audio_input_impl_->IsRecordingForTesting();
  }

  std::string GetOpenDeviceId() const {
    return audio_input_impl_->GetOpenDeviceIdForTesting().value_or("<none>");
  }

  bool IsUsingDeadStreamDetection() const {
    return audio_input_impl_->IsUsingDeadStreamDetectionForTesting().value_or(
        false);
  }

  bool IsUsingHotwordDevice() const {
    return audio_input_impl_->IsUsingHotwordDeviceForTesting();
  }

  void CreateNewAudioInputImpl() {
    audio_input_impl_ = std::make_unique<AudioInputImpl>(
        &audio_stream_factory_delegate_, "fake-device-id");

    audio_input_host_ = std::make_unique<AudioInputHost>(
        audio_input_impl_.get(), cras_audio_handler_.Get(),
        FakePowerManagerClient::Get(), "initial-locale");
    audio_input_host_->SetDeviceId("initial-device-id");

    audio_input_impl_->AddObserver(this);

    // Allow the asynchronous triggered events to run.
    base::RunLoop().RunUntilIdle();
  }

  void StopAudioRecording() {
    SetLidState(LidState::CLOSED);
    // Allow the asynchronous triggered events to run.
    base::RunLoop().RunUntilIdle();
  }

  AudioInputImpl* audio_input_impl() { return audio_input_impl_.get(); }

  AudioInputHost& audio_input_host() { return *audio_input_host_; }

  // assistant_client::AudioInput::Observer overrides:
  void OnAudioBufferAvailable(const assistant_client::AudioBuffer& buffer,
                              int64_t timestamp) override {}
  void OnAudioError(assistant_client::AudioInput::Error error) override {}
  void OnAudioStopped() override {}

 protected:
  void ReportLidEvent(LidState state) {
    FakePowerManagerClient::Get()->SetLidState(state,
                                               base::TimeTicks::UnixEpoch());
  }

  void SetLidState(LidState state) { ReportLidEvent(state); }

  void StartAudioRecording() {
    // We are guaranteed to start audio recording if the following conditions
    // are all met.
    SetLidState(LidState::OPEN);
    audio_input_impl()->SetMicState(/*mic_open=*/true);
    audio_input_impl_->AddObserver(this);
    EXPECT_TRUE(GetRecordingStatus());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedFakeAssistantClient fake_assistant_client_;
  DefaultAudioStreamFactoryDelegate audio_stream_factory_delegate_;
  ScopedCrasAudioHandler cras_audio_handler_;
  std::unique_ptr<AudioInputImpl> audio_input_impl_;
  std::unique_ptr<AudioInputHost> audio_input_host_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputImplTest);
};

TEST_F(AudioInputImplTest, StopRecordingWhenLidClosed) {
  // Trigger a lid open event.
  ReportLidEvent(LidState::OPEN);
  EXPECT_TRUE(GetRecordingStatus());

  // Trigger a lid closed event.
  ReportLidEvent(LidState::CLOSED);
  EXPECT_FALSE(GetRecordingStatus());

  // Trigger a lid open event again.
  ReportLidEvent(LidState::OPEN);
  EXPECT_TRUE(GetRecordingStatus());
}

TEST_F(AudioInputImplTest, StartRecordingWhenThereIsNoLid) {
  ReportLidEvent(LidState::NOT_PRESENT);
  EXPECT_TRUE(GetRecordingStatus());
}

TEST_F(AudioInputImplTest, StopRecordingWithNoPreferredDevice) {
  // Start as recording.
  ReportLidEvent(LidState::OPEN);
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
  ReportLidEvent(LidState::OPEN);
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
  ReportLidEvent(LidState::OPEN);
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

TEST_F(AudioInputImplTest, ShouldReadCurrentLidStateWhenLaunching) {
  SetLidState(LidState::OPEN);
  CreateNewAudioInputImpl();
  EXPECT_TRUE(GetRecordingStatus());

  SetLidState(LidState::CLOSED);
  CreateNewAudioInputImpl();
  EXPECT_FALSE(GetRecordingStatus());
}

TEST_F(AudioInputImplTest, ShouldUseDefaultDeviceIdIfNoDeviceIdIsSet) {
  audio_input_impl()->SetDeviceId(std::string());
  audio_input_impl()->SetHotwordDeviceId(std::string());

  StartAudioRecording();

  EXPECT_EQ(media::AudioDeviceDescription::kDefaultDeviceId, GetOpenDeviceId());
}

TEST_F(AudioInputImplTest, SettingHotwordDeviceDoesNotAffectRecordingState) {
  StartAudioRecording();

  // Hotword device does not change recording state.
  audio_input_impl()->SetHotwordDeviceId(std::string());
  EXPECT_TRUE(GetRecordingStatus());

  audio_input_impl()->SetHotwordDeviceId("fake-hotword-device");
  EXPECT_TRUE(GetRecordingStatus());
}

TEST_F(AudioInputImplTest, SettingHotwordDeviceUsesHotwordDeviceForRecording) {
  StartAudioRecording();

  // Hotword device does not change recording state.
  audio_input_impl()->SetHotwordDeviceId(std::string());
  EXPECT_TRUE(GetRecordingStatus());
  EXPECT_FALSE(IsUsingHotwordDevice());

  audio_input_impl()->SetHotwordDeviceId("fake-hotword-device");
  EXPECT_TRUE(GetRecordingStatus());
  EXPECT_TRUE(IsUsingHotwordDevice());
}

TEST_F(AudioInputImplTest,
       DeadStreamDetectionShouldBeDisabledWhenUsingHotwordDevice) {
  StartAudioRecording();

  audio_input_impl()->SetHotwordDeviceId(std::string());
  EXPECT_TRUE(IsUsingDeadStreamDetection());

  audio_input_impl()->SetHotwordDeviceId("fake-hotword-device");
  EXPECT_FALSE(IsUsingDeadStreamDetection());
}

}  // namespace assistant
}  // namespace chromeos
