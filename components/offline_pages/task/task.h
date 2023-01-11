// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_TASK_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_TASK_TASK_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace offline_pages {
class TaskQueue;

// A task which may run asynchronous steps. Its primary purpose is to implement
// operations to be inserted into a |TaskQueue|, however, tasks can also be run
// outside of a |TaskQueue|.
class Task {
 public:
  Task();
  virtual ~Task();
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;

  void Execute(base::OnceClosure complete_callback);

 protected:
  friend TaskQueue;
  // Entry point to the task. Called by |Execute()| to perform the task.
  // Must call |TaskComplete()| as the final step.
  virtual void Run() = 0;

  // These functions are intended to be called by the implementor of Task:

  // Tasks must call |TaskComplete()| as their last step.
  void TaskComplete();
  // Suspends task execution, and allows execution of other tasks in the queue.
  // Afterward, either `Resume()` or `TaskComplete()` should eventually be
  // called on this task, or it will remain alive until the owning `TaskQueue`
  // is destroyed. Must be called on the TaskQueue's sequence.
  void Suspend();
  // Request continuation of task execution after a prior `Suspend()` call.
  // `on_resume` is invoked when the task can being executing again. Resumed
  // tasks are given priority above other tasks. Must be called on the
  // TaskQueue's sequence.
  void Resume(base::OnceClosure on_resume);

 private:
  enum class TaskState {
    kWaiting,
    kRunning,
    kSuspended,
    kPendingResume,
    kCompleted,
  };
  // TaskQueue outlives and owns this task. Non-null only when this task is
  // owned by a task queue.
  raw_ptr<TaskQueue> task_queue_ = nullptr;
  // Reports completion or suspension to the caller.
  base::OnceClosure task_completion_callback_;
  TaskState state_ = TaskState::kWaiting;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_TASK_TASK_H_
