// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALLER_API_H_
#define CHROME_UPDATER_WIN_INSTALLER_API_H_

#include <optional>
#include <string>
#include <utility>

#include "base/win/registry.h"
#include "chrome/updater/enum_traits.h"
#include "chrome/updater/installer.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {

enum class UpdaterScope;

// The installer API consists of a set of registry values which are written by
// the application installer at various times during its execution.
// These values are under the corresponding app id subkey under ClientState key.
//
// `InstallerProgress` (DWORD) - a percentage value [0-100].
//
// `InstallerApiResult` (DWORD) - specifies the result type and how to determine
// success or failure. Allowable values are:
//
//   0 - Reports success regardless of the exit code or `InstallerError`.
//       If provided, `InstallerSuccessLaunchCmdLine` is used.
//
//   1 - The installer failed. If provided, `InstallerError`,
//   `InstallerExtraCode1`, and `InstallerResultUIString` are used.
//   The exit code is used if `InstallerError` is not available.
//
//   2 - The installer failed while executing an MSI. This is useful for custom
//   installers that wrap an MSI installer and report the msiexec exit code.
//   If provided, `InstallerError` and `InstallerExtraCode1` are used.
//   The exit code is used if `InstallerError` is not available.
//   The text describing the error is provided by using ::FormatMessage to
//   query the localized message tables of the operating system.
//
//   3 - The installer (non MSI) failed with a Windows system error code.
//   This is similar the MSI case above.
//
//   4 - Determines success or failure based on the exit code of the installer
//   process. Reports success if the exit code is 0, otherwise, an error is
//   reported. In the success case, `InstallerSuccessLaunchCmdLine` is used if
//   it is available.
//
// `InstallerError` (DWORD) - specifies the error (or success) value. Overrides
// the exit code unless `InstallerApiResult` indicates that the exit code must
// be used (case 4).
//
// `InstallerExtraCode1` (DWORD) - additional information set by the installer.
//
// `InstallerResultUIString` (String) - localized text to be displayed to the
// user in the error cases, if applicable.
//
// `InstallerSuccessLaunchCmdLine` (String) - command line to run in the
// success case.
//
// The following environment variables are passed to the installer:
//
// `UpdateIsMachine`: "1" (for a machine install) or "0" (for a user install).
//
// `%COMPANY%_USAGE_STATS_ENABLED`: "1" (if the updater sends usage stats), or
// "0" (if the updater does not send usage stats); %COMPANY% is the uppercase
// short company name specified in branding.gni (e.g. "GOOGLE").

template <>
struct EnumTraits<InstallerApiResult> {
  using R = InstallerApiResult;
  static constexpr R first_elem = R::kSuccess;
  static constexpr R last_elem = R::kExitCode;
};

// Contains the result of running the installer. These members correspond to
// the Installer API values written by the installer before the installer
// process exits. This data does not include the installer progress.
struct InstallerOutcome {
  InstallerOutcome();
  InstallerOutcome(const InstallerOutcome&);
  ~InstallerOutcome();

  std::optional<InstallerApiResult> installer_result;
  std::optional<int> installer_error;
  std::optional<int> installer_extracode1;
  std::optional<std::string> installer_text;
  std::optional<std::string> installer_cmd_line;
};

// Opens the registry ClientState subkey for the `app_id`.
std::optional<base::win::RegKey> ClientStateAppKeyOpen(
    UpdaterScope updater_scope,
    const std::string& app_id,
    REGSAM regsam);

// Deletes the `app_id` registry sub key under the `ClientState`.
bool ClientStateAppKeyDelete(UpdaterScope updater_scope,
                             const std::string& app_id);

// Reads installer progress for `app_id` from registry. The installer progress
// is written by the application installer. Returns a value in the [0, 100]
// range or -1 if the install progress is not available.
int GetInstallerProgress(UpdaterScope updater_scope, const std::string& app_id);
bool SetInstallerProgressForTesting(UpdaterScope updater_scope,
                                    const std::string& app_id,
                                    int value);

// Clear the Installer API values.
bool DeleteInstallerOutput(UpdaterScope updater_scope,
                           const std::string& app_id);

// Returns the Installer API outcome, best-effort, and renames the InstallerXXX
// values to LastInstallerXXX values. The LastInstallerXXX values remain around
// until the next update or install.
std::optional<InstallerOutcome> GetInstallerOutcome(UpdaterScope updater_scope,
                                                    const std::string& app_id);

// Returns the Last Installer API outcome, i.e., the LastInstallerXXX values.
std::optional<InstallerOutcome> GetClientStateKeyLastInstallerOutcome(
    UpdaterScope updater_scope,
    const std::string& app_id);
std::optional<InstallerOutcome> GetUpdaterKeyLastInstallerOutcome(
    UpdaterScope updater_scope);

bool SetInstallerOutcomeForTesting(UpdaterScope updater_scope,
                                   const std::string& app_id,
                                   const InstallerOutcome& installer_outcome);

// Translates the Installer API outcome into an `Installer::Result` value.
// * Handles installer exit codes correctly.
// * Handles non-zero success codes `ERROR_SUCCESS_RE{xxx}` correctly.
// * Uniformly sets `CrxInstaller::Result::error` to `0` for success, and
//   `kErrorApplicationInstallerFailed` for failure. The installer API code (or
//   exit code in the case of no installer API) is stored within
//   `CrxInstaller::Result::original_error` to avoid overlaps with
//   `update_client` error codes. Otherwise for instance error code `2` could
//   mean `FINGERPRINT_WRITE_FAILED = 2` or the windows error
//   `ERROR_FILE_NOT_FOUND`.
Installer::Result MakeInstallerResult(
    std::optional<InstallerOutcome> installer_outcome,
    int exit_code);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_INSTALLER_API_H_
