// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/extension/task_manager.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/timer/timer.h"
#include "chrome/credential_provider/extension/extension_strings.h"
#include "chrome/credential_provider/extension/task.h"
#include "chrome/credential_provider/extension/user_context_enumerator.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {
namespace extension {

namespace {
// Specifies the period of executing tasks.
constexpr auto kPollingInterval = base::TimeDelta::FromHours(1);
}  // namespace

LastPeriodicSyncUpdater::LastPeriodicSyncUpdater() {}

LastPeriodicSyncUpdater::~LastPeriodicSyncUpdater() {
  UpdateLastRunTimestamp();
}

void LastPeriodicSyncUpdater::UpdateLastRunTimestamp() {
  const base::Time sync_time = base::Time::Now();
  const base::string16 sync_time_millis = base::NumberToString16(
      sync_time.ToDeltaSinceWindowsEpoch().InMilliseconds());

  SetGlobalFlag(kLastPeriodicSyncTimeRegKey, sync_time_millis);
}

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

void TaskManager::ScheduleTasks() {
  timer_.Start(FROM_HERE, kPollingInterval, this,
               &TaskManager::RunTasksInternal);
}

void TaskManager::RunTasksInternal() {
  LOGFN(VERBOSE);
  LastPeriodicSyncUpdater last_periodic_sync_updater;

  for (auto it = task_list_.begin(); it != task_list_.end(); ++it) {
    LOGFN(VERBOSE) << "Executing " << it->first;

    std::unique_ptr<Task> task((it->second).Run());
    if (task == nullptr) {
      LOGFN(ERROR) << it->first << " task is null";
      continue;
    }

    UserContextEnumerator::Get()->PerformTask(it->first, *task.get());
  }
}

void TaskManager::RunTasks(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  LOGFN(VERBOSE);

  // Calculate the next run so that periodic polling  happens within
  // proper time intervals. When the tasks are scheduled, we don't want to
  // immediately start executing periodic tasks to allow some warm-up.
  base::TimeDelta next_run = base::TimeDelta::FromSeconds(10);
  const base::TimeDelta time_since_last_run =
      GetTimeDeltaSinceLastPeriodicSync();
  if (time_since_last_run < kPollingInterval)
    next_run = kPollingInterval - time_since_last_run;

  // Post an individual task that runs after completing a polling period.
  task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TaskManager::RunTasksInternal, base::Unretained(this)),
      next_run);

  // Schedule a repeating task to run after running the task above.
  task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TaskManager::ScheduleTasks, base::Unretained(this)),
      next_run);
}

base::TimeDelta TaskManager::GetTimeDeltaSinceLastPeriodicSync() {
  wchar_t last_sync_millis[512];
  ULONG last_sync_size = base::size(last_sync_millis);
  HRESULT hr = GetGlobalFlag(kLastPeriodicSyncTimeRegKey, last_sync_millis,
                             &last_sync_size);

  if (FAILED(hr)) {
    // The periodic sync has never happened before.
    return base::TimeDelta::Max();
  }

  int64_t last_sync_millis_int64;
  base::StringToInt64(last_sync_millis, &last_sync_millis_int64);
  const auto last_sync = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMilliseconds(last_sync_millis_int64));
  return base::Time::Now() - last_sync;
}

void TaskManager::RegisterTask(const std::string& task_name,
                               TaskCreator task_creator) {
  LOGFN(VERBOSE);

  task_list_.emplace(task_name, std::move(task_creator));
}

void TaskManager::Quit() {
  LOGFN(VERBOSE);

  timer_.AbandonAndStop();
}

}  // namespace extension
}  // namespace credential_provider
