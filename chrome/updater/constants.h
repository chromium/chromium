// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CONSTANTS_H_
#define CHROME_UPDATER_CONSTANTS_H_

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/updater/updater_branding.h"
#include "components/update_client/update_client_errors.h"

namespace updater {

// Key for storing the installer version in the install settings dictionary.
inline constexpr char kInstallerVersion[] = "installer_version";

// The updater specific app ID.
inline constexpr char kUpdaterAppId[] = UPDATER_APPID;

// The app ID used to qualify the updater.
inline constexpr char kQualificationAppId[] = QUALIFICATION_APPID;

// The name of the updater program image.
#if BUILDFLAG(IS_WIN)
inline constexpr char kExecutableName[] = "updater.exe";
#else
inline constexpr char kExecutableName[] = "updater";
#endif

// A suffix appended to the updater executable name before any file extension.
extern const char kExecutableSuffix[];

// "0.0.0.0". Historically, a null version has been used to indicate a
// new install.
inline constexpr char kNullVersion[] = "0.0.0.0";

// Command line switches.
//
// If a command line switch is marked as `backward-compatibility`, it
// means the switch name cannot be changed, and the parser must be able to
// handle command line in the DOS style '/<switch> <optional_value>'. This is to
// make sure the new updater understands the hand-off requests from the legacy
// updaters.

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
inline constexpr char kServerSwitch[] = "server";

// This switch specifies the XPC service the server registers to listen to.
inline constexpr char kServerServiceSwitch[] = "service";

// Valid values for the kServerServiceSwitch.
inline constexpr char kServerUpdateServiceInternalSwitchValue[] =
    "update-internal";
inline constexpr char kServerUpdateServiceSwitchValue[] = "update";

// This switch starts the Windows service. This switch is invoked by the SCM
// either as a part of system startup (`SERVICE_AUTO_START`) or when `CoCreate`
// is called on one of several CLSIDs that the server supports.
inline constexpr char kWindowsServiceSwitch[] = "windows-service";

// This switch indicates that the Windows service is in the COM server mode.
// This switch is passed to `ServiceMain` by the SCM when CoCreate is called on
// one of several CLSIDs that the server supports. We expect to use the COM
// service for the following scenarios:
// * The Server for the UI when installing Machine applications.
// * The On-Demand COM Server for Machine applications.
// * COM Server for launching processes at System Integrity, i.e., an Elevator.
inline constexpr char kComServiceSwitch[] = "com-service";

// Crash the program for testing purposes.
inline constexpr char kCrashMeSwitch[] = "crash-me";

// Runs as the Crashpad handler.
inline constexpr char kCrashHandlerSwitch[] = "crash-handler";

// Updates the updater.
inline constexpr char kUpdateSwitch[] = "update";

// Installs the updater. Takes an optional argument for the meta installer tag.
// The tag is a string of arguments, separated by a delimiter (in this case, the
// delimiter is `&`). The tag is typically embedded in the program image of the
// metainstaller, but for testing purposes, the tag could be passed directly as
// a command line argument. The tag is currently encoded as an ASCII string.
inline constexpr char kInstallSwitch[] = "install";

inline constexpr char kEulaRequiredSwitch[] = "eularequired";

// Specifies that this is an OEM install in audit mode.
inline constexpr char kOemSwitch[] = "oem";

// The --installerdata=file.dat switch is passed to an installer if an
// installdataindex is specified in the tag or if installerdata is passed in via
// --appargs. The corresponding installerdata is written to file.dat with an
// UTF8 encoding as well as a UTF8 BOM.
inline constexpr char kInstallerDataSwitch[] = "installerdata";

// Uninstalls the updater.
inline constexpr char kUninstallSwitch[] = "uninstall";

// Uninstalls this version of the updater.
inline constexpr char kUninstallSelfSwitch[] = "uninstall-self";

// Uninstalls the updater if no apps are managed by it.
inline constexpr char kUninstallIfUnusedSwitch[] = "uninstall-if-unused";

// Kicks off the update service. This switch is typically used for by a
// scheduled to invoke the updater periodically.
inline constexpr char kWakeSwitch[] = "wake";

// Kicks off the update service for all versions.
inline constexpr char kWakeAllSwitch[] = "wake-all";

// The updater needs to operate in the system context.
inline constexpr char kSystemSwitch[] = "system";

// Runs in test mode. Currently, it exits right away.
inline constexpr char kTestSwitch[] = "test";

// Run in recovery mode.
inline constexpr char kRecoverSwitch[] = "recover";

// The version of the program triggering recovery.
inline constexpr char kBrowserVersionSwitch[] = "browser-version";

// The session ID of the Omaha session triggering recovery.
inline constexpr char kSessionIdSwitch[] =
    "sessionid";  // backward-compatibility.

// The app ID of the program triggering recovery.
inline constexpr char kAppGuidSwitch[] = "appguid";

// Disables throttling for the crash reported until the following bug is fixed:
// https://bugs.chromium.org/p/crashpad/issues/detail?id=23
inline constexpr char kNoRateLimitSwitch[] = "no-rate-limit";

// Causes crashpad handler to start a second instance to monitor the first
// instance for exceptions.
inline constexpr char kMonitorSelfSwitch[] = "monitor-self";

// Additional arguments passed to the --monitor-self instance.
inline constexpr char kMonitorSelfSwitchArgument[] = "monitor-self-argument";

// The handle of an event to signal when the initialization of the main process
// is complete.
inline constexpr char kInitDoneNotifierSwitch[] = "init-done-notifier";

// Enables logging.
inline constexpr char kEnableLoggingSwitch[] = "enable-logging";

// Specifies the logging module filter and its value. Note that some call sites
// may still use different values for the logging module filter.
inline constexpr char kLoggingModuleSwitch[] = "vmodule";
inline constexpr char kLoggingModuleSwitchValue[] =
#if BUILDFLAG(IS_WIN)
    "*/components/winhttp/*=1,"
#endif
    "*/components/update_client/*=2,*/chrome/updater/*=2";

// Specifies the application that the Updater needs to install.
inline constexpr char kAppIdSwitch[] = "app-id";

// Specifies the version of the application that the updater needs to register.
inline constexpr char kAppVersionSwitch[] = "app-version";

// Specifies that the Updater should perform some minimal checks to verify that
// it is operational/healthy. This is for backward compatibility with Omaha 3.
// Omaha 3 runs "GoogleUpdate.exe /healthcheck" and expects an exit code of
// HRESULT SUCCESS, i.e., S_OK, in which case it will hand off the installation
// to Omaha 4.
inline constexpr char kHealthCheckSwitch[] = "healthcheck";

// Specifies the enterprise request argument. On Windows, the request may
// be from legacy updaters which pass the argument in the format of
// `/enterprise`. Manual argument parsing is needed for that scenario.
inline constexpr char kEnterpriseSwitch[] =
    "enterprise";  // backward-compatibility.

// Specifies that no UI should be shown.
inline constexpr char kSilentSwitch[] = "silent";  // backward-compatibility.

// The "alwayslaunchcmd" switch specifies that launch commands are to be run
// unconditionally, even for silent modes.
inline constexpr char kAlwaysLaunchCmdSwitch[] = "alwayslaunchcmd";

// Specifies the handoff request argument. On Windows, the request may
// be from legacy updaters which pass the argument in the format of
// `/handoff <install-args-details>`. Manual argument parsing is needed for that
// scenario.
inline constexpr char kHandoffSwitch[] = "handoff";  // backward-compatibility.

// Specifies the full path to the offline install resources. The folder
// contains offline installer and the manifest file.
inline constexpr char kOfflineDirSwitch[] =
    "offlinedir";  // backward-compatibility.

// Specifies extra app args. The switch must be in the following format:
//     --appargs="appguid=<appid>&installerdata=<URL-encoded-installer-data>"
// On Windows, the request may be from legacy updaters which pass the argument
// in the format of `/appargs <value>`. Manual argument parsing is needed for
// that scenario.
inline constexpr char kAppArgsSwitch[] = "appargs";  // backward-compatibility.

// The "expect-elevated" switch indicates that updater setup should be running
// elevated (at high integrity). This switch is needed to avoid running into a
// loop trying (but failing repeatedly) to elevate updater setup when attempting
// to install on a standard user account with UAC disabled.
inline constexpr char kCmdLineExpectElevated[] = "expect-elevated";

// The "expect-de-elevated" switch indicates that updater setup should be
// running de-elevated (at medium integrity). This switch is needed to avoid
// running into a loop trying (but failing repeatedly) to de-elevate updater
// setup when attempting to install as a standard user account with UAC enabled.
inline constexpr char kCmdLineExpectDeElevated[] = "expect-de-elevated";

// The "prefers-user" switch indicates that updater setup could not elevate, and
// is now trying to install the app per-user.
inline constexpr char kCmdLinePrefersUser[] = "prefers-user";

// Environment variables.
inline constexpr char kUsageStatsEnabled[] =
    COMPANY_SHORTNAME_UPPERCASE_STRING "_USAGE_STATS_ENABLED";
inline constexpr char kUsageStatsEnabledValueEnabled[] = "1";

// File system paths.
//
// The directory name where CRX apps get installed. This is provided for demo
// purposes, since products installed by this updater will be installed in
// their specific locations.
inline constexpr char kAppsDir[] = "apps";

// The name of the uninstall script which is invoked by the --uninstall switch.
inline constexpr char kUninstallScript[] = "uninstall.cmd";

// Developer override keys.
inline constexpr char kDevOverrideKeyUrl[] = "url";
inline constexpr char kDevOverrideKeyCrashUploadUrl[] = "crash_upload_url";
inline constexpr char kDevOverrideKeyDeviceManagementUrl[] =
    "device_management_url";
inline constexpr char kDevOverrideKeyAppLogoUrl[] = "app_logo_url";
inline constexpr char kDevOverrideKeyUseCUP[] = "use_cup";
inline constexpr char kDevOverrideKeyInitialDelay[] = "initial_delay";
inline constexpr char kDevOverrideKeyServerKeepAliveSeconds[] =
    "server_keep_alive";
inline constexpr char kDevOverrideKeyCrxVerifierFormat[] =
    "crx_verifier_format";
inline constexpr char kDevOverrideKeyGroupPolicies[] = "group_policies";
inline constexpr char kDevOverrideKeyOverinstallTimeout[] =
    "overinstall_timeout";
inline constexpr char kDevOverrideKeyIdleCheckPeriodSeconds[] =
    "idle_check_period";
inline constexpr char kDevOverrideKeyManagedDevice[] = "managed_device";
inline constexpr char kDevOverrideKeyEnableDiffUpdates[] =
    "enable_diff_updates";

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
inline constexpr char kUserDefaultsSuiteName[] =
    MAC_BUNDLE_IDENTIFIER_STRING ".defaults";
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

// The app command failed to launch. This code is reported in the `extra_code1`
// in the ping, along with the actual error code that caused that launch failure
// in `error`.
inline constexpr int kErrorAppCommandLaunchFailed = kCustomInstallErrorBase + 5;

// The app command timed out. This code is reported in the `extra_code1` in the
// ping, along with the error code `HRESULT_FROM_WIN32(ERROR_TIMEOUT)` in
// `error`.
inline constexpr int kErrorAppCommandTimedOut = kCustomInstallErrorBase + 6;

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

// The call was rejected because the user needs to accept the EULA / Terms of
// service.
inline constexpr int kErrorEulaRequired = 46;

// The current operating system is not supported.
inline constexpr int kErrorUnsupportedOperatingSystem = 47;

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

inline constexpr char kProxyModeDirect[] = "direct";
inline constexpr char kProxyModeAutoDetect[] = "auto_detect";
inline constexpr char kProxyModePacScript[] = "pac_script";
inline constexpr char kProxyModeFixedServers[] = "fixed_servers";
inline constexpr char kProxyModeSystem[] = "system";

inline constexpr char kDownloadPreferenceCacheable[] = "cacheable";

// UTF8 byte order mark (BOM) used to prefix the contents of the installerdata
// file.
inline constexpr char kUTF8BOM[] = "\xEF\xBB\xBF";

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
inline constexpr char kSourceGroupPolicyManager[] = "Group Policy";
inline constexpr char kSourceDMPolicyManager[] = "Device Management";
inline constexpr char kSourceManagedPreferencePolicyManager[] =
    "Managed Preferences";
inline constexpr char kSourceDefaultValuesPolicyManager[] = "Default";
inline constexpr char kSourceDictValuesPolicyManager[] = "DictValuePolicy";

// Serializes updater installs.
inline constexpr char kSetupMutex[] = SETUP_MUTEX;

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

// Install Sources.
inline constexpr char kInstallSourceTaggedMetainstaller[] = "taggedmi";
inline constexpr char kInstallSourceOffline[] = "offline";
inline constexpr char kInstallSourcePolicy[] = "policy";
inline constexpr char kInstallSourceOnDemand[] = "ondemand";

}  // namespace updater

#endif  // CHROME_UPDATER_CONSTANTS_H_
