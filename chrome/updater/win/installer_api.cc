// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer_api.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/enum_traits.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"
#include "components/update_client/update_client.h"

namespace updater {
namespace {

// Creates or opens the registry ClientState subkey for the `app_id`. `regsam`
// must contain the KEY_WRITE access right for the creation of the subkey to
// succeed.
std::optional<base::win::RegKey> ClientStateAppKeyCreate(
    UpdaterScope updater_scope,
    const std::string& app_id,
    REGSAM regsam) {
  std::wstring subkey;
  if (!base::UTF8ToWide(app_id.c_str(), app_id.size(), &subkey)) {
    return std::nullopt;
  }
  base::win::RegKey key(UpdaterScopeToHKeyRoot(updater_scope), CLIENT_STATE_KEY,
                        Wow6432(regsam));
  if (!key.Valid() ||
      key.CreateKey(subkey.c_str(), Wow6432(regsam)) != ERROR_SUCCESS) {
    return std::nullopt;
  }
  return key;
}

bool RegCopyValue(base::win::RegKey& from_key,
                  const std::wstring& from_value_name,
                  base::win::RegKey& to_key,
                  const std::wstring& to_value_name) {
  DWORD size = 0;
  if (from_key.ReadValue(from_value_name.c_str(), nullptr, &size, nullptr) !=
      ERROR_SUCCESS) {
    return false;
  }

  std::vector<char> raw_value(size);
  DWORD dtype = 0;
  if (from_key.ReadValue(from_value_name.c_str(), raw_value.data(), &size,
                         &dtype) != ERROR_SUCCESS) {
    return false;
  }

  if (to_key.WriteValue(to_value_name.c_str(), raw_value.data(), size, dtype) !=
      ERROR_SUCCESS) {
    PLOG(WARNING) << "could not write: " << to_value_name;
    return false;
  }
  return true;
}

bool RegRenameValue(base::win::RegKey& key,
                    const std::wstring& from_value_name,
                    const std::wstring& to_value_name) {
  if (!RegCopyValue(key, from_value_name, key, to_value_name)) {
    return false;
  }
  key.DeleteValue(from_value_name.c_str());
  return true;
}

void PersistLastInstallerResultValues(UpdaterScope updater_scope,
                                      const std::string& app_id) {
  std::optional<base::win::RegKey> key =
      ClientStateAppKeyOpen(updater_scope, app_id, KEY_READ | KEY_WRITE);
  if (!key) {
    return;
  }

  // Rename `InstallerResultXXX` values to `LastXXX`.
  for (const auto& [rename_from, rename_to] : {
           std::make_pair(kRegValueInstallerResult,
                          kRegValueLastInstallerResult),
           std::make_pair(kRegValueInstallerError, kRegValueLastInstallerError),
           std::make_pair(kRegValueInstallerExtraCode1,
                          kRegValueLastInstallerExtraCode1),
           std::make_pair(kRegValueInstallerResultUIString,
                          kRegValueLastInstallerResultUIString),
           std::make_pair(kRegValueInstallerSuccessLaunchCmdLine,
                          kRegValueLastInstallerSuccessLaunchCmdLine),
       }) {
    RegRenameValue(key.value(), rename_from, rename_to);
  }

  // The updater copies and retains the last installer result values as a backup
  // under the `UPDATER_KEY`, and the values under the `UPDATER_KEY` are not
  // deleted when the updater uninstalls itself. The MSI installer uses this
  // information to display the installer result in cases where an error occurs
  // during app installation and no other apps are installed, and the updater
  // immediately uninstalls itself and deletes the `ClientState` registry key
  // under which the last installer results are usually stored.
  if (base::win::RegKey updater_key(UpdaterScopeToHKeyRoot(updater_scope),
                                    UPDATER_KEY, Wow6432(KEY_WRITE));
      updater_key.Valid()) {
    for (const wchar_t* const reg_value_last_installer :
         kRegValuesLastInstaller) {
      RegCopyValue(key.value(), reg_value_last_installer, updater_key,
                   reg_value_last_installer);
    }
  }
}

bool ClientStateAppKeyExists(UpdaterScope updater_scope,
                             const std::string& app_id) {
  return base::win::RegKey(UpdaterScopeToHKeyRoot(updater_scope),
                           GetAppClientStateKey(app_id).c_str(),
                           Wow6432(KEY_QUERY_VALUE))
      .Valid();
}

std::optional<InstallerOutcome> GetLastInstallerOutcome(
    std::optional<base::win::RegKey> key) {
  if (!key) {
    return std::nullopt;
  }
  InstallerOutcome installer_outcome;
  {
    DWORD val = 0;
    if (key->ReadValueDW(kRegValueLastInstallerResult, &val) == ERROR_SUCCESS) {
      installer_outcome.installer_result =
          *CheckedCastToEnum<InstallerApiResult>(val);
    }
    if (key->ReadValueDW(kRegValueLastInstallerError, &val) == ERROR_SUCCESS) {
      installer_outcome.installer_error = val;
    }
    if (key->ReadValueDW(kRegValueLastInstallerExtraCode1, &val) ==
        ERROR_SUCCESS) {
      installer_outcome.installer_extracode1 = val;
    }
  }
  {
    std::wstring val;
    if (key->ReadValue(kRegValueLastInstallerResultUIString, &val) ==
        ERROR_SUCCESS) {
      std::string installer_text;
      if (base::WideToUTF8(val.c_str(), val.size(), &installer_text)) {
        installer_outcome.installer_text = installer_text;
      }
    }
    if (key->ReadValue(kRegValueLastInstallerSuccessLaunchCmdLine, &val) ==
        ERROR_SUCCESS) {
      std::string installer_cmd_line;
      if (base::WideToUTF8(val.c_str(), val.size(), &installer_cmd_line)) {
        installer_outcome.installer_cmd_line = installer_cmd_line;
      }
    }
  }

  return installer_outcome;
}

}  // namespace

InstallerOutcome::InstallerOutcome() = default;
InstallerOutcome::InstallerOutcome(const InstallerOutcome&) = default;
InstallerOutcome::~InstallerOutcome() = default;

std::optional<base::win::RegKey> ClientStateAppKeyOpen(
    UpdaterScope updater_scope,
    const std::string& app_id,
    REGSAM regsam) {
  std::wstring subkey;
  if (!base::UTF8ToWide(app_id.c_str(), app_id.size(), &subkey)) {
    return std::nullopt;
  }
  base::win::RegKey key(UpdaterScopeToHKeyRoot(updater_scope), CLIENT_STATE_KEY,
                        Wow6432(regsam));
  if (!key.Valid() ||
      key.OpenKey(subkey.c_str(), Wow6432(regsam)) != ERROR_SUCCESS) {
    return std::nullopt;
  }
  return key;
}

bool ClientStateAppKeyDelete(UpdaterScope updater_scope,
                             const std::string& app_id) {
  std::wstring subkey;
  if (!base::UTF8ToWide(app_id.c_str(), app_id.size(), &subkey)) {
    return false;
  }
  return base::win::RegKey(UpdaterScopeToHKeyRoot(updater_scope),
                           CLIENT_STATE_KEY, Wow6432(DELETE))
             .DeleteKey(subkey.c_str()) == ERROR_SUCCESS;
}

// Reads the installer progress from the registry value at:
// {HKLM|HKCU}\Software\Google\Update\ClientState\<appid>\InstallerProgress.
int GetInstallerProgress(UpdaterScope updater_scope,
                         const std::string& app_id) {
  std::optional<base::win::RegKey> key =
      ClientStateAppKeyOpen(updater_scope, app_id, KEY_READ);
  DWORD progress = 0;
  if (!key || key->ReadValueDW(kRegValueInstallerProgress, &progress) !=
                  ERROR_SUCCESS) {
    return -1;
  }
  return std::clamp(progress, DWORD{0}, DWORD{100});
}

bool SetInstallerProgressForTesting(UpdaterScope updater_scope,
                                    const std::string& app_id,
                                    int value) {
  std::optional<base::win::RegKey> key =
      ClientStateAppKeyCreate(updater_scope, app_id, KEY_WRITE);
  return key && key->WriteValue(kRegValueInstallerProgress,
                                static_cast<DWORD>(value)) == ERROR_SUCCESS;
}

bool DeleteInstallerProgress(UpdaterScope updater_scope,
                             const std::string& app_id) {
  if (!ClientStateAppKeyExists(updater_scope, app_id)) {
    return false;
  }
  std::optional<base::win::RegKey> key =
      ClientStateAppKeyOpen(updater_scope, app_id, KEY_SET_VALUE);
  return key && key->DeleteValue(kRegValueInstallerProgress) == ERROR_SUCCESS;
}

bool DeleteInstallerOutput(UpdaterScope updater_scope,
                           const std::string& app_id) {
  if (!ClientStateAppKeyExists(updater_scope, app_id)) {
    return false;
  }
  std::optional<base::win::RegKey> key = ClientStateAppKeyOpen(
      updater_scope, app_id, KEY_SET_VALUE | KEY_QUERY_VALUE);
  if (!key) {
    return false;
  }
  auto delete_value = [&key](const wchar_t* value) {
    return !key->HasValue(value) || key->DeleteValue(value) == ERROR_SUCCESS;
  };
  const bool results[] = {
      delete_value(kRegValueInstallerProgress),
      delete_value(kRegValueInstallerResult),
      delete_value(kRegValueInstallerError),
      delete_value(kRegValueInstallerExtraCode1),
      delete_value(kRegValueInstallerResultUIString),
      delete_value(kRegValueInstallerSuccessLaunchCmdLine),
  };
  return !base::Contains(results, false);
}

std::optional<InstallerOutcome> GetInstallerOutcome(UpdaterScope updater_scope,
                                                    const std::string& app_id) {
  std::optional<base::win::RegKey> key =
      ClientStateAppKeyOpen(updater_scope, app_id, KEY_READ);
  if (!key) {
    return std::nullopt;
  }
  InstallerOutcome installer_outcome;
  {
    DWORD val = 0;
    if (key->ReadValueDW(kRegValueInstallerResult, &val) == ERROR_SUCCESS) {
      installer_outcome.installer_result =
          *CheckedCastToEnum<InstallerApiResult>(val);
    }
    if (key->ReadValueDW(kRegValueInstallerError, &val) == ERROR_SUCCESS) {
      installer_outcome.installer_error = val;
    }
    if (key->ReadValueDW(kRegValueInstallerExtraCode1, &val) == ERROR_SUCCESS) {
      installer_outcome.installer_extracode1 = val;
    }
  }
  {
    std::wstring val;
    if (key->ReadValue(kRegValueInstallerResultUIString, &val) ==
        ERROR_SUCCESS) {
      std::string installer_text;
      if (base::WideToUTF8(val.c_str(), val.size(), &installer_text)) {
        installer_outcome.installer_text = installer_text;
      }
    }
    if (key->ReadValue(kRegValueInstallerSuccessLaunchCmdLine, &val) ==
        ERROR_SUCCESS) {
      std::string installer_cmd_line;
      if (base::WideToUTF8(val.c_str(), val.size(), &installer_cmd_line)) {
        installer_outcome.installer_cmd_line = installer_cmd_line;
      }
    }
  }

  PersistLastInstallerResultValues(updater_scope, app_id);

  return installer_outcome;
}

std::optional<InstallerOutcome> GetClientStateKeyLastInstallerOutcome(
    UpdaterScope updater_scope,
    const std::string& app_id) {
  return GetLastInstallerOutcome(
      ClientStateAppKeyOpen(updater_scope, app_id, KEY_READ));
}

std::optional<InstallerOutcome> GetUpdaterKeyLastInstallerOutcome(
    UpdaterScope updater_scope) {
  return GetLastInstallerOutcome(
      [&updater_scope]() -> std::optional<base::win::RegKey> {
        if (base::win::RegKey updater_key(UpdaterScopeToHKeyRoot(updater_scope),
                                          UPDATER_KEY, Wow6432(KEY_READ));
            updater_key.Valid()) {
          return updater_key;
        }
        return {};
      }());
}

bool SetInstallerOutcomeForTesting(UpdaterScope updater_scope,
                                   const std::string& app_id,
                                   const InstallerOutcome& installer_outcome) {
  std::optional<base::win::RegKey> key =
      ClientStateAppKeyCreate(updater_scope, app_id, KEY_WRITE);
  if (!key) {
    return false;
  }
  if (installer_outcome.installer_result) {
    if (key->WriteValue(
            kRegValueInstallerResult,
            static_cast<DWORD>(*installer_outcome.installer_result)) !=
        ERROR_SUCCESS) {
      return false;
    }
  }
  if (installer_outcome.installer_error) {
    if (key->WriteValue(kRegValueInstallerError,
                        *installer_outcome.installer_error) != ERROR_SUCCESS) {
      return false;
    }
  }
  if (installer_outcome.installer_extracode1) {
    if (key->WriteValue(kRegValueInstallerExtraCode1,
                        *installer_outcome.installer_extracode1) !=
        ERROR_SUCCESS) {
      return false;
    }
  }
  if (installer_outcome.installer_text) {
    if (key->WriteValue(
            kRegValueInstallerResultUIString,
            base::UTF8ToWide(*installer_outcome.installer_text).c_str()) !=
        ERROR_SUCCESS) {
      return false;
    }
  }
  if (installer_outcome.installer_cmd_line) {
    if (key->WriteValue(
            kRegValueInstallerSuccessLaunchCmdLine,
            base::UTF8ToWide(*installer_outcome.installer_cmd_line).c_str()) !=
        ERROR_SUCCESS) {
      return false;
    }
  }
  return true;
}

// As much as possible, the implementation of this function is intended to be
// backward compatible with the implementation of the Installer API in
// Omaha/Google Update. Some edge cases could be missing.
Installer::Result MakeInstallerResult(
    std::optional<InstallerOutcome> installer_outcome,
    int exit_code) {
  InstallerOutcome outcome;
  if (installer_outcome && installer_outcome->installer_result) {
    outcome = *installer_outcome;
  } else {
    // Set the installer result based on whether this is a success or an error.
    if (exit_code == 0) {
      outcome.installer_result = InstallerApiResult::kSuccess;
    } else {
      outcome.installer_result = InstallerApiResult::kExitCode;
      outcome.installer_error = exit_code;
    }
  }

  Installer::Result result;

  // Read and set the installer extra code in all cases if available. Installers
  // can use the `installer_extracode1` to transmit a custom value even in the
  // case of success.
  if (outcome.installer_extracode1) {
    result.result.extra_ = *outcome.installer_extracode1;
  }

  switch (*outcome.installer_result) {
    case InstallerApiResult::kSuccess:
      // This is unconditional success:
      // - use the command line if available, and ignore everything else.
      if (outcome.installer_cmd_line) {
        result.installer_cmd_line = *outcome.installer_cmd_line;
      }
      break;

    case InstallerApiResult::kCustomError:
    case InstallerApiResult::kMsiError:
    case InstallerApiResult::kSystemError:
    case InstallerApiResult::kExitCode:
      // These are usually unconditional errors:
      // - use the installer error, or the exit code, or report a generic
      //   error.
      // - use the installer extra code if available.
      // - use the text description of the error if available.
      result.result.code_ = outcome.installer_error.value_or(exit_code);
      if (!result.result.code_) {
        result.result.code_ = kErrorApplicationInstallerFailed;
      }

      // `update_client` needs to view the below codes as a success, otherwise
      // it will consider the app as not installed; set the error category to
      // kNone in this case.
      result.result.category_ =
          result.result.code_ == ERROR_SUCCESS_REBOOT_INITIATED ||
                  result.result.code_ == ERROR_SUCCESS_REBOOT_REQUIRED ||
                  result.result.code_ == ERROR_SUCCESS_RESTART_REQUIRED
              ? update_client::ErrorCategory::kNone
              : update_client::ErrorCategory::kInstaller;
      result.installer_text = outcome.installer_text.value_or("");
      CHECK_NE(result.result.code_, 0);
      break;
  }

  return result;
}

// Clears the previous installer output, runs the application installer,
// queries the installer progress, then collects the process exit code, if
// waiting for the installer does not time out.
//
// Reports the exit code of the installer process as -1 if waiting for the
// process to exit times out.
//
// The installer progress is written by the application installer as a value
// under the application's client state in the Windows registry and read by
// polling in a loop, while waiting for the installer to exit.
InstallerResult RunApplicationInstaller(
    const AppInfo& app_info,
    const base::FilePath& app_installer,
    const std::string& arguments,
    const std::optional<base::FilePath>& installer_data_file,
    bool usage_stats_enabled,
    base::TimeDelta timeout,
    InstallProgressCallback progress_callback) {
  if (!app_installer.MatchesExtension(L".exe") &&
      !app_installer.MatchesExtension(L".msi")) {
    return InstallerResult(GOOPDATEINSTALL_E_FILENAME_INVALID,
                           kErrorInvalidFileExtension);
  }

  DeleteInstallerOutput(app_info.scope, app_info.app_id);

  const std::wstring argsw = base::UTF8ToWide(arguments);
  const std::wstring cmdline =
      app_installer.MatchesExtension(L".msi")
          ? BuildMsiCommandLine(argsw, installer_data_file, app_installer)
          : BuildExeCommandLine(argsw, installer_data_file, app_installer);
  VLOG(1) << "Running application installer: " << cmdline;

  base::LaunchOptions options;
  options.start_hidden = true;
  options.environment = {
      {ENV_GOOGLE_UPDATE_IS_MACHINE,
       IsSystemInstall(app_info.scope) ? L"1" : L"0"},
      {base::UTF8ToWide(kUsageStatsEnabled),
       usage_stats_enabled ? base::UTF8ToWide(kUsageStatsEnabledValueEnabled)
                           : L"0"},
  };

  int num_tries = 0;
  base::TimeDelta retry_delay = kAlreadyRunningRetryInitialDelay;
  int exit_code = -1;
  const base::ElapsedTimer timer;
  base::Process process;
  do {
    if (!num_tries || exit_code == ERROR_INSTALL_ALREADY_RUNNING) {
      if (num_tries > 0) {
        VLOG(1) << "Retrying: " << num_tries;
        base::PlatformThread::Sleep(retry_delay);
        retry_delay *= 2;  // Double the retry delay each time.
      }
      ++num_tries;

      process = base::LaunchProcess(cmdline, options);
      if (!process.IsValid()) {
        return InstallerResult(GOOPDATEINSTALL_E_INSTALLER_FAILED_START,
                               HRESULTFromLastError());
      }
    }

    bool wait_result = process.WaitForExitWithTimeout(
        base::Seconds(kWaitForInstallerProgressSec), &exit_code);
    auto progress = GetInstallerProgress(app_info.scope, app_info.app_id);
    VLOG(3) << "installer progress: " << progress;
    progress_callback.Run(progress);
    if (wait_result) {
      const Installer::Result installer_result = MakeInstallerResult(
          GetInstallerOutcome(app_info.scope, app_info.app_id), exit_code);
      exit_code = installer_result.result.code_;
      VLOG(1) << "Installer exit code " << exit_code;
      if (exit_code == ERROR_INSTALL_ALREADY_RUNNING) {
        continue;
      }
      return installer_result;
    }
  } while (timer.Elapsed() < timeout && num_tries < kNumAlreadyRunningMaxTries);

  return InstallerResult(exit_code == ERROR_INSTALL_ALREADY_RUNNING
                             ? GOOPDATEINSTALL_E_INSTALL_ALREADY_RUNNING
                             : GOOPDATEINSTALL_E_INSTALLER_TIMED_OUT);
}

std::string LookupString(const base::FilePath& path,
                         const std::string& keyname,
                         const std::string& default_value) {
  return default_value;
}

base::Version LookupVersion(UpdaterScope scope,
                            const std::string& app_id,
                            const base::FilePath& version_path,
                            const std::string& version_key,
                            const base::Version& default_value) {
  std::wstring pv;
  if (base::win::RegKey(UpdaterScopeToHKeyRoot(scope),
                        GetAppClientsKey(app_id).c_str(), Wow6432(KEY_READ))
          .ReadValue(kRegValuePV, &pv) == ERROR_SUCCESS) {
    base::Version value_version = base::Version(base::WideToUTF8(pv));
    return value_version.IsValid() ? value_version : default_value;
  }
  return default_value;
}

}  // namespace updater
