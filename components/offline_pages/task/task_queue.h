// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_TASK_TASK_QUEUE_H_
#define COMPONENTS_OFFLINE_PAGES_TASK_TASK_QUEUE_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/offline_pages/task/task.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace offline_pages {

// Class for coordinating |Task|s in relation to access to a specific resource.
// As a task, we understand a set of asynchronous operations (possibly switching
// threads) that access a set of sensitive resource(s). Because the resource
// state is modified and individual steps of a task are asynchronous, allowing
// certain tasks to run in parallel may lead to incorrect results. This class
// allows for ordering of tasks in a FIFO manner, to ensure two tasks modifying
// a resources are not run at the same time.
//
// Consumers of this class should create an instance of TaskQueue and implement
// tasks that need to be run sequentially. New task will only be started when
// the previous one calls |Task::TaskComplete|.
//
// Methods on TaskQueue should be called from the same thread from which it
// is created.
class TaskQueue {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Invoked once when TaskQueue reached 0 tasks.
    virtual void OnTaskQueueIsIdle() = 0;
  };

  explicit TaskQueue(Delegate* delegate);
  ~TaskQueue();

  // Adds a task to the queue. Queue takes ownership of the task.
  void AddTask(std::unique_ptr<Task> task);
  // Whether the task queue has any pending (not-running) tasks.
  bool HasPendingTasks() const;
  // Whether there is a task currently running.
  bool HasRunningTask() const;

 private:
  // Checks whether there are any tasks to run, as well as whether no task is
  // currently running. When both are met, it will start the next task in the
  // queue.
  void StartTaskIfAvailable();

  void RunCurrentTask();

  // Callback for informing the queue that a task was completed. Can be called
  // from any thread.
  static void TaskCompletedCallback(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::WeakPtr<TaskQueue> task_queue,
      Task* task);

  void TaskCompleted(Task* task);

  void InformTaskQueueIsIdle();

  // This TaskQueue's task runner, set on construction using the instance
  // assigned to the current thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Owns and outlives this TaskQueue.
  Delegate* delegate_;

  // Currently running tasks.
  std::unique_ptr<Task> current_task_;

  // A FIFO queue of tasks that will be run using this task queue.
  base::queue<std::unique_ptr<Task>> tasks_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TaskQueue> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TaskQueue);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_TASK_TASK_QUEUE_H_
