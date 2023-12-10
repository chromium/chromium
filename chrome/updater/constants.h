// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CONSTANTS_H_
#define CHROME_UPDATER_CONSTANTS_H_

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/update_client/update_client_errors.h"

namespace updater {

// Key for storing the installer version in the install settings dictionary.
extern const char kInstallerVersion[];

// The updater specific app ID.
extern const char kUpdaterAppId[];

// The app ID used to qualify the updater.
extern const char kQualificationAppId[];

// The name of the updater program image, typically, "updater.exe" or "updater".
extern const char kExecutableName[];

// A suffix appended to the updater executable name before any file extension.
extern const char kExecutableSuffix[];

// "0.0.0.0". Historically, a null version has been used to indicate a
// new install.
extern const char kNullVersion[];

// Command line switches.
//
// This switch starts the COM server. This switch is invoked by the COM runtime
// when CoCreate is called on one of several CLSIDs that the server supports.
// We expect to use the COM server for the following scenarios:
// * The Server for the UI when installing User applications, and as a fallback
//   if the Service fails when installing Machine applications.
// * The On-Demand COM Server when running for User, and as a fallback if the
//   Service fails.
// * COM Server for launching processes at Medium Integrity, i.e., de-elevating.
// * The On-Demand COM Server when the client requests read-only interfaces
//   (say, Policy queries) at Medium Integrity.
// * A COM broker when running modes such as On-Demand for Machine. A COM broker
//   is something that acts as an intermediary, allowing creating one of several
//   possible COM objects that can service a particular COM request, ranging
//   from Privileged Services that can offer full functionality for a particular
//   set of interfaces, to Medium Integrity processes that can offer limited
//   (say, read-only) functionality for that same set of interfaces.
extern const char kServerSwitch[];

// This switch specifies the XPC service the server registers to listen to.
extern const char kServerServiceSwitch[];

// Valid values for the kServerServiceSwitch.
extern const char kServerUpdateServiceInternalSwitchValue[];
extern const char kServerUpdateServiceSwitchValue[];

// This switch starts the Windows service. This switch is invoked by the SCM
// either as a part of system startup (`SERVICE_AUTO_START`) or when `CoCreate`
// is called on one of several CLSIDs that the server supports.
extern const char kWindowsServiceSwitch[];

// This switch indicates that the Windows service is in the COM server mode.
// This switch is passed to `ServiceMain` by the SCM when CoCreate is called on
// one of several CLSIDs that the server supports. We expect to use the COM
// service for the following scenarios:
// * The Server for the UI when installing Machine applications.
// * The On-Demand COM Server for Machine applications.
// * COM Server for launching processes at System Integrity, i.e., an Elevator.
extern const char kComServiceSwitch[];

// Crash the program for testing purposes.
extern const char kCrashMeSwitch[];

// Runs as the Crashpad handler.
extern const char kCrashHandlerSwitch[];

// Updates the updater.
extern const char kUpdateSwitch[];

// Installs the updater.
extern const char kInstallSwitch[];
extern const char kRuntimeSwitch[];

// Contains the meta installer tag. The tag is a string of arguments, separated
// by a delimiter (in this case, the delimiter is =). The tag is typically
// embedded in the program image of the metainstaller, but for testing purposes,
// the tag could be passed directly as a command line argument. The tag is
// currently encoded as a ASCII string.
extern const char kTagSwitch[];

// The --installerdata=file.dat switch is passed to an installer if an
// installdataindex is specified in the tag or if installerdata is passed in via
// --appargs. The corresponding installerdata is written to file.dat with an
// UTF8 encoding as well as a UTF8 BOM.
extern const char kInstallerDataSwitch[];

// Uninstalls the updater.
extern const char kUninstallSwitch[];

// Uninstalls this version of the updater.
extern const char kUninstallSelfSwitch[];

// Uninstalls the updater if no apps are managed by it.
extern const char kUninstallIfUnusedSwitch[];

// Kicks off the update service. This switch is typically used for by a
// scheduled to invoke the updater periodically.
extern const char kWakeSwitch[];

// Kicks off the update service for all versions.
extern const char kWakeAllSwitch[];

// The updater needs to operate in the system context.
extern const char kSystemSwitch[];

// Runs in test mode. Currently, it exits right away.
extern const char kTestSwitch[];

// Run in recovery mode.
extern const char kRecoverSwitch[];

// The version of the program triggering recovery.
extern const char kBrowserVersionSwitch[];

// The session ID of the Omaha session triggering recovery.
extern const char kSessionIdSwitch[];

// The app ID of the program triggering recovery.
extern const char kAppGuidSwitch[];

// Disables throttling for the crash reported until the following bug is fixed:
// https://bugs.chromium.org/p/crashpad/issues/detail?id=23
extern const char kNoRateLimitSwitch[];

// The handle of an event to signal when the initialization of the main process
// is complete.
extern const char kInitDoneNotifierSwitch[];

// Enables logging.
extern const char kEnableLoggingSwitch[];

// Specifies the logging module filter and its value. Note that some call sites
// may still use different values for the logging module filter.
extern const char kLoggingModuleSwitch[];
extern const char kLoggingModuleSwitchValue[];

// Specifies the application that the Updater needs to install.
extern const char kAppIdSwitch[];

// Specifies the version of the application that the updater needs to register.
extern const char kAppVersionSwitch[];

// Specifies that the Updater should perform some minimal checks to verify that
// it is operational/healthy. This is for backward compatibility with Omaha 3.
// Omaha 3 runs "GoogleUpdate.exe /healthcheck" and expects an exit code of
// HRESULT SUCCESS, i.e., S_OK, in which case it will hand off the installation
// to Omaha 4.
extern const char kHealthCheckSwitch[];

// Specifies the enterprise request argument. On Windows, the request may
// be from legacy updaters which pass the argument in the format of
// `/enterprise`. Manual argument parsing is needed for that scenario.
extern const char kEnterpriseSwitch[];

// Specifies that no UI should be shown.
extern const char kSilentSwitch[];

// Specifies the handoff request argument. On Windows, the request may
// be from legacy updaters which pass the argument in the format of
// `/handoff <install-args-details>`. Manual argument parsing is needed for that
// scenario.
extern const char kHandoffSwitch[];

// Specifies the full path to the offline install resources. The folder
// contains offline installer and the manifest file.
extern const char kOfflineDirSwitch[];

// Specifies extra app args. The switch must be in the following format:
//     --appargs="appguid=<appid>&installerdata=<URL-encoded-installer-data>"
// On Windows, the request may be from legacy updaters which pass the argument
// in the format of `/appargs <value>`. Manual argument parsing is needed for
// that scenario.
extern const char kAppArgsSwitch[];

// The "expect-elevated" switch indicates that updater setup should be running
// elevated (at high integrity). This switch is needed to avoid running into a
// loop trying (but failing repeatedly) to elevate updater setup when attempting
// to install on a standard user account with UAC disabled.
extern const char kCmdLineExpectElevated[];

// The "expect-de-elevated" switch indicates that updater setup should be
// running de-elevated (at medium integrity). This switch is needed to avoid
// running into a loop trying (but failing repeatedly) to de-elevate updater
// setup when attempting to install as a standard user account with UAC enabled.
extern const char kCmdLineExpectDeElevated[];

// The "prefers-user" switch indicates that updater setup could not elevate, and
// is now trying to install the app per-user.
extern const char kCmdLinePrefersUser[];

// Environment variables.
extern const char kUsageStatsEnabled[];
extern const char kUsageStatsEnabledValueEnabled[];

// File system paths.
//
// The directory name where CRX apps get installed. This is provided for demo
// purposes, since products installed by this updater will be installed in
// their specific locations.
extern const char kAppsDir[];

// The name of the uninstall script which is invoked by the --uninstall switch.
extern const char kUninstallScript[];

// Developer override keys.
extern const char kDevOverrideKeyUrl[];
extern const char kDevOverrideKeyCrashUploadUrl[];
extern const char kDevOverrideKeyDeviceManagementUrl[];
extern const char kDevOverrideKeyUseCUP[];
extern const char kDevOverrideKeyInitialDelay[];
extern const char kDevOverrideKeyServerKeepAliveSeconds[];
extern const char kDevOverrideKeyCrxVerifierFormat[];
extern const char kDevOverrideKeyGroupPolicies[];
extern const char kDevOverrideKeyOverinstallTimeout[];
extern const char kDevOverrideKeyIdleCheckPeriodSeconds[];
extern const char kDevOverrideKeyManagedDevice[];
extern const char kDevOverrideKeyEnableDiffUpdates[];

// Timing constants.
// How long to wait for an application installer (such as chrome_installer.exe)
// to complete.
inline constexpr base::TimeDelta kWaitForAppInstaller = base::Minutes(15);

// How long to wait for the common setup lock for
// AppInstall/AppUninstall/AppUpdate.
inline constexpr base::TimeDelta kWaitForSetupLock = base::Seconds(5);

// The default last check period is 4.5 hours.
inline constexpr base::TimeDelta kDefaultLastCheckPeriod =
    base::Hours(4) + base::Minutes(30);

#if BUILDFLAG(IS_WIN)
// How often the installer progress from registry is sampled. This value may
// be changed to provide a smoother progress experience (crbug.com/1067475).
inline constexpr int kWaitForInstallerProgressSec = 1;
#elif BUILDFLAG(IS_MAC)
// How long to wait for launchd changes to be reported by launchctl.
inline constexpr int kWaitForLaunchctlUpdateSec = 5;
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_MAC)
// The user defaults suite name.
extern const char kUserDefaultsSuiteName[];
#endif  // BUILDFLAG(IS_MAC)

// Install Errors.
//
// Specific install errors for the updater are reported in such a way that
// their range does not conflict with the range of generic errors defined by
// the |update_client| module.
inline constexpr int kCustomInstallErrorBase =
    static_cast<int>(update_client::InstallError::CUSTOM_ERROR_BASE);

// Running the application installer failed.
inline constexpr int kErrorApplicationInstallerFailed =
    kCustomInstallErrorBase + 3;

// The errors below are reported in the `extra_code1` in the
// `CrxInstaller::Result` structure, with the `error` reported as
// `GOOPDATEINSTALL_E_FILENAME_INVALID`. `GOOPDATEINSTALL_E_FILENAME_INVALID` is
// used to avoid overlaps of the specific error codes below with Windows error
// codes.

// The install params are missing. This usually means that the update
// response does not include the name of the installer and its command line
// arguments.
inline constexpr int kErrorMissingInstallParams = kCustomInstallErrorBase + 1;

// The file specified by the manifest |run| attribute could not be found
// inside the CRX.
inline constexpr int kErrorMissingRunableFile = kCustomInstallErrorBase + 2;

// The file extension for the installer is not supported. For instance, on
// Windows, only `.exe` and `.msi` extensions are supported.
inline constexpr int kErrorInvalidFileExtension = kCustomInstallErrorBase + 4;

// Error codes.
//
// The server process may exit with any of these exit codes.
inline constexpr int kErrorOk = 0;

// The server could not acquire the lock needed to run.
inline constexpr int kErrorFailedToLockPrefsMutex = 1;

// The server candidate failed to promote itself to active.
inline constexpr int kErrorFailedToSwap = 2;

inline constexpr int kErrorRegistrationFailed = 3;
inline constexpr int kErrorPermissionDenied = 4;
inline constexpr int kErrorWaitFailedUninstall = 5;
inline constexpr int kErrorWaitFailedInstall = 6;
inline constexpr int kErrorPathServiceFailed = 7;
inline constexpr int kErrorComInitializationFailed = 8;
inline constexpr int kErrorUnknownCommandLine = 9;
inline constexpr int kErrorNoVersionedDirectory = 11;
inline constexpr int kErrorNoBaseDirectory = 12;
inline constexpr int kErrorPathTooLong = 13;
inline constexpr int kErrorProcessLaunchFailed = 14;

// Failed to copy the updater's bundle.
inline constexpr int kErrorFailedToCopyBundle = 15;

// Failed to delete the updater's install folder.
inline constexpr int kErrorFailedToDeleteFolder = 16;

// Failed to delete the updater's data folder.
inline constexpr int kErrorFailedToDeleteDataFolder = 17;

// Failed to get versioned updater folder path.
inline constexpr int kErrorFailedToGetVersionedInstallDirectory = 18;

// Failed to get the install directory.
inline constexpr int kErrorFailedToGetInstallDir = 19;

// Failed to remove the active(unversioned) update service job from Launchd.
inline constexpr int kErrorFailedToRemoveActiveUpdateServiceJobFromLaunchd = 20;

// Failed to remove versioned update service job from Launchd.
inline constexpr int kErrorFailedToRemoveCandidateUpdateServiceJobFromLaunchd =
    21;

// Failed to remove versioned update service internal job from Launchd.
inline constexpr int kErrorFailedToRemoveUpdateServiceInternalJobFromLaunchd =
    22;

// Failed to remove versioned wake job from Launchd.
inline constexpr int kErrorFailedToRemoveWakeJobFromLaunchd = 23;

// Failed to create the active(unversioned) update service Launchd plist.
inline constexpr int kErrorFailedToCreateUpdateServiceLaunchdJobPlist = 24;

// Failed to create the versioned update service Launchd plist.
inline constexpr int kErrorFailedToCreateVersionedUpdateServiceLaunchdJobPlist =
    25;

// Failed to create the versioned update service internal Launchd plist.
inline constexpr int kErrorFailedToCreateUpdateServiceInternalLaunchdJobPlist =
    26;

// Failed to create the versioned wake Launchd plist.
inline constexpr int kErrorFailedToCreateWakeLaunchdJobPlist = 27;

// Failed to start the active(unversioned) update service job.
inline constexpr int kErrorFailedToStartLaunchdActiveServiceJob = 28;

// Failed to start the versioned update service job.
inline constexpr int kErrorFailedToStartLaunchdVersionedServiceJob = 29;

// Failed to start the update service internal job.
inline constexpr int kErrorFailedToStartLaunchdUpdateServiceInternalJob = 30;

// Failed to start the wake job.
inline constexpr int kErrorFailedToStartLaunchdWakeJob = 31;

// Timed out while awaiting launchctl to become aware of the update service
// internal job.
inline constexpr int kErrorFailedAwaitingLaunchdUpdateServiceInternalJob = 32;

// DM registration failure with mandatory enrollment.
inline constexpr int kErrorDMRegistrationFailed = 33;

inline constexpr int kErrorFailedToInstallLegacyUpdater = 34;

// A Mojo remote was unexpectedly disconnected.
inline constexpr int kErrorIpcDisconnect = 35;

// Failed to copy the updater binary.
inline constexpr int kErrorFailedToCopyBinary = 36;

// Failed to delete a socket file.
inline constexpr int kErrorFailedToDeleteSocket = 37;

// Failed to create a symlink to the current version.
inline constexpr int kErrorFailedToLinkCurrent = 38;

// Failed to rename the current symlink during activation.
inline constexpr int kErrorFailedToRenameCurrent = 39;

// Failed to install one or more Systemd units.
inline constexpr int kErrorFailedToInstallSystemdUnit = 40;

// Failed to remove one or more Systemd units during uninstallation.
inline constexpr int kErrorFailedToRemoveSystemdUnit = 41;

// Running as the wrong user for the provided UpdaterScope.
inline constexpr int kErrorWrongUser = 42;

// Failed to get the setup files.
inline constexpr int kErrorFailedToGetSetupFiles = 43;

// Failed to run install list.
inline constexpr int kErrorFailedToRunInstallList = 44;

// The server was running but had no tasks to do.
inline constexpr int kErrorIdle = 45;

inline constexpr int kErrorTagParsing = 50;

// Metainstaller errors.
inline constexpr int kErrorCreatingTempDir = 60;
inline constexpr int kErrorUnpackingResource = 61;
inline constexpr int kErrorInitializingBackupDir = 62;

// Launcher errors.
constexpr int kErrorGettingUpdaterPath = 71;
constexpr int kErrorStattingPath = 72;
constexpr int kErrorLaunchingProcess = 73;
constexpr int kErrorPathOwnershipMismatch = 74;

// A setup process could not acquire the lock needed to run.
inline constexpr int kErrorFailedToLockSetupMutex = 75;

// Policy Management constants.
// The maximum value allowed for policy AutoUpdateCheckPeriodMinutes.
inline constexpr int kMaxAutoUpdateCheckPeriodMinutes = 43200;

// The maximum value allowed for policy UpdatesSuppressedDurationMin.
inline constexpr int kMaxUpdatesSuppressedDurationMinutes = 960;

extern const char kProxyModeDirect[];
extern const char kProxyModeAutoDetect[];
extern const char kProxyModePacScript[];
extern const char kProxyModeFixedServers[];
extern const char kProxyModeSystem[];

extern const char kDownloadPreferenceCacheable[];

// UTF8 byte order mark (BOM) used to prefix the contents of the installerdata
// file.
extern const char kUTF8BOM[];

inline constexpr int kPolicyNotSet = -1;
inline constexpr int kPolicyDisabled = 0;
inline constexpr int kPolicyEnabled = 1;
inline constexpr int kPolicyEnabledMachineOnly = 4;
inline constexpr int kPolicyManualUpdatesOnly = 2;
inline constexpr int kPolicyAutomaticUpdatesOnly = 3;
inline constexpr int kPolicyForceInstallMachine = 5;
inline constexpr int kPolicyForceInstallUser = 6;

inline constexpr bool kInstallPolicyDefault = kPolicyEnabled;
inline constexpr bool kUpdatePolicyDefault = kPolicyEnabled;

// Policy manager `source()` constants.
extern const char kSourceGroupPolicyManager[];
extern const char kSourceDMPolicyManager[];
extern const char kSourceManagedPreferencePolicyManager[];
extern const char kSourceDefaultValuesPolicyManager[];
extern const char kSourceDictValuesPolicyManager[];

// Serializes updater installs.
extern const char kSetupMutex[];

inline constexpr int kUninstallPingReasonUninstalled = 0;
inline constexpr int kUninstallPingReasonUserNotAnOwner = 1;
inline constexpr int kUninstallPingReasonNoAppsRemain = 2;
inline constexpr int kUninstallPingReasonNeverHadApps = 3;

// The file downloaded to a temporary location could not be moved.
inline constexpr int kErrorFailedToMoveDownloadedFile = 5;

inline constexpr base::TimeDelta kInitialDelay = base::Minutes(1);
inline constexpr base::TimeDelta kServerKeepAliveTime = base::Seconds(10);

// The maximum number of server starts before the updater uninstalls itself
// while waiting for the first app registration.
inline constexpr int kMaxServerStartsBeforeFirstReg = 24;

// Number of tries when an installer returns `ERROR_INSTALL_ALREADY_RUNNING`.
inline constexpr int kNumAlreadyRunningMaxTries = 4;

// Initial delay between retries when an installer returns
// `ERROR_INSTALL_ALREADY_RUNNING`.
inline constexpr base::TimeDelta kAlreadyRunningRetryInitialDelay =
    base::Seconds(5);

// These are GoogleUpdate error codes, which must be retained by this
// implementation in order to be backward compatible with the existing update
// client code in Chrome.
inline constexpr int GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY = 0x80040812;
inline constexpr int GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY = 0x80040813;
inline constexpr int GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL =
    0x8004081f;
inline constexpr int GOOPDATEINSTALL_E_FILENAME_INVALID = 0x80040900;
inline constexpr int GOOPDATEINSTALL_E_INSTALLER_FAILED_START = 0x80040901;
inline constexpr int GOOPDATEINSTALL_E_INSTALLER_FAILED = 0x80040902;
inline constexpr int GOOPDATEINSTALL_E_INSTALLER_TIMED_OUT = 0x80040904;
inline constexpr int GOOPDATEINSTALL_E_INSTALL_ALREADY_RUNNING = 0x80040907;

}  // namespace updater

#endif  // CHROME_UPDATER_CONSTANTS_H_
