// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_TASK_TASK_QUEUE_H_
#define COMPONENTS_OFFLINE_PAGES_TASK_TASK_QUEUE_H_

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
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
    virtual ~Delegate() = default;

    // Invoked once when TaskQueue reached 0 tasks.
    virtual void OnTaskQueueIsIdle() = 0;
  };

  explicit TaskQueue(Delegate* delegate);

  TaskQueue(const TaskQueue&) = delete;
  TaskQueue& operator=(const TaskQueue&) = delete;

  ~TaskQueue();

  // Adds a task to the queue. Queue takes ownership of the task. Optionally,
  // use FROM_HERE as the first parameter for debugging.
  void AddTask(std::unique_ptr<Task> task);
  void AddTask(const base::Location& from_here, std::unique_ptr<Task> task);

  // Whether the task queue has any pending (not-running) tasks.
  bool HasPendingTasks() const;
  // Whether there is a task currently running.
  bool HasRunningTask() const;
  // Returns a human-readable string describing the contents of the task queue.
  std::string GetStateForTesting() const;

 private:
  friend Task;
  struct Entry;
  // Checks whether there are any tasks to run, as well as whether no task is
  // currently running. When both are met, it will start the next task in the
  // queue.
  void StartTaskIfAvailable();

  void RunCurrentTask();
  void ResumeCurrentTask(base::OnceClosure on_resume);

  // Callback for informing the queue that a task was completed. Can be called
  // from any thread.
  static void TaskCompletedCallback(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::WeakPtr<TaskQueue> task_queue,
      Task* task);

  void SuspendTask(Task* task);
  void ResumeTask(Task* task, base::OnceClosure on_resume);
  void TaskCompleted(Task* task);

  void InformTaskQueueIsIdle();

  // This TaskQueue's task runner, set on construction using the instance
  // assigned to the current thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Owns and outlives this TaskQueue.
  raw_ptr<Delegate> delegate_;

  // Currently running tasks.
  std::unique_ptr<Task> current_task_;
  base::Location current_task_location_;

  // A FIFO queue of tasks that will be run using this task queue.
  base::circular_deque<Entry> tasks_;

  // A set of tasks which are suspended.
  std::vector<Entry> suspended_tasks_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TaskQueue> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_TASK_TASK_QUEUE_H_
