// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_TASK_TASK_MANAGER_IMPL_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_TASK_TASK_MANAGER_IMPL_H_

#include <stdint.h>
#include <map>

#include "components/download/public/task/task_manager.h"
#include "components/download/public/task/task_scheduler.h"

namespace download {

using TaskFinishedCallback = base::OnceCallback<void(bool)>;

// A class to manage the calls made to the TaskScheduler, that abstracts away
// the details of the TaskScheduler from the calling code. The tasks can run
// independently of each other as long as they have different |task_type|.
// Scheduling another task of same |task_type| before the task is started will
// overwrite the params of the scheduled task.
class TaskManagerImpl : public TaskManager {
 public:
  explicit TaskManagerImpl(std::unique_ptr<TaskScheduler> task_scheduler);

  TaskManagerImpl(const TaskManagerImpl&) = delete;
  TaskManagerImpl& operator=(const TaskManagerImpl&) = delete;

  ~TaskManagerImpl() override;

  // Called to schedule a new task. Overwrites the params if a task of the same
  // type is already scheduled. If the task is currently running, it will cache
  // the params and schedule the task after the completion/stopping of the
  // current task.
  void ScheduleTask(DownloadTaskType task_type,
                    const TaskParams& params) override;

  // Called to unschedule a scheduled task of the given type if it is not yet
  // started. Doesn't cancel the currently running task.
  void UnscheduleTask(DownloadTaskType task_type) override;

  // Called when the system starts a scheduled task. The callback will be cached
  // by the class and run after receiving a call to NotifyTaskFinished().
  void OnStartScheduledTask(DownloadTaskType task_type,
                            TaskFinishedCallback callback) override;

  // Called when the system decides to stop an already running task.
  void OnStopScheduledTask(DownloadTaskType task_type) override;

  // Should be called once the task is complete. The callback passed through
  // OnStartScheduledTask() will be run in order to notify that the task is done
  // and the system should reschedule the task with the original params if
  // |needs_reschedule| is true. If there are pending params for a new task, a
  // new task will be scheduled immediately and reschedule logic will not be
  // run.
  void NotifyTaskFinished(DownloadTaskType task_type,
                          bool needs_reschedule) override;

 private:
  // Whether a task of the given type is already running.
  bool IsRunningTask(DownloadTaskType task_type) const;

  std::unique_ptr<TaskScheduler> task_scheduler_;

  // Contains the params for the currently running task, which gets cleared
  // after the task is completed or stopped.
  std::map<DownloadTaskType, TaskParams> current_task_params_;

  // Contains params for the task to be scheduled. If a task is currently
  // running, the new task will be scheduled after the current task is finished
  // or immediately if there is no task running right now. The params will move
  // to current_task_params_ when the task is started.
  std::map<DownloadTaskType, TaskParams> pending_task_params_;

  // Contains the callbacks passed through the OnStartScheduledTask(). These
  // will be cleared when the task is completed or stopped by the system.
  std::map<DownloadTaskType, TaskFinishedCallback> task_finished_callbacks_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_TASK_TASK_MANAGER_IMPL_H_
