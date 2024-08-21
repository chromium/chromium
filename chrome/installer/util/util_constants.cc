// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/util_constants.h"

namespace installer {

namespace switches {

// Allow an update of Chrome from a higher version to a lower version.
// Ordinarily, such downgrades are disallowed. An administrator may wish to
// allow them in circumstances where the potential loss of user data is
// permissible.
const char kAllowDowngrade[] = "allow-downgrade";

// A channel name specified via administrative policy. This switch sets the
// channel both of the installer and of the version of Chrome being installed.
// This switch has no effect for secondary install modes (i.e., installs that
// use --chrome-sxs or another mode switch).
const char kChannel[] = "channel";

// Create shortcuts for this user to point to a system-level install (which
// must already be installed on the machine). The shortcuts created will
// match the preferences of the already present system-level install as such
// this option is not compatible with any other installer options.
const char kConfigureUserSettings[] = "configure-user-settings";

// Create shortcuts with the installer operation arg.
const char kCreateShortcuts[] = "create-shortcuts";

// The version number of an update containing critical fixes, for which an
// in-use Chrome should be restarted ASAP.
const char kCriticalUpdateVersion[] = "critical-update-version";

// Deletes any existing DMToken from the registry.
const char kDeleteDMToken[] = "delete-dmtoken";

// Delete files that belong to old versions of Chrome from the install
// directory.
const char kDeleteOldVersions[] = "delete-old-versions";

// Delete user profile data. This param is useful only when specified with
// kUninstall, otherwise it is silently ignored.
const char kDeleteProfile[] = "delete-profile";

// Disable logging.
const char kDisableLogging[] = "disable-logging";

// Specifies the DM server URL to use with the rotate device key command.
const char kDmServerUrl[] = "dm-server-url";

// Prevent installer from launching Chrome after a successful first install.
const char kDoNotLaunchChrome[] = "do-not-launch-chrome";

// Prevents installer from writing the Google Update key that causes Google
// Update to launch Chrome after a first install.
const char kDoNotRegisterForUpdateLaunch[] =
    "do-not-register-for-update-launch";

// By default we remove all shared (between users) files, registry entries etc
// during uninstall. If this option is specified together with kUninstall option
// we do not clean up shared entries otherwise this option is ignored.
const char kDoNotRemoveSharedItems[] = "do-not-remove-shared-items";

// Enable logging at the error level. This is the default behavior.
const char kEnableLogging[] = "enable-logging";

// Same as kConfigureUserSettings above; except the checks to know whether
// first run already occurred are bypassed and shortcuts are created either way
// (kConfigureUserSettings also needs to be on the command-line for this to have
// any effect).
const char kForceConfigureUserSettings[] = "force-configure-user-settings";

// If present, setup will uninstall chrome without asking for any
// confirmation from user.
const char kForceUninstall[] = "force-uninstall";

// Specify the path to the Chrome archive for install. If not specified,
// chrome.packed.7z or chrome.7z in the same directory as setup.exe
// is used.
const char kInstallArchive[] = "install-archive";

// Use the given uncompressed chrome.7z archive as the source of files to
// install.
const char kUncompressedArchive[] = "uncompressed-archive";

// Specify the file path of Chrome initial preference file.
const char kInstallerData[] = "installerdata";

// What install level to create shortcuts for, if "create-shortcuts" is present.
const char kInstallLevel[] = "install-level";

// If present, specify file path to write logging info.
const char kLogFile[] = "log-file";

// Register Chrome as default browser on the system. Usually this will require
// that setup is running as admin. If running as admin we try to register
// as default browser at system level, if running as non-admin we try to
// register as default browser only for the current user.
const char kMakeChromeDefault[] = "make-chrome-default";

// Tells installer to expect to be run as a subsidiary to an MSI.
const char kMsi[] = "msi";

// Useful only when used with --update-setup-exe; otherwise ignored. Specifies
// the full path where the updated setup.exe will be written. Any other files
// created in the indicated directory may be deleted by the caller after process
// termination.
const char kNewSetupExe[] = "new-setup-exe";

// Specifies a nonce to use with the rotate device key command.
const char kNonce[] = "nonce";

// Notify the installer that the OS has been upgraded.
const char kOnOsUpgrade[] = "on-os-upgrade";

// Provide the previous version that patch is for.
const char kPreviousVersion[] = "previous-version";

// Requests that setup attempt to reenable autoupdates for Chrome.
const char kReenableAutoupdates[] = "reenable-autoupdates";

// Register Chrome as a valid browser on the current system. This option
// requires that setup.exe is running as admin. If this option is specified,
// options kInstallArchive and kUninstall are ignored.
const char kRegisterChromeBrowser[] = "register-chrome-browser";

// Used by the installer to forward the registration suffix of the
// (un)installation in progress when launching an elevated setup.exe to finish
// registration work.
const char kRegisterChromeBrowserSuffix[] = "register-chrome-browser-suffix";

// Specify the path to the dev build of chrome.exe the user wants to install
// (register and install Start menu shortcut for) on the system. This will
// always result in a user-level install and will make this install default
// browser.
const char kRegisterDevChrome[] = "register-dev-chrome";

// Switch to allow an extra URL protocol to be registered. This option is used
// in conjunction with kRegisterChromeBrowser to specify an extra protocol
// in addition to the standard set of protocols.
const char kRegisterURLProtocol[] = "register-url-protocol";

// Removes Chrome registration from current machine. Requires admin rights.
const char kRemoveChromeRegistration[] = "remove-chrome-registration";

// Renames chrome.exe to old_chrome.exe and renames new_chrome.exe to chrome.exe
// to support in-use updates. Also deletes opv key.
const char kRenameChromeExe[] = "rename-chrome-exe";

// Rotate the stored device trust signing key.
const char kRotateDeviceTrustKey[] = "rotate-dtkey";

// When we try to relaunch setup.exe as admin on Vista, we append this command
// line flag so that we try the launch only once.
const char kRunAsAdmin[] = "run-as-admin";

// Combined with --uninstall, signals to setup.exe that this uninstall was
// triggered by a self-destructing Chrome.
const char kSelfDestruct[] = "self-destruct";

// Show the embedded EULA dialog.
const char kShowEula[] = "show-eula";

// Saves the specified device management token to the registry.
const char kStoreDMToken[] = "store-dmtoken";

// Install Chrome to system wise location. The default is per user install.
const char kSystemLevel[] = "system-level";

// Signals to setup.exe that it should trigger the active setup command.
const char kTriggerActiveSetup[] = "trigger-active-setup";

// If present, setup will uninstall chrome.
const char kUninstall[] = "uninstall";

// Also see --new-setup-exe. This command line option specifies a diff patch
// that setup.exe will apply to itself and store the resulting binary in the
// path given by --new-setup-exe.
const char kUpdateSetupExe[] = "update-setup-exe";

// Enable verbose logging (info level).
const char kVerboseLogging[] = "verbose-logging";

}  // namespace switches

namespace env_vars {

// The presence of this environment variable with a value of 1 implies that
// setup.exe should run as a system installation regardless of what is on the
// command line.
const char kGoogleUpdateIsMachineEnvVar[] = "GoogleUpdateIsMachine";

}  // namespace env_vars

// The Active Setup executable will be an identical copy of setup.exe; this is
// necessary because Windows' installer detection heuristics (which include
// things like process name being "setup.exe") will otherwise force elevation
// for non-admin users when setup.exe is launched. This is mitigated by adding
// requestedExecutionLevel="asInvoker" to setup.exe's manifest on Vista+, but
// there is no such manifest entry on Windows XP (which results in
// crbug.com/166473).
// TODO(gab): Rename setup.exe itself altogether and use the same binary for
// Active Setup.
const wchar_t kActiveSetupExe[] = L"chrmstp.exe";
const wchar_t kChromeDll[] = L"chrome.dll";
const wchar_t kChromeExe[] = L"chrome.exe";
const wchar_t kChromeNewExe[] = L"new_chrome.exe";
const wchar_t kChromeOldExe[] = L"old_chrome.exe";
const wchar_t kChromeProxyExe[] = L"chrome_proxy.exe";
const wchar_t kChromeProxyNewExe[] = L"new_chrome_proxy.exe";
const wchar_t kChromeProxyOldExe[] = L"old_chrome_proxy.exe";
const wchar_t kCmdAlternateRenameChromeExe[] = L"rename-chrome-exe";
const wchar_t kCmdRenameChromeExe[] = L"cmd";
const wchar_t kCmdOnOsUpgrade[] = L"on-os-upgrade";
const wchar_t kCmdRotateDeviceTrustKey[] = L"rotate-dtkey";
const wchar_t kCmdStoreDMToken[] = L"store-dmtoken";
const wchar_t kCmdDeleteDMToken[] = L"delete-dmtoken";
const wchar_t kEulaSentinelFile[] = L"EULA Accepted";
const wchar_t kInstallBinaryDir[] = L"Application";
const wchar_t kInstallerDir[] = L"Installer";
const wchar_t kInstallTempDir[] = L"Temp";
const wchar_t kLnkExt[] = L".lnk";
const wchar_t kNotificationHelperExe[] = L"notification_helper.exe";
const wchar_t kWerDll[] = L"chrome_wer.dll";

// DowngradeVersion holds the version from which Chrome was downgraded. In case
// of multiple downgrades (e.g., 75->74->73), it retains the highest version
// installed prior to any downgrades. DowngradeVersion is deleted on upgrade
// once Chrome reaches the version from which it was downgraded.
const wchar_t kRegDowngradeVersion[] = L"DowngradeVersion";

const wchar_t kSetupExe[] = L"setup.exe";
const wchar_t kUninstallStringField[] = L"UninstallString";
const wchar_t kUninstallArgumentsField[] = L"UninstallArguments";
const wchar_t kUninstallDisplayNameField[] = L"DisplayName";
const wchar_t kUninstallInstallationDate[] = L"installation_date";

// Elevation Service constants.
const base::FilePath::CharType kElevationServiceExe[] =
    FILE_PATH_LITERAL("elevation_service.exe");

// Google Update installer result API.
const wchar_t kInstallerError[] = L"InstallerError";
const wchar_t kInstallerExtraCode1[] = L"InstallerExtraCode1";
const wchar_t kInstallerResult[] = L"InstallerResult";
const wchar_t kInstallerResultUIString[] = L"InstallerResultUIString";
const wchar_t kInstallerSuccessLaunchCmdLine[] =
    L"InstallerSuccessLaunchCmdLine";

// Chrome channel display names.
const wchar_t kChromeChannelUnknown[] = L"unknown";
const wchar_t kChromeChannelCanary[] = L"canary";
const wchar_t kChromeChannelDev[] = L"dev";
const wchar_t kChromeChannelBeta[] = L"beta";
const wchar_t kChromeChannelStable[] = L"";
const wchar_t kChromeChannelStableExplicit[] = L"stable";

const size_t kMaxAppModelIdLength = 64U;

const char kSetupHistogramAllocatorName[] = "SetupMetrics";

}  // namespace installer
