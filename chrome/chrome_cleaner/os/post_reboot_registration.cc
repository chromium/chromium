// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/post_reboot_registration.h"

#include <windows.h>

#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/registry.h"
#include "components/chrome_cleaner/public/constants/constants.h"

namespace chrome_cleaner {

namespace {

// The key for Window's RunOnce mechanism.
constexpr wchar_t kRunOnceKeyPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce";

}  // namespace

PostRebootRegistration::PostRebootRegistration(
    const std::wstring& product_shortname)
    : product_shortname_(product_shortname) {}

bool PostRebootRegistration::RegisterRunOnceOnRestart(
    const std::string& cleanup_id,
    const base::CommandLine& switches) {
  base::FilePath exe_path = PreFetchedPaths::GetInstance()->GetExecutablePath();
  base::CommandLine command_line(exe_path);
  // There is a limit of 260 characters on RunOnce values.
  // See https://msdn.microsoft.com/en-us/library/aa376977
  // To work around this, the full list of command line switches is stored in
  // a separate command-line entry. Do not add additional switches here.
  command_line.AppendSwitchASCII(kCleanupIdSwitch, cleanup_id);
  command_line.AppendSwitch(kPostRebootSwitchesInOtherRegistryKeySwitch);

  base::win::RegKey run_once_key(HKEY_CURRENT_USER, kRunOnceKeyPath, KEY_WRITE);
  DCHECK(run_once_key.Valid());
  std::wstring run_once_value(command_line.GetCommandLineString());
  if (run_once_value.length() > 260) {
    LOG(ERROR) << "RunOnce value is too long: " << run_once_value.length()
               << " characters.";
    return false;
  }

  base::win::RegKey switches_key(HKEY_CURRENT_USER,
                                 GetPostRebootSwitchKeyPath().c_str(),
                                 KEY_SET_VALUE | KEY_WOW64_32KEY);
  std::wstring switches_value(switches.GetCommandLineString());
  if (switches_key.WriteValue(base::UTF8ToWide(cleanup_id).c_str(),
                              switches_value.c_str()) != ERROR_SUCCESS) {
    PLOG(ERROR) << "Failed to Write RunOnce value with: "
                << SanitizeCommandLine(switches)
                << " to registry key: " << GetPostRebootSwitchKeyPath();
    return false;
  }

  if (run_once_key.WriteValue(product_shortname_.c_str(),
                              run_once_value.c_str()) != ERROR_SUCCESS) {
    PLOG(ERROR) << "Failed to Write RunOnce value with: "
                << SanitizeCommandLine(command_line);
    return false;
  }

  LOG(INFO) << "Successfully registered RunOnce value with: "
            << SanitizeCommandLine(command_line);

  return true;
}

void PostRebootRegistration::UnregisterRunOnceOnRestart() {
  base::win::RegKey run_once_key(HKEY_CURRENT_USER, kRunOnceKeyPath,
                                 KEY_WRITE | KEY_QUERY_VALUE);
  if (!run_once_key.Valid()) {
    PLOG(WARNING) << "RunOnce key was not found.";
    return;
  }

  if (run_once_key.HasValue(product_shortname_.c_str())) {
    LONG result = run_once_key.DeleteValue(product_shortname_.c_str());
    if (result != ERROR_SUCCESS) {
      PLOG(ERROR) << "Failed to delete RunOnce entry for "
                  << "'" << product_shortname_ << "' - " << result;
    }
  }
}

std::wstring PostRebootRegistration::RunOnceOnRestartRegisteredValue() {
  std::wstring reg_value;
  base::win::RegKey run_once_key(HKEY_CURRENT_USER, kRunOnceKeyPath, KEY_READ);
  if (run_once_key.Valid()) {
    // There is no need to check the return value, since ReadValue will leave
    // |reg_value| empty on error.
    run_once_key.ReadValue(product_shortname_.c_str(), &reg_value);
  }
  return reg_value;
}

bool PostRebootRegistration::ReadRunOncePostRebootCommandLine(
    const std::string& cleanup_id,
    base::CommandLine* out_command_line) {
  base::win::RegKey switches_key(HKEY_CURRENT_USER,
                                 GetPostRebootSwitchKeyPath().c_str(),
                                 KEY_QUERY_VALUE | KEY_WOW64_32KEY);

  std::wstring string_value;
  if (switches_key.ReadValue(base::UTF8ToWide(cleanup_id).c_str(),
                             &string_value) != ERROR_SUCCESS) {
    PLOG(ERROR) << "Failed to Read RunOnce reboot switches.";
    return false;
  }

  base::CommandLine tmp(base::CommandLine::NO_PROGRAM);
  tmp.ParseFromString(L"unused.exe " + string_value);

  out_command_line->AppendArguments(tmp, /*include_program*/ false);

  // Note that this also deletes the entire key, cleaning up stale switches
  // from previous failures or un-registrations.
  return switches_key.DeleteKey(L"") == ERROR_SUCCESS;
}

// static
std::wstring PostRebootRegistration::GetPostRebootSwitchKeyPath() {
  std::wstring path(chrome_cleaner::kSoftwareRemovalToolRegistryKey);
  path += L"\\";
  path += chrome_cleaner::kCleanerSubKey;
  path += L"\\";
  path += L"RunOnceRebootSwitches";
  return path;
}

}  // namespace chrome_cleaner
