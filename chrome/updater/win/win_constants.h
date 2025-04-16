// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_WIN_CONSTANTS_H_
#define CHROME_UPDATER_WIN_WIN_CONSTANTS_H_

#include <windows.h>

#include "base/time/time.h"
#include "chrome/updater/updater_branding.h"

namespace updater {

inline constexpr wchar_t kLegacyGoogleUpdateAppID[] =
    L"" LEGACY_GOOGLE_UPDATE_APPID;

inline constexpr wchar_t kGoogleUpdate3WebSystemClassProgId[] =
    COMPANY_SHORTNAME_STRING L"Update.Update3WebMachine";
inline constexpr wchar_t kGoogleUpdate3WebUserClassProgId[] =
    COMPANY_SHORTNAME_STRING L"Update.Update3WebUser";

// The prefix to use for global names in WIN32 API's. The prefix is necessary
// to avoid collision on kernel object names.
inline constexpr wchar_t kGlobalPrefix[] = L"Global\\G";

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

inline constexpr wchar_t kRegValuePV[] = L"pv";
inline constexpr wchar_t kRegValueBrandCode[] = L"brand";
inline constexpr wchar_t kRegValueAP[] = L"ap";
inline constexpr wchar_t kRegValueLang[] = L"lang";
inline constexpr wchar_t kRegValueDateOfLastActivity[] = L"DayOfLastActivity";
inline constexpr wchar_t kRegValueDateOfLastRollcall[] = L"DayOfLastRollCall";
inline constexpr wchar_t kRegValueDayOfInstall[] = L"DayOfInstall";
inline constexpr wchar_t kRegValueName[] = L"name";

// Values created under `UPDATER_KEY`.
inline constexpr wchar_t kRegValueUninstallCmdLine[] = L"UninstallCmdLine";
inline constexpr wchar_t kRegValueVersion[] = L"version";

// Timestamp when an OEM install is started, stored as minutes since the Windows
// Epoch.
inline constexpr wchar_t kRegValueOemInstallTimeMin[] = L"OemInstallTime";

// OEM installs are expected to be completed within 72 hours.
inline constexpr base::TimeDelta kMinOemModeTime = base::Hours(72);

// Windows Audit mode registry constants queried for OEM installs.
inline constexpr wchar_t kSetupStateKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Setup\\State";
inline constexpr wchar_t kImageStateValueName[] = L"ImageState";
inline constexpr wchar_t kImageStateUnuseableValue[] =
    L"IMAGE_STATE_UNDEPLOYABLE";
inline constexpr wchar_t kImageStateGeneralAuditValue[] =
    L"IMAGE_STATE_GENERALIZE_RESEAL_TO_AUDIT";
inline constexpr wchar_t kImageStateSpecialAuditValue[] =
    L"IMAGE_STATE_SPECIALIZE_RESEAL_TO_AUDIT";

// Cohort registry constants.
inline constexpr wchar_t kRegKeyCohort[] = L"cohort";
inline constexpr wchar_t kRegValueCohortName[] = L"name";
inline constexpr wchar_t kRegValueCohortHint[] = L"hint";

// Installer API registry names.
// Registry values read from the Clients key for transmitting custom install
// errors, messages, etc. On an update or install, the InstallerXXX values are
// renamed to LastInstallerXXX values. The LastInstallerXXX values remain around
// until the next update or install. Legacy MSI installers read values such as
// the `LastInstallerResultUIString` from the `ClientState` key in the registry
// and display the string.
inline constexpr wchar_t kRegValueInstallerError[] = L"InstallerError";
inline constexpr wchar_t kRegValueInstallerExtraCode1[] =
    L"InstallerExtraCode1";
inline constexpr wchar_t kRegValueInstallerProgress[] = L"InstallerProgress";
inline constexpr wchar_t kRegValueInstallerResult[] = L"InstallerResult";
inline constexpr wchar_t kRegValueInstallerResultUIString[] =
    L"InstallerResultUIString";
inline constexpr wchar_t kRegValueInstallerSuccessLaunchCmdLine[] =
    L"InstallerSuccessLaunchCmdLine";

inline constexpr wchar_t kRegValueLastInstallerResult[] =
    L"LastInstallerResult";
inline constexpr wchar_t kRegValueLastInstallerError[] = L"LastInstallerError";
inline constexpr wchar_t kRegValueLastInstallerExtraCode1[] =
    L"LastInstallerExtraCode1";
inline constexpr wchar_t kRegValueLastInstallerResultUIString[] =
    L"LastInstallerResultUIString";
inline constexpr wchar_t kRegValueLastInstallerSuccessLaunchCmdLine[] =
    L"LastInstallerSuccessLaunchCmdLine";

inline constexpr const wchar_t* kRegValuesLastInstaller[5] = {
    kRegValueLastInstallerResult, kRegValueLastInstallerError,
    kRegValueLastInstallerExtraCode1, kRegValueLastInstallerResultUIString,
    kRegValueLastInstallerSuccessLaunchCmdLine};

// AppCommand registry constants.
inline constexpr wchar_t kRegKeyCommands[] = L"Commands";
inline constexpr wchar_t kRegValueCommandLine[] = L"CommandLine";
inline constexpr wchar_t kRegValueAutoRunOnOSUpgrade[] = L"AutoRunOnOSUpgrade";

// Device management.
//
// Registry for enrollment token.
inline constexpr wchar_t kRegKeyCompanyCloudManagement[] =
    COMPANY_POLICIES_KEY L"CloudManagement\\";
inline constexpr wchar_t kRegValueEnrollmentToken[] = L"EnrollmentToken";

// Legacy registry for enrollment token.
inline constexpr wchar_t kRegKeyCompanyLegacyCloudManagement[] =
    COMPANY_POLICIES_KEY BROWSER_NAME_STRING L"\\";
inline constexpr wchar_t kRegValueCloudManagementEnrollmentToken[] =
    L"CloudManagementEnrollmentToken";

// The name of the policy indicating that enrollment in cloud-based device
// management is mandatory.
inline constexpr wchar_t kRegValueEnrollmentMandatory[] =
    L"EnrollmentMandatory";

// Registry for DM token.
inline constexpr wchar_t kRegKeyCompanyEnrollment[] =
    COMPANY_KEY L"Enrollment\\";
inline constexpr wchar_t kRegKeyCompanyLegacyEnrollment[] =
    COMPANY_KEY L"\\" BROWSER_NAME_STRING L"\\Enrollment\\";  // Path is in
                                                              // HKLM64.
inline constexpr wchar_t kRegValueDmToken[] = L"dmtoken";

inline constexpr wchar_t kWindowsServiceName[] = L"Service";
inline constexpr wchar_t kWindowsInternalServiceName[] = L"InternalService";

// Windows event name used to signal the legacy GoogleUpdate processes to exit.
inline constexpr wchar_t kShutdownEvent[] =
    L"{A0C1F415-D2CE-4ddc-9B48-14E56FD55162}";

// EXE name for the legacy GoogleUpdate processes.
inline constexpr wchar_t kLegacyExeName[] =
    COMPANY_SHORTNAME_STRING L"Update.exe";

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
inline constexpr wchar_t kLegacyServiceNamePrefix[] =
    L"" LEGACY_SERVICE_NAME_PREFIX;

// "Google Update Service" is the common prefix for the legacy GoogleUpdate
// service display names.
inline constexpr wchar_t kLegacyServiceDisplayNamePrefix[] =
    COMPANY_SHORTNAME_STRING L" Update Service";

// "Google Update" is the prefix for the legacy GoogleUpdate "Run" key value
// under HKCU.
inline constexpr wchar_t kLegacyRunValuePrefix[] =
    COMPANY_SHORTNAME_STRING L" Update";

// "GoogleUpdateTask{Machine/User}" is the common prefix for the legacy
// GoogleUpdate tasks for system and user respectively.
inline constexpr wchar_t kLegacyTaskNamePrefixSystem[] =
    COMPANY_SHORTNAME_STRING L"UpdateTaskMachine";
inline constexpr wchar_t kLegacyTaskNamePrefixUser[] =
    COMPANY_SHORTNAME_STRING L"UpdateTaskUser";

// `InstallerResult` values defined by the Installer API.
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
