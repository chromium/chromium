// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_FAKE_BROWSER_ACTUATOR_TASK_CLIENT_H_
#define COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_FAKE_BROWSER_ACTUATOR_TASK_CLIENT_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/browser_actuator/public/browser_actuator_task_client.h"

namespace browser_actuator {

// A fake `BrowserActuatorTaskClient` that needs no server and no transport.
// Consumers of the task client (the actor task UI model, the side panel, and
// their tests) use it to drive every task lifecycle locally.
//
// The test drives it by calling `SimulateTaskStarted()`,
// `SimulateWorklogUpdate()`, and `SimulateTaskCompleted()`. These stand for
// server events and notify the observer synchronously, from the test's own
// call stack.
//
// The fake also handles these cases:
//
// - It drops events before `StartTask()`, worklog updates before
//   `SimulateTaskStarted()`, and events after the task completes.
// - It reports `OnTaskStarted()` at most once.
// - It fails the task when no progress arrives within the idle timeout
//   (`kDefaultIdleTimeout` unless `SetIdleTimeout()` changes it).
// - `SimulateShutdown()` detaches like `KeyedService::Shutdown()`: no more
//   observer callbacks. Calling `StartTask()` after `SimulateShutdown()` posts
//   `OnTaskCompleted(kFailed)`.
//
// The fake never calls the observer synchronously inside `StartTask()`,
// `Cancel()`, or the destructor, and it is safe to destroy the fake inside any
// observer callback.
//
// Code that gets its client from `BrowserActuatorService::CreateTaskClient()`
// can get the fake in tests through `MockBrowserActuatorService`:
//
//   auto fake = std::make_unique<FakeBrowserActuatorTaskClient>();
//   FakeBrowserActuatorTaskClient* fake_ptr = fake.get();
//   EXPECT_CALL(mock_service, CreateTaskClient())
//       .WillOnce(Return(ByMove(std::move(fake))));
//   // ... code under test calls StartTask() ...
//   fake_ptr->SimulateTaskStarted();
class FakeBrowserActuatorTaskClient : public BrowserActuatorTaskClient {
 public:
  // Default for `SetIdleTimeout()`.
  static constexpr base::TimeDelta kDefaultIdleTimeout = base::Minutes(5);
  // The `Result::error_message` of an idle timeout. Like every
  // `error_message`, it is for diagnostics only.
  static constexpr std::string_view kIdleTimeoutErrorMessage =
      "Fake: idle timeout";
  // The `Result::error_message` when `StartTask()` runs after
  // `SimulateShutdown()`.
  static constexpr std::string_view kShutdownErrorMessage =
      "Fake: service shut down";

  FakeBrowserActuatorTaskClient();
  ~FakeBrowserActuatorTaskClient() override;

  // Sets how long the task may go without progress (start, task started, or a
  // worklog update) before it fails with `kIdleTimeoutErrorMessage`. A zero or
  // negative value turns the timeout off. The default is
  // `kDefaultIdleTimeout`. Call before `StartTask()`.
  void SetIdleTimeout(base::TimeDelta idle_timeout);

  // BrowserActuatorTaskClient:
  void StartTask(UseCase use_case,
                 const google::protobuf::MessageLite& feature_payload,
                 Observer* observer) override;
  void Cancel() override;

  // Server events. Each one does nothing unless a task is running.
  void SimulateTaskStarted();
  void SimulateWorklogUpdate(std::string_view step_description);
  void SimulateTaskCompleted(const Result& result);

  // Stands for `KeyedService::Shutdown()`, which the interface says aborts
  // silently: drops the observer, and no observer callback runs after this, not
  // even a pending cancellation. Calling `StartTask()` on a detached client
  // posts `OnTaskCompleted(kFailed)`. Does nothing on a client whose task
  // already completed.
  void SimulateShutdown();

  // Inspection.
  bool start_task_called() const;
  std::optional<UseCase> use_case() const;
  // The `google.protobuf.Any` type URL of the `feature_payload` passed to
  // `StartTask()`: "type.googleapis.com/" followed by the message type name.
  const std::string& feature_payload_type_url() const;
  // The serialized `feature_payload` passed to `StartTask()`.
  const std::string& serialized_feature_payload() const;
  int cancel_call_count() const;
  // True once `OnTaskCompleted()` has run.
  bool is_completed() const;

 private:
  enum class State {
    kNotStarted,
    kRunning,
    // `Cancel()` posted `OnTaskCompleted(kCancelled)`, which has not run yet.
    kCancelling,
    kCompleted,
    // `SimulateShutdown()` ran.
    kDetached,
  };

  void NotifyTaskStarted();
  void NotifyWorklogUpdate(std::string_view step_description);
  void NotifyTaskCompleted(const Result& result);
  void ResetIdleTimer();
  void OnIdleTimeout();

  SEQUENCE_CHECKER(sequence_checker_);

  State state_ GUARDED_BY_CONTEXT(sequence_checker_) = State::kNotStarted;
  bool task_started_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  raw_ptr<Observer> observer_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
  std::optional<UseCase> use_case_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string feature_payload_type_url_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string serialized_feature_payload_ GUARDED_BY_CONTEXT(sequence_checker_);
  int cancel_call_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  base::TimeDelta idle_timeout_ GUARDED_BY_CONTEXT(sequence_checker_) =
      kDefaultIdleTimeout;
  base::OneShotTimer idle_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Used for the posted cancellation. Invalidated by `SimulateShutdown()`.
  base::WeakPtrFactory<FakeBrowserActuatorTaskClient> weak_factory_{this};
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_FAKE_BROWSER_ACTUATOR_TASK_CLIENT_H_
