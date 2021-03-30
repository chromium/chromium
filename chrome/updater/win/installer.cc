// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/installer.h"

#include <algorithm>
#include <iterator>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "base/optional.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/enum_traits.h"
#include "chrome/updater/win/constants.h"
#include "chrome/updater/win/installer.h"

namespace updater {
namespace {

// Opens the registry ClientState subkey for the `app_id`.
base::Optional<base::win::RegKey> ClientStateAppKeyOpen(
    const std::string& app_id,
    REGSAM regsam) {
  std::wstring subkey;
  if (!base::UTF8ToWide(app_id.c_str(), app_id.size(), &subkey))
    return base::nullopt;
  regsam = regsam | KEY_WOW64_32KEY;
  base::win::RegKey key(HKEY_CURRENT_USER,
                        base::ASCIIToWide(CLIENT_STATE_KEY).c_str(), regsam);
  if (key.OpenKey(subkey.c_str(), regsam) != ERROR_SUCCESS)
    return base::nullopt;
  return key;
}

// Creates or opens the registry ClientState subkey for the `app_id`. `regsam`
// must contain the KEY_WRITE access right for the creation of the subkey to
// succeed.
base::Optional<base::win::RegKey> ClientStateAppKeyCreate(
    const std::string& app_id,
    REGSAM regsam) {
  std::wstring subkey;
  if (!base::UTF8ToWide(app_id.c_str(), app_id.size(), &subkey))
    return base::nullopt;
  regsam = regsam | KEY_WOW64_32KEY;
  base::win::RegKey key(HKEY_CURRENT_USER,
                        base::ASCIIToWide(CLIENT_STATE_KEY).c_str(), regsam);
  if (key.CreateKey(subkey.c_str(), regsam) != ERROR_SUCCESS)
    return base::nullopt;
  return key;
}

}  // namespace

InstallerOutcome::InstallerOutcome() = default;
InstallerOutcome::InstallerOutcome(const InstallerOutcome&) = default;
InstallerOutcome::~InstallerOutcome() = default;

bool ClientStateAppKeyDelete(const std::string& app_id) {
  std::wstring subkey;
  if (!base::UTF8ToWide(app_id.c_str(), app_id.size(), &subkey))
    return false;
  constexpr REGSAM kRegSam = KEY_WRITE | KEY_WOW64_32KEY;
  base::win::RegKey key(HKEY_CURRENT_USER,
                        base::ASCIIToWide(CLIENT_STATE_KEY).c_str(), kRegSam);
  return key.DeleteKey(subkey.c_str()) == ERROR_SUCCESS;
}

// Reads the installer progress from the registry value at:
// {HKLM|HKCU}\Software\Google\Update\ClientState\<appid>\InstallerProgress.
int GetInstallerProgress(const std::string& app_id) {
  base::Optional<base::win::RegKey> key =
      ClientStateAppKeyOpen(app_id, KEY_READ);
  DWORD progress = 0;
  if (!key || key->ReadValueDW(kRegValueInstallerProgress, &progress) !=
                  ERROR_SUCCESS) {
    return -1;
  }
  return base::ClampToRange(progress, DWORD{0}, DWORD{100});
}

bool SetInstallerProgressForTesting(const std::string& app_id, int value) {
  base::Optional<base::win::RegKey> key =
      ClientStateAppKeyCreate(app_id, KEY_WRITE);
  return key && key->WriteValue(kRegValueInstallerProgress, DWORD{value}) ==
                    ERROR_SUCCESS;
}

bool DeleteInstallerProgress(const std::string& app_id) {
  base::Optional<base::win::RegKey> key =
      ClientStateAppKeyOpen(app_id, KEY_SET_VALUE);
  return key && key->DeleteValue(kRegValueInstallerProgress) == ERROR_SUCCESS;
}

bool DeleteInstallerOutput(const std::string& app_id) {
  base::Optional<base::win::RegKey> key =
      ClientStateAppKeyOpen(app_id, KEY_SET_VALUE | KEY_QUERY_VALUE);
  if (!key)
    return false;
  auto delete_value = [&key](const wchar_t* value) {
    return key->HasValue(value) ? key->DeleteValue(value) == ERROR_SUCCESS
                                : true;
  };
  const bool results[] = {
      delete_value(kRegValueInstallerProgress),
      delete_value(kRegValueInstallerResult),
      delete_value(kRegValueInstallerError),
      delete_value(kRegValueInstallerExtraCode1),
      delete_value(kRegValueInstallerResultUIString),
      delete_value(kRegValueInstallerSuccessLaunchCmdLine),
  };
  return std::all_of(std::begin(results), std::end(results),
                     [](auto result) { return result; });
}

base::Optional<InstallerOutcome> GetInstallerOutcome(
    const std::string& app_id) {
  base::Optional<base::win::RegKey> key =
      ClientStateAppKeyOpen(app_id, KEY_READ);
  if (!key)
    return base::nullopt;
  InstallerOutcome installer_outcome;
  {
    DWORD val = 0;
    if (key->ReadValueDW(kRegValueInstallerResult, &val) == ERROR_SUCCESS) {
      installer_outcome.installer_result =
          *CheckedCastToEnum<InstallerResult>(val);
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

  return installer_outcome;
}

bool SetInstallerOutcomeForTesting(const std::string& app_id,
                                   const InstallerOutcome& installer_outcome) {
  base::Optional<base::win::RegKey> key =
      ClientStateAppKeyCreate(app_id, KEY_WRITE);
  if (!key)
    return false;
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

std::string GetTextForSystemError(int error) {
  wchar_t* system_allocated_buffer = nullptr;
  constexpr DWORD kFormatOptions =
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK;
  const DWORD chars_written = ::FormatMessage(
      kFormatOptions, nullptr, error, 0,
      reinterpret_cast<wchar_t*>(&system_allocated_buffer), 0, nullptr);
  auto free_buffer = base::ScopedClosureRunner(
      base::BindOnce(base::IgnoreResult(&LocalFree), system_allocated_buffer));
  return chars_written > 0 ? base::WideToUTF8(system_allocated_buffer)
                           : std::string();
}

// As much as possible, the implementation of this function is intended to be
// backward compatible with the implementation of the Installer API in
// Omaha/Google Update. Some edge cases could be missing.
// TODO(crbug.com/1172866): remove the hardcoded assumption that error must
// be zero to indicate success.
Installer::Result MakeInstallerResult(
    base::Optional<InstallerOutcome> installer_outcome,
    int exit_code) {
  if (installer_outcome && installer_outcome->installer_result) {
    Installer::Result result;
    switch (*installer_outcome->installer_result) {
      case InstallerResult::kSuccess:
        // This is unconditional success:
        // - use the command line if available, and ignore everything else.
        result.error = 0;
        if (installer_outcome->installer_cmd_line)
          result.installer_cmd_line = *installer_outcome->installer_cmd_line;
        DCHECK_EQ(result.error, 0);
        break;

      case InstallerResult::kCustomError:
        // This is an unconditional error:
        // - use the installer error, or the exit code, or report a generic
        //   error.
        // - use the installer extra code if available.
        // - use the text description of the error if available.
        result.error = installer_outcome->installer_error
                           ? *installer_outcome->installer_error
                           : exit_code;
        if (!result.error)
          result.error = kErrorApplicationInstallerFailed;
        if (installer_outcome->installer_extracode1)
          result.extended_error = *installer_outcome->installer_extracode1;
        if (installer_outcome->installer_text)
          result.installer_text = *installer_outcome->installer_text;
        DCHECK_NE(result.error, 0);
        break;

      case InstallerResult::kMsiError:
      case InstallerResult::kSystemError:
        // This is an unconditional error:
        // - same as the case above but use a system-provided text.
        result.error = installer_outcome->installer_error
                           ? *installer_outcome->installer_error
                           : exit_code;
        if (!result.error)
          result.error = kErrorApplicationInstallerFailed;
        if (installer_outcome->installer_extracode1)
          result.extended_error = *installer_outcome->installer_extracode1;
        result.installer_text = GetTextForSystemError(result.error);
        DCHECK_NE(result.error, 0);
        break;

      case InstallerResult::kExitCode:
        // This is could be a success or an error.
        // - if success, then use the command line if available.
        // - if an error, then ignore everything.
        result.error = exit_code;
        if (result.error == 0 && installer_outcome->installer_cmd_line)
          result.installer_cmd_line = *installer_outcome->installer_cmd_line;
        break;
    }
    return result;
  }

  return exit_code == 0
             ? Installer::Result(update_client::InstallError::NONE)
             : Installer::Result(kErrorApplicationInstallerFailed, exit_code);
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
Installer::Result Installer::RunApplicationInstaller(
    const base::FilePath& app_installer,
    const std::string& arguments,
    ProgressCallback progress_callback) {
  DeleteInstallerOutput(app_id());

  base::LaunchOptions options;
  options.start_hidden = true;
  const auto cmdline =
      base::StrCat({base::CommandLine(app_installer).GetCommandLineString(),
                    L" ", base::UTF8ToWide(arguments)});
  VLOG(1) << "Running application installer: " << cmdline;
  auto process = base::LaunchProcess(cmdline, options);
  int exit_code = -1;
  const auto time_begin = base::Time::NowFromSystemTime();
  do {
    bool wait_result = process.WaitForExitWithTimeout(
        base::TimeDelta::FromSeconds(kWaitForInstallerProgressSec), &exit_code);
    auto progress = GetInstallerProgress(app_id());
    DVLOG(3) << "installer progress: " << progress;
    progress_callback.Run(progress);
    if (wait_result) {
      VLOG(1) << "Installer exit code " << exit_code;
      break;
    }
  } while (base::Time::NowFromSystemTime() - time_begin <=
           base::TimeDelta::FromSeconds(kWaitForAppInstallerSec));

  return MakeInstallerResult(GetInstallerOutcome(app_id()), exit_code);
}

}  // namespace updater
