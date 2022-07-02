// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/audio_input_controller.h"

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/services/libassistant/audio/audio_input_impl.h"
#include "chromeos/services/libassistant/public/mojom/audio_input_controller.mojom.h"
#include "chromeos/services/libassistant/test_support/fake_platform_delegate.h"
#include "media/audio/audio_device_description.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace libassistant {

namespace {
using mojom::LidState;
using Resolution = assistant_client::ConversationStateListener::Resolution;

class FakeAudioInputObserver : public assistant_client::AudioInput::Observer {
 public:
  FakeAudioInputObserver() = default;
  FakeAudioInputObserver(FakeAudioInputObserver&) = delete;
  FakeAudioInputObserver& operator=(FakeAudioInputObserver&) = delete;
  ~FakeAudioInputObserver() override = default;

  // assistant_client::AudioInput::Observer implementation:
  void OnAudioBufferAvailable(const assistant_client::AudioBuffer& buffer,
                              int64_t timestamp) override {}
  void OnAudioError(assistant_client::AudioInput::Error error) override {}
  void OnAudioStopped() override {}
};

class AssistantAudioInputControllerTest : public testing::Test {
 public:
  AssistantAudioInputControllerTest() : controller_() {
    controller_.Bind(client_.BindNewPipeAndPassReceiver(), &platform_delegate_);

    // Enable DSP feature flag.
    scoped_feature_list_.InitAndEnableFeature(
        assistant::features::kEnableDspHotword);
  }

  // See |InitializeForTestOfType| for an explanation of this enum.
  enum TestType {
    kLidStateTest,
    kAudioInputObserverTest,
    kDeviceIdTest,
    kHotwordDeviceIdTest,
    kHotwordEnabledTest,
  };

  // To successfully start recording audio, a lot of requirements must be met
  // (we need a device-id, an audio-input-observer, the lid must be open, and so
  // on).
  // This method will ensure all these requirements are met *except* the ones
  // that we're testing. So for example if you call
  // InitializeForTestOfType(kLidState) then this will ensure all requirements
  // are set but not the lid state (which is left in its initial value).
  void InitializeForTestOfType(TestType type) {
    if (type != kLidStateTest)
      SetLidState(LidState::kOpen);

    if (type != kAudioInputObserverTest)
      AddAudioInputObserver();

    if (type != kDeviceIdTest)
      SetDeviceId("fake-audio-device");

    if (type != kHotwordEnabledTest)
      SetHotwordEnabled(true);
  }

  mojo::Remote<mojom::AudioInputController>& client() { return client_; }

  AudioInputController& controller() { return controller_; }

  AudioInputImpl& audio_input() {
    return controller().audio_input_provider().GetAudioInput();
  }

  bool IsRecordingAudio() { return audio_input().IsRecordingForTesting(); }

  bool IsUsingDeadStreamDetection() {
    return audio_input().IsUsingDeadStreamDetectionForTesting().value_or(false);
  }

  std::string GetOpenDeviceId() {
    return audio_input().GetOpenDeviceIdForTesting().value_or("<none>");
  }

  bool IsMicOpen() { return audio_input().IsMicOpenForTesting(); }

  void SetLidState(LidState new_state) {
    client()->SetLidState(new_state);
    client().FlushForTesting();
  }

  void SetDeviceId(const absl::optional<std::string>& value) {
    client()->SetDeviceId(value);
    client().FlushForTesting();
  }

  void SetHotwordDeviceId(const absl::optional<std::string>& value) {
    client()->SetHotwordDeviceId(value);
    client().FlushForTesting();
  }

  void SetHotwordEnabled(bool value) {
    client()->SetHotwordEnabled(value);
    client().FlushForTesting();
  }

  void SetMicOpen(bool mic_open) {
    client()->SetMicOpen(mic_open);
    client().FlushForTesting();
  }

  void AddAudioInputObserver() {
    audio_input().AddObserver(&audio_input_observer_);
  }

  void OnConversationTurnStarted() { controller().OnConversationTurnStarted(); }

  void OnConversationTurnFinished(Resolution resolution = Resolution::NORMAL) {
    controller().OnInteractionFinished(resolution);
  }

 private:
  base::test::TaskEnvironment environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  mojo::Remote<mojom::AudioInputController> client_;
  AudioInputController controller_;
  FakeAudioInputObserver audio_input_observer_;
  assistant::FakePlatformDelegate platform_delegate_;
};

}  // namespace

TEST_F(AssistantAudioInputControllerTest, ShouldOnlyRecordWhenLidIsOpen) {
  InitializeForTestOfType(kLidStateTest);

  // Initially the lid is considered closed.
  EXPECT_FALSE(IsRecordingAudio());

  SetLidState(LidState::kOpen);
  EXPECT_TRUE(IsRecordingAudio());

  SetLidState(LidState::kClosed);
  EXPECT_FALSE(IsRecordingAudio());
}

TEST_F(AssistantAudioInputControllerTest, ShouldOnlyRecordWhenDeviceIdIsSet) {
  InitializeForTestOfType(kDeviceIdTest);

  // Initially there is no device id.
  EXPECT_FALSE(IsRecordingAudio());

  SetDeviceId("device-id");
  EXPECT_TRUE(IsRecordingAudio());

  SetDeviceId(absl::nullopt);
  EXPECT_FALSE(IsRecordingAudio());
}

TEST_F(AssistantAudioInputControllerTest, StopOnlyRecordWhenHotwordIsEnabled) {
  InitializeForTestOfType(kHotwordEnabledTest);

  // Hotword is enabled by default.
  EXPECT_TRUE(IsRecordingAudio());

  SetHotwordEnabled(false);
  EXPECT_FALSE(IsRecordingAudio());

  SetHotwordEnabled(true);
  EXPECT_TRUE(IsRecordingAudio());
}

TEST_F(AssistantAudioInputControllerTest,
       StartRecordingWhenDisableHotwordAndForceOpenMic) {
  InitializeForTestOfType(kHotwordEnabledTest);
  SetHotwordEnabled(false);
  EXPECT_FALSE(IsRecordingAudio());

  // Force open mic should start recording.
  SetMicOpen(true);
  EXPECT_TRUE(IsRecordingAudio());

  SetMicOpen(false);
  EXPECT_FALSE(IsRecordingAudio());
}

TEST_F(AssistantAudioInputControllerTest, ShouldUseProvidedDeviceId) {
  InitializeForTestOfType(kDeviceIdTest);
  SetDeviceId("the-expected-device-id");

  EXPECT_TRUE(IsRecordingAudio());
  EXPECT_EQ("the-expected-device-id", GetOpenDeviceId());
}

TEST_F(AssistantAudioInputControllerTest,
       ShouldSwitchToHotwordDeviceIdWhenSet) {
  InitializeForTestOfType(kHotwordDeviceIdTest);

  SetDeviceId("the-device-id");
  EXPECT_TRUE(IsRecordingAudio());
  EXPECT_EQ("the-device-id", GetOpenDeviceId());

  SetHotwordDeviceId("the-hotword-device-id");
  EXPECT_TRUE(IsRecordingAudio());
  EXPECT_EQ("the-hotword-device-id", GetOpenDeviceId());
}

TEST_F(AssistantAudioInputControllerTest,
       ShouldKeepUsingHotwordDeviceIdWhenDeviceIdChanges) {
  InitializeForTestOfType(kHotwordDeviceIdTest);

  SetDeviceId("the-original-device-id");
  SetHotwordDeviceId("the-hotword-device-id");

  EXPECT_TRUE(IsRecordingAudio());
  EXPECT_EQ("the-hotword-device-id", GetOpenDeviceId());

  SetDeviceId("the-new-device-id");
  EXPECT_TRUE(IsRecordingAudio());
  EXPECT_EQ("the-hotword-device-id", GetOpenDeviceId());
}

TEST_F(AssistantAudioInputControllerTest,
       ShouldUseDefaultDeviceIdIfNoDeviceIdIsSet) {
  InitializeForTestOfType(kDeviceIdTest);
  // Mic must be open, otherwise we will not start recording audio if the
  // device id is not set.
  SetMicOpen(true);
  SetDeviceId(absl::nullopt);
  SetHotwordDeviceId(absl::nullopt);

  EXPECT_TRUE(IsRecordingAudio());
  EXPECT_EQ(media::AudioDeviceDescription::kDefaultDeviceId, GetOpenDeviceId());
}

TEST_F(AssistantAudioInputControllerTest,
       DeadStreamDetectionShouldBeDisabledWhenUsingHotwordDevice) {
  InitializeForTestOfType(kHotwordDeviceIdTest);

  SetHotwordDeviceId(absl::nullopt);
  EXPECT_TRUE(IsUsingDeadStreamDetection());

  SetHotwordDeviceId("fake-hotword-device");
  EXPECT_FALSE(IsUsingDeadStreamDetection());
}

TEST_F(AssistantAudioInputControllerTest,
       ShouldSwitchToNormalAudioDeviceWhenConversationTurnStarts) {
  InitializeForTestOfType(kDeviceIdTest);
  SetDeviceId("normal-device-id");
  SetHotwordDeviceId("hotword-device-id");

  // While checking for hotword we should be using the hotword device.
  EXPECT_EQ("hotword-device-id", GetOpenDeviceId());

  // But once the conversation starts we should be using the normal audio
  // device.
  OnConversationTurnStarted();
  EXPECT_EQ("normal-device-id", GetOpenDeviceId());
}

TEST_F(AssistantAudioInputControllerTest,
       ShouldSwitchToHotwordAudioDeviceWhenConversationIsFinished) {
  InitializeForTestOfType(kDeviceIdTest);
  SetDeviceId("normal-device-id");
  SetHotwordDeviceId("hotword-device-id");

  // During the conversation we should be using the normal audio device.
  OnConversationTurnStarted();
  EXPECT_EQ("normal-device-id", GetOpenDeviceId());

  // But once the conversation finishes, we should check for hotwords using the
  // hotword device.
  OnConversationTurnFinished();
  EXPECT_EQ("hotword-device-id", GetOpenDeviceId());
}

TEST_F(AssistantAudioInputControllerTest,
       ShouldCloseMicWhenConversationIsFinishedNormally) {
  InitializeForTestOfType(kDeviceIdTest);
  SetMicOpen(true);
  SetDeviceId("normal-device-id");
  SetHotwordDeviceId("hotword-device-id");

  // Mic should keep opened during the conversation.
  OnConversationTurnStarted();
  EXPECT_EQ(true, IsMicOpen());

  // Once the conversation has finished normally without needing mic to keep
  // opened, we should close it.
  OnConversationTurnFinished();
  EXPECT_EQ(false, IsMicOpen());
}

TEST_F(AssistantAudioInputControllerTest,
       ShouldKeepMicOpenedIfNeededWhenConversationIsFinished) {
  InitializeForTestOfType(kDeviceIdTest);
  SetMicOpen(true);
  SetDeviceId("normal-device-id");
  SetHotwordDeviceId("hotword-device-id");

  // Mic should keep opened during the conversation.
  OnConversationTurnStarted();
  EXPECT_EQ(true, IsMicOpen());

  // If the conversation is finished where mic should still be kept opened
  // (i.e. there's a follow-up interaction), we should keep mic opened.
  OnConversationTurnFinished(Resolution::NORMAL_WITH_FOLLOW_ON);
  EXPECT_EQ(true, IsMicOpen());
}

}  // namespace libassistant
}  // namespace chromeos
