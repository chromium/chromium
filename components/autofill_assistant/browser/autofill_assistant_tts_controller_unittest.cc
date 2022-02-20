// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/autofill_assistant_tts_controller.h"

#include "base/callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_assistant {
namespace {

class MockTtsController : public content::TtsController {
 public:
  MockTtsController() = default;
  ~MockTtsController() override = default;

  MOCK_METHOD1(SpeakOrEnqueue,
               void(std::unique_ptr<content::TtsUtterance> utterance));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(RemoveUtteranceEventDelegate,
               void(content::UtteranceEventDelegate* delegate));

  // Unused functions.
  bool IsSpeaking() override { return false; }
  void Stop(const GURL& source_url) override {}
  void Pause() override {}
  void Resume() override {}
  void OnTtsEvent(int utterance_id,
                  content::TtsEventType event_type,
                  int char_index,
                  int length,
                  const std::string& error_message) override {}
  void GetVoices(content::BrowserContext* browser_context,
                 const GURL& source_url,
                 std::vector<content::VoiceData>* out_voices) override {}
  void VoicesChanged() override {}
  void AddVoicesChangedDelegate(
      content::VoicesChangedDelegate* delegate) override {}
  void RemoveVoicesChangedDelegate(
      content::VoicesChangedDelegate* delegate) override {}
  void SetTtsEngineDelegate(content::TtsEngineDelegate* delegate) override {}
  content::TtsEngineDelegate* GetTtsEngineDelegate() override {
    return nullptr;
  }
  void SetTtsPlatform(content::TtsPlatform* tts_platform) override {}
  int QueueSize() override { return 0; }
  void StripSSML(
      const std::string& utterance,
      base::OnceCallback<void(const std::string&)> callback) override {}
  void SetRemoteTtsEngineDelegate(
      content::RemoteTtsEngineDelegate* delegate) override {}
};

class MockTtsEventDelegate
    : public AutofillAssistantTtsController::TtsEventDelegate {
 public:
  MockTtsEventDelegate() = default;
  ~MockTtsEventDelegate() override = default;

  MOCK_METHOD1(OnTtsEvent, void(AutofillAssistantTtsController::TtsEventType));

  base::WeakPtr<MockTtsEventDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockTtsEventDelegate> weak_ptr_factory_{this};
};

class AutofillAssistantTtsControllerTest : public ::testing::Test {
 public:
  void SetUp() override {
    autofill_assistant_tts_controller_.SetTtsEventDelegate(
        mock_tts_event_delegate_.GetWeakPtr());
  }

  void SimulateTtsEvent(content::TtsEventType tts_event_type) {
    autofill_assistant_tts_controller_.OnTtsEvent(
        /* utterance= */ nullptr, tts_event_type, /* char_index= */ 0,
        /* char_length= */ 0, /* error_message= */ std::string());
  }

  testing::NiceMock<MockTtsController> mock_tts_controller_;
  // Declared the delegate first, because it must outlive the
  // |autofill_assistant_tts_controller_|.
  testing::NiceMock<MockTtsEventDelegate> mock_tts_event_delegate_;
  AutofillAssistantTtsController autofill_assistant_tts_controller_{
      &mock_tts_controller_};
};

TEST_F(AutofillAssistantTtsControllerTest, SpeakMessage) {
  EXPECT_CALL(mock_tts_controller_, SpeakOrEnqueue(testing::_))
      .WillOnce([](std::unique_ptr<content::TtsUtterance> utterance) {
        ASSERT_EQ(utterance->GetText(), "message");
        ASSERT_EQ(utterance->GetLang(), "locale");
        ASSERT_EQ(utterance->GetEngineId(), "com.google.android.tts");
      });

  autofill_assistant_tts_controller_.Speak("message", "locale");
}

TEST_F(AutofillAssistantTtsControllerTest, Stop) {
  EXPECT_CALL(mock_tts_controller_, Stop());

  autofill_assistant_tts_controller_.Stop();
}

TEST_F(AutofillAssistantTtsControllerTest, OnTtsEvent) {
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_tts_event_delegate_,
              OnTtsEvent(AutofillAssistantTtsController::TTS_START));
  EXPECT_CALL(mock_tts_event_delegate_,
              OnTtsEvent(AutofillAssistantTtsController::TTS_END));
  EXPECT_CALL(mock_tts_event_delegate_,
              OnTtsEvent(AutofillAssistantTtsController::TTS_ERROR));

  SimulateTtsEvent(content::TTS_EVENT_START);
  SimulateTtsEvent(content::TTS_EVENT_END);
  SimulateTtsEvent(content::TTS_EVENT_ERROR);
}

TEST(AutofillAssistantTtsControllerStandaloneTest,
     DestructorRemovesUtteranceEventDelegate) {
  testing::NiceMock<MockTtsController> mock_tts_controller;
  auto autofill_assistant_tts_controller =
      std::make_unique<AutofillAssistantTtsController>(&mock_tts_controller);

  EXPECT_CALL(
      mock_tts_controller,
      RemoveUtteranceEventDelegate(autofill_assistant_tts_controller.get()));

  autofill_assistant_tts_controller.reset();
}

TEST(AutofillAssistantTtsControllerStandaloneTest,
     OnTtsEventDoesNotCrashesWhenTtsEventDelegateIsInvalid) {
  testing::NiceMock<MockTtsController> mock_tts_controller;
  auto mock_tts_event_delegate = std::make_unique<MockTtsEventDelegate>();
  auto autofill_assistant_tts_controller =
      std::make_unique<AutofillAssistantTtsController>(&mock_tts_controller);
  autofill_assistant_tts_controller->SetTtsEventDelegate(
      mock_tts_event_delegate->GetWeakPtr());
  mock_tts_event_delegate.reset();

  // This method should not crash.
  autofill_assistant_tts_controller->OnTtsEvent(
      /* utterance= */ nullptr, content::TTS_EVENT_START, /* char_index= */ 0,
      /* char_length= */ 0, /* error_message= */ std::string());
}
}  // namespace
}  // namespace autofill_assistant
