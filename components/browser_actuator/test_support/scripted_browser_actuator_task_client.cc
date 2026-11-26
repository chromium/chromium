// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/test_support/scripted_browser_actuator_task_client.h"

#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace browser_actuator {

namespace {

using Step = ScriptedBrowserActuatorTaskClient::Step;
using Scenario = ScriptedBrowserActuatorTaskClient::Scenario;
using CompletionStatus = BrowserActuatorTaskClient::CompletionStatus;

constexpr int kLongRunningWorklogCount = 30;

struct NamedScenario {
  std::string_view name;
  Scenario (*build)();
};

// The one list of named scenarios. `GetScenario()` and `GetScenarioNames()`
// both read it, so a scenario cannot exist without a name.
constexpr NamedScenario kNamedScenarios[] = {
    {"success",
     [] {
       return Scenario{
           Step::TaskStarted(base::Milliseconds(500)),
           Step::WorklogUpdate("Opening the website", base::Seconds(1)),
           Step::WorklogUpdate("Finding the change password form",
                               base::Milliseconds(1500)),
           Step::WorklogUpdate("Entering the new password",
                               base::Milliseconds(1500)),
           Step::TaskCompleted(CompletionStatus::kSucceeded, base::Seconds(1)),
       };
     }},
    {"failure",
     [] {
       return Scenario{
           Step::TaskStarted(base::Milliseconds(500)),
           Step::WorklogUpdate("Opening the website", base::Seconds(1)),
           Step::TaskCompleted(CompletionStatus::kFailed,
                               base::Milliseconds(1500), "Simulated failure"),
       };
     }},
    {"server-cancelled",
     [] {
       return Scenario{
           Step::TaskStarted(base::Milliseconds(500)),
           Step::WorklogUpdate("Opening the website", base::Seconds(1)),
           Step::TaskCompleted(CompletionStatus::kCancelled,
                               base::Milliseconds(1500)),
       };
     }},
    // A failure to start the session: completes with no `OnTaskStarted()`.
    {"start-failed",
     [] {
       return Scenario{
           Step::TaskCompleted(CompletionStatus::kFailed, base::Seconds(1),
                               std::string(ScriptedBrowserActuatorTaskClient::
                                               kStartFailedErrorMessage)),
       };
     }},
    {"session-dropped",
     [] {
       return Scenario{
           Step::TaskStarted(base::Milliseconds(500)),
           Step::WorklogUpdate("Opening the website", base::Seconds(1)),
           Step::TaskCompleted(CompletionStatus::kFailed,
                               base::Milliseconds(1500),
                               std::string(ScriptedBrowserActuatorTaskClient::
                                               kSessionDroppedErrorMessage)),
       };
     }},
    {"long-running",
     [] {
       Scenario scenario{Step::TaskStarted(base::Milliseconds(500))};
       scenario.reserve(kLongRunningWorklogCount + 2);
       for (int i = 1; i <= kLongRunningWorklogCount; ++i) {
         scenario.push_back(Step::WorklogUpdate(
             base::StrCat({"Step ", base::NumberToString(i)}),
             base::Seconds(2)));
       }
       scenario.push_back(
           Step::TaskCompleted(CompletionStatus::kSucceeded, base::Seconds(2)));
       return scenario;
     }},
    // Ends with the idle timeout, unless it is turned off.
    {"hangs",
     [] {
       return Scenario{
           Step::TaskStarted(base::Milliseconds(500)),
           Step::WorklogUpdate("Opening the website", base::Seconds(1)),
       };
     }},
};

}  // namespace

// static
Step Step::TaskStarted(base::TimeDelta delay) {
  Step step;
  step.type = Type::kTaskStarted;
  step.delay = delay;
  return step;
}

// static
Step Step::WorklogUpdate(std::string step_description, base::TimeDelta delay) {
  Step step;
  step.type = Type::kWorklogUpdate;
  step.delay = delay;
  step.step_description = std::move(step_description);
  return step;
}

// static
Step Step::TaskCompleted(CompletionStatus status,
                         base::TimeDelta delay,
                         std::string error_message) {
  Step step;
  step.type = Type::kTaskCompleted;
  step.delay = delay;
  step.result.status = status;
  step.result.error_message = std::move(error_message);
  return step;
}

Step::Step() = default;
Step::Step(const Step&) = default;
Step& Step::operator=(const Step&) = default;
Step::Step(Step&&) noexcept = default;
Step& Step::operator=(Step&&) noexcept = default;
Step::~Step() = default;

// static
std::optional<Scenario> ScriptedBrowserActuatorTaskClient::GetScenario(
    std::string_view name) {
  for (const NamedScenario& entry : kNamedScenarios) {
    if (entry.name == name) {
      return entry.build();
    }
  }
  return std::nullopt;
}

// static
std::vector<std::string_view>
ScriptedBrowserActuatorTaskClient::GetScenarioNames() {
  std::vector<std::string_view> names;
  names.reserve(std::size(kNamedScenarios));
  for (const NamedScenario& entry : kNamedScenarios) {
    names.push_back(entry.name);
  }
  return names;
}

ScriptedBrowserActuatorTaskClient::ScriptedBrowserActuatorTaskClient(
    Scenario scenario)
    : scenario_(std::move(scenario)) {}

ScriptedBrowserActuatorTaskClient::~ScriptedBrowserActuatorTaskClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ScriptedBrowserActuatorTaskClient::StartTask(
    UseCase use_case,
    const google::protobuf::MessageLite& feature_payload,
    Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fake_.StartTask(use_case, feature_payload, observer);
  if (is_shut_down_) {
    return;
  }
  ScheduleNextStep();
}

void ScriptedBrowserActuatorTaskClient::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (fake_.start_task_called()) {
    StopScenario();
  }
  fake_.Cancel();
}

void ScriptedBrowserActuatorTaskClient::SimulateShutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_shut_down_ = true;
  StopScenario();
  fake_.SimulateShutdown();
}

FakeBrowserActuatorTaskClient& ScriptedBrowserActuatorTaskClient::fake() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return fake_;
}

const FakeBrowserActuatorTaskClient& ScriptedBrowserActuatorTaskClient::fake()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return fake_;
}

void ScriptedBrowserActuatorTaskClient::ScheduleNextStep() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (next_step_index_ >= scenario_.size()) {
    return;
  }
  // Always post, even for a zero delay, so that no step runs synchronously
  // inside `StartTask()`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ScriptedBrowserActuatorTaskClient::RunNextStep,
                     weak_factory_.GetWeakPtr()),
      scenario_[next_step_index_].delay);
}

void ScriptedBrowserActuatorTaskClient::RunNextStep() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_LT(next_step_index_, scenario_.size());

  // Copy the step: the observer can destroy `this`, and `scenario_` with it.
  const Step step = scenario_[next_step_index_++];
  base::WeakPtr<ScriptedBrowserActuatorTaskClient> weak_this =
      weak_factory_.GetWeakPtr();
  RunStep(step);
  // Stop if the observer destroyed `this`, called `Cancel()` /
  // `SimulateShutdown()` (which invalidate `weak_factory_`), or the task
  // completed.
  if (!weak_this || fake_.is_completed()) {
    return;
  }
  ScheduleNextStep();
}

void ScriptedBrowserActuatorTaskClient::RunStep(const Step& step) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (step.type) {
    case Step::Type::kTaskStarted:
      fake_.SimulateTaskStarted();
      return;
    case Step::Type::kWorklogUpdate:
      fake_.SimulateWorklogUpdate(step.step_description);
      return;
    case Step::Type::kTaskCompleted:
      fake_.SimulateTaskCompleted(step.result);
      return;
  }
}

void ScriptedBrowserActuatorTaskClient::StopScenario() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  next_step_index_ = scenario_.size();
}

}  // namespace browser_actuator
