// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/services/assistant/platform/audio_input_host_impl.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/libassistant/public/mojom/audio_input_controller.mojom.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::assistant {

namespace {

using LidState = chromeos::PowerManagerClient::LidState;
using MojomLidState = libassistant::mojom::LidState;
using MojomAudioInputController = libassistant::mojom::AudioInputController;
using ::testing::_;
using ::testing::NiceMock;

class AudioInputControllerMock : public MojomAudioInputController {
 public:
  AudioInputControllerMock() = default;
  AudioInputControllerMock(const AudioInputControllerMock&) = delete;
  AudioInputControllerMock& operator=(const AudioInputControllerMock&) = delete;
  ~AudioInputControllerMock() override = default;

  mojo::PendingRemote<MojomAudioInputController> BindNewPipeAndPassRemote() {
    receiver_.reset();
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void, SetMicOpen, (bool mic_open));
  MOCK_METHOD(void, SetHotwordEnabled, (bool enable));
  MOCK_METHOD(void, SetDeviceId, (const std::optional<std::string>& device_id));
  MOCK_METHOD(void,
              SetHotwordDeviceId,
              (const std::optional<std::string>& device_id));
  MOCK_METHOD(void, SetLidState, (MojomLidState new_state));
  MOCK_METHOD(void, OnConversationTurnStarted, ());

 private:
  mojo::Receiver<MojomAudioInputController> receiver_{this};
};

class AssistantAudioInputHostTest : public testing::Test {
 public:
  AssistantAudioInputHostTest() {
    chromeos::PowerManagerClient::InitializeFake();
  }

  AssistantAudioInputHostTest(const AssistantAudioInputHostTest&) = delete;
  AssistantAudioInputHostTest& operator=(const AssistantAudioInputHostTest&) =
      delete;
  ~AssistantAudioInputHostTest() override {
    // |audio_input_host_| uses the fake power manager client, so must be
    // destroyed before the power manager client.
    audio_input_host_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

  void SetUp() override {
    // Enable DSP Hotword
    scoped_feature_list_.InitAndEnableFeature(features::kEnableDspHotword);

    CreateNewAudioInputHost();
  }

  AudioInputControllerMock& mojom_audio_input_controller() {
    return audio_input_controller_;
  }

  AudioInputHostImpl& audio_input_host() {
    CHECK(audio_input_host_);
    return *audio_input_host_;
  }

  void CreateNewAudioInputHost() {
    audio_input_host_ = std::make_unique<AudioInputHostImpl>(
        audio_input_controller_.BindNewPipeAndPassRemote(),
        &cras_audio_handler_.Get(), chromeos::FakePowerManagerClient::Get(),
        "default-locale");

    FlushPendingMojomCalls();
  }

  void DestroyAudioInputHost() { audio_input_host_ = nullptr; }

  void ReportLidEvent(LidState state) {
    chromeos::FakePowerManagerClient::Get()->SetLidState(
        state, base::TimeTicks::UnixEpoch());
    FlushPendingMojomCalls();
  }

  void SetLidState(LidState state) { ReportLidEvent(state); }

  void SetDeviceId(const std::optional<std::string>& device_id) {
    audio_input_host().SetDeviceId(device_id);
    FlushPendingMojomCalls();
  }

  void SetHotwordDeviceId(const std::optional<std::string>& device_id) {
    audio_input_host().SetHotwordDeviceId(device_id);
    FlushPendingMojomCalls();
  }

  void OnHotwordEnabled(bool enabled) {
    audio_input_host().OnHotwordEnabled(enabled);
    FlushPendingMojomCalls();
  }

  void SetMicState(bool mic_open) {
    audio_input_host().SetMicState(mic_open);
    FlushPendingMojomCalls();
  }

  void OnConversationTurnStarted() {
    audio_input_host().OnConversationTurnStarted();
    FlushPendingMojomCalls();
  }

  void FlushPendingMojomCalls() { base::RunLoop().RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedCrasAudioHandlerForTesting cras_audio_handler_;
  NiceMock<AudioInputControllerMock> audio_input_controller_;
  std::unique_ptr<AudioInputHostImpl> audio_input_host_;
};

}  // namespace

TEST_F(AssistantAudioInputHostTest, ShouldSendLidOpenEventsToMojom) {
  EXPECT_CALL(mojom_audio_input_controller(),
              SetLidState(MojomLidState::kOpen));
  ReportLidEvent(LidState::OPEN);
}

TEST_F(AssistantAudioInputHostTest, ShouldSendLidClosedEventsToMojom) {
  EXPECT_CALL(mojom_audio_input_controller(),
              SetLidState(MojomLidState::kClosed));
  ReportLidEvent(LidState::CLOSED);
}

TEST_F(AssistantAudioInputHostTest, ShouldSendLidNotPresentEventsToMojom) {
  // If there is no lid it can not be closed by the user so we consider it to be
  // open.
  EXPECT_CALL(mojom_audio_input_controller(),
              SetLidState(MojomLidState::kOpen));
  ReportLidEvent(LidState::NOT_PRESENT);
}

TEST_F(AssistantAudioInputHostTest, ShouldReadCurrentLidStateWhenLaunching) {
  DestroyAudioInputHost();
  SetLidState(LidState::OPEN);
  EXPECT_CALL(mojom_audio_input_controller(),
              SetLidState(MojomLidState::kOpen));
  CreateNewAudioInputHost();

  DestroyAudioInputHost();
  SetLidState(LidState::CLOSED);
  EXPECT_CALL(mojom_audio_input_controller(),
              SetLidState(MojomLidState::kClosed));
  CreateNewAudioInputHost();
}

TEST_F(AssistantAudioInputHostTest, ShouldSendDeviceIdToMojom) {
  EXPECT_CALL(mojom_audio_input_controller(),
              SetDeviceId(std::optional<std::string>("device-id")));
  SetDeviceId("device-id");
}

TEST_F(AssistantAudioInputHostTest, ShouldUnsetDeviceIdWhenItsEmpty) {
  // Note this variable is required as directly passing std::nullopt into the
  // EXPECT_CALL doesn't compile.
  const std::optional<std::string> expected = std::nullopt;
  EXPECT_CALL(mojom_audio_input_controller(), SetDeviceId(expected));

  SetDeviceId(std::nullopt);
}

TEST_F(AssistantAudioInputHostTest, ShouldSendHotwordDeviceIdToMojom) {
  EXPECT_CALL(
      mojom_audio_input_controller(),
      SetHotwordDeviceId(std::optional<std::string>("hotword-device-id")));
  SetHotwordDeviceId("hotword-device-id");
}

TEST_F(AssistantAudioInputHostTest, ShouldUnsetHotwordDeviceIdWhenItsEmpty) {
  // Note this variable is required as directly passing std::nullopt into the
  // EXPECT_CALL doesn't compile.
  const std::optional<std::string> expected = std::nullopt;
  EXPECT_CALL(mojom_audio_input_controller(), SetHotwordDeviceId(expected));

  SetHotwordDeviceId(std::nullopt);
}
TEST_F(AssistantAudioInputHostTest, ShouldSendHotwordEnabledToMojom) {
  EXPECT_CALL(mojom_audio_input_controller(), SetHotwordEnabled(true));
  OnHotwordEnabled(true);

  EXPECT_CALL(mojom_audio_input_controller(), SetHotwordEnabled(false));
  OnHotwordEnabled(false);
}

TEST_F(AssistantAudioInputHostTest, ShouldSendMicOpenToMojom) {
  EXPECT_CALL(mojom_audio_input_controller(), SetMicOpen(true));
  SetMicState(/*mic_open=*/true);

  EXPECT_CALL(mojom_audio_input_controller(), SetMicOpen(false));
  SetMicState(/*mic_open=*/false);
}

TEST_F(AssistantAudioInputHostTest,
       ShouldSendOnConversationTurnStartedToMojom) {
  EXPECT_CALL(mojom_audio_input_controller(), OnConversationTurnStarted);
  OnConversationTurnStarted();
}

}  // namespace ash::assistant
