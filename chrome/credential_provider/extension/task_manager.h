// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_EXTENSION_TASK_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_EXTENSION_TASK_MANAGER_H_

#include "chrome/credential_provider/extension/task.h"

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"

namespace credential_provider {
namespace extension {

using TaskCreator = base::RepeatingCallback<std::unique_ptr<Task>()>;

// Utility to make sure the registry is updated with the last periodic sync.
class LastPeriodicSyncUpdater {
 public:
  LastPeriodicSyncUpdater();
  virtual ~LastPeriodicSyncUpdater();

 private:
  // Update the registry with the last time the periodic tasks are executed.
  virtual void UpdateLastRunTimestamp();
};

class TaskManager {
 public:
  // Used to retrieve singleton instance of the TaskManager.
  static TaskManager* Get();

  TaskManager();

  virtual ~TaskManager();

  // Returns the storage used for the instance pointer.
  static TaskManager** GetInstanceStorage();

  // This function schedules periodic tasks using PostTask interface. It
  // schedules the tasks on the provided |task_runner|.
  virtual void RunTasks(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Each implementation needs to register the task with the task manager so
  // that they can be executed at runtime.
  virtual void RegisterTask(const std::string& task_name,
                            TaskCreator task_creator);

  // Can be called to end the periodically running tasks.
  virtual void Quit();

 protected:
  // Actual method which goes through registered tasks and runs them.
  virtual void RunTasksInternal();

 private:
  // Schedules a RepeatingTimer with the period specified in TaskManagerConfig.
  virtual void ScheduleTasks();

  // Returns the elapsed time delta since the last time the periodic sync were
  // successfully performed.
  base::TimeDelta GetTimeDeltaSinceLastPeriodicSync();

  // List of tasks registered to be executed at every periodic run.
  std::map<std::string, TaskCreator> task_list_;

  // The timer that controls posting task to be executed every time the period
  // elapses.
  base::RepeatingTimer timer_;
};

}  // namespace extension
}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_EXTENSION_TASK_MANAGER_H_
