// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/test_support/scripted_browser_actuator_task_client.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/browser_actuator/public/browser_actuator_task_client.h"
#include "components/browser_actuator/public/proto/auto_password_change.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::StrictMock;

using CompletionStatus = BrowserActuatorTaskClient::CompletionStatus;
using Result = BrowserActuatorTaskClient::Result;
using Scenario = ScriptedBrowserActuatorTaskClient::Scenario;
using Step = ScriptedBrowserActuatorTaskClient::Step;
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

class ScriptedBrowserActuatorTaskClientTest : public testing::Test {
 protected:
  AutoPasswordChangeTaskPayload MakePayload() {
    AutoPasswordChangeTaskPayload payload;
    payload.set_domain("example.com");
    return payload;
  }

  void Start(ScriptedBrowserActuatorTaskClient& client) {
    client.StartTask(UseCase::kAutoPasswordChange, MakePayload(), &observer_);
  }

  // Runs the tasks that are due now, without advancing to the idle timeout.
  void RunReadyTasks() { task_environment_.FastForwardBy(base::TimeDelta()); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  StrictMock<MockObserver> observer_;
};

TEST_F(ScriptedBrowserActuatorTaskClientTest, PlaysScenarioWithDelays) {
  ScriptedBrowserActuatorTaskClient client(Scenario{
      Step::TaskStarted(base::Seconds(1)),
      Step::WorklogUpdate("Working", base::Seconds(2)),
      Step::TaskCompleted(CompletionStatus::kSucceeded, base::Seconds(3)),
  });

  // No step runs inside `StartTask()`.
  Start(client);
  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnTaskStarted());
  task_environment_.FastForwardBy(base::Seconds(1));
  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnWorklogUpdate("Working"));
  task_environment_.FastForwardBy(base::Seconds(2));
  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_,
              OnTaskCompleted(HasStatus(CompletionStatus::kSucceeded)));
  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_TRUE(client.fake().is_completed());
}

TEST_F(ScriptedBrowserActuatorTaskClientTest,
       ZeroDelayStepIsStillAsynchronous) {
  ScriptedBrowserActuatorTaskClient client(Scenario{Step::TaskStarted()});

  Start(client);
  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnTaskStarted());
  RunReadyTasks();
}

TEST_F(ScriptedBrowserActuatorTaskClientTest, StepsAfterCompletionDoNotRun) {
  ScriptedBrowserActuatorTaskClient client(Scenario{
      Step::TaskCompleted(CompletionStatus::kFailed),
      Step::WorklogUpdate("Too late"),
  });
  Start(client);

  EXPECT_CALL(observer_, OnTaskCompleted(HasStatus(CompletionStatus::kFailed)));
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(ScriptedBrowserActuatorTaskClientTest, CancelStopsScenario) {
  ScriptedBrowserActuatorTaskClient client(Scenario{
      Step::TaskStarted(base::Seconds(1)),
      Step::WorklogUpdate("Never shown", base::Seconds(10)),
      Step::TaskCompleted(CompletionStatus::kSucceeded, base::Seconds(10)),
  });
  Start(client);

  EXPECT_CALL(observer_, OnTaskStarted());
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_CALL(observer_,
              OnTaskCompleted(HasStatus(CompletionStatus::kCancelled)));
  client.Cancel();
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(ScriptedBrowserActuatorTaskClientTest, ShutdownStopsScenario) {
  ScriptedBrowserActuatorTaskClient client(Scenario{
      Step::TaskCompleted(CompletionStatus::kSucceeded, base::Seconds(1)),
  });
  Start(client);

  client.SimulateShutdown();
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(client.fake().is_completed());
}

TEST_F(ScriptedBrowserActuatorTaskClientTest,
       CancelBeforeStartLeavesScenarioIntact) {
  ScriptedBrowserActuatorTaskClient client(Scenario{
      Step::TaskStarted(base::Seconds(1)),
      Step::TaskCompleted(CompletionStatus::kSucceeded, base::Seconds(1)),
  });
  client.Cancel();
  Start(client);

  {
    InSequence sequence;
    EXPECT_CALL(observer_, OnTaskStarted());
    EXPECT_CALL(observer_,
                OnTaskCompleted(HasStatus(CompletionStatus::kSucceeded)));
  }
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(client.fake().is_completed());
}

TEST_F(ScriptedBrowserActuatorTaskClientTest,
       StartTaskAfterShutdownDoesNotPlayScenario) {
  ScriptedBrowserActuatorTaskClient client(Scenario{
      Step::TaskStarted(base::Seconds(1)),
      Step::TaskCompleted(CompletionStatus::kSucceeded, base::Seconds(1)),
  });
  client.SimulateShutdown();
  Start(client);

  EXPECT_CALL(observer_,
              OnTaskCompleted(HasResult(
                  CompletionStatus::kFailed,
                  FakeBrowserActuatorTaskClient::kShutdownErrorMessage)));
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(ScriptedBrowserActuatorTaskClientTest, DestroyWhileScenarioRunning) {
  auto client = std::make_unique<ScriptedBrowserActuatorTaskClient>(Scenario{
      Step::TaskStarted(base::Seconds(1)),
      Step::WorklogUpdate("Never shown", base::Seconds(1)),
  });
  Start(*client);

  EXPECT_CALL(observer_, OnTaskStarted());
  task_environment_.FastForwardBy(base::Seconds(1));

  client.reset();
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(ScriptedBrowserActuatorTaskClientTest,
       ObserverCanDestroyClientInScenarioStep) {
  auto client = std::make_unique<ScriptedBrowserActuatorTaskClient>(Scenario{
      Step::TaskStarted(),
      Step::WorklogUpdate("Never shown"),
  });
  Start(*client);

  EXPECT_CALL(observer_, OnTaskStarted()).WillOnce([&] { client.reset(); });
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(client);
}

TEST_F(ScriptedBrowserActuatorTaskClientTest,
       ObserverCanCancelInCallbackDuringScenario) {
  ScriptedBrowserActuatorTaskClient client(Scenario{
      Step::TaskStarted(),
      Step::WorklogUpdate("Never shown"),
  });
  Start(client);

  {
    InSequence sequence;
    EXPECT_CALL(observer_, OnTaskStarted()).WillOnce([&] { client.Cancel(); });
    EXPECT_CALL(observer_,
                OnTaskCompleted(HasStatus(CompletionStatus::kCancelled)));
  }
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(client.fake().is_completed());
}

TEST_F(ScriptedBrowserActuatorTaskClientTest,
       StartFailedScenarioCompletesWithoutTaskStarted) {
  ScriptedBrowserActuatorTaskClient client(
      *ScriptedBrowserActuatorTaskClient::GetScenario("start-failed"));
  Start(client);

  EXPECT_CALL(observer_, OnTaskStarted()).Times(0);
  EXPECT_CALL(
      observer_,
      OnTaskCompleted(HasResult(
          CompletionStatus::kFailed,
          ScriptedBrowserActuatorTaskClient::kStartFailedErrorMessage)));
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(ScriptedBrowserActuatorTaskClientTest,
       SessionDroppedScenarioReportsErrorMessage) {
  ScriptedBrowserActuatorTaskClient client(
      *ScriptedBrowserActuatorTaskClient::GetScenario("session-dropped"));
  Start(client);

  EXPECT_CALL(observer_, OnTaskStarted());
  EXPECT_CALL(observer_, OnWorklogUpdate(_));
  EXPECT_CALL(
      observer_,
      OnTaskCompleted(HasResult(
          CompletionStatus::kFailed,
          ScriptedBrowserActuatorTaskClient::kSessionDroppedErrorMessage)));
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(ScriptedBrowserActuatorTaskClientTest, EveryNamedScenarioCompletes) {
  for (std::string_view name :
       ScriptedBrowserActuatorTaskClient::GetScenarioNames()) {
    SCOPED_TRACE(name);
    std::optional<Scenario> scenario =
        ScriptedBrowserActuatorTaskClient::GetScenario(name);
    ASSERT_TRUE(scenario.has_value());
    ASSERT_THAT(*scenario, Not(IsEmpty()));

    testing::NiceMock<MockObserver> observer;
    ScriptedBrowserActuatorTaskClient client(*scenario);
    client.StartTask(UseCase::kAutoPasswordChange, MakePayload(), &observer);
    task_environment_.FastForwardUntilNoTasksRemain();

    // "hangs" ends with the idle timeout.
    EXPECT_TRUE(client.fake().is_completed());
  }
}

TEST_F(ScriptedBrowserActuatorTaskClientTest, UnknownScenarioName) {
  EXPECT_FALSE(
      ScriptedBrowserActuatorTaskClient::GetScenario("no-such-scenario"));
}

}  // namespace
}  // namespace browser_actuator
