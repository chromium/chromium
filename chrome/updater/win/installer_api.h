// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALLER_API_H_
#define CHROME_UPDATER_WIN_INSTALLER_API_H_

#include <string>

#include "chrome/updater/enum_traits.h"
#include "chrome/updater/installer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

enum class UpdaterScope;

// The installer API consists of a set of registry values which are written by
// the application installer at various times during its execution.
// These values are under the corresponding app id subkey under ClientState key.
//
// `InstallerProgress` (DWORD) - a percentage value [0-100].
//
// `InstallerResult` (DWORD) - specifies the result type and how to determine
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
// the exit code unless `InstallerResult` indicates that the exit code must be
// used (case 4).
//
// `InstallerExtraCode1` (DWORD) - additional information set by the installer.
//
// `InstallerResultUIString` (String) - localized text to be displayed to the
// user in the error cases, if applicable.
//
// `InstallerSuccessLaunchCmdLine` (String) - command line to run in the
// success case.

// These values are defined by the Installer API.
enum class InstallerResult {
  // The installer succeeded, unconditionally.
  kSuccess = 0,

  // The installer returned a specific error using the Installer API mechanism.
  kCustomError = 1,

  // TODO(crbug.com/1139013): support MSI payloads.
  // The MSI installer failed, with a system error.
  kMsiError = 2,

  // The installer failed with a a system error.
  kSystemError = 3,

  // The installer failed. The exit code of the installer process contains
  // the error.
  kExitCode = 4,
};

template <>
struct EnumTraits<InstallerResult> {
  using R = InstallerResult;
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

  absl::optional<InstallerResult> installer_result;
  absl::optional<int> installer_error;
  absl::optional<int> installer_extracode1;
  absl::optional<std::string> installer_text;
  absl::optional<std::string> installer_cmd_line;
};

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

// Returns the Instaler API outcome, best-effort.
absl::optional<InstallerOutcome> GetInstallerOutcome(UpdaterScope updater_scope,
                                                     const std::string& app_id);
bool SetInstallerOutcomeForTesting(UpdaterScope updater_scope,
                                   const std::string& app_id,
                                   const InstallerOutcome& installer_outcome);

// Translates the Installer API outcome into an `Installer::Result` value.
// `exit_code` is the exit code of the installer process, which may be used
// in some cases, depending on the installer outcome.
Installer::Result MakeInstallerResult(
    absl::optional<InstallerOutcome> installer_outcome,
    int exit_code);

// Returns the textual description of a system `error` as provided
// by the operating system. The function assumes that the locale value for
// the calling thread is set, otherwise, the function uses the user/system
// default LANGID, or it defaults to US English.
std::string GetTextForSystemError(int error);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_INSTALLER_API_H_
