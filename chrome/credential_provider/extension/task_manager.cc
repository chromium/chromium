// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/extension/task_manager.h"

#include <windows.h>

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/credential_provider/extension/extension_utils.h"
#include "chrome/credential_provider/extension/user_context_enumerator.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {
namespace extension {

namespace {

// Backoff policy for errors returned by performing task operation.
const net::BackoffEntry::Policy kRetryLaterPolicy = {
    // Number of initial errors to ignore before starting to back off.
    0,

    // Initial delay in ms: 1 minute.
    1000 * 60,

    // Factor by which the waiting time is multiplied.
    2,

    // Fuzzing percentage; this spreads delays randomly between 80% and 100%
    // of the calculated time.
    0.20,

    // Maximum delay in ms: 3 hours. However this field of back off policy is
    // overridden by individual task's execution period when actually backing
    // off.
    1000 * 60 * 60 * 3,

    // When to discard an entry: never.
    -1,

    // |always_use_initial_delay|; false means that the initial delay is
    // applied after the first error, and starts backing off from there.
    true,
};

// Returns the elapsed time delta since the last time the periodic sync were
// successfully performed for the given task registry.
base::TimeDelta GetTimeDeltaSinceLastPeriodicSync(
    const std::wstring& task_reg_name) {
  wchar_t last_sync_millis[512];
  ULONG last_sync_size = std::size(last_sync_millis);
  HRESULT hr = GetGlobalFlag(task_reg_name, last_sync_millis, &last_sync_size);

  if (FAILED(hr)) {
    // The periodic sync has never happened before.
    return base::TimeDelta::Max();
  }

  int64_t last_sync_millis_int64;
  base::StringToInt64(last_sync_millis, &last_sync_millis_int64);
  const auto last_sync = base::Time::FromDeltaSinceWindowsEpoch(
      base::Milliseconds(last_sync_millis_int64));
  return base::Time::Now() - last_sync;
}

}  // namespace

// static
TaskManager** TaskManager::GetInstanceStorage() {
  static TaskManager* instance = new TaskManager();

  return &instance;
}

// static
TaskManager* TaskManager::Get() {
  return *GetInstanceStorage();
}

TaskManager::TaskManager() {}

TaskManager::~TaskManager() {}

void TaskManager::ExecuteTask(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const std::string& task_name) {
  LOGFN(VERBOSE);

  // Get an instance of the task and execute it.
  std::unique_ptr<Task> task((task_list_[task_name]).Run());
  if (task == nullptr) {
    LOGFN(ERROR) << task_name << " task is null";
    return;
  }

  HRESULT hr =
      UserContextEnumerator::Get()->PerformTask(task_name, *task.get());

  // Calculate next time the task should be executed.
  base::TimeDelta next_run;

  if (FAILED(hr)) {
    LOGFN(ERROR) << task_name << " failed hr=" << putHR(hr);

    // Check whether a backoff entry exists for the task. If so, consider the
    // backoff period when calculating next run.
    if (task_execution_backoffs_.find(task_name) ==
        task_execution_backoffs_.end()) {
      // First set the template backoff policy for the task.
      task_execution_policies_[task_name] =
          std::make_unique<net::BackoffEntry::Policy>();
      *task_execution_policies_[task_name] = kRetryLaterPolicy;

      // Max backoff time shouldn't be more than task execution period.
      task_execution_policies_[task_name]->maximum_backoff_ms =
          task->GetConfig().execution_period.InMilliseconds();

      const net::BackoffEntry::Policy* policy =
          task_execution_policies_.find(task_name)->second.get();

      // Create backoff entry as this is the first failure after a success.
      task_execution_backoffs_[task_name] =
          std::make_unique<net::BackoffEntry>(policy);
    }

    task_execution_backoffs_[task_name]->InformOfRequest(false);

    // Get backoff time for the next request.
    next_run = task_execution_backoffs_[task_name]->GetTimeUntilRelease();

  } else {
    // Clear the back off entry as the task succeeded. An alternative was to
    // keep it and report success.
    task_execution_backoffs_.erase(task_name);
    next_run = task->GetConfig().execution_period;

    LOGFN(INFO) << task_name << " was executed successfully!";
    const base::Time sync_time = base::Time::Now();
    const std::wstring sync_time_millis = base::NumberToWString(
        sync_time.ToDeltaSinceWindowsEpoch().InMilliseconds());

    SetGlobalFlag(GetLastSyncRegNameForTask(base::UTF8ToWide(task_name)),
                  sync_time_millis);
  }

  // Schedule next task execution.
  task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TaskManager::ExecuteTask, base::Unretained(this),
                     task_runner, task_name),
      next_run);
}

void TaskManager::RunTasks(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  // Enumerate registered tasks and kick-off the periodic execution.
  for (auto it = task_list_.begin(); it != task_list_.end(); ++it) {
    // Get an instance of registered Task.
    std::unique_ptr<Task> task((it->second).Run());
    if (task == nullptr) {
      LOGFN(ERROR) << it->first << " task is null";
      continue;
    }

    // Calculate the next run so that periodic polling  happens within
    // proper time intervals. When the tasks are scheduled, we don't want to
    // immediately start executing to allow some warm-up.
    base::TimeDelta next_run = base::Seconds(10);
    const base::TimeDelta time_since_last_run =
        GetTimeDeltaSinceLastPeriodicSync(
            GetLastSyncRegNameForTask(base::UTF8ToWide(it->first)));

    if (time_since_last_run < task->GetConfig().execution_period)
      next_run = task->GetConfig().execution_period - time_since_last_run;

    task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TaskManager::ExecuteTask, base::Unretained(this),
                       task_runner, it->first),
        next_run);
  }
}

void TaskManager::RegisterTask(const std::string& task_name,
                               TaskCreator task_creator) {
  LOGFN(VERBOSE);

  task_list_.emplace(task_name, std::move(task_creator));
}

}  // namespace extension
}  // namespace credential_provider
