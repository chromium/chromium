// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/conversation_controller.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/assistant/test_support/expect_utils.h"
#include "chromeos/ash/services/libassistant/test_support/fake_assistant_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

namespace {

class AssistantClientMock : public FakeAssistantClient {
 public:
  AssistantClientMock(std::unique_ptr<chromeos::assistant::FakeAssistantManager>
                          assistant_manager)
      : FakeAssistantClient(std::move(assistant_manager)) {}
  ~AssistantClientMock() override = default;

  // AssistantClient:
  MOCK_METHOD(void, StartVoiceInteraction, ());
  MOCK_METHOD(void, StopAssistantInteraction, (bool cancel_conversation));
  MOCK_METHOD(void,
              SendVoicelessInteraction,
              (const ::assistant::api::Interaction& interaction,
               const std::string& description,
               const ::assistant::api::VoicelessOptions& options,
               base::OnceCallback<void(bool)> on_done));
};

}  // namespace

class ConversationControllerTest : public ::testing::Test {
 public:
  ConversationControllerTest() = default;
  ConversationControllerTest(const ConversationControllerTest&) = delete;
  ConversationControllerTest& operator=(const ConversationControllerTest&) =
      delete;
  ~ConversationControllerTest() override = default;

  void StartLibassistant() {
    controller_.OnAssistantClientRunning(&assistant_client_);
  }

  ConversationController& controller() { return controller_; }

  AssistantClientMock& assistant_client_mock() { return assistant_client_; }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  ConversationController controller_;
  AssistantClientMock assistant_client_{nullptr};
};

TEST_F(ConversationControllerTest, ShouldStartVoiceInteraction) {
  StartLibassistant();

  EXPECT_CALL(assistant_client_mock(), StartVoiceInteraction());

  controller().StartVoiceInteraction();
}

TEST_F(ConversationControllerTest, ShouldStopInteractionAfterDelay) {
  StartLibassistant();

  EXPECT_CALL(assistant_client_mock(), StopAssistantInteraction).Times(0);

  controller().StopActiveInteraction(true);
  testing::Mock::VerifyAndClearExpectations(&assistant_client_mock());

  WAIT_FOR_CALL(assistant_client_mock(), StopAssistantInteraction);
}

TEST_F(ConversationControllerTest,
       ShouldStopInteractionImmediatelyBeforeNewVoiceInteraction) {
  StartLibassistant();

  EXPECT_CALL(assistant_client_mock(), StopAssistantInteraction).Times(0);

  controller().StopActiveInteraction(true);
  testing::Mock::VerifyAndClearExpectations(&assistant_client_mock());

  ::testing::Expectation stop =
      EXPECT_CALL(assistant_client_mock(), StopAssistantInteraction).Times(1);
  EXPECT_CALL(assistant_client_mock(), StartVoiceInteraction)
      .Times(1)
      .After(stop);
  controller().StartVoiceInteraction();
}

TEST_F(ConversationControllerTest,
       ShouldStopInteractionImmediatelyBeforeNewEditReminderInteraction) {
  StartLibassistant();

  EXPECT_CALL(assistant_client_mock(), StopAssistantInteraction).Times(0);

  controller().StopActiveInteraction(true);
  testing::Mock::VerifyAndClearExpectations(&assistant_client_mock());

  ::testing::Expectation stop =
      EXPECT_CALL(assistant_client_mock(), StopAssistantInteraction).Times(1);
  EXPECT_CALL(assistant_client_mock(), SendVoicelessInteraction)
      .Times(1)
      .After(stop);
  controller().StartEditReminderInteraction("client-id");
}

}  // namespace ash::libassistant
