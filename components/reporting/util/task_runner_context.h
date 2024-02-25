// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_TASK_RUNNER_CONTEXT_H_
#define COMPONENTS_REPORTING_UTIL_TASK_RUNNER_CONTEXT_H_

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace reporting {

// This class defines context for multiple actions executed on a sequenced task
// runner with the ability to make asynchronous calls to other threads and
// resuming sequenced execution by calling |Schedule| or |ScheduleAfter|.
// Multiple actions can be scheduled at once; they will be executed on the same
// sequenced task runner. Ends execution and self-destructs when one of the
// actions calls |Response| (all previously scheduled actions must be completed
// or cancelled by then, otherwise they will crash).
//
// Code snippet:
//
// Declaration:
// class SeriesOfActionsContext {
//    public:
//     SeriesOfActionsContext(
//         ...,
//         base::OnceCallback<void(...)> callback,
//         scoped_refptr<base::SequencedTaskRunner> task_runner)
//         : TaskRunnerContext<...>(std::move(callback),
//                                  std::move(task_runner)) {}
//
//    private:
//     // Context can only be deleted by calling Response method.
//     ~SeriesOfActionsContext() override = default;
//
//     void Action1(...) {
//       ...
//       if (...) {
//         Response(...);
//         return;
//       }
//       Schedule(&SeriesOfActionsContext::Action2,
//                base::Unretained(this),
//                ...);
//       ...
//       ScheduleAfter(delay,
//                     &SeriesOfActionsContext::Action3,
//                     base::Unretained(this),
//                     ...);
//     }
//
//     void OnStart() override { Action1(...); }
//   };
//
// Usage:
//   Start<SeriesOfActionsContext>(
//       ...,
//       returning_callback,
//       base::SequencedTaskRunner::GetCurrentDefault());
//
template <typename ResponseType>
class TaskRunnerContext {
 public:
  TaskRunnerContext(const TaskRunnerContext& other) = delete;
  TaskRunnerContext& operator=(const TaskRunnerContext& other) = delete;

  // Schedules next execution (can be called from any thread).
  template <class Function, class... Args>
  void Schedule(Function&& proc, Args&&... args) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::forward<Function>(proc),
                                          std::forward<Args>(args)...));
  }

  // Schedules next execution with delay (can be called from any thread).
  template <class Function, class... Args>
  void ScheduleAfter(base::TimeDelta delay, Function&& proc, Args&&... args) {
    task_runner_->PostDelayedTask(FROM_HERE,
                                  base::BindOnce(std::forward<Function>(proc),
                                                 std::forward<Args>(args)...),
                                  delay);
  }

  // Responds to the caller once completed the work sequence
  // (can only be called by action scheduled to the sequenced task runner).
  void Response(ResponseType result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    OnCompletion(result);

    // Respond to the caller.
    CHECK(!callback_.is_null()) << "Already responded";
    std::move(callback_).Run(std::forward<ResponseType>(result));

    // Self-destruct.
    delete this;
  }

  // Helper method checks that the caller runs on valid sequence.
  // Can be used by any scheduled action.
  // No need to call it by OnStart, OnCompletion and destructor.
  // For non-debug builds it is a no-op.
  void CheckOnValidSequence() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 protected:
  // Constructor is protected, for derived class to refer to.
  TaskRunnerContext(base::OnceCallback<void(ResponseType)> callback,
                    scoped_refptr<base::SequencedTaskRunner> task_runner)
      : callback_(std::move(callback)), task_runner_(std::move(task_runner)) {
    // Constructor can be called from any thread.
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  // Context can only be deleted by calling Response method.
  virtual ~TaskRunnerContext() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(callback_.is_null()) << "Deleted without responding to the caller";
  }

 private:
  template <typename ContextType /* derived from TaskRunnerContext*/,
            class... Args>
  friend void Start(Args&&... args);

  // Hook for execution start. Should be overridden to do non-trivial work.
  virtual void OnStart() = 0;

  // Finalization action before responding and deleting the context.
  // May be overridden, if necessary.
  virtual void OnCompletion(const ResponseType& result) {}

  // Wrapper for OnStart to mandate sequence checker.
  void OnStartWrap() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    OnStart();
  }

  // User callback to deliver result.
  base::OnceCallback<void(ResponseType)> callback_;

  // Sequential task runner (guarantees that each action is executed
  // sequentially in order of submission).
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Constructs the context and starts execution on the assigned sequential task
// runner. Can be called from any thread to schedule the first action in the
// sequence.
template <typename ContextType /* derived from TaskRunnerContext*/,
          class... Args>
void Start(Args&&... args) {
  ContextType* const context = new ContextType(std::forward<Args>(args)...);
  // Start execution handing |context| over to the callback, in order
  // to make sure final |OnStart| (with possible |Response| and self-destruct)
  // can only happen on |task_runner|.
  context->task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ContextType::OnStartWrap, base::Unretained(context)));
}

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_TASK_RUNNER_CONTEXT_H_
