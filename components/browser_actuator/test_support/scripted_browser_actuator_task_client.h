// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_SCRIPTED_BROWSER_ACTUATOR_TASK_CLIENT_H_
#define COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_SCRIPTED_BROWSER_ACTUATOR_TASK_CLIENT_H_

#include <stddef.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/browser_actuator/public/browser_actuator_task_client.h"
#include "components/browser_actuator/test_support/fake_browser_actuator_task_client.h"

namespace browser_actuator {

// A `BrowserActuatorTaskClient` that owns a `FakeBrowserActuatorTaskClient` and
// plays a scripted `Scenario` against it.
//
// `FakeBrowserActuatorTaskClient` stays journey-agnostic and handles protocol
// state, idle timeout, and cancellation. This wrapper schedules each `Step` as
// a posted (optionally delayed) task on the current sequence after
// `StartTask()`, driving the inner fake through its `Simulate*()` methods so
// every observer callback stays asynchronous.
//
// `Cancel()`, `SimulateShutdown()`, task completion, or destroying this object
// stops any remaining steps.
class ScriptedBrowserActuatorTaskClient : public BrowserActuatorTaskClient {
 public:
  // The `Result::error_message` of the "start-failed" scenario.
  static constexpr std::string_view kStartFailedErrorMessage =
      "Fake: start failed";
  // The `Result::error_message` of the "session-dropped" scenario.
  static constexpr std::string_view kSessionDroppedErrorMessage =
      "Fake: session dropped";

  // One scripted event. `delay` is measured from the previous step (or from
  // `StartTask()` for the first step).
  struct Step {
    enum class Type { kTaskStarted, kWorklogUpdate, kTaskCompleted };

    static Step TaskStarted(base::TimeDelta delay = base::TimeDelta());
    static Step WorklogUpdate(std::string step_description,
                              base::TimeDelta delay = base::TimeDelta());
    static Step TaskCompleted(CompletionStatus status,
                              base::TimeDelta delay = base::TimeDelta(),
                              std::string error_message = std::string());

    Step();
    Step(const Step&);
    Step& operator=(const Step&);
    Step(Step&&) noexcept;
    Step& operator=(Step&&) noexcept;
    ~Step();

    Type type = Type::kTaskStarted;
    base::TimeDelta delay;
    // Used only by `kWorklogUpdate`.
    std::string step_description;
    // Used only by `kTaskCompleted`.
    Result result;
  };
  using Scenario = std::vector<Step>;

  // Returns the named scenario, or `std::nullopt` if `name` is unknown. Named
  // scenarios use short delays so that a person can watch them in the UI.
  static std::optional<Scenario> GetScenario(std::string_view name);

  // Returns the names that `GetScenario()` accepts.
  static std::vector<std::string_view> GetScenarioNames();

  explicit ScriptedBrowserActuatorTaskClient(Scenario scenario);
  ~ScriptedBrowserActuatorTaskClient() override;

  // BrowserActuatorTaskClient:
  void StartTask(UseCase use_case,
                 const google::protobuf::MessageLite& feature_payload,
                 Observer* observer) override;
  void Cancel() override;

  // Stops the scenario and forwards `SimulateShutdown()` to the inner fake.
  void SimulateShutdown();

  // Direct access to the underlying journey-agnostic fake for configuration
  // (e.g. `SetIdleTimeout()`) or state inspection.
  FakeBrowserActuatorTaskClient& fake();
  const FakeBrowserActuatorTaskClient& fake() const;

 private:
  void ScheduleNextStep();
  void RunNextStep();
  void RunStep(const Step& step);
  void StopScenario();

  SEQUENCE_CHECKER(sequence_checker_);

  FakeBrowserActuatorTaskClient fake_ GUARDED_BY_CONTEXT(sequence_checker_);
  Scenario scenario_ GUARDED_BY_CONTEXT(sequence_checker_);
  size_t next_step_index_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  bool is_shut_down_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Invalidated by `StopScenario()` so posted steps never run after
  // cancellation, shutdown, or completion, and to detect destruction of `this`
  // inside an observer callback.
  base::WeakPtrFactory<ScriptedBrowserActuatorTaskClient> weak_factory_{this};
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_SCRIPTED_BROWSER_ACTUATOR_TASK_CLIENT_H_
