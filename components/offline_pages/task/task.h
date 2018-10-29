// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_TASK_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_TASK_TASK_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace offline_pages {

// Task interface for consumers of the TaskQueue. Implements a mechanism for
// task completion.
//
// To create your own Task:
// * Derive your new task from offline_pages::Task.
// * Implement your task with as many async operations on the controlled
//   resource as is required. (In general the smaller the task the better.)
// * Whenever the task is terminated, call |Task::TaskComplete|. This step is
//   mandatory to ensure |TaskQueue| can pick another task. It should be called
//   once, but in every situation when task is exited.
// * To schedule task for execution call |TaskQueue::AddTask|.
//
// If there is a chance that a task callback will come after the task is
// destroyed, it is up to the task to actually implement mechanism to deal with
// that, such as using a |base::WeakPtrFactory|.
class Task {
 public:
  // Signature of the method to be called by a task, when its work is done.
  typedef base::OnceCallback<void(Task*)> TaskCompletionCallback;

  Task();
  virtual ~Task();

  // Entry point to the task. This is used by the queue to start the task, and
  // first step of the task should be implemented by overloading this method.
  // The task should define an additional method for every asynchronous step,
  // with each step setting up the next step as a callback.
  virtual void Run() = 0;

  // Sets the callback normally used by |TaskQueue| for testing. See
  // |SetTaskCompletionCallback| for details.
  void SetTaskCompletionCallbackForTesting(
      TaskCompletionCallback task_completion_callback);

 protected:
  // Tasks must call |TaskComplete| as their last step. This will cause
  // |TaskQueue| to schedule the task's destruction and start another task if
  // one is available.
  void TaskComplete();

 private:
  friend class TaskQueue;

  // Allows |TaskQueue| to set the |task_completion_callback| that will be used
  // to inform it when the task is done. If the task is run outside of the
  // |TaskQueue| and completion callback is not set, it will also work.
  //
  // Note: The callback implementation is responsible for scheduling work with
  // the appropriate task runner and not to cause side effects to the |Task|
  // calling into |TaskComplete|.
  void SetTaskCompletionCallback(
      TaskCompletionCallback task_completion_callback);

  // Completion callback for this task set by |SetTaskCompletionCallback|.
  TaskCompletionCallback task_completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(Task);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_TASK_TASK_H_
