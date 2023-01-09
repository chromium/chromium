// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_EXTENSION_TASK_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_EXTENSION_TASK_MANAGER_H_


#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/credential_provider/extension/task.h"
#include "net/base/backoff_entry.h"

namespace credential_provider {
namespace extension {

using TaskCreator = base::RepeatingCallback<std::unique_ptr<Task>()>;

// Manager for all the tasks that are supposed to be executed by GCPW
// extension. Tasks are registered and execution is triggered via TaskManager.
class TaskManager {
 public:
  // Used to retrieve singleton instance of the TaskManager.
  static TaskManager* Get();

  // This function schedules periodic tasks using PostDelayedTask interface. It
  // schedules the tasks on the provided |task_runner|.
  virtual void RunTasks(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Each implementation needs to register the task with the task manager so
  // that they can be executed at runtime.
  virtual void RegisterTask(const std::string& task_name,
                            TaskCreator task_creator);

 protected:
  TaskManager();

  virtual ~TaskManager();

  // Returns the storage used for the instance pointer.
  static TaskManager** GetInstanceStorage();

  // Executes the task with the name |task_name| and re-schedules it for the
  // next execution.
  virtual void ExecuteTask(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const std::string& task_name);

 private:
  // List of tasks registered to be executed at every periodic run.
  std::map<std::string, TaskCreator> task_list_;

  // If a task fails, the next execution time is calculated based on the backoff
  // entry created and kept as long as task fails.
  std::map<std::string, std::unique_ptr<net::BackoffEntry>>
      task_execution_backoffs_;

  // Set of backoff policies for the tasks where there is a backoff entry.
  std::map<std::string, std::unique_ptr<net::BackoffEntry::Policy>>
      task_execution_policies_;
};

}  // namespace extension
}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_EXTENSION_TASK_MANAGER_H_
