// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines all install related constants that need to be used by Chrome as
// well as Chrome Installer.

#ifndef CHROME_INSTALLER_UTIL_UTIL_CONSTANTS_H_
#define CHROME_INSTALLER_UTIL_UTIL_CONSTANTS_H_

#include <stddef.h>

#include "base/files/file_path.h"

namespace installer {

// Return status of installer. Values in this enum must not change. Always add
// to the end. When removing an unused value, retain the deprecated name and
// value in a comment for posterity's sake, but take the liberty of removing the
// old doc string.
// The values in this enum must be kept in sync with the SetupInstallResult enum
// in enums.xml
enum InstallStatus {
  FIRST_INSTALL_SUCCESS = 0,      // First install of Chrome succeeded.
  INSTALL_REPAIRED = 1,           // Same version reinstalled for repair.
  NEW_VERSION_UPDATED = 2,        // Chrome successfully updated to new version.
  EXISTING_VERSION_LAUNCHED = 3,  // No work done; launched existing Chrome.
  HIGHER_VERSION_EXISTS = 4,      // Higher version of Chrome already exists
  USER_LEVEL_INSTALL_EXISTS = 5,  // User level install already exists.
  SYSTEM_LEVEL_INSTALL_EXISTS = 6,  // Machine level install already exists.
  INSTALL_FAILED = 7,               // Install/update failed.
  // SETUP_PATCH_FAILED = 8,        // Failed to patch setup.exe.
  OS_NOT_SUPPORTED = 9,       // Current OS not supported.
  OS_ERROR = 10,              // OS API call failed.
  TEMP_DIR_FAILED = 11,       // Unable to get Temp directory.
  UNCOMPRESSION_FAILED = 12,  // Failed to uncompress Chrome archive.
  INVALID_ARCHIVE = 13,       // Something wrong with the installer archive.
  INSUFFICIENT_RIGHTS = 14,   // User trying system level install is not Admin.
  CHROME_NOT_INSTALLED = 15,  // Chrome not installed (returned in case of
                              // uninstall).
  CHROME_RUNNING = 16,        // Chrome currently running (when trying to
                              // uninstall).
  UNINSTALL_CONFIRMED = 17,   // User has confirmed Chrome uninstall.
  UNINSTALL_DELETE_PROFILE = 18,  // User okayed uninstall and profile deletion.
  UNINSTALL_SUCCESSFUL = 19,      // Chrome successfully uninstalled.
  UNINSTALL_FAILED = 20,          // Chrome uninstallation failed.
  UNINSTALL_CANCELLED = 21,       // User cancelled Chrome uninstallation.
  UNKNOWN_STATUS = 22,            // Unknown status (this should never happen).
  RENAME_SUCCESSFUL = 23,     // Rename of new_chrome.exe to chrome.exe worked.
  RENAME_FAILED = 24,         // Rename of new_chrome.exe failed.
  EULA_REJECTED = 25,         // EULA dialog was not accepted by user.
  EULA_ACCEPTED = 26,         // EULA dialog was accepted by user.
  EULA_ACCEPTED_OPT_IN = 27,  // EULA accepted with the crash option selected.
  INSTALL_DIR_IN_USE = 28,    // Installation directory is in use by another
                              // process
  UNINSTALL_REQUIRES_REBOOT = 29,  // Uninstallation required a reboot.
  IN_USE_UPDATED = 30,  // Chrome successfully updated but old version
                        // running.
  SAME_VERSION_REPAIR_FAILED = 31,  // Chrome repair failed as Chrome was
                                    // running.
  REENTRY_SYS_UPDATE = 32,  // Setup has been re-launched as the interactive
                            // user.
  SXS_OPTION_NOT_SUPPORTED = 33,  // The chrome-sxs option provided does not
                                  // work with other command line options.
  // NON_MULTI_INSTALLATION_EXISTS = 34,
  // MULTI_INSTALLATION_EXISTS = 35,
  // READY_MODE_OPT_IN_FAILED = 36,
  // READY_MODE_TEMP_OPT_OUT_FAILED = 37,
  // READY_MODE_END_TEMP_OPT_OUT_FAILED = 38,
  // CONFLICTING_CHANNEL_EXISTS = 39,
  // READY_MODE_REQUIRES_CHROME = 40,
  // APP_HOST_REQUIRES_MULTI_INSTALL = 41,
  // APPLY_DIFF_PATCH_FAILED = 42,  // Failed to apply a diff patch.
  // INCONSISTENT_UPDATE_POLICY = 43,
  // APP_HOST_REQUIRES_USER_LEVEL = 44,
  // APP_HOST_REQUIRES_BINARIES = 45,
  // INSTALL_OF_GOOGLE_UPDATE_FAILED = 46,
  INVALID_STATE_FOR_OPTION = 47,  // A non-install option was called with an
                                  // invalid installer state.
  // WAIT_FOR_EXISTING_FAILED = 48,
  // PATCH_INVALID_ARGUMENTS = 49,    // The arguments of --patch were missing
  // or they were invalid for any reason.
  // DIFF_PATCH_SOURCE_MISSING = 50,  // No previous version archive found for
  // differential update.
  // UNUSED_BINARIES = 51,
  // UNUSED_BINARIES_UNINSTALLED = 52,
  UNSUPPORTED_OPTION = 53,          // An unsupported legacy option was given.
  CPU_NOT_SUPPORTED = 54,           // Current OS not supported
  REENABLE_UPDATES_SUCCEEDED = 55,  // Autoupdates are now enabled.
  REENABLE_UPDATES_FAILED = 56,     // Autoupdates could not be enabled.
  UNPACKING_FAILED = 57,       // Unpacking the (possibly patched) uncompressed
                               // archive failed.
  IN_USE_DOWNGRADE = 58,       // Successfully downgrade chrome but current
                               // version is still running.
  OLD_VERSION_DOWNGRADE = 59,  // Successfully downgrade chrome to an older
                               // version.
  SETUP_SINGLETON_ACQUISITION_FAILED = 60,  // The setup process could not
                                            // acquire the exclusive right to
                                            // modify the Chrome installation.
  SETUP_SINGLETON_RELEASED = 61,            // The task did not complete because
                                            // another process asked this
                                            // process to release the exclusive
                                            // right to modify the Chrome
                                            // installation.
  DELETE_OLD_VERSIONS_SUCCESS = 62,         // All files that belong to old
                                            // versions of Chrome were
                                            // successfully deleted.
  DELETE_OLD_VERSIONS_TOO_MANY_ATTEMPTS = 63,  // A --delete-old-versions
                                               // process exited after trying to
                                               // delete all files that belong
                                               // to old versions of Chrome too
                                               // many times without success.
  STORE_DMTOKEN_FAILED = 64,   // Failed to write the specified DMToken to the
                               // registry.
  STORE_DMTOKEN_SUCCESS = 65,  // Writing the specified DMToken to the registry
                               // succeeded.
  DOWNGRADE_CLEANUP_FAILED = 66,
  DOWNGRADE_CLEANUP_SUCCESS = 67,
  UNDO_DOWNGRADE_CLEANUP_FAILED = 68,
  UNDO_DOWNGRADE_CLEANUP_SUCCESS = 69,
  DOWNGRADE_CLEANUP_UNKNOWN_OPERATION = 70,
  ROTATE_DTKEY_FAILED = 71,   // Failed to rotate device trust signing key.
  ROTATE_DTKEY_SUCCESS = 72,  // Successfully rotated device trust signing key.
  CREATE_SHORTCUTS_SUCCESS = 73,  // Successfully created Chrome shortcuts.
  DELETE_DMTOKEN_FAILED = 74,     // Failed to delete DMToken from the registry.
  DELETE_DMTOKEN_SUCCESS = 75,    // Successfully deleted DMToken from the
                                  // registry.
  ROTATE_DTKEY_FAILED_PERMISSIONS = 76,  // Failed to rotate the device trust
                                         // key due to missing permissions.
  ROTATE_DTKEY_FAILED_CONFLICT = 77,  // Failed to rotate the device trust key
                                      // due to a conflict during upload.
  CONFIGURE_APP_CONTAINER_SANDBOX_SUCCESS = 78,
  CONFIGURE_APP_CONTAINER_SANDBOX_FAILED = 79,
  MAX_INSTALL_STATUS = 80,  // When adding a new result, bump this and update
                            // the SetupInstallResult enum in enums.xml.
};

// Stages of an installation from which a progress indication is derived.
// Generally listed in the order in which they are reached, except that
// ROLLINGBACK may occur throughout in case of error.
enum InstallerStage {
  NO_STAGE,                  // No stage to report.
  PRECONDITIONS,             // Evaluating pre-install conditions.
  UNCOMPRESSING,             // Uncompressing chrome.packed.7z.
  UNPACKING,                 // Unpacking chrome.7z.
  BUILDING,                  // Building the install work item list.
  EXECUTING,                 // Executing the install work item list.
  COPYING_PREFERENCES_FILE,  // Copying preferences file.
  CREATING_SHORTCUTS,        // Creating shortcuts.
  REGISTERING_CHROME,        // Performing Chrome registration.
  REMOVING_OLD_VERSIONS,     // Deleting old version directories.
  ROLLINGBACK,               // Rolling-back the install work item list.
  FINISHING,                 // Finishing the install.
  NUM_STAGES                 // The number of stages.
};

namespace switches {

// Allow an update of Chrome from a higher version to a lower version.
// Ordinarily, such downgrades are disallowed. An administrator may wish to
// allow them in circumstances where the potential loss of user data is
// permissible.
inline constexpr char kAllowDowngrade[] = "allow-downgrade";

inline constexpr char kBrowserVersionSwitch[] = "browser-version";

// A channel name specified via administrative policy. This switch sets the
// channel both of the installer and of the version of Chrome being installed.
// This switch has no effect for secondary install modes (i.e., installs that
// use --chrome-sxs or another mode switch).
inline constexpr char kChannel[] = "channel";

// Create shortcuts for this user to point to a system-level install (which
// must already be installed on the machine). The shortcuts created will
// match the preferences of the already present system-level install as such
// this option is not compatible with any other installer options.
inline constexpr char kConfigureUserSettings[] = "configure-user-settings";

// Create shortcuts with the installer operation arg.
inline constexpr char kCreateShortcuts[] = "create-shortcuts";

// The version number of an update containing critical fixes, for which an
// in-use Chrome should be restarted ASAP.
inline constexpr char kCriticalUpdateVersion[] = "critical-update-version";

// Deletes any existing DMToken from the registry.
inline constexpr char kDeleteDMToken[] = "delete-dmtoken";

// Delete files that belong to old versions of Chrome from the install
// directory.
inline constexpr char kDeleteOldVersions[] = "delete-old-versions";

// Delete user profile data. This param is useful only when specified with
// kUninstall, otherwise it is silently ignored.
inline constexpr char kDeleteProfile[] = "delete-profile";

// Disable logging.
inline constexpr char kDisableLogging[] = "disable-logging";

// Uninstalls the elevated tracing service; see kEnableSystemTracing.
inline constexpr char kDisableSystemTracing[] = "disable-system-tracing";

// Specifies the DM server URL to use with the rotate device key command.
inline constexpr char kDmServerUrl[] = "dm-server-url";

// Prevent installer from launching Chrome after a successful first install.
inline constexpr char kDoNotLaunchChrome[] = "do-not-launch-chrome";

// Prevents installer from writing the Google Update key that causes Google
// Update to launch Chrome after a first install.
inline constexpr char kDoNotRegisterForUpdateLaunch[] =
    "do-not-register-for-update-launch";

// By default we remove all shared (between users) files, registry entries etc
// during uninstall. If this option is specified together with kUninstall option
// we do not clean up shared entries otherwise this option is ignored.
inline constexpr char kDoNotRemoveSharedItems[] = "do-not-remove-shared-items";

// Enable logging at the error level. This is the default behavior.
inline constexpr char kEnableLogging[] = "enable-logging";

// Installs the elevated tracing service to capture system-wide ETW events for
// tracing.
inline constexpr char kEnableSystemTracing[] = "enable-system-tracing";

// Same as kConfigureUserSettings above; except the checks to know whether
// first run already occurred are bypassed and shortcuts are created either way
// (kConfigureUserSettings also needs to be on the command-line for this to have
// any effect).
inline constexpr char kForceConfigureUserSettings[] =
    "force-configure-user-settings";

// If present, setup will uninstall chrome without asking for any
// confirmation from user.
inline constexpr char kForceUninstall[] = "force-uninstall";

// Specify the path to the Chrome archive for install. If not specified,
// chrome.packed.7z or chrome.7z in the same directory as setup.exe
// is used.
inline constexpr char kInstallArchive[] = "install-archive";

// Use the given uncompressed chrome.7z archive as the source of files to
// install.
inline constexpr char kUncompressedArchive[] = "uncompressed-archive";

// Specify the file path of Chrome initial preference file.
inline constexpr char kInstallerData[] = "installerdata";

// What install level to create shortcuts for, if "create-shortcuts" is present.
inline constexpr char kInstallLevel[] = "install-level";

// If present, specify file path to write logging info.
inline constexpr char kLogFile[] = "log-file";

// Tells installer to expect to be run as a subsidiary to an MSI.
inline constexpr char kMsi[] = "msi";

// Specifies a nonce to use with the rotate device key command.
inline constexpr char kNonce[] = "nonce";

// Notify the installer that the OS has been upgraded.
inline constexpr char kOnOsUpgrade[] = "on-os-upgrade";

// Tells the updater the previous and new Windows versions.
inline constexpr char kOsUpgradeVersions[] = "os-upgrade-versions";

// Requests that setup attempt to reenable autoupdates for Chrome.
inline constexpr char kReenableAutoupdates[] = "reenable-autoupdates";

// Register Chrome as a valid browser on the current system. This option
// requires that setup.exe is running as admin. If this option is specified,
// options kInstallArchive and kUninstall are ignored.
inline constexpr char kRegisterChromeBrowser[] = "register-chrome-browser";

// Used by the installer to forward the registration suffix of the
// (un)installation in progress when launching an elevated setup.exe to finish
// registration work.
inline constexpr char kRegisterChromeBrowserSuffix[] =
    "register-chrome-browser-suffix";

// Specify the path to the dev build of chrome.exe the user wants to install
// (register and install Start menu shortcut for) on the system. This will
// always result in a user-level install and will make this install default
// browser.
inline constexpr char kRegisterDevChrome[] = "register-dev-chrome";

// Switch to allow an extra URL protocol to be registered. This option is used
// in conjunction with kRegisterChromeBrowser to specify an extra protocol
// in addition to the standard set of protocols.
inline constexpr char kRegisterURLProtocol[] = "register-url-protocol";

// Removes Chrome registration from current machine. Requires admin rights.
inline constexpr char kRemoveChromeRegistration[] =
    "remove-chrome-registration";

// Renames chrome.exe to old_chrome.exe and renames new_chrome.exe to chrome.exe
// to support in-use updates. Also deletes opv key.
inline constexpr char kRenameChromeExe[] = "rename-chrome-exe";

// Rotate the stored device trust signing key.
inline constexpr char kRotateDeviceTrustKey[] = "rotate-dtkey";

// When we try to relaunch setup.exe as admin on Vista, we append this command
// line flag so that we try the launch only once.
inline constexpr char kRunAsAdmin[] = "run-as-admin";

// Combined with --uninstall, signals to setup.exe that this uninstall was
// triggered by a self-destructing Chrome.
inline constexpr char kSelfDestruct[] = "self-destruct";

// Show the embedded EULA dialog.
inline constexpr char kShowEula[] = "show-eula";

// Saves the specified device management token to the registry.
inline constexpr char kStoreDMToken[] = "store-dmtoken";

// Install Chrome to system wise location. The default is per user install.
inline constexpr char kSystemLevel[] = "system-level";

// Signals to setup.exe that it should trigger the active setup command.
inline constexpr char kTriggerActiveSetup[] = "trigger-active-setup";

// If present, setup will uninstall chrome.
inline constexpr char kUninstall[] = "uninstall";

// Enable verbose logging (info level).
inline constexpr char kVerboseLogging[] = "verbose-logging";

}  // namespace switches

namespace env_vars {

// The presence of this environment variable with a value of 1 implies that
// setup.exe should run as a system installation regardless of what is on the
// command line.
inline constexpr char kGoogleUpdateIsMachineEnvVar[] = "GoogleUpdateIsMachine";

}  // namespace env_vars

// The Active Setup executable will be an identical copy of setup.exe; this is
// necessary because Windows' installer detection heuristics (which include
// things like process name being "setup.exe") will otherwise force elevation
// for non-admin users when setup.exe is launched. This is mitigated by adding
// requestedExecutionLevel="asInvoker" to setup.exe's manifest on Vista+, but
// there is no such manifest entry on Windows XP (which results in
// crbug.com/40296982).
// TODO(gab): Rename setup.exe itself altogether and use the same binary for
// Active Setup.
inline constexpr wchar_t kActiveSetupExe[] = L"chrmstp.exe";
inline constexpr wchar_t kChromeDll[] = L"chrome.dll";
inline constexpr wchar_t kChromeExe[] = L"chrome.exe";
inline constexpr wchar_t kChromeNewExe[] = L"new_chrome.exe";
inline constexpr wchar_t kChromeOldExe[] = L"old_chrome.exe";
inline constexpr wchar_t kChromeProxyExe[] = L"chrome_proxy.exe";
inline constexpr wchar_t kChromeProxyNewExe[] = L"new_chrome_proxy.exe";
inline constexpr wchar_t kChromeProxyOldExe[] = L"old_chrome_proxy.exe";
inline constexpr wchar_t kCmdAlternateRenameChromeExe[] = L"rename-chrome-exe";
inline constexpr wchar_t kCmdRenameChromeExe[] = L"cmd";
inline constexpr wchar_t kCmdOnOsUpgrade[] = L"on-os-upgrade";
inline constexpr wchar_t kCmdRotateDeviceTrustKey[] = L"rotate-dtkey";
inline constexpr wchar_t kCmdStoreDMToken[] = L"store-dmtoken";
inline constexpr wchar_t kCmdDeleteDMToken[] = L"delete-dmtoken";
inline constexpr wchar_t kCmdInstallPEH[] = L"install-peh";

// LINT.IfChange(kEulaSentinelFile)
inline constexpr wchar_t kEulaSentinelFile[] = L"EULA Accepted";
// LINT.ThenChange(//chrome/browser/first_run/first_run_internal_linux.cc:kEulaSentinelFile,
// //chrome/browser/ui/views/eula_dialog_linux_unittest.cc:kEulaSentinelFile)

inline constexpr wchar_t kInstallBinaryDir[] = L"Application";
inline constexpr wchar_t kInstallerDir[] = L"Installer";
inline constexpr wchar_t kInstallTempDir[] = L"Temp";
inline constexpr wchar_t kLnkExt[] = L".lnk";
inline constexpr wchar_t kNotificationHelperExe[] = L"notification_helper.exe";
inline constexpr wchar_t kWerDll[] = L"chrome_wer.dll";

// DowngradeVersion holds the version from which Chrome was downgraded. In case
// of multiple downgrades (e.g., 75->74->73), it retains the highest version
// installed prior to any downgrades. DowngradeVersion is deleted on upgrade
// once Chrome reaches the version from which it was downgraded.
inline constexpr wchar_t kRegDowngradeVersion[] = L"DowngradeVersion";

inline constexpr wchar_t kSetupExe[] = L"setup.exe";
inline constexpr wchar_t kUninstallStringField[] = L"UninstallString";
inline constexpr wchar_t kUninstallArgumentsField[] = L"UninstallArguments";
inline constexpr wchar_t kUninstallDisplayNameField[] = L"DisplayName";
inline constexpr wchar_t kUninstallInstallationDate[] = L"installation_date";

// Elevation Service constants.
inline constexpr base::FilePath::CharType kElevationServiceExe[] =
    FILE_PATH_LITERAL("elevation_service.exe");

// Google Update installer result API.
inline constexpr wchar_t kInstallerError[] = L"InstallerError";
inline constexpr wchar_t kInstallerExtraCode1[] = L"InstallerExtraCode1";
inline constexpr wchar_t kInstallerResult[] = L"InstallerResult";
inline constexpr wchar_t kInstallerResultUIString[] =
    L"InstallerResultUIString";
inline constexpr wchar_t kInstallerSuccessLaunchCmdLine[] =
    L"InstallerSuccessLaunchCmdLine";

// Chrome channel display names.
// NOTE: Canary is not strictly a 'channel', but rather a separate product
//     installed side-by-side. However, GoogleUpdateSettings::GetChromeChannel
//     will return "canary" for that product.
inline constexpr wchar_t kChromeChannelUnknown[] = L"unknown";
inline constexpr wchar_t kChromeChannelCanary[] = L"canary";
inline constexpr wchar_t kChromeChannelDev[] = L"dev";
inline constexpr wchar_t kChromeChannelBeta[] = L"beta";
inline constexpr wchar_t kChromeChannelStable[] = L"";
inline constexpr wchar_t kChromeChannelStableExplicit[] = L"stable";

inline constexpr size_t kMaxAppModelIdLength = 64U;

enum : size_t { kMaxDMTokenLength = 4096 };

// Name of the allocator (and associated file) for storing histograms to be
// reported by Chrome during its next upload.
inline constexpr char kSetupHistogramAllocatorName[] = "SetupMetrics";

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_UTIL_CONSTANTS_H_
