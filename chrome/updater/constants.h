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
inline constexpr char kInstallerVersion[] = "installer_version";

// The updater specific app ID. Defined in the .cc file so that the updater
// branding constants don't leak in this public header.
extern const char kUpdaterAppId[];

// The app ID used to qualify the updater. Defined in the .cc file so that the
// updater branding constants don't leak in this public header.
extern const char kQualificationAppId[];

// The name of the updater program image.
#if BUILDFLAG(IS_WIN)
inline constexpr char kExecutableName[] = "updater.exe";
#else
inline constexpr char kExecutableName[] = "updater";
#endif

// The name of the enterprise companion program image.
#if BUILDFLAG(IS_WIN)
inline constexpr char kCompanionAppExecutableName[] =
    "enterprise_companion.exe";
#else
inline constexpr char kCompanionAppExecutableName[] = "enterprise_companion";
#endif

// Uninstall switch for the enterprise companion app.
inline constexpr char kUninstallCompanionAppSwitch[] = "uninstall";

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

// Run as a network worker.
inline constexpr char kNetWorkerSwitch[] = "net-worker";

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
    "*/components/update_client/*=2,"
    "*/chrome/enterprise_companion/*=2,"
    "*/chrome/updater/*=2";

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

// Environment variables. Defined in the .cc file so that the updater branding
// constants don't leak in this public header.
extern const char kUsageStatsEnabled[];
extern const char kUsageStatsEnabledValueEnabled[];

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
inline constexpr char kDevOverrideKeyCecaConnectionTimeout[] =
    "ceca_connection_timeout";

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
// The user defaults suite name. Defined in the .cc file so that the updater
// branding constants don't leak in this public header.
extern const char kUserDefaultsSuiteName[];
#endif  // BUILDFLAG(IS_MAC)

// Install Errors.
//
// Specific errors for the updater that are passed through `update_client` are
// reported in such a way that their range does not conflict with the range of
// generic errors defined by the metainstaller, the `update_client` module, or
// Windows.
#if BUILDFLAG(IS_WIN)
inline constexpr int kCustomInstallErrorBase =
    static_cast<int>(update_client::InstallError::CUSTOM_ERROR_BASE) + 74000;
#else
inline constexpr int kCustomInstallErrorBase =
    static_cast<int>(update_client::InstallError::CUSTOM_ERROR_BASE);
#endif

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

// Specific error codes for the updater are reported in such a way that their
// range does not conflict with the range of generic errors defined by the
// metainstaller, the `update_client` module, or Windows.
#if BUILDFLAG(IS_WIN)
inline constexpr int kUpdaterErrorBase = 75000;
#else
inline constexpr int kUpdaterErrorBase = 0;
#endif

// Error codes.
//
// The server process may exit with any of these exit codes.
inline constexpr int kErrorOk = 0;

// The server could not acquire the lock needed to run.
inline constexpr int kErrorFailedToLockPrefsMutex = kUpdaterErrorBase + 1;

// The server candidate failed to promote itself to active.
inline constexpr int kErrorFailedToSwap = kUpdaterErrorBase + 2;

inline constexpr int kErrorRegistrationFailed = kUpdaterErrorBase + 3;
inline constexpr int kErrorPermissionDenied = kUpdaterErrorBase + 4;
inline constexpr int kErrorWaitFailedUninstall = kUpdaterErrorBase + 5;
inline constexpr int kErrorWaitFailedInstall = kUpdaterErrorBase + 6;
inline constexpr int kErrorPathServiceFailed = kUpdaterErrorBase + 7;
inline constexpr int kErrorComInitializationFailed = kUpdaterErrorBase + 8;
inline constexpr int kErrorUnknownCommandLine = kUpdaterErrorBase + 9;
inline constexpr int kErrorNoVersionedDirectory = kUpdaterErrorBase + 11;
inline constexpr int kErrorNoBaseDirectory = kUpdaterErrorBase + 12;
inline constexpr int kErrorPathTooLong = kUpdaterErrorBase + 13;
inline constexpr int kErrorProcessLaunchFailed = kUpdaterErrorBase + 14;

// Failed to copy the updater's bundle.
inline constexpr int kErrorFailedToCopyBundle = kUpdaterErrorBase + 15;

// Failed to delete the updater's install folder.
inline constexpr int kErrorFailedToDeleteFolder = kUpdaterErrorBase + 16;

// Failed to delete the updater's data folder.
inline constexpr int kErrorFailedToDeleteDataFolder = kUpdaterErrorBase + 17;

// Failed to get versioned updater folder path.
inline constexpr int kErrorFailedToGetVersionedInstallDirectory =
    kUpdaterErrorBase + 18;

// Failed to get the install directory.
inline constexpr int kErrorFailedToGetInstallDir = kUpdaterErrorBase + 19;

// Failed to remove the active(unversioned) update service job from Launchd.
inline constexpr int kErrorFailedToRemoveActiveUpdateServiceJobFromLaunchd =
    kUpdaterErrorBase + 20;

// Failed to remove versioned update service job from Launchd.
inline constexpr int kErrorFailedToRemoveCandidateUpdateServiceJobFromLaunchd =
    kUpdaterErrorBase + 21;

// Failed to remove versioned update service internal job from Launchd.
inline constexpr int kErrorFailedToRemoveUpdateServiceInternalJobFromLaunchd =
    kUpdaterErrorBase + 22;

// Failed to remove versioned wake job from Launchd.
inline constexpr int kErrorFailedToRemoveWakeJobFromLaunchd =
    kUpdaterErrorBase + 23;

// Failed to create the active(unversioned) update service Launchd plist.
inline constexpr int kErrorFailedToCreateUpdateServiceLaunchdJobPlist =
    kUpdaterErrorBase + 24;

// Failed to create the versioned update service Launchd plist.
inline constexpr int kErrorFailedToCreateVersionedUpdateServiceLaunchdJobPlist =
    kUpdaterErrorBase + 25;

// Failed to create the versioned update service internal Launchd plist.
inline constexpr int kErrorFailedToCreateUpdateServiceInternalLaunchdJobPlist =
    kUpdaterErrorBase + 26;

// Failed to create the versioned wake Launchd plist.
inline constexpr int kErrorFailedToCreateWakeLaunchdJobPlist =
    kUpdaterErrorBase + 27;

// Failed to start the active(unversioned) update service job.
inline constexpr int kErrorFailedToStartLaunchdActiveServiceJob =
    kUpdaterErrorBase + 28;

// Failed to start the versioned update service job.
inline constexpr int kErrorFailedToStartLaunchdVersionedServiceJob =
    kUpdaterErrorBase + 29;

// Failed to start the update service internal job.
inline constexpr int kErrorFailedToStartLaunchdUpdateServiceInternalJob =
    kUpdaterErrorBase + 30;

// Failed to start the wake job.
inline constexpr int kErrorFailedToStartLaunchdWakeJob = kUpdaterErrorBase + 31;

// Timed out while awaiting launchctl to become aware of the update service
// internal job.
inline constexpr int kErrorFailedAwaitingLaunchdUpdateServiceInternalJob =
    kUpdaterErrorBase + 32;

// DM registration failure with mandatory enrollment.
inline constexpr int kErrorDMRegistrationFailed = kUpdaterErrorBase + 33;

inline constexpr int kErrorFailedToInstallLegacyUpdater =
    kUpdaterErrorBase + 34;

// A Mojo remote was unexpectedly disconnected.
inline constexpr int kErrorIpcDisconnect = kUpdaterErrorBase + 35;

// Failed to copy the updater binary.
inline constexpr int kErrorFailedToCopyBinary = kUpdaterErrorBase + 36;

// Failed to delete a socket file.
inline constexpr int kErrorFailedToDeleteSocket = kUpdaterErrorBase + 37;

// Failed to create a symlink to the current version.
inline constexpr int kErrorFailedToLinkCurrent = kUpdaterErrorBase + 38;

// Failed to rename the current symlink during activation.
inline constexpr int kErrorFailedToRenameCurrent = kUpdaterErrorBase + 39;

// Failed to install one or more Systemd units.
inline constexpr int kErrorFailedToInstallSystemdUnit = kUpdaterErrorBase + 40;

// Failed to remove one or more Systemd units during uninstallation.
inline constexpr int kErrorFailedToRemoveSystemdUnit = kUpdaterErrorBase + 41;

// Running as the wrong user for the provided UpdaterScope.
inline constexpr int kErrorWrongUser = kUpdaterErrorBase + 42;

// Failed to get the setup files.
inline constexpr int kErrorFailedToGetSetupFiles = kUpdaterErrorBase + 43;

// Failed to run install list.
inline constexpr int kErrorFailedToRunInstallList = kUpdaterErrorBase + 44;

// The server was running but had no tasks to do.
inline constexpr int kErrorIdle = kUpdaterErrorBase + 45;

// The call was rejected because the user needs to accept the EULA / Terms of
// service.
inline constexpr int kErrorEulaRequired = kUpdaterErrorBase + 46;

// The current operating system is not supported.
inline constexpr int kErrorUnsupportedOperatingSystem = kUpdaterErrorBase + 47;

inline constexpr int kErrorTagParsing = kUpdaterErrorBase + 50;

// Metainstaller errors.
inline constexpr int kErrorCreatingTempDir = kUpdaterErrorBase + 60;
inline constexpr int kErrorUnpackingResource = kUpdaterErrorBase + 61;
inline constexpr int kErrorInitializingBackupDir = kUpdaterErrorBase + 62;

// Launcher errors.
inline constexpr int kErrorGettingUpdaterPath = kUpdaterErrorBase + 71;
inline constexpr int kErrorStattingPath = kUpdaterErrorBase + 72;
inline constexpr int kErrorLaunchingProcess = kUpdaterErrorBase + 73;
inline constexpr int kErrorPathOwnershipMismatch = kUpdaterErrorBase + 74;

// A setup process could not acquire the lock needed to run.
inline constexpr int kErrorFailedToLockSetupMutex = kUpdaterErrorBase + 75;

// Cannot establish a Mojo connection.
inline constexpr int kErrorMojoConnectionFailure = kUpdaterErrorBase + 76;

// Mojo server rejected the request.
inline constexpr int kErrorMojoRequestRejected = kUpdaterErrorBase + 77;

// Cannot find the console user, for example when the user is not logged on.
inline constexpr int kErrorNoConsoleUser = kUpdaterErrorBase + 78;

// Failed to fetch enterprise policies.
inline constexpr int kErrorPolicyFetchFailed = kUpdaterErrorBase + 79;

// Failed to uninstall the enterprise companion app.
inline constexpr int kErrorFailedToUninstallCompanionApp =
    kUpdaterErrorBase + 80;

// Failed to uninstall the other versions of updater.
inline constexpr int kErrorFailedToUninstallOtherVersion =
    kUpdaterErrorBase + 81;

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

// Serializes updater installs. Defined in the .cc file so that the updater
// branding constants don't leak in this public header.
extern const char kSetupMutex[];

inline constexpr int kUninstallPingReasonUninstalled = 0;
inline constexpr int kUninstallPingReasonUserNotAnOwner = 1;
inline constexpr int kUninstallPingReasonNoAppsRemain = 2;
inline constexpr int kUninstallPingReasonNeverHadApps = 3;

// The file downloaded to a temporary location could not be moved.
inline constexpr int kErrorFailedToMoveDownloadedFile = 5;

// Error occurred during file writing.
inline constexpr int kErrorFailedToWriteFile = 6;

inline constexpr base::TimeDelta kInitialDelay = base::Minutes(1);
inline constexpr base::TimeDelta kServerKeepAliveTime = base::Seconds(10);

inline constexpr base::TimeDelta kCecaConnectionTimeout = base::Seconds(30);

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
