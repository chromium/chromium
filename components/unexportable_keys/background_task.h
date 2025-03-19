// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_H_

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/background_task_type.h"

namespace base {
class SequencedTaskRunner;
}

namespace unexportable_keys {

// Interface for tasks scheduled on `BackgroundLongTaskScheduler`.
//
// A typical task lifetime is the following:
//   1) Task is created.
//   2) Task is added to a `BackgroundLongTaskScheduler` queue.
//   3) `BackgroundLongTaskScheduler` calls `Run()`.
//   4) Task completes.
//      a) If the the task should not be retried, `BackgroundLongTaskScheduler`
//         calls `ReplyWithResult()` and deletes the task, otherwise
//      b) `BackgroundLongTaskScheduler` calls `ResetStateBeforeRetry()` and
//         goes back to step 2)
class COMPONENT_EXPORT(UNEXPORTABLE_KEYS) BackgroundTask {
 public:
  // Different statuses that the task can have.
  enum class Status {
    kPending,   // The task is waiting in a queue.
    kCanceled,  // The task has been canceled by the caller.
    kPosted     // The task has been posted on the background thread.
  };

  virtual ~BackgroundTask() = default;

  // Runs the task on `background_task_runner` and invokes
  // `on_complete_callback` with `this` on the posting thread once the task
  // completes.
  virtual void Run(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      base::OnceCallback<void(BackgroundTask* task)> on_complete_callback) = 0;

  // Invokes the "reply" callback (if any) to return the result back to the
  // client.
  // Must be called after the task is completed, and no more than once.
  virtual void ReplyWithResult() = 0;

  // Resets the task state before `Run()` can be run again.
  virtual void ResetStateBeforeRetry() = 0;

  // Returns the current status of the task.
  virtual Status GetStatus() const = 0;

  // Returns the current priority of the task.
  virtual BackgroundTaskPriority GetPriority() const = 0;

  // Returns the task type.
  virtual BackgroundTaskType GetType() const = 0;

  // Returns the elapsed time since the task was scheduled.
  virtual base::TimeDelta GetElapsedTimeSinceScheduled() const = 0;

  // Returns the elapsed time since the task was run.
  // Returns std::nullopt if the task hasn't been run yet.
  virtual std::optional<base::TimeDelta> GetElapsedTimeSinceRun() const = 0;

  // Returns the number of times this task was retried.
  virtual size_t GetRetryCount() const = 0;

  // Returns whether the task should be retried.
  // Must be called after the task is completed but before `ReplyWithResult()`
  // or `ResetStateBeforeRetry()`.
  virtual bool ShouldRetry() const = 0;
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_H_
