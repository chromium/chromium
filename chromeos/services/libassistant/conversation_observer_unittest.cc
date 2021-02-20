// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/services/libassistant/libassistant_service.h"
#include "chromeos/services/libassistant/public/mojom/conversation_observer.mojom.h"
#include "chromeos/services/libassistant/test_support/libassistant_service_tester.h"
#include "libassistant/shared/public/conversation_state_listener.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace libassistant {

namespace {

class ConversationObserverMock : public mojom::ConversationObserver {
 public:
  ConversationObserverMock() = default;
  ConversationObserverMock(const ConversationObserverMock&) = delete;
  ConversationObserverMock& operator=(const ConversationObserverMock&) = delete;
  ~ConversationObserverMock() override = default;

  // mojom::ConversationObserver implementation:
  MOCK_METHOD(void,
              OnInteractionFinished,
              (chromeos::assistant::AssistantInteractionResolution resolution));
  MOCK_METHOD(void, OnTtsStarted, (bool due_to_error));

  mojo::PendingRemote<mojom::ConversationObserver> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<mojom::ConversationObserver> receiver_{this};
};

}  // namespace

class ConversationObserverTest : public ::testing::Test {
 public:
  ConversationObserverTest() = default;
  ConversationObserverTest(const ConversationObserverTest&) = delete;
  ConversationObserverTest& operator=(const ConversationObserverTest&) = delete;
  ~ConversationObserverTest() override = default;

  void SetUp() override {
    service_tester_.conversation_controller().AddRemoteObserver(
        observer_mock_.BindNewPipeAndPassRemote());

    service_tester_.Start();
  }

  assistant_client::ConversationStateListener& conversation_state_listener() {
    return *service_tester_.assistant_manager().conversation_state_listener();
  }

  ConversationObserverMock& observer_mock() { return observer_mock_; }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  ::testing::StrictMock<ConversationObserverMock> observer_mock_;
  LibassistantServiceTester service_tester_;
};

TEST_F(ConversationObserverTest,
       ShouldReceiveOnTurnFinishedEventWhenFinishedNormally) {
  EXPECT_CALL(
      observer_mock(),
      OnInteractionFinished(
          chromeos::assistant::AssistantInteractionResolution::kNormal));

  conversation_state_listener().OnConversationTurnFinished(
      assistant_client::ConversationStateListener::Resolution::NORMAL);
  observer_mock().FlushForTesting();
}

TEST_F(ConversationObserverTest,
       ShouldReceiveOnTurnFinishedEventWhenBeingInterrupted) {
  EXPECT_CALL(
      observer_mock(),
      OnInteractionFinished(
          chromeos::assistant::AssistantInteractionResolution::kInterruption));

  conversation_state_listener().OnConversationTurnFinished(
      assistant_client::ConversationStateListener::Resolution::BARGE_IN);
  observer_mock().FlushForTesting();
}

TEST_F(ConversationObserverTest,
       ShouldReceiveOnTtsStartedEventWhenFinishingNormally) {
  EXPECT_CALL(observer_mock(), OnTtsStarted(/*due_to_error=*/false));

  conversation_state_listener().OnRespondingStarted(false);
  observer_mock().FlushForTesting();
}

TEST_F(ConversationObserverTest,
       ShouldReceiveOnTtsStartedEventWhenErrorOccured) {
  EXPECT_CALL(observer_mock(), OnTtsStarted(/*due_to_error=*/true));

  conversation_state_listener().OnRespondingStarted(true);
  observer_mock().FlushForTesting();
}

}  // namespace libassistant
}  // namespace chromeos
