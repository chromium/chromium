// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/rebooter.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/os/post_reboot_registration.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/resource_util.h"
#include "chrome/chrome_cleaner/os/system_util_cleaner.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "components/chrome_cleaner/public/constants/constants.h"

namespace chrome_cleaner {

namespace {

const char* kSwitchesToPropagate[]{
    kChromeChannelSwitch, kChromePromptSwitch,  kChromeSystemInstallSwitch,
    kChromeVersionSwitch, kDumpRawLogsSwitch,   kEnableCrashReportingSwitch,
    kEngineSwitch,        kExecutionModeSwitch, kLogUploadRetryIntervalSwitch,
    kNoSelfDeleteSwitch,  kTestingSwitch,       kUmaUserSwitch,
};

// The name of the task to run post reboot.
base::string16 PostRebootRunTaskName(const base::string16& product_shortname) {
  return product_shortname + L" post reboot run";
}

}  // namespace

// static
bool Rebooter::IsPostReboot() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kPostRebootSwitch);
}

Rebooter::Rebooter(const base::string16& product_shortname)
    : product_shortname_(product_shortname),
      switches_(base::CommandLine::NO_PROGRAM) {}

void Rebooter::AppendPostRebootSwitch(const std::string& switch_string) {
  switches_.AppendSwitch(switch_string);
}

void Rebooter::AppendPostRebootSwitchASCII(const std::string& switch_string,
                                           const std::string& value) {
  switches_.AppendSwitchASCII(switch_string, value);
}

bool Rebooter::RegisterPostRebootRun(
    const base::CommandLine* current_command_line,
    const std::string& cleanup_id,
    ExecutionMode execution_mode,
    bool logs_uploads_enabled) {
  // Avoid getting in a post-reboot infinite loop.
  if (IsPostReboot()) {
    LOG(ERROR) << "Registering a post reboot run while running post-reboot?";
    return false;
  }

  if (execution_mode != ExecutionMode::kCleanup) {
    LOG(ERROR) << "Registering a post reboot run while not in cleanup mode?";
    return false;
  }

  base::CommandLine local_switches(switches_);
  for (const char* switch_name : kSwitchesToPropagate) {
    if (current_command_line->HasSwitch(switch_name))
      local_switches.AppendSwitchNative(
          switch_name, current_command_line->GetSwitchValueNative(switch_name));
  }

  local_switches.AppendSwitch(kPostRebootSwitch);

  if (current_command_line->HasSwitch(kTestLoggingURLSwitch)) {
    local_switches.AppendSwitchASCII(
        kTestLoggingURLSwitch,
        current_command_line->GetSwitchValueASCII(kTestLoggingURLSwitch));
  }

  // kCleanup mode: kEnableCrashReportingSwitch and
  // kWithCleanupModeLogsSwitch are responsible for logs and crash reporting
  // respectively and should be propagated as-is to the post-reboot run.
  if (current_command_line->HasSwitch(kWithCleanupModeLogsSwitch)) {
    local_switches.AppendSwitchASCII(
        kWithCleanupModeLogsSwitch,
        current_command_line->GetSwitchValueASCII(kWithCleanupModeLogsSwitch));
  }
  if (current_command_line->HasSwitch(kEnableCrashReportingSwitch)) {
    local_switches.AppendSwitchASCII(
        kEnableCrashReportingSwitch,
        current_command_line->GetSwitchValueASCII(kEnableCrashReportingSwitch));
  }

  // Propagate the cleanup id for the current process, so we can identify the
  // corresponding post-reboot logs.
  local_switches.AppendSwitchASCII(kCleanupIdSwitch, cleanup_id);

  base::FilePath exec_path =
      PreFetchedPaths::GetInstance()->GetExecutablePath();
  base::CommandLine post_reboot_run(local_switches);
  post_reboot_run.SetProgram(exec_path);

  // We add a RunOnce entry and a scheduled task, and hope that one of them
  // runs post-reboot. The first to run will prevent the other one from running,
  // and will also remove the scheduled task + RunOnce entry on success.
  PostRebootRegistration(product_shortname_)
      .RegisterRunOnceOnRestart(cleanup_id, local_switches);

  // Include a switch to mark that the post-reboot run was triggered from
  // TaskScheduler rather than RunOnce, for forensics. (Don't include the same
  // switch for RunOnce because it has a command-line length limit. We can tell
  // that the run was triggered from RunOnce because the switch is missing.)
  post_reboot_run.AppendSwitchASCII(kPostRebootTriggerSwitch, "TaskScheduler");

  std::unique_ptr<TaskScheduler> task_scheduler(
      TaskScheduler::CreateInstance());
  return task_scheduler->RegisterTask(
      PostRebootRunTaskName(product_shortname_).c_str(),
      /*task_description=*/L"", post_reboot_run,
      TaskScheduler::TRIGGER_TYPE_POST_REBOOT, false);
}

void Rebooter::UnregisterPostRebootRun() {
  // Delete both the scheduled task and the RunOnce entry, to make sure Chrome
  // Cleanup doesn't run again.
  std::unique_ptr<TaskScheduler> task_scheduler(
      TaskScheduler::CreateInstance());
  task_scheduler->DeleteTask(PostRebootRunTaskName(product_shortname_).c_str());
  PostRebootRegistration(product_shortname_).UnregisterRunOnceOnRestart();
}

}  // namespace chrome_cleaner
