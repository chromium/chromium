// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_TASK_SCHEDULER_H_
#define CHROME_UPDATER_WIN_TASK_SCHEDULER_H_

#include <stdint.h>

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"

namespace base {
class CommandLine;
class Time;
}  // namespace base

namespace updater {

enum class UpdaterScope;

// This class wraps a scheduled task and expose an API to parametrize a task
// before calling |Register|, or to verify its existence, or delete it.
class TaskScheduler : public base::RefCountedThreadSafe<TaskScheduler> {
 public:
  // The types of trigger to register for this task. Multiple triggers types can
  // be combined using the bitwise OR operator.
  enum TriggerType {
    // Run when the current user logs on (for user installs). Or when any user
    // logs on (for system installs).
    TRIGGER_TYPE_LOGON = 1 << 0,

    // Run right now (mainly for tests).
    TRIGGER_TYPE_NOW = 1 << 1,

    // Run every hour.
    TRIGGER_TYPE_HOURLY = 1 << 2,

    // Run every five hours.
    TRIGGER_TYPE_EVERY_FIVE_HOURS = 1 << 3,

    TRIGGER_TYPE_MAX = 1 << 4,
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

    // The task is run as a service for user and as such will be on an
    // invisible desktop.
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

    // Description of the task.
    std::wstring description;

    // A scheduled task can have more than one action associated with it and
    // actions can be of types other than executables (for example, sending
    // emails). This list however contains only the execution actions.
    std::vector<TaskExecAction> exec_actions;

    // The log-on requirements for the task's actions to be run. A bit mask with
    // the mapping defined by LogonType.
    uint32_t logon_type = 0;

    // User ID under which the task runs.
    std::wstring user_id;

    int trigger_types = 0;
  };

  // Creates an instance of the task scheduler for the given `scope`.
  // `use_task_subfolders` dictates whether the scheduler creates and works with
  // tasks that are created within a subfolder (`true` by default), or tasks
  // that are created at the root folder. When `use_task_subfolders` is `true`,
  // the tasks are created within the subfolder returned by
  // `GetTaskSubfolderName()`.
  static scoped_refptr<TaskScheduler> CreateInstance(
      UpdaterScope scope,
      bool use_task_subfolders = true);

  TaskScheduler(const TaskScheduler&) = delete;
  TaskScheduler& operator=(const TaskScheduler&) = delete;

  // Identify whether the task is registered or not.
  [[nodiscard]] virtual bool IsTaskRegistered(
      const std::wstring& task_name) = 0;

  // Return the time of the next schedule run for the given task name. Return
  // false on failure.
  // `next_run_time` is returned as local time on the current system, not UTC.
  [[nodiscard]] virtual bool GetNextTaskRunTime(const std::wstring& task_name,
                                                base::Time& next_run_time) = 0;

  // Delete the task if it exists. No-op if the task doesn't exist. Return false
  // on failure to delete an existing task.
  [[nodiscard]] virtual bool DeleteTask(const std::wstring& task_name) = 0;

  // Enable or disable task based on the value of |enabled|. Return true if the
  // task exists and the operation succeeded.
  [[nodiscard]] virtual bool SetTaskEnabled(const std::wstring& task_name,
                                            bool enabled) = 0;

  // Return true if task exists and is enabled.
  [[nodiscard]] virtual bool IsTaskEnabled(const std::wstring& task_name) = 0;

  // Return true if task exists and is running.
  [[nodiscard]] virtual bool IsTaskRunning(const std::wstring& task_name) = 0;

  // List all currently registered scheduled tasks.
  [[nodiscard]] virtual bool GetTaskNameList(
      std::vector<std::wstring>& task_names) = 0;

  // Returns the first instance of a scheduled task installed with the given
  // `task_prefix`.
  [[nodiscard]] virtual std::wstring FindFirstTaskName(
      const std::wstring& task_prefix) = 0;

  // Return detailed information about a task. Return true if no errors were
  // encountered. On error, the struct is left unmodified.
  [[nodiscard]] virtual bool GetTaskInfo(const std::wstring& task_name,
                                         TaskInfo& info) = 0;

  // Returns true if the task folder specified by |folder_name| exists.
  [[nodiscard]] virtual bool HasTaskFolder(const std::wstring& folder_name) = 0;

  // Register the task to run the specified application and using the given
  // `trigger_types`. `trigger_types` is a bitwise OR of one or more types in
  // the `TriggerType` enum.
  [[nodiscard]] virtual bool RegisterTask(const std::wstring& task_name,
                                          const std::wstring& task_description,
                                          const base::CommandLine& run_command,
                                          int trigger_types,
                                          bool hidden) = 0;

  // Returns true if the scheduled task specified by |task_name| can be started
  // successfully or is currently running.
  [[nodiscard]] virtual bool StartTask(const std::wstring& task_name) = 0;

  // Name of the sub-folder that the scheduled tasks are created in, prefixed
  // with the company folder `GetTaskCompanyFolder`.
  [[nodiscard]] virtual std::wstring GetTaskSubfolderName() = 0;

  // Runs `callback` for each task that matches `prefix`.
  virtual void ForEachTaskWithPrefix(
      const std::wstring& prefix,
      base::FunctionRef<void(const std::wstring&)> callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<TaskScheduler>;
  TaskScheduler();
  virtual ~TaskScheduler() = default;
};

std::ostream& operator<<(std::ostream& stream,
                         const TaskScheduler::TaskExecAction& t);
std::ostream& operator<<(std::ostream& stream,
                         const TaskScheduler::TaskInfo& t);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_TASK_SCHEDULER_H_
