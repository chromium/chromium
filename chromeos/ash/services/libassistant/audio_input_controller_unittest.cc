// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/audio_input_controller.h"

#include <optional>

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/libassistant/audio/audio_input_impl.h"
#include "chromeos/ash/services/libassistant/audio/audio_input_stream.h"
#include "chromeos/ash/services/libassistant/public/mojom/audio_input_controller.mojom.h"
#include "chromeos/ash/services/libassistant/test_support/fake_platform_delegate.h"
#include "media/audio/audio_device_description.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

namespace {
using mojom::LidState;
using testing::_;
using Resolution = assistant_client::ConversationStateListener::Resolution;

constexpr char kNormalDeviceId[] = "normal-device-id";
constexpr char kHotwordDeviceId[] = "hotword-device-id";
constexpr char kSkipForNonDspMessage[] = "This test case is for DSP";

class MockStreamFactory : public audio::FakeStreamFactory {
 public:
  MOCK_METHOD(
      void,
      CreateInputStream,
      (mojo::PendingReceiver<::media::mojom::AudioInputStream> stream_receiver,
       mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
       mojo::PendingRemote<::media::mojom::AudioInputStreamObserver> observer,
       mojo::PendingRemote<::media::mojom::AudioLog> log,
       const std::string& device_id,
       const media::AudioParameters& params,
       uint32_t shared_memory_count,
       bool enable_agc,
       base::ReadOnlySharedMemoryRegion key_press_count_buffer,
       media::mojom::AudioProcessingConfigPtr processing_config,
       CreateInputStreamCallback callback),
      (override));
};

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

class AssistantAudioInputControllerTest : public testing::TestWithParam<bool> {
 public:
  AssistantAudioInputControllerTest() : enable_dsp_(GetParam()), controller_() {
    controller_.Bind(client_.BindNewPipeAndPassReceiver(), &platform_delegate_);

    if (enable_dsp_) {
      // Enable DSP feature flag.
      scoped_feature_list_.InitAndEnableFeature(
          assistant::features::kEnableDspHotword);
    }
  }

  void TearDown() override {
    if (IsSkipped()) {
      return;
    }

    EXPECT_TRUE(pre_condition_checked_)
        << "You must call AssertHotwordAvailableState or MarkPreconditionMet "
           "to confirm that you are testing the expected environment";
  }

  // Hotword test requires some set up. AudioInputImpl automatically falls back
  // to non-DSP hotword if it doesn't meet the condition. This function checks
  // whether the test is exercising the expected environment.
  void AssertHotwordAvailableState() {
    ASSERT_EQ(enable_dsp_, audio_input().IsHotwordAvailable());
    pre_condition_checked_ = true;
  }

  // Some test cases exercises non-DSP behavior even for DSP available variant.
  // Call this function at the end of its test body to mark that the test is
  // intentionally exercising that scenario.
  void MarkPreconditionMet() {
    ASSERT_FALSE(pre_condition_checked_) << "Pre-condition is already checked.";
    pre_condition_checked_ = true;
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
  //
  // TODO(b/242776750): Set up in test body instead of using this utility method
  // to make it clear what set up the test is testing.
  void InitializeForTestOfType(TestType type) {
    if (type != kLidStateTest)
      SetLidState(LidState::kOpen);

    if (type != kAudioInputObserverTest)
      AddAudioInputObserver();

    if (type != kDeviceIdTest)
      SetDeviceId(kNormalDeviceId);

    if (type != kHotwordEnabledTest)
      SetHotwordEnabled(true);

    if (type != kHotwordDeviceIdTest && enable_dsp_)
      SetHotwordDeviceId(kHotwordDeviceId);
  }

  mojo::Remote<mojom::AudioInputController>& client() { return client_; }

  AudioInputController& controller() { return controller_; }

  AudioInputImpl& audio_input() {
    return controller().audio_input_provider().GetAudioInput();
  }

  bool IsEnableDspFlagOn() { return enable_dsp_; }

  // TODO(b/242776750): Change this to NotRecordingAudio. If we test that it's
  // recording, we should test expected channel (query or hotword) as well with
  // using IsRecordingForQuery or IsRecordingHotword.
  bool IsRecordingAudio() { return audio_input().IsRecordingForTesting(); }

  // TODO(b/242776750): Make this a custom matcher to provide better error
  // message.
  bool IsRecordingForQuery() {
    return audio_input().IsRecordingForTesting() &&
           audio_input().IsMicOpenForTesting() &&
           audio_input().GetOpenDeviceIdForTesting() == kNormalDeviceId;
  }

  bool IsRecordingHotword() {
    if (enable_dsp_) {
      return audio_input().IsRecordingForTesting() &&
             !audio_input().IsMicOpenForTesting() &&
             audio_input().GetOpenDeviceIdForTesting() == kHotwordDeviceId;
    } else {
      return audio_input().IsRecordingForTesting() &&
             !audio_input().IsMicOpenForTesting() &&
             audio_input().GetOpenDeviceIdForTesting() == kNormalDeviceId;
    }
  }

  bool IsUsingDeadStreamDetection() {
    return audio_input().IsUsingDeadStreamDetectionForTesting().value_or(false);
  }

  bool HasCreateInputStreamCalled(MockStreamFactory* mock_stream_factory) {
    EXPECT_CALL(*mock_stream_factory,
                CreateInputStream(_, _, _, _, _, _, _, _, _, _, _))
        .WillOnce(testing::Invoke(
            [](testing::Unused, testing::Unused, testing::Unused,
               testing::Unused, testing::Unused, testing::Unused,
               testing::Unused, testing::Unused, testing::Unused,
               testing::Unused,
               media::mojom::AudioStreamFactory::CreateInputStreamCallback
                   callback) {
              // Invoke the callback as it becomes error if the callback never
              // gets invoked.
              std::move(callback).Run(nullptr, false, std::nullopt);
            }));

    mojo::PendingReceiver<media::mojom::AudioStreamFactory> pending_receiver =
        platform_delegate_.stream_factory_receiver();
    EXPECT_TRUE(pending_receiver.is_valid());
    mock_stream_factory->receiver_.Bind(std::move(pending_receiver));
    mock_stream_factory->receiver_.FlushForTesting();

    return testing::Mock::VerifyAndClearExpectations(mock_stream_factory);
  }

  std::string GetOpenDeviceId() {
    return audio_input().GetOpenDeviceIdForTesting().value_or("<none>");
  }

  void SetLidState(LidState new_state) {
    client()->SetLidState(new_state);
    client().FlushForTesting();
  }

  void SetDeviceId(const std::optional<std::string>& value) {
    client()->SetDeviceId(value);
    client().FlushForTesting();
  }

  void SetHotwordDeviceId(const std::optional<std::string>& value) {
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

 protected:
  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  const bool enable_dsp_;
  bool pre_condition_checked_ = false;
  base::test::ScopedFeatureList scoped_feature_list_;
  mojo::Remote<mojom::AudioInputController> client_;
  AudioInputController controller_;
  FakeAudioInputObserver audio_input_observer_;
  assistant::FakePlatformDelegate platform_delegate_;
};

INSTANTIATE_TEST_SUITE_P(Assistant,
                         AssistantAudioInputControllerTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& param) {
                           return param.param ? "DSP" : "NonDSP";
                         });

}  // namespace

TEST_P(AssistantAudioInputControllerTest, ShouldOnlyRecordWhenLidIsOpen) {
  InitializeForTestOfType(kLidStateTest);
  AssertHotwordAvailableState();

  // Initially the lid is considered closed.
  EXPECT_FALSE(IsRecordingAudio());

  SetLidState(LidState::kOpen);
  EXPECT_TRUE(IsRecordingAudio());

  SetLidState(LidState::kClosed);
  EXPECT_FALSE(IsRecordingAudio());
}

TEST_P(AssistantAudioInputControllerTest, ShouldOnlyRecordWhenDeviceIdIsSet) {
  InitializeForTestOfType(kDeviceIdTest);

  // Initially there is no device id.
  EXPECT_FALSE(IsRecordingAudio());

  SetDeviceId(kNormalDeviceId);
  AssertHotwordAvailableState();
  EXPECT_TRUE(IsRecordingHotword());

  SetDeviceId(std::nullopt);
  EXPECT_FALSE(IsRecordingAudio());
}

TEST_P(AssistantAudioInputControllerTest, StopOnlyRecordWhenHotwordIsEnabled) {
  InitializeForTestOfType(kHotwordEnabledTest);
  AssertHotwordAvailableState();

  // Hotword is enabled by InitializeForTestOfType.
  EXPECT_TRUE(IsRecordingHotword());

  SetHotwordEnabled(false);
  EXPECT_FALSE(IsRecordingHotword());
  // Double check that AudioInputImpl is not recording any other type of audio.
  EXPECT_FALSE(IsRecordingAudio());

  SetHotwordEnabled(true);
  EXPECT_TRUE(IsRecordingHotword());
}

TEST_P(AssistantAudioInputControllerTest,
       StartRecordingWhenDisableHotwordAndForceOpenMic) {
  InitializeForTestOfType(kHotwordEnabledTest);
  SetHotwordEnabled(false);
  AssertHotwordAvailableState();

  EXPECT_FALSE(IsRecordingAudio());

  // Force open mic should start recording.
  // This is exercising a corner case. OnConversationTurnStarted() should be
  // called if mic gets opened.
  // TODO(b/242776750): Change the query recording condition as mic open +
  // OnConversationTurnStarted, i.e. do not record for a query if
  // OnConversationTurnStarted not called.
  SetMicOpen(true);
  EXPECT_TRUE(IsRecordingForQuery());

  SetMicOpen(false);
  EXPECT_FALSE(IsRecordingAudio());
}

TEST_P(AssistantAudioInputControllerTest, ShouldUseProvidedDeviceId) {
  InitializeForTestOfType(kDeviceIdTest);
  SetDeviceId("the-expected-device-id");
  AssertHotwordAvailableState();

  SetMicOpen(true);
  OnConversationTurnStarted();
  EXPECT_TRUE(IsRecordingAudio());
  EXPECT_EQ("the-expected-device-id", GetOpenDeviceId());
}

TEST_P(AssistantAudioInputControllerTest,
       ShouldSwitchToHotwordDeviceIdWhenSet) {
  if (!IsEnableDspFlagOn()) {
    GTEST_SKIP() << kSkipForNonDspMessage;
  }

  InitializeForTestOfType(kHotwordDeviceIdTest);

  SetDeviceId(kNormalDeviceId);
  EXPECT_TRUE(IsRecordingAudio());
  EXPECT_EQ(kNormalDeviceId, GetOpenDeviceId());

  SetHotwordDeviceId(kHotwordDeviceId);
  AssertHotwordAvailableState();
  EXPECT_TRUE(IsRecordingAudio());
  EXPECT_EQ(kHotwordDeviceId, GetOpenDeviceId());
}

TEST_P(AssistantAudioInputControllerTest,
       ShouldKeepUsingHotwordDeviceIdWhenDeviceIdChanges) {
  if (!IsEnableDspFlagOn()) {
    GTEST_SKIP() << kSkipForNonDspMessage;
  }

  InitializeForTestOfType(kHotwordDeviceIdTest);

  SetDeviceId(kNormalDeviceId);
  SetHotwordDeviceId(kHotwordDeviceId);
  AssertHotwordAvailableState();

  EXPECT_TRUE(IsRecordingAudio());
  EXPECT_EQ(kHotwordDeviceId, GetOpenDeviceId());

  SetDeviceId("new-normal-device-id");
  EXPECT_TRUE(IsRecordingAudio());
  EXPECT_EQ(kHotwordDeviceId, GetOpenDeviceId());
}

TEST_P(AssistantAudioInputControllerTest,
       ShouldUseDefaultDeviceIdIfNoDeviceIdIsSet) {
  InitializeForTestOfType(kDeviceIdTest);

  // Mic must be open, otherwise we will not start recording audio if the
  // device id is not set.
  SetMicOpen(true);
  SetDeviceId(std::nullopt);
  SetHotwordDeviceId(std::nullopt);

  EXPECT_TRUE(IsRecordingAudio());
  EXPECT_EQ(media::AudioDeviceDescription::kDefaultDeviceId, GetOpenDeviceId());

  MarkPreconditionMet();
}

TEST_P(AssistantAudioInputControllerTest,
       DeadStreamDetectionShouldBeDisabledWhenUsingHotwordDevice) {
  if (!IsEnableDspFlagOn()) {
    GTEST_SKIP() << kSkipForNonDspMessage;
  }

  InitializeForTestOfType(kHotwordDeviceIdTest);

  SetHotwordDeviceId(std::nullopt);
  EXPECT_TRUE(IsUsingDeadStreamDetection());

  SetHotwordDeviceId(kHotwordDeviceId);
  AssertHotwordAvailableState();
  EXPECT_FALSE(IsUsingDeadStreamDetection());
}

TEST_P(AssistantAudioInputControllerTest,
       ShouldSwitchToNormalAudioDeviceWhenConversationTurnStarts) {
  if (!IsEnableDspFlagOn()) {
    GTEST_SKIP() << kSkipForNonDspMessage;
  }

  InitializeForTestOfType(kDeviceIdTest);
  SetDeviceId("normal-device-id");
  SetHotwordDeviceId("hotword-device-id");
  AssertHotwordAvailableState();

  // While checking for hotword we should be using the hotword device.
  EXPECT_EQ("hotword-device-id", GetOpenDeviceId());

  // But once the conversation starts we should be using the normal audio
  // device.
  OnConversationTurnStarted();
  EXPECT_EQ("normal-device-id", GetOpenDeviceId());
}

TEST_P(AssistantAudioInputControllerTest,
       ShouldSwitchToHotwordAudioDeviceWhenConversationIsFinished) {
  if (!IsEnableDspFlagOn()) {
    GTEST_SKIP() << kSkipForNonDspMessage;
  }

  InitializeForTestOfType(kDeviceIdTest);
  SetDeviceId("normal-device-id");
  SetHotwordDeviceId("hotword-device-id");
  AssertHotwordAvailableState();

  // During the conversation we should be using the normal audio device.
  OnConversationTurnStarted();
  EXPECT_EQ("normal-device-id", GetOpenDeviceId());

  // But once the conversation finishes, we should check for hotwords using the
  // hotword device.
  OnConversationTurnFinished();
  EXPECT_EQ("hotword-device-id", GetOpenDeviceId());
}

TEST_P(AssistantAudioInputControllerTest,
       ShouldCloseMicWhenConversationIsFinishedNormally) {
  InitializeForTestOfType(kDeviceIdTest);
  SetMicOpen(true);
  SetDeviceId(kNormalDeviceId);
  SetHotwordDeviceId(kHotwordDeviceId);
  AssertHotwordAvailableState();

  // Mic should keep opened during the conversation.
  OnConversationTurnStarted();
  EXPECT_TRUE(IsRecordingForQuery());

  // Once the conversation has finished normally without needing mic to keep
  // opened, we should close it.
  OnConversationTurnFinished();
  EXPECT_TRUE(IsRecordingHotword());
}

TEST_P(AssistantAudioInputControllerTest,
       ShouldKeepMicOpenedIfNeededWhenConversationIsFinished) {
  InitializeForTestOfType(kDeviceIdTest);
  SetMicOpen(true);
  SetDeviceId(kNormalDeviceId);
  SetHotwordDeviceId(kHotwordDeviceId);
  AssertHotwordAvailableState();

  // Mic should keep opened during the conversation.
  OnConversationTurnStarted();
  EXPECT_EQ(true, IsRecordingForQuery());

  // If the conversation is finished where mic should still be kept opened
  // (i.e. there's a follow-up interaction), we should keep mic opened.
  OnConversationTurnFinished(Resolution::NORMAL_WITH_FOLLOW_ON);

  // TODO(b/242776750): MicOpen=true doesn't mean that AudioInputImpl is
  // recording. Double check that whether it's expected behavior, i.e. whether
  // this expects that IsRecordingForQuery=true or not.
  EXPECT_EQ(true, audio_input().IsMicOpenForTesting());
}

TEST_P(AssistantAudioInputControllerTest,
       ShouldCloseMicWhenConversationIsFinishedNormallyHotwordOff) {
  InitializeForTestOfType(kDeviceIdTest);
  SetDeviceId(kNormalDeviceId);
  SetHotwordDeviceId(kHotwordDeviceId);
  SetHotwordEnabled(false);
  AssertHotwordAvailableState();
  ASSERT_EQ(false, IsRecordingAudio());

  SetMicOpen(true);
  OnConversationTurnStarted();
  EXPECT_EQ(true, IsRecordingForQuery());

  OnConversationTurnFinished();
  EXPECT_EQ(false, IsRecordingAudio());
}

TEST_P(AssistantAudioInputControllerTest, DSPTrigger) {
  if (!IsEnableDspFlagOn()) {
    GTEST_SKIP() << kSkipForNonDspMessage;
  }

  InitializeForTestOfType(kHotwordDeviceIdTest);
  SetHotwordDeviceId(kHotwordDeviceId);
  SetHotwordEnabled(true);
  AssertHotwordAvailableState();
  ASSERT_EQ(true, IsRecordingHotword());

  MockStreamFactory mock_stream_factory;
  EXPECT_TRUE(HasCreateInputStreamCalled(&mock_stream_factory));

  // Until the conversation ends, no new input stream should be created.
  EXPECT_CALL(mock_stream_factory,
              CreateInputStream(_, _, _, _, _, _, _, _, _, _, _))
      .Times(0);

  // Simulate DSP hotword activation. When DSP detects a hotword, it starts
  // sending audio data until the channel gets closed.
  audio_input().OnCaptureDataArrivedForTesting();
  EXPECT_EQ(GetOpenDeviceId(), kHotwordDeviceId);

  // |OnConversationTurnStarted| gets called once libassistant also detects a
  // hotword in the stream.
  OnConversationTurnStarted();

  // Forward 3 seconds to make sure that software rejection timer is already
  // cancelled.
  environment_.FastForwardBy(base::Seconds(3));
  environment_.RunUntilIdle();

  // During the conversation, an audio stream used for detecting the hotword
  // should be used.
  EXPECT_TRUE(IsRecordingHotword());

  testing::Mock::VerifyAndClearExpectations(&mock_stream_factory);
  OnConversationTurnFinished();

  // Once the converstation ends, the old audio stream will get closed and a new
  // one should be created.
  mock_stream_factory.ResetReceiver();
  EXPECT_TRUE(HasCreateInputStreamCalled(&mock_stream_factory));
  EXPECT_TRUE(IsRecordingHotword());
  EXPECT_EQ(GetOpenDeviceId(), kHotwordDeviceId);
}

TEST_P(AssistantAudioInputControllerTest, DSPTriggerredButSoftwareRejection) {
  if (!IsEnableDspFlagOn()) {
    GTEST_SKIP() << kSkipForNonDspMessage;
  }

  InitializeForTestOfType(kHotwordDeviceIdTest);
  SetHotwordDeviceId(kHotwordDeviceId);
  SetHotwordEnabled(true);
  AssertHotwordAvailableState();
  ASSERT_EQ(true, IsRecordingHotword());

  MockStreamFactory mock_stream_factory;
  EXPECT_TRUE(HasCreateInputStreamCalled(&mock_stream_factory));

  // Simulate DSP hotword activation. When DSP detects a hotword, it starts
  // sending audio data until the channel gets closed.
  audio_input().OnCaptureDataArrivedForTesting();
  EXPECT_EQ(GetOpenDeviceId(), kHotwordDeviceId);

  // If libassistant does not detect a hotword in the audio stream, it will not
  // call |OnConversationTurnStarted|. |DspHotwordStateManager| considers that
  // the hotword gets rejected if it doesn't get the callback in 1 second.
  environment_.FastForwardBy(base::Seconds(1));
  environment_.RunUntilIdle();

  // If it's rejected by libassistant, DSP audio stream should be re-created.
  mock_stream_factory.ResetReceiver();
  EXPECT_TRUE(HasCreateInputStreamCalled(&mock_stream_factory));
  EXPECT_TRUE(IsRecordingHotword());
  EXPECT_EQ(GetOpenDeviceId(), kHotwordDeviceId);
}

}  // namespace ash::libassistant
