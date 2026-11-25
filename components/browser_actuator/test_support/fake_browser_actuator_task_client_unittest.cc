// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/test_support/fake_browser_actuator_task_client.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/browser_actuator/public/proto/auto_password_change.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::StrictMock;

using CompletionStatus = BrowserActuatorTaskClient::CompletionStatus;
using Result = BrowserActuatorTaskClient::Result;
using UseCase = BrowserActuatorTaskClient::UseCase;

class MockObserver : public BrowserActuatorTaskClient::Observer {
 public:
  MOCK_METHOD(void, OnTaskStarted, (), (override));
  MOCK_METHOD(void, OnWorklogUpdate, (std::string_view), (override));
  MOCK_METHOD(void, OnTaskCompleted, (const Result&), (override));
};

auto HasStatus(CompletionStatus status) {
  return Field(&Result::status, status);
}

auto HasResult(CompletionStatus status, std::string_view error_message) {
  return AllOf(Field(&Result::status, status),
               Field(&Result::error_message, std::string(error_message)));
}

class FakeBrowserActuatorTaskClientTest : public testing::Test {
 protected:
  AutoPasswordChangeTaskPayload MakePayload() {
    AutoPasswordChangeTaskPayload payload;
    payload.set_domain("example.com");
    return payload;
  }

  void Start(FakeBrowserActuatorTaskClient& client) {
    client.StartTask(UseCase::kAutoPasswordChange, MakePayload(), &observer_);
  }

  // Runs the tasks that are due now, without advancing to the idle timeout.
  void RunReadyTasks() { task_environment_.FastForwardBy(base::TimeDelta()); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  StrictMock<MockObserver> observer_;
};

TEST_F(FakeBrowserActuatorTaskClientTest, RecordsStartTaskArguments) {
  FakeBrowserActuatorTaskClient client;
  EXPECT_FALSE(client.start_task_called());
  EXPECT_EQ(client.use_case(), std::nullopt);

  Start(client);

  EXPECT_TRUE(client.start_task_called());
  EXPECT_EQ(client.use_case(), UseCase::kAutoPasswordChange);
  EXPECT_EQ(
      client.feature_payload_type_url(),
      "type.googleapis.com/browser_actuator.AutoPasswordChangeTaskPayload");
  EXPECT_EQ(client.serialized_feature_payload(),
            MakePayload().SerializeAsString());
  EXPECT_FALSE(client.is_completed());
}

TEST_F(FakeBrowserActuatorTaskClientTest, ManualStartWorklogAndCompletion) {
  FakeBrowserActuatorTaskClient client;
  Start(client);

  {
    InSequence sequence;
    EXPECT_CALL(observer_, OnTaskStarted());
    EXPECT_CALL(observer_, OnWorklogUpdate("Step one"));
    EXPECT_CALL(observer_, OnWorklogUpdate("Step two"));
    EXPECT_CALL(observer_,
                OnTaskCompleted(HasStatus(CompletionStatus::kSucceeded)));
  }
  client.SimulateTaskStarted();
  client.SimulateWorklogUpdate("Step one");
  client.SimulateWorklogUpdate("Step two");
  client.SimulateTaskCompleted({.status = CompletionStatus::kSucceeded});

  EXPECT_TRUE(client.is_completed());
}

TEST_F(FakeBrowserActuatorTaskClientTest, ManualFailurePassesErrorMessage) {
  FakeBrowserActuatorTaskClient client;
  Start(client);

  EXPECT_CALL(observer_,
              OnTaskCompleted(HasResult(CompletionStatus::kFailed, "boom")));
  client.SimulateTaskCompleted(
      {.status = CompletionStatus::kFailed, .error_message = "boom"});
}

TEST_F(FakeBrowserActuatorTaskClientTest, CompletionWithoutTaskStarted) {
  FakeBrowserActuatorTaskClient client;
  Start(client);

  EXPECT_CALL(observer_, OnTaskStarted()).Times(0);
  EXPECT_CALL(observer_, OnTaskCompleted(HasStatus(CompletionStatus::kFailed)));
  client.SimulateTaskCompleted({.status = CompletionStatus::kFailed});
}

TEST_F(FakeBrowserActuatorTaskClientTest, WorklogBeforeTaskStartedIsDropped) {
  FakeBrowserActuatorTaskClient client;
  Start(client);

  EXPECT_CALL(observer_, OnWorklogUpdate(_)).Times(0);
  client.SimulateWorklogUpdate("Early step");
}

TEST_F(FakeBrowserActuatorTaskClientTest, DuplicateTaskStartedIsDropped) {
  FakeBrowserActuatorTaskClient client;
  Start(client);

  EXPECT_CALL(observer_, OnTaskStarted()).Times(1);
  client.SimulateTaskStarted();
  client.SimulateTaskStarted();
}

TEST_F(FakeBrowserActuatorTaskClientTest, EventsBeforeStartTaskAreDropped) {
  FakeBrowserActuatorTaskClient client;

  client.SimulateTaskStarted();
  client.SimulateWorklogUpdate("Too early");
  client.SimulateTaskCompleted({.status = CompletionStatus::kSucceeded});

  EXPECT_FALSE(client.start_task_called());
  EXPECT_FALSE(client.is_completed());
}

TEST_F(FakeBrowserActuatorTaskClientTest, EventsAfterCompletionAreDropped) {
  FakeBrowserActuatorTaskClient client;
  Start(client);
  EXPECT_CALL(observer_,
              OnTaskCompleted(HasStatus(CompletionStatus::kSucceeded)));
  client.SimulateTaskCompleted({.status = CompletionStatus::kSucceeded});

  client.SimulateTaskStarted();
  client.SimulateWorklogUpdate("Too late");
  client.SimulateTaskCompleted({.status = CompletionStatus::kFailed});
}

TEST_F(FakeBrowserActuatorTaskClientTest, CancelCompletesAsynchronously) {
  FakeBrowserActuatorTaskClient client;
  Start(client);
  EXPECT_CALL(observer_, OnTaskStarted());
  client.SimulateTaskStarted();

  // Nothing runs inside `Cancel()` itself.
  EXPECT_CALL(observer_, OnTaskCompleted(_)).Times(0);
  client.Cancel();
  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_,
              OnTaskCompleted(HasStatus(CompletionStatus::kCancelled)));
  RunReadyTasks();
  EXPECT_TRUE(client.is_completed());
  EXPECT_EQ(client.cancel_call_count(), 1);
}

TEST_F(FakeBrowserActuatorTaskClientTest, EventsWhileCancellingAreDropped) {
  FakeBrowserActuatorTaskClient client;
  Start(client);
  client.Cancel();

  client.SimulateTaskStarted();
  client.SimulateWorklogUpdate("Too late");
  client.SimulateTaskCompleted({.status = CompletionStatus::kSucceeded});

  EXPECT_CALL(observer_,
              OnTaskCompleted(HasStatus(CompletionStatus::kCancelled)));
  RunReadyTasks();
}

TEST_F(FakeBrowserActuatorTaskClientTest, SecondCancelIsNoOp) {
  FakeBrowserActuatorTaskClient client;
  Start(client);

  EXPECT_CALL(observer_,
              OnTaskCompleted(HasStatus(CompletionStatus::kCancelled)))
      .Times(1);
  client.Cancel();
  client.Cancel();
  RunReadyTasks();
  client.Cancel();
  RunReadyTasks();

  EXPECT_EQ(client.cancel_call_count(), 3);
}

TEST_F(FakeBrowserActuatorTaskClientTest, CancelBeforeStartIsNoOp) {
  FakeBrowserActuatorTaskClient client;
  client.Cancel();
  RunReadyTasks();

  EXPECT_FALSE(client.start_task_called());
  EXPECT_FALSE(client.is_completed());
  EXPECT_EQ(client.cancel_call_count(), 1);
}

TEST_F(FakeBrowserActuatorTaskClientTest,
       DestroyAfterCancelDropsPendingCompletion) {
  auto client = std::make_unique<FakeBrowserActuatorTaskClient>();
  Start(*client);
  client->Cancel();

  // Destroying the client aborts silently, without `OnTaskCompleted()`.
  EXPECT_CALL(observer_, OnTaskCompleted(_)).Times(0);
  client.reset();
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(FakeBrowserActuatorTaskClientTest, IdleTimeoutIsOnByDefault) {
  FakeBrowserActuatorTaskClient client;
  Start(client);

  task_environment_.FastForwardBy(
      FakeBrowserActuatorTaskClient::kDefaultIdleTimeout - base::Seconds(1));
  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_,
              OnTaskCompleted(HasResult(
                  CompletionStatus::kFailed,
                  FakeBrowserActuatorTaskClient::kIdleTimeoutErrorMessage)));
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(client.is_completed());
}

TEST_F(FakeBrowserActuatorTaskClientTest, SetIdleTimeoutChangesTimeout) {
  FakeBrowserActuatorTaskClient client;
  client.SetIdleTimeout(base::Minutes(1));
  Start(client);

  task_environment_.FastForwardBy(base::Minutes(1) - base::Seconds(1));
  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_,
              OnTaskCompleted(HasResult(
                  CompletionStatus::kFailed,
                  FakeBrowserActuatorTaskClient::kIdleTimeoutErrorMessage)));
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(client.is_completed());
}

TEST_F(FakeBrowserActuatorTaskClientTest, ProgressResetsIdleTimeout) {
  FakeBrowserActuatorTaskClient client;
  client.SetIdleTimeout(base::Minutes(1));
  Start(client);

  EXPECT_CALL(observer_, OnTaskStarted());
  task_environment_.FastForwardBy(base::Seconds(50));
  client.SimulateTaskStarted();

  EXPECT_CALL(observer_, OnWorklogUpdate("Working"));
  task_environment_.FastForwardBy(base::Seconds(50));
  client.SimulateWorklogUpdate("Working");

  task_environment_.FastForwardBy(base::Seconds(50));
  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnTaskCompleted(HasStatus(CompletionStatus::kFailed)));
  task_environment_.FastForwardBy(base::Seconds(10));
}

TEST_F(FakeBrowserActuatorTaskClientTest, ZeroIdleTimeoutTurnsTimeoutOff) {
  FakeBrowserActuatorTaskClient client;
  client.SetIdleTimeout(base::Minutes(1));
  client.SetIdleTimeout(base::TimeDelta());
  Start(client);

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(client.is_completed());
}

TEST_F(FakeBrowserActuatorTaskClientTest, ShutdownBeforeStartTask) {
  FakeBrowserActuatorTaskClient client;
  client.SimulateShutdown();

  EXPECT_FALSE(client.start_task_called());
  EXPECT_FALSE(client.is_completed());
}

TEST_F(FakeBrowserActuatorTaskClientTest, ShutdownAfterCompletionIsNoOp) {
  FakeBrowserActuatorTaskClient client;
  Start(client);
  EXPECT_CALL(observer_,
              OnTaskCompleted(HasStatus(CompletionStatus::kSucceeded)));
  client.SimulateTaskCompleted({.status = CompletionStatus::kSucceeded});

  client.SimulateShutdown();
  client.SimulateShutdown();

  EXPECT_TRUE(client.is_completed());
}

TEST_F(FakeBrowserActuatorTaskClientTest, ShutdownDropsPendingCancellation) {
  FakeBrowserActuatorTaskClient client;
  Start(client);
  client.Cancel();

  client.SimulateShutdown();
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(client.is_completed());
}

TEST_F(FakeBrowserActuatorTaskClientTest,
       StartTaskAfterShutdownFailsAsynchronously) {
  FakeBrowserActuatorTaskClient client;
  client.SimulateShutdown();

  // Nothing runs inside `StartTask()` itself.
  EXPECT_CALL(observer_, OnTaskCompleted(_)).Times(0);
  Start(client);
  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_,
              OnTaskCompleted(HasResult(
                  CompletionStatus::kFailed,
                  FakeBrowserActuatorTaskClient::kShutdownErrorMessage)));
  RunReadyTasks();
  EXPECT_TRUE(client.start_task_called());
  EXPECT_TRUE(client.is_completed());
}

TEST_F(FakeBrowserActuatorTaskClientTest,
       SecondShutdownDropsPendingPostShutdownFailure) {
  FakeBrowserActuatorTaskClient client;
  client.SimulateShutdown();
  Start(client);

  client.SimulateShutdown();
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(client.is_completed());
}

TEST_F(FakeBrowserActuatorTaskClientTest, ShutdownDropsObserverSilently) {
  FakeBrowserActuatorTaskClient client;
  Start(client);

  client.SimulateShutdown();
  client.SimulateTaskStarted();
  client.SimulateWorklogUpdate("Too late");
  client.SimulateTaskCompleted({.status = CompletionStatus::kSucceeded});
  client.Cancel();
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(client.is_completed());
}

TEST_F(FakeBrowserActuatorTaskClientTest, DestroyWhileRunning) {
  auto client = std::make_unique<FakeBrowserActuatorTaskClient>();
  Start(*client);
  EXPECT_CALL(observer_, OnTaskStarted());
  client->SimulateTaskStarted();

  // Destroying the client aborts silently: no idle timeout fires later.
  client.reset();
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(FakeBrowserActuatorTaskClientTest, ObserverCanDestroyClientInCallback) {
  auto client = std::make_unique<FakeBrowserActuatorTaskClient>();
  Start(*client);

  EXPECT_CALL(observer_, OnTaskStarted()).WillOnce([&] { client.reset(); });
  client->SimulateTaskStarted();
  EXPECT_FALSE(client);
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(FakeBrowserActuatorTaskClientTest,
       ObserverCanDestroyClientInCompletion) {
  auto client = std::make_unique<FakeBrowserActuatorTaskClient>();
  Start(*client);

  EXPECT_CALL(observer_, OnTaskCompleted(_)).WillOnce([&] { client.reset(); });
  client->Cancel();
  RunReadyTasks();
  EXPECT_FALSE(client);
}

TEST_F(FakeBrowserActuatorTaskClientTest,
       ObserverCanDestroyClientInIdleTimeout) {
  auto client = std::make_unique<FakeBrowserActuatorTaskClient>();
  client->SetIdleTimeout(base::Minutes(1));
  Start(*client);

  EXPECT_CALL(observer_, OnTaskCompleted(_)).WillOnce([&] { client.reset(); });
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(client);
}

}  // namespace
}  // namespace browser_actuator
