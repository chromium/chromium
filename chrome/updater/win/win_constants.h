// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_WIN_CONSTANTS_H_
#define CHROME_UPDATER_WIN_WIN_CONSTANTS_H_

#include <windows.h>

#include "base/time/time.h"
#include "chrome/updater/updater_branding.h"

namespace updater {

extern const wchar_t kLegacyGoogleUpdateAppID[];

extern const wchar_t kGoogleUpdate3WebSystemClassProgId[];
extern const wchar_t kGoogleUpdate3WebUserClassProgId[];

// The prefix to use for global names in WIN32 API's. The prefix is necessary
// to avoid collision on kernel object names.
extern const wchar_t kGlobalPrefix[];

// Registry keys and value names.
#define COMPANY_KEY L"Software\\" COMPANY_SHORTNAME_STRING L"\\"

// Use |Update| instead of PRODUCT_FULLNAME_STRING for the registry key name
// to be backward compatible with Google Update / Omaha.
#define UPDATER_KEY COMPANY_KEY L"Update\\"
#define CLIENTS_KEY UPDATER_KEY L"Clients\\"
#define CLIENT_STATE_KEY UPDATER_KEY L"ClientState\\"
#define CLIENT_STATE_MEDIUM_KEY UPDATER_KEY L"ClientStateMedium\\"

#define COMPANY_POLICIES_KEY \
  L"Software\\Policies\\" COMPANY_SHORTNAME_STRING L"\\"
#define UPDATER_POLICIES_KEY COMPANY_POLICIES_KEY L"Update\\"

#define USER_REG_VISTA_LOW_INTEGRITY_HKCU     \
  L"Software\\Microsoft\\Internet Explorer\\" \
  L"InternetRegistry\\REGISTRY\\USER"

// The environment variable is created into the environment of the installer
// process to indicate the updater scope. The valid values for the environment
// variable are "0" and "1".
#define ENV_GOOGLE_UPDATE_IS_MACHINE COMPANY_SHORTNAME_STRING L"UpdateIsMachine"

extern const wchar_t kRegValuePV[];
extern const wchar_t kRegValueBrandCode[];
extern const wchar_t kRegValueAP[];
extern const wchar_t kRegValueDateOfLastActivity[];
extern const wchar_t kRegValueDateOfLastRollcall[];
extern const wchar_t kRegValueDayOfInstall[];
extern const wchar_t kRegValueName[];

// Values created under `UPDATER_KEY`.
extern const wchar_t kRegValueUninstallCmdLine[];
extern const wchar_t kRegValueVersion[];

// Timestamp when an OEM install is started, stored as minutes since the Windows
// Epoch.
extern const wchar_t kRegValueOemInstallTimeMin[];

// OEM installs are expected to be completed within 72 hours.
inline constexpr base::TimeDelta kMinOemModeTime = base::Hours(72);

// Windows Audit mode registry constants queried for OEM installs.
extern const wchar_t kSetupStateKey[];
extern const wchar_t kImageStateValueName[];
extern const wchar_t kImageStateUnuseableValue[];
extern const wchar_t kImageStateGeneralAuditValue[];
extern const wchar_t kImageStateSpecialAuditValue[];

// Cohort registry constants.
extern const wchar_t kRegKeyCohort[];
extern const wchar_t kRegValueCohortName[];
extern const wchar_t kRegValueCohortHint[];

// Installer API registry names.
// Registry values read from the Clients key for transmitting custom install
// errors, messages, etc. On an update or install, the InstallerXXX values are
// renamed to LastInstallerXXX values. The LastInstallerXXX values remain around
// until the next update or install. Legacy MSI installers read values such as
// the `LastInstallerResultUIString` from the `ClientState` key in the registry
// and display the string.
extern const wchar_t kRegValueInstallerError[];
extern const wchar_t kRegValueInstallerExtraCode1[];
extern const wchar_t kRegValueInstallerProgress[];
extern const wchar_t kRegValueInstallerResult[];
extern const wchar_t kRegValueInstallerResultUIString[];
extern const wchar_t kRegValueInstallerSuccessLaunchCmdLine[];

extern const wchar_t kRegValueLastInstallerResult[];
extern const wchar_t kRegValueLastInstallerError[];
extern const wchar_t kRegValueLastInstallerExtraCode1[];
extern const wchar_t kRegValueLastInstallerResultUIString[];
extern const wchar_t kRegValueLastInstallerSuccessLaunchCmdLine[];

extern const wchar_t* const kRegValuesLastInstaller[5];

// AppCommand registry constants.
extern const wchar_t kRegKeyCommands[];
extern const wchar_t kRegValueCommandLine[];
extern const wchar_t kRegValueAutoRunOnOSUpgrade[];

// Device management.
//
// Registry for enrollment token.
extern const wchar_t kRegKeyCompanyCloudManagement[];
extern const wchar_t kRegValueEnrollmentToken[];

// Legacy registry for enrollment token.
extern const wchar_t kRegKeyCompanyLegacyCloudManagement[];
extern const wchar_t kRegValueCloudManagementEnrollmentToken[];

// The name of the policy indicating that enrollment in cloud-based device
// management is mandatory.
extern const wchar_t kRegValueEnrollmentMandatory[];

// Registry for DM token.
extern const wchar_t kRegKeyCompanyEnrollment[];
extern const wchar_t kRegKeyCompanyLegacyEnrollment[];  // Path is in HKLM64.
extern const wchar_t kRegValueDmToken[];

extern const wchar_t kWindowsServiceName[];
extern const wchar_t kWindowsInternalServiceName[];

// Windows event name used to signal the legacy GoogleUpdate processes to exit.
extern const wchar_t kShutdownEvent[];

// EXE name for the legacy GoogleUpdate processes.
extern const wchar_t kLegacyExeName[];

// crbug.com/1259178: there is a race condition on activating the COM service
// and the service shutdown. The race condition is likely to occur when a new
// instance of an updater coclass is created right after the last reference to
// an object hosted by the COM service is released. Therefore, introducing a
// slight delay before creating coclasses reduces (but it does not eliminate)
// the probability of running into this race condition, until a better
// solution is found.
inline constexpr base::TimeDelta kCreateUpdaterInstanceDelay =
    base::Milliseconds(200);

// `kLegacyServiceNamePrefix` is the common prefix for the legacy GoogleUpdate
// service names.
extern const wchar_t kLegacyServiceNamePrefix[];

// "Google Update Service" is the common prefix for the legacy GoogleUpdate
// service display names.
extern const wchar_t kLegacyServiceDisplayNamePrefix[];

// "Google Update" is the prefix for the legacy GoogleUpdate "Run" key value
// under HKCU.
extern const wchar_t kLegacyRunValuePrefix[];

// "GoogleUpdateTask{Machine/User}" is the common prefix for the legacy
// GoogleUpdate tasks for system and user respectively.
extern const wchar_t kLegacyTaskNamePrefixSystem[];
extern const wchar_t kLegacyTaskNamePrefixUser[];

// `InstallerApiResult` values defined by the Installer API.
enum class InstallerApiResult {
  // The installer succeeded, unconditionally.
  // - if a launch command was provided via the installer API, the command will
  //   be launched and the updater UI will exit silently. Otherwise, the updater
  //   will show an install success dialog.
  kSuccess = 0,

  // All the error installer results below are treated the same.
  // - if an installer error was not provided via the installer API or the exit
  //   code, generic error `kErrorApplicationInstallerFailed` will be reported.
  // - the installer extra code is used if reported via the installer API.
  // - the text description of the error is used if reported via the installer
  //   API.
  // If an installer result is not explicitly reported by the installer, the
  // installer API values are internally set based on whether the exit code from
  // the installer process is a success or an error:
  // - If the exit code is a success, the installer result is set to success. If
  //   a launch command was provided via the installer API, the command will be
  //   launched and the updater UI will exit silently. Otherwise, the updater
  //   will show an install success dialog.
  // - If the exit code is a failure, the installer result is set to
  //   `kExitCode`, the installer error is set to
  //   `kErrorApplicationInstallerFailed`, and the installer extra code is set
  //   to the exit code.
  // - If a text description is reported via the installer API, it will be used.
  kCustomError = 1,
  kMsiError = 2,
  kSystemError = 3,
  kExitCode = 4,
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_WIN_CONSTANTS_H_
