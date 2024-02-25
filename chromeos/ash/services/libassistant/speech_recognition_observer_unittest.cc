// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment.h"
#include "chromeos/ash/services/libassistant/display_connection.h"
#include "chromeos/ash/services/libassistant/libassistant_service.h"
#include "chromeos/ash/services/libassistant/public/mojom/speech_recognition_observer.mojom.h"
#include "chromeos/ash/services/libassistant/test_support/libassistant_service_tester.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/assistant/display_connection.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/display_interface.pb.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

namespace {

using RecognitionState =
    assistant_client::ConversationStateListener::RecognitionState;
using RecognitionResult =
    assistant_client::ConversationStateListener::RecognitionResult;

std::string CreateDisplayAssistantEvent(float speech_level) {
  ::assistant::display::AssistantEvent result;
  result.mutable_speech_level_event()->set_speech_level(speech_level);
  return result.SerializeAsString();
}

class SpeechRecognitionObserverMock : public mojom::SpeechRecognitionObserver {
 public:
  SpeechRecognitionObserverMock() = default;
  SpeechRecognitionObserverMock(const SpeechRecognitionObserverMock&) = delete;
  SpeechRecognitionObserverMock& operator=(
      const SpeechRecognitionObserverMock&) = delete;
  ~SpeechRecognitionObserverMock() override = default;

  // mojom::SpeechRecognitionObserver implementation:
  MOCK_METHOD(void, OnSpeechLevelUpdated, (float speech_level_in_decibels));
  MOCK_METHOD(void, OnSpeechRecognitionStart, ());
  MOCK_METHOD(void,
              OnIntermediateResult,
              (const std::string& high_confidence_text,
               const std::string& low_confidence_text));
  MOCK_METHOD(void, OnSpeechRecognitionEnd, ());
  MOCK_METHOD(void, OnFinalResult, (const std::string& recognized_text));

  mojo::PendingRemote<mojom::SpeechRecognitionObserver>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<mojom::SpeechRecognitionObserver> receiver_{this};
};

}  // namespace

class AssistantSpeechRecognitionObserverTest : public ::testing::Test {
 public:
  AssistantSpeechRecognitionObserverTest() = default;
  AssistantSpeechRecognitionObserverTest(
      const AssistantSpeechRecognitionObserverTest&) = delete;
  AssistantSpeechRecognitionObserverTest& operator=(
      const AssistantSpeechRecognitionObserverTest&) = delete;
  ~AssistantSpeechRecognitionObserverTest() override = default;

  void SetUp() override {
    service_tester_.service().AddSpeechRecognitionObserver(
        observer_mock_.BindNewPipeAndPassRemote());

    service_tester_.Start();
  }

  SpeechRecognitionObserverMock& observer_mock() { return observer_mock_; }

  assistant_client::ConversationStateListener& conversation_state_listener() {
    return *service_tester_.assistant_manager().conversation_state_listener();
  }

  void SendDisplayConnectionEvent(const std::string& event) {
    ::assistant::api::OnAssistantDisplayEventRequest request;
    auto* assistant_display_event = request.mutable_event();
    auto* on_assistant_event =
        assistant_display_event->mutable_on_assistant_event();
    on_assistant_event->set_assistant_event_bytes(event);
    service_tester_.GetDisplayConnection().OnGrpcMessage(request);
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  ::testing::StrictMock<SpeechRecognitionObserverMock> observer_mock_;
  LibassistantServiceTester service_tester_;
};

TEST_F(AssistantSpeechRecognitionObserverTest,
       ShouldReceiveSpeechLevelUpdates) {
  EXPECT_CALL(observer_mock(), OnSpeechLevelUpdated(0.5));
  SendDisplayConnectionEvent(CreateDisplayAssistantEvent(/*speech_level=*/0.5));
  observer_mock().FlushForTesting();
}

TEST_F(AssistantSpeechRecognitionObserverTest,
       ShouldReceiveOnSpeechRecognitionStartEvent) {
  EXPECT_CALL(observer_mock(), OnSpeechRecognitionStart());

  conversation_state_listener().OnRecognitionStateChanged(
      RecognitionState::STARTED, RecognitionResult());
  observer_mock().FlushForTesting();
}

TEST_F(AssistantSpeechRecognitionObserverTest,
       ShouldReceiveOnSpeechRecognitionEndEvent) {
  EXPECT_CALL(observer_mock(), OnSpeechRecognitionEnd());

  conversation_state_listener().OnRecognitionStateChanged(
      RecognitionState::END_OF_UTTERANCE, RecognitionResult());
  observer_mock().FlushForTesting();
}

TEST_F(AssistantSpeechRecognitionObserverTest,
       ShouldReceiveOnIntermediateResultEvent) {
  EXPECT_CALL(observer_mock(), OnIntermediateResult("high-confidence-text",
                                                    "low-confidence-text"));

  RecognitionResult recognition_result;
  recognition_result.high_confidence_text = "high-confidence-text";
  recognition_result.low_confidence_text = "low-confidence-text";
  conversation_state_listener().OnRecognitionStateChanged(
      RecognitionState::INTERMEDIATE_RESULT, recognition_result);
  observer_mock().FlushForTesting();
}

TEST_F(AssistantSpeechRecognitionObserverTest,
       ShouldReceiveOnFinalResultEvent) {
  EXPECT_CALL(observer_mock(), OnFinalResult("recognized-speech"));

  RecognitionResult recognition_result;
  recognition_result.recognized_speech = "recognized-speech";
  conversation_state_listener().OnRecognitionStateChanged(
      RecognitionState::FINAL_RESULT, recognition_result);
  observer_mock().FlushForTesting();
}
}  // namespace ash::libassistant
