// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_LONG_TASK_SCHEDULER_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_LONG_TASK_SCHEDULER_H_

#include <memory>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/unexportable_keys/background_task_priority.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace unexportable_keys {

class BackgroundTask;

// `BackgroundLongTaskScheduler` allows scheduling `BackgroundTask`s to be run
// on a background thread. It's designed specifically to run long blocking tasks
// that cannot be run in parallel.
//
// The scheduler posts tasks to the background thread one by one to have a
// full control of which task is running next on the main thread. Since the
// tasks being run are long, the risk of running a wrong task outweighs extra
// overhead caused by additional thread hops.
//
// Supported features:
// - Multiple task priorities (defined in background_task_priority.h). Tasks
//   with a higher priority are always posted to the background thread before
//   tasks with a lower priority.
//   Lower-priority tasks are subject to starvation.
// - Dynamic priority changes. Not implemented yet.
//   TODO(b/263249728): support dynamic priorities.
// - Task cancellation. A task never runs if it gets cancelled before it's been
//   posted on the background thread.
class COMPONENT_EXPORT(UNEXPORTABLE_KEYS) BackgroundLongTaskScheduler {
 public:
  explicit BackgroundLongTaskScheduler(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  ~BackgroundLongTaskScheduler();

  BackgroundLongTaskScheduler(const BackgroundLongTaskScheduler&) = delete;
  BackgroundLongTaskScheduler& operator=(const BackgroundLongTaskScheduler&) =
      delete;

  void PostTask(std::unique_ptr<BackgroundTask> task);

 private:
  // Type representing a single task queue with a specific priority.
  using TaskQueue = base::circular_deque<std::unique_ptr<BackgroundTask>>;

  void OnTaskCompleted(BackgroundTask* task);

  void MaybeRunNextPendingTask();
  TaskQueue& GetTaskQueueForPriority(BackgroundTaskPriority priority);
  TaskQueue* GetHighestPriorityNonEmptyTaskQueue();
  std::unique_ptr<BackgroundTask> TakeNextPendingTask();

  std::array<TaskQueue, kNumTaskPriorities> task_queue_by_priority_;

  // `BackgroundTask` that is currently running on `background_task_runner_`.
  // It is nullptr if no task is running.
  std::unique_ptr<BackgroundTask> running_task_;

  // Task runner that has at most one task (`running_task_`) in its queue at any
  // moment.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  base::WeakPtrFactory<BackgroundLongTaskScheduler> weak_ptr_factory_{this};
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_LONG_TASK_SCHEDULER_H_
