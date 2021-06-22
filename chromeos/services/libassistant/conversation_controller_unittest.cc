// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/conversation_controller.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/assistant/test_support/expect_utils.h"
#include "chromeos/services/libassistant/test_support/fake_assistant_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace libassistant {

namespace {

class AssistantManagerInternalMock
    : public assistant::FakeAssistantManagerInternal {
 public:
  AssistantManagerInternalMock() = default;
  AssistantManagerInternalMock(const AssistantManagerInternalMock&) = delete;
  AssistantManagerInternalMock& operator=(const AssistantManagerInternalMock&) =
      delete;
  ~AssistantManagerInternalMock() override = default;

  // assistant::FakeAssistantManagerInternal implementation:
  MOCK_METHOD(void,
              StopAssistantInteractionInternal,
              (bool cancel_conversation));
  MOCK_METHOD(void,
              SendVoicelessInteraction,
              (const std::string&,
               const std::string&,
               const assistant_client::VoicelessOptions& options,
               assistant_client::SuccessCallbackInternal on_done));
};

class AssistantManagerMock : public assistant::FakeAssistantManager {
 public:
  AssistantManagerMock() = default;
  AssistantManagerMock(const AssistantManagerMock&) = delete;
  AssistantManagerMock& operator=(const AssistantManagerMock&) = delete;
  ~AssistantManagerMock() override = default;

  // assistant::FakeAssistantManager implementation:
  MOCK_METHOD(void, StartAssistantInteraction, ());
};

}  // namespace

class ConversationControllerTest : public ::testing::Test {
 public:
  ConversationControllerTest() {
    auto fake_assistant_manager = std::make_unique<AssistantManagerMock>();
    assistant_client_ = std::make_unique<FakeAssistantClient>(
        std::move(fake_assistant_manager), &assistant_manager_internal_);
  }
  ConversationControllerTest(const ConversationControllerTest&) = delete;
  ConversationControllerTest& operator=(const ConversationControllerTest&) =
      delete;
  ~ConversationControllerTest() override = default;

  void StartLibassistant() {
    controller_.OnAssistantClientRunning(assistant_client_.get());
  }

  ConversationController& controller() { return controller_; }

  AssistantManagerMock& assistant_manager_mock() {
    return *(reinterpret_cast<AssistantManagerMock*>(
        assistant_client_->assistant_manager()));
  }

  AssistantManagerInternalMock& assistant_manager_internal_mock() {
    return *(reinterpret_cast<AssistantManagerInternalMock*>(
        assistant_client_->assistant_manager_internal()));
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  ConversationController controller_;
  AssistantManagerInternalMock assistant_manager_internal_;
  std::unique_ptr<FakeAssistantClient> assistant_client_;
};

TEST_F(ConversationControllerTest, ShouldStartVoiceInteraction) {
  StartLibassistant();

  EXPECT_CALL(assistant_manager_mock(), StartAssistantInteraction());

  controller().StartVoiceInteraction();
}

TEST_F(ConversationControllerTest, ShouldStopInteractionAfterDelay) {
  StartLibassistant();

  EXPECT_CALL(assistant_manager_internal_mock(),
              StopAssistantInteractionInternal)
      .Times(0);

  controller().StopActiveInteraction(true);
  testing::Mock::VerifyAndClearExpectations(&assistant_manager_internal_mock());

  WAIT_FOR_CALL(assistant_manager_internal_mock(),
                StopAssistantInteractionInternal);
}

TEST_F(ConversationControllerTest,
       ShouldStopInteractionImmediatelyBeforeNewVoiceInteraction) {
  StartLibassistant();

  EXPECT_CALL(assistant_manager_internal_mock(),
              StopAssistantInteractionInternal)
      .Times(0);

  controller().StopActiveInteraction(true);
  testing::Mock::VerifyAndClearExpectations(&assistant_manager_internal_mock());

  ::testing::Expectation stop = EXPECT_CALL(assistant_manager_internal_mock(),
                                            StopAssistantInteractionInternal)
                                    .Times(1);
  EXPECT_CALL(assistant_manager_mock(), StartAssistantInteraction)
      .Times(1)
      .After(stop);
  controller().StartVoiceInteraction();
}

TEST_F(ConversationControllerTest,
       ShouldStopInteractionImmediatelyBeforeNewEditReminderInteraction) {
  StartLibassistant();

  EXPECT_CALL(assistant_manager_internal_mock(),
              StopAssistantInteractionInternal)
      .Times(0);

  controller().StopActiveInteraction(true);
  testing::Mock::VerifyAndClearExpectations(&assistant_manager_internal_mock());

  ::testing::Expectation stop = EXPECT_CALL(assistant_manager_internal_mock(),
                                            StopAssistantInteractionInternal)
                                    .Times(1);
  EXPECT_CALL(assistant_manager_internal_mock(), SendVoicelessInteraction)
      .Times(1)
      .After(stop);
  controller().StartEditReminderInteraction("client-id");
}

}  // namespace libassistant
}  // namespace chromeos
