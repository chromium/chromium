// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/installer_crash_reporting.h"

#include <iterator>
#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/install_details.h"
#include "chrome/installer/setup/installer_crash_reporter_client.h"
#include "chrome/installer/setup/installer_state.h"
#include "components/crash/core/app/crashpad.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/crash_keys.h"

namespace installer {

namespace {

const char* OperationToString(InstallerState::Operation operation) {
  switch (operation) {
    case InstallerState::SINGLE_INSTALL_OR_UPDATE:
      return "single-install-or-update";
    case InstallerState::UNINSTALL:
      return "uninstall";
    case InstallerState::UNINITIALIZED:
      // Fall out of switch.
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

// Returns `SystemTemp` if available. Otherwise, retrieves the SYSTEM version of
// TEMP. We do this instead of GetTempPath so that both elevated and SYSTEM runs
// share the same directory.
bool GetSystemTemp(base::FilePath* temp) {
  if (base::PathService::Get(base::DIR_SYSTEM_TEMP, temp)) {
    return true;
  }

  base::win::RegKey reg_key(
      HKEY_LOCAL_MACHINE,
      L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
      KEY_READ);
  std::wstring temp_wstring;  // presubmit: allow wstring
  bool success = reg_key.ReadValue(L"TEMP", &temp_wstring) == ERROR_SUCCESS;
  if (success)
    *temp = base::FilePath(temp_wstring);  // presubmit: allow wstring
  return success;
}

}  // namespace

void ConfigureCrashReporting(const InstallerState& installer_state) {
  // This is inspired by work done in various parts of Chrome startup to connect
  // to the crash service. Since the installer does not split its work between
  // a stub .exe and a main .dll, crash reporting can be configured in one place
  // right here.

  // Create the crash client and install it (a la MainDllLoader::Launch).
  InstallerCrashReporterClient* crash_client =
      new InstallerCrashReporterClient(!installer_state.system_install());
  ANNOTATE_LEAKING_OBJECT_PTR(crash_client);
  crash_reporter::SetCrashReporterClient(crash_client);

  crash_reporter::InitializeCrashKeys();

  if (installer_state.system_install()) {
    base::FilePath temp_dir;
    if (GetSystemTemp(&temp_dir)) {
      base::FilePath crash_dir = temp_dir.Append(FILE_PATH_LITERAL("Crashpad"));
      base::PathService::OverrideAndCreateIfNeeded(chrome::DIR_CRASH_DUMPS,
                                                   crash_dir, true, true);
    } else {
      // Failed to get a temp dir, something's gone wrong.
      return;
    }
  }

  crash_reporter::InitializeCrashpadWithEmbeddedHandler(
      true, "Chrome Installer", "", base::FilePath());
}

void SetInitialCrashKeys(const InstallerState& state) {
  using crash_reporter::CrashKeyString;

  static CrashKeyString<64> operation("operation");
  operation.Set(OperationToString(state.operation()));

  static CrashKeyString<6> is_system_level("system-level");
  is_system_level.Set(state.system_install() ? "true" : "false");

  // This is a Windows registry key, which maxes out at 255 chars.
  static CrashKeyString<256> state_crash_key("state-key");
  const std::wstring state_key = state.state_key();
  if (!state_key.empty())
    state_crash_key.Set(base::WideToUTF8(state_key));

  // Set crash keys containing the registry values used to determine Chrome's
  // update channel at process startup; see https://crbug.com/579504.
  const auto& details = install_static::InstallDetails::Get();

  static CrashKeyString<50> ap_value("ap");
  ap_value.Set(base::WideToUTF8(details.update_ap()));

  static CrashKeyString<32> update_cohort_name("cohort-name");
  update_cohort_name.Set(base::WideToUTF8(details.update_cohort_name()));
}

void SetCrashKeysFromCommandLine(const base::CommandLine& command_line) {
  crash_keys::SetSwitchesFromCommandLine(command_line, nullptr);
}

void SetCurrentVersionCrashKey(const base::Version& current_version) {
  static crash_reporter::CrashKeyString<32> version_key("current-version");
  if (current_version.IsValid())
    version_key.Set(current_version.GetString());
  else
    version_key.Clear();
}

}  // namespace installer
