// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/downgrade_cleanup.h"

#include <optional>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/callback_work_item.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"

namespace {

constexpr std::wstring_view kCleanupOperation = L"cleanup";
constexpr std::wstring_view kRevertCleaunpOperation = L"revert";

// Returns the last version of Chrome which introduced breaking changes to the
// installer, or no value if Chrome is not installed or the version installed
// predates support for this feature.
std::optional<base::Version> GetLastBreakingInstallerVersion(HKEY reg_root) {
  base::win::RegKey key;
  std::wstring last_breaking_installer_version;
  if (key.Open(reg_root, install_static::GetClientStateKeyPath().c_str(),
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) != ERROR_SUCCESS ||
      key.ReadValue(google_update::kRegCleanInstallRequiredForVersionBelowField,
                    &last_breaking_installer_version) != ERROR_SUCCESS ||
      last_breaking_installer_version.empty()) {
    return std::nullopt;
  }
  base::Version version(base::WideToASCII(last_breaking_installer_version));
  if (!version.IsValid())
    return std::nullopt;
  return version;
}
// Formats `cmd_line_with_placeholders` by replacing the placeholders with
// `version` and `operation`. Returns an empty string if some placeholder
// replacements are missing.
std::wstring GetCleanupCommandLine(
    const std::wstring& cmd_line_with_placeholders,
    const base::Version& version,
    std::wstring_view operation) {
  DCHECK(version.IsValid());
  DCHECK(!cmd_line_with_placeholders.empty());
  DCHECK(operation == kCleanupOperation ||
         operation == kRevertCleaunpOperation);
  std::vector<size_t> offsets;
  std::vector<std::u16string> replacements{
      base::ASCIIToUTF16(version.GetString()), base::AsString16(operation)};
  auto cmd = base::ReplaceStringPlaceholders(
      base::AsString16(cmd_line_with_placeholders), replacements, &offsets);
  // The `offsets` size and `replacements` size should be equal. If they are
  // not, no command should be returned to avoid running an invalid command
  // line.
  if (offsets.size() != replacements.size())
    cmd.clear();
  return base::AsWString(std::move(cmd));
}

// Returns true if after a downgrade, `cmd` was run successfully to cleanup
// after a downgrade crossing a breaking installer version. `cmd` is expected to
// be a correctly formatted command line that calls the installer of the version
// we downgraded from with the right version and 'cleanup' as operation
// parameter.
bool LaunchCleanupForBreakingDowngradeProcess(
    const std::wstring& cmd,
    const CallbackWorkItem& work_item) {
  DCHECK(!cmd.empty());
  VLOG(1) << "Launching downgrade cleanup process: " << cmd;
  base::Process process = base::LaunchProcess(cmd, base::LaunchOptions());
  if (!process.IsValid()) {
    PLOG(ERROR) << "Failed to launch child process \"" << cmd << "\"";
    return false;
  }
  int exit_code = installer::DOWNGRADE_CLEANUP_SUCCESS;
  process.WaitForExit(&exit_code);

  if (exit_code == installer::DOWNGRADE_CLEANUP_SUCCESS) {
    VLOG(1) << "Downgrade cleanup process succeeded";
    return true;
  }
  LOG(ERROR) << "Downgrade cleanup process \"" << cmd
             << "\" failed with exit code " << exit_code;
  return false;
}

// Runs `cmd` to revert any cleanup done after a downgrade crossing a breaking
// installer version in the context of a CallbackWorkItem. `cmd` is expected to
// be a correctly formatted command line that calls the installer of the version
// we downgraded from with the right version and 'revert' as operation
// parameter.
void LaunchUndoCleanupForBreakingDowngradeProcess(
    const std::wstring& cmd,
    const CallbackWorkItem& work_item) {
  DCHECK(!cmd.empty());
  VLOG(1) << "Launching downgrade cleanup undo process: " << cmd;
  base::Process process = base::LaunchProcess(cmd, base::LaunchOptions());
  if (!process.IsValid()) {
    PLOG(ERROR) << "Failed to launch child process \"" << cmd << "\"";
    return;
  }

  int exit_code = installer::UNDO_DOWNGRADE_CLEANUP_SUCCESS;
  process.WaitForExit(&exit_code);

  if (exit_code == installer::UNDO_DOWNGRADE_CLEANUP_SUCCESS) {
    VLOG(1) << "Downgrade cleanup undo process succeeded";
    return;
  }

  LOG(ERROR) << "Downgrade cleanup undo process \"" << cmd
             << "\" failed with exit code " << exit_code;
}

}  // namespace

namespace installer {

InstallStatus ProcessCleanupForDowngrade(const base::Version& version,
                                         bool revert) {
  if (revert) {
    return version.IsValid() ? UNDO_DOWNGRADE_CLEANUP_SUCCESS
                             : UNDO_DOWNGRADE_CLEANUP_FAILED;
  }
  return version.IsValid() ? DOWNGRADE_CLEANUP_SUCCESS
                           : DOWNGRADE_CLEANUP_FAILED;
}

std::wstring GetDowngradeCleanupCommandWithPlaceholders(
    const base::FilePath& installer_path,
    const InstallerState& installer_state) {
  base::CommandLine downgrade_cleanup_cmd(installer_path);
  downgrade_cleanup_cmd.AppendSwitchNative(
      switches::kCleanupForDowngradeVersion, L"$1");
  downgrade_cleanup_cmd.AppendSwitchNative(
      switches::kCleanupForDowngradeOperation, L"$2");
  InstallUtil::AppendModeAndChannelSwitches(&downgrade_cleanup_cmd);
  if (installer_state.system_install())
    downgrade_cleanup_cmd.AppendSwitch(switches::kSystemLevel);
  if (installer_state.verbose_logging())
    downgrade_cleanup_cmd.AppendSwitch(switches::kVerboseLogging);
  return downgrade_cleanup_cmd.GetCommandLineString();
}

bool AddDowngradeCleanupItems(const base::Version& new_version,
                              WorkItemList* list) {
  DCHECK(new_version.IsValid());
  HKEY reg_root = install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                    : HKEY_CURRENT_USER;
  if (GetLastBreakingInstallerVersion(reg_root) <= new_version)
    return false;

  std::wstring dowgrade_cleanup_cmd;
  base::win::RegKey(reg_root, install_static::GetClientStateKeyPath().c_str(),
                    KEY_QUERY_VALUE | KEY_WOW64_32KEY)
      .ReadValue(google_update::kRegDowngradeCleanupCommandField,
                 &dowgrade_cleanup_cmd);
  if (dowgrade_cleanup_cmd.empty())
    return false;

  auto cleanup_cmd = GetCleanupCommandLine(dowgrade_cleanup_cmd, new_version,
                                           kCleanupOperation);
  if (cleanup_cmd.empty()) {
    LOG(ERROR) << "Unable to format the downgrade cleanup command \""
               << dowgrade_cleanup_cmd << "\"";
    return false;
  }

  auto revert_cmd = GetCleanupCommandLine(dowgrade_cleanup_cmd, new_version,
                                          kRevertCleaunpOperation);
  if (revert_cmd.empty()) {
    LOG(ERROR) << "Unable to format the revert downgrade cleanup command \""
               << dowgrade_cleanup_cmd << "\"";
    return false;
  }

  VLOG(1) << "Setting up cleanup for downgrade to version " << new_version;

  list->AddCallbackWorkItem(
      base::BindOnce(&LaunchCleanupForBreakingDowngradeProcess,
                     std::move(cleanup_cmd)),
      base::BindOnce(&LaunchUndoCleanupForBreakingDowngradeProcess,
                     std::move(revert_cmd)));
  return true;
}

}  // namespace installer
