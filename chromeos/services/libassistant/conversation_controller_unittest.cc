// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/conversation_controller.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/assistant/test_support/expect_utils.h"
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
  ConversationControllerTest() = default;
  ConversationControllerTest(const ConversationControllerTest&) = delete;
  ConversationControllerTest& operator=(const ConversationControllerTest&) =
      delete;
  ~ConversationControllerTest() override = default;

  void StartLibassistant() {
    controller_.OnAssistantManagerRunning(&assistant_manager_,
                                          &assistant_manager_internal_);
  }

  ConversationController& controller() { return controller_; }

  AssistantManagerMock& assistant_manager_mock() { return assistant_manager_; }

  AssistantManagerInternalMock& assistant_manager_internal_mock() {
    return assistant_manager_internal_;
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  ConversationController controller_;
  AssistantManagerMock assistant_manager_;
  AssistantManagerInternalMock assistant_manager_internal_;
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
       ShouldStopInteractionImmediatelyBeforeNewInteraction) {
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

}  // namespace libassistant
}  // namespace chromeos
