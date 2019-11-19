// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/setup_util.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "chrome/updater/win/task_scheduler.h"

namespace updater {

namespace {

constexpr base::char16 kTaskName[] = L"GoogleUpdaterUA";
constexpr base::char16 kTaskDescription[] = L"Update all applications.";

}  // namespace

bool RegisterUpdateAppsTask(const base::CommandLine& run_command) {
  auto task_scheduler = TaskScheduler::CreateInstance();
  if (!task_scheduler->RegisterTask(
          kTaskName, kTaskDescription, run_command,
          TaskScheduler::TriggerType::TRIGGER_TYPE_HOURLY, true)) {
    LOG(ERROR) << "RegisterUpdateAppsTask failed.";
    return false;
  }
  VLOG(1) << "RegisterUpdateAppsTask succeeded.";
  return true;
}

void UnregisterUpdateAppsTask() {
  auto task_scheduler = TaskScheduler::CreateInstance();
  task_scheduler->DeleteTask(kTaskName);
}

}  // namespace updater
