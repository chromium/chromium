// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_TASK_SCHEDULER_H_
#define CHROME_CHROME_CLEANER_OS_TASK_SCHEDULER_H_

#include <stdint.h>

#include <vector>

#include "base/files/file_path.h"

namespace base {
class CommandLine;
class Time;
}  // namespace base

namespace chrome_cleaner {

// This class wraps a scheduled task and expose an API to parametrize a task
// before calling |Register|, or to verify its existence, or delete it.
class TaskScheduler {
 public:
  // The type of trigger to register for this task.
  enum TriggerType {
    // Only run once post-reboot.
    TRIGGER_TYPE_POST_REBOOT = 0,
    // Run right now (mainly for tests).
    TRIGGER_TYPE_NOW = 1,
    // Run every hour.
    TRIGGER_TYPE_HOURLY = 2,
    TRIGGER_TYPE_EVERY_SIX_HOURS = 3,
    TRIGGER_TYPE_MAX,
  };

  // The log-on requirements for a task to be scheduled. Note that a task can
  // have both the interactive and service bit set. In that case the
  // interactive token will be used when available, and a stored password
  // otherwise.
  enum LogonType {
    LOGON_UNKNOWN = 0,
    // Run the task with the user's interactive token when logged in.
    LOGON_INTERACTIVE = 1 << 0,
    // The task will run whether the user is logged in or not using either a
    // user/password specified at registration time, a service account or a
    // service for user (S4U).
    LOGON_SERVICE = 1 << 1,
    // Vista and later only: the task is run as a service for user and as such
    // will be on an invisible desktop.
    LOGON_S4U = 1 << 2,
  };

  // Struct representing a single scheduled task action.
  struct TaskExecAction {
    base::FilePath application_path;
    base::FilePath working_dir;
    std::wstring arguments;
  };

  // Detailed description of a scheduled task. This type is returned by the
  // GetTaskInfo() method.
  struct TaskInfo {
    TaskInfo();
    TaskInfo(const TaskInfo&);
    TaskInfo(TaskInfo&&);
    ~TaskInfo();
    TaskInfo& operator=(const TaskInfo&);
    TaskInfo& operator=(TaskInfo&&);

    std::wstring name;
    // Description (Vista and later) or comment (XP and earlier) of the task.
    std::wstring description;
    // On Windows Vista and later, a scheduled task can have more than one
    // action associated with it and actions can be of types other than
    // executables (for example, sending emails). This list however contains
    // only the execution actions.
    std::vector<TaskExecAction> exec_actions;
    // The log-on requirements for the task's actions to be run. A bit mask with
    // the mapping defined by LogonType.
    uint32_t logon_type = 0;
  };

  // Control the lifespan of static data for the TaskScheduler. |Initialize|
  // must be called before the first call to |CreateInstance|, and not other
  // methods can be called after |Terminate| was called (unless |Initialize| is
  // called again). |Initialize| can't be called out of balance with
  // |Terminate|. |Terminate| can be called any number of times.
  static bool Initialize();
  static void Terminate();

  // Instantiate the proper version based on the Windows version. Ownership is
  // transfered to caller.
  static TaskScheduler* CreateInstance();

  TaskScheduler(const TaskScheduler&) = delete;
  TaskScheduler& operator=(const TaskScheduler&) = delete;

  virtual ~TaskScheduler() {}

  // Set a delegate to be given when creating new instances of a mock
  // TaskScheduler. Return to regular TaskScheduler when set to null. This is to
  // allow using the same instance of a mock_delegate even though
  // |CreateInstance| will always create a new instance to be owned by the
  // caller.
  static void SetMockDelegateForTesting(TaskScheduler* mock_delegate) {
    mock_delegate_ = mock_delegate;
  }

  // Identify whether the task is registered or not.
  virtual bool IsTaskRegistered(const wchar_t* task_name) = 0;

  // Return the time of the next schedule run for the given task name. Return
  // false on failure.
  virtual bool GetNextTaskRunTime(const wchar_t* task_name,
                                  base::Time* next_run_time) = 0;

  // Delete the task if it exists. No-op if the task doesn't exist. Return false
  // on failure to delete an existing task.
  virtual bool DeleteTask(const wchar_t* task_name) = 0;

  // Enable or disable task based on the value of |enabled|. Return true if the
  // task exists and the operation succeeded.
  virtual bool SetTaskEnabled(const wchar_t* task_name, bool enabled) = 0;

  // Return true if task exists and is enabled.
  virtual bool IsTaskEnabled(const wchar_t* task_name) = 0;

  // List all currently registered scheduled tasks.
  virtual bool GetTaskNameList(std::vector<std::wstring>* task_names) = 0;

  // Return detailed information about a task. Return true if no errors were
  // encountered. On error, the struct is left unmodified.
  virtual bool GetTaskInfo(const wchar_t* task_name, TaskInfo* info) = 0;

  // Register the task to run the specified application and using the given
  // |trigger_type|.
  virtual bool RegisterTask(const wchar_t* task_name,
                            const wchar_t* task_description,
                            const base::CommandLine& run_command,
                            TriggerType trigger_type,
                            bool hidden) = 0;

 protected:
  TaskScheduler();

 private:
  static TaskScheduler* mock_delegate_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_TASK_SCHEDULER_H_
