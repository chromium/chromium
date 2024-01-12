// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/auto_run_on_os_upgrade_task.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/util/util.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/app_command_runner.h"
#include "chrome/updater/win/win_constants.h"
#endif

namespace updater {

AutoRunOnOsUpgradeTask::AutoRunOnOsUpgradeTask(
    UpdaterScope scope,
    scoped_refptr<PersistedData> persisted_data)
    : scope_(scope), persisted_data_(persisted_data) {}

AutoRunOnOsUpgradeTask::~AutoRunOnOsUpgradeTask() = default;

void AutoRunOnOsUpgradeTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!HasOSUpgraded()) {
    std::move(callback).Run();
    return;
  }

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&AutoRunOnOsUpgradeTask::RunOnOsUpgradeForApps, this,
                     persisted_data_->GetAppIds()),
      base::BindOnce(&AutoRunOnOsUpgradeTask::SetOSUpgraded, this)
          .Then(std::move(callback)));
}

void AutoRunOnOsUpgradeTask::RunOnOsUpgradeForApps(
    const std::vector<std::string>& app_ids) {
  base::ranges::for_each(
      app_ids, [&](const auto& app_id) { RunOnOsUpgradeForApp(app_id); });
}

#if BUILDFLAG(IS_WIN)
namespace {

// Returns an OS upgrade string in the format
// `{previous_os_version}-{current_os_version}`. For example,
// "9.0.1200.0.0-10.0.19042.0.0".
std::string GetOSUpgradeVersionsString(
    const OSVERSIONINFOEX& previous_os_version,
    const OSVERSIONINFOEX& current_os_version) {
  std::string os_upgrade_string;

  for (const auto& version : {previous_os_version, current_os_version}) {
    os_upgrade_string += base::StringPrintf(
        "%lu.%lu.%lu.%u.%u%s", version.dwMajorVersion, version.dwMinorVersion,
        version.dwBuildNumber, version.wServicePackMajor,
        version.wServicePackMinor, os_upgrade_string.empty() ? "-" : "");
  }

  return os_upgrade_string;
}

}  // namespace

size_t AutoRunOnOsUpgradeTask::RunOnOsUpgradeForApp(const std::string& app_id) {
  size_t number_of_successful_tasks = 0;
  base::ranges::for_each(
      AppCommandRunner::LoadAutoRunOnOsUpgradeAppCommands(
          scope_, base::SysUTF8ToWide(app_id)),
      [&](const auto& app_command_runner) {
        base::Process process;
        if (FAILED(app_command_runner.Run(
                {base::SysUTF8ToWide(os_upgrade_string_)}, process))) {
          return;
        }

        VLOG(1) << "Successfully launched OS upgrade task with PID: "
                << process.Pid() << ": " << os_upgrade_string_;
        ++number_of_successful_tasks;
      });

  return number_of_successful_tasks;
}

bool AutoRunOnOsUpgradeTask::HasOSUpgraded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::optional<OSVERSIONINFOEX> previous_os_version =
      persisted_data_->GetLastOSVersion();
  if (!previous_os_version) {
    // Initialize the OS version.
    persisted_data_->SetLastOSVersion();
    return false;
  }

  if (!CompareOSVersions(previous_os_version.value(), VER_GREATER)) {
    return false;
  }

  if (const std::optional<OSVERSIONINFOEX> current_os_version = GetOSVersion();
      current_os_version) {
    os_upgrade_string_ = GetOSUpgradeVersionsString(previous_os_version.value(),
                                                    current_os_version.value());
  }

  return true;
}

void AutoRunOnOsUpgradeTask::SetOSUpgraded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Save the current OS as the old OS version.
  persisted_data_->SetLastOSVersion();
}

#else   // BUILDFLAG(IS_WIN)
size_t AutoRunOnOsUpgradeTask::RunOnOsUpgradeForApp(const std::string& app_id) {
  return 0;
}

bool AutoRunOnOsUpgradeTask::HasOSUpgraded() {
  return false;
}

void AutoRunOnOsUpgradeTask::SetOSUpgraded() {}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace updater
