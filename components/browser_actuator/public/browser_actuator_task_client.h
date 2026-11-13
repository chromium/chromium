// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_BROWSER_ACTUATOR_TASK_CLIENT_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_BROWSER_ACTUATOR_TASK_CLIENT_H_

#include <string>
#include <string_view>

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace browser_actuator {

// Client interface for starting and observing direct task execution.
class BrowserActuatorTaskClient {
 public:
  enum class UseCase { kAutoPasswordChange };
  enum class CompletionStatus { kSucceeded, kFailed, kCancelled };

  // Final outcome and diagnostic details of an executed task.
  struct Result {
    CompletionStatus status = CompletionStatus::kFailed;
    std::string error_message;  // Diagnostics only. Never shown in UI.
  };

  // Observer interface for receiving task lifecycle and progress events.
  class Observer {
   public:
    virtual ~Observer();

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    virtual void OnTaskStarted();

    // Invoked when an intermediate step update is reported.
    // NOTE: `step_description` contains untrusted remote text. Render strictly
    // as plain text; never log to UMA/UKM or DLOG.
    virtual void OnWorklogUpdate(std::string_view step_description);

    // May run without `OnTaskStarted()`.
    virtual void OnTaskCompleted(const Result& result) = 0;

   protected:
    Observer();
  };

  virtual ~BrowserActuatorTaskClient();

  BrowserActuatorTaskClient(const BrowserActuatorTaskClient&) = delete;
  BrowserActuatorTaskClient& operator=(const BrowserActuatorTaskClient&) =
      delete;

  // Starts execution of the specified task. Valid once per instance.
  //
  // `observer` must be non-null and must remain valid until `OnTaskCompleted()`
  // runs unless `this` is destroyed first (including across `Cancel()`, which
  // posts `OnTaskCompleted()` asynchronously). `this` may be destroyed inside
  // any `Observer` callback.
  //
  // Not every outcome reaches `observer`: destroying `this` or
  // `KeyedService::Shutdown()` aborts silently without `OnTaskCompleted()`.
  //
  // Callbacks always run asynchronously on the UI sequence—never synchronously
  // inside `StartTask()`, `Cancel()`, or `~BrowserActuatorTaskClient()`.
  virtual void StartTask(UseCase use_case,
                         const google::protobuf::MessageLite& feature_payload,
                         Observer* observer) = 0;

  // Cancels an ongoing task, resulting in an asynchronous `OnTaskCompleted()`
  // call with `CompletionStatus::kCancelled`.
  //
  // Calling `Cancel()` is a no-op if the task was not started or has already
  // completed.
  virtual void Cancel() = 0;

 protected:
  BrowserActuatorTaskClient();
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_BROWSER_ACTUATOR_TASK_CLIENT_H_
