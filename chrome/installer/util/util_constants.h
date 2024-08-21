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
  SETUP_PATCH_FAILED = 8,           // Failed to patch setup.exe.
  OS_NOT_SUPPORTED = 9,             // Current OS not supported.
  OS_ERROR = 10,                    // OS API call failed.
  TEMP_DIR_FAILED = 11,             // Unable to get Temp directory.
  UNCOMPRESSION_FAILED = 12,        // Failed to uncompress Chrome archive.
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
  APPLY_DIFF_PATCH_FAILED = 42,  // Failed to apply a diff patch.
  // INCONSISTENT_UPDATE_POLICY = 43,
  // APP_HOST_REQUIRES_USER_LEVEL = 44,
  // APP_HOST_REQUIRES_BINARIES = 45,
  // INSTALL_OF_GOOGLE_UPDATE_FAILED = 46,
  INVALID_STATE_FOR_OPTION = 47,  // A non-install option was called with an
                                  // invalid installer state.
  // WAIT_FOR_EXISTING_FAILED = 48,
  PATCH_INVALID_ARGUMENTS = 49,    // The arguments of --patch were missing or
                                   // they were invalid for any reason.
  DIFF_PATCH_SOURCE_MISSING = 50,  // No previous version archive found for
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

// The type of an update archive.
enum ArchiveType {
  UNKNOWN_ARCHIVE_TYPE,     // Unknown or uninitialized.
  FULL_ARCHIVE_TYPE,        // Full chrome.7z archive.
  INCREMENTAL_ARCHIVE_TYPE  // Incremental or differential archive.
};

// Stages of an installation from which a progress indication is derived.
// Generally listed in the order in which they are reached. The exceptions to
// this are the fork-and-join for diff vs. full installers (where there are
// additional (costly) stages for the former) and rollback in case of error.
enum InstallerStage {
  NO_STAGE,                  // No stage to report.
  UPDATING_SETUP,            // Patching setup.exe with differential update.
  PRECONDITIONS,             // Evaluating pre-install conditions.
  UNCOMPRESSING,             // Uncompressing chrome.packed.7z.
  PATCHING,                  // Patching chrome.7z with differential update.
  UNPACKING,                 // Unpacking chrome.7z.
  CREATING_VISUAL_MANIFEST,  // Creating VisualElementsManifest.xml.
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

extern const char kAllowDowngrade[];
extern const char kChannel[];
extern const char kConfigureUserSettings[];
extern const char kCreateShortcuts[];
extern const char kCriticalUpdateVersion[];
extern const char kDeleteDMToken[];
extern const char kDeleteOldVersions[];
extern const char kDeleteProfile[];
extern const char kDisableLogging[];
extern const char kDmServerUrl[];
extern const char kDoNotLaunchChrome[];
extern const char kDoNotRegisterForUpdateLaunch[];
extern const char kDoNotRemoveSharedItems[];
extern const char kEnableLogging[];
extern const char kForceConfigureUserSettings[];
extern const char kForceUninstall[];
extern const char kInstallArchive[];
extern const char kInstallerData[];
extern const char kInstallLevel[];
extern const char kLogFile[];
extern const char kMakeChromeDefault[];
extern const char kMsi[];
extern const char kNewSetupExe[];
extern const char kNonce[];
extern const char kOnOsUpgrade[];
extern const char kPreviousVersion[];
extern const char kReenableAutoupdates[];
extern const char kRegisterChromeBrowser[];
extern const char kRegisterChromeBrowserSuffix[];
extern const char kRegisterDevChrome[];
extern const char kRegisterURLProtocol[];
extern const char kRemoveChromeRegistration[];
extern const char kRenameChromeExe[];
extern const char kRotateDeviceTrustKey[];
extern const char kRunAsAdmin[];
extern const char kSelfDestruct[];
extern const char kShowEula[];
extern const char kStoreDMToken[];
extern const char kSystemLevel[];
extern const char kTriggerActiveSetup[];
extern const char kUncompressedArchive[];
extern const char kUninstall[];
extern const char kUpdateSetupExe[];
extern const char kVerboseLogging[];

}  // namespace switches

namespace env_vars {

extern const char kGoogleUpdateIsMachineEnvVar[];

}  // namespace env_vars

extern const wchar_t kActiveSetupExe[];
extern const wchar_t kChromeDll[];
extern const wchar_t kChromeExe[];
extern const wchar_t kChromeNewExe[];
extern const wchar_t kChromeOldExe[];
extern const wchar_t kChromeProxyExe[];
extern const wchar_t kChromeProxyNewExe[];
extern const wchar_t kChromeProxyOldExe[];
extern const wchar_t kCmdAlternateRenameChromeExe[];
extern const wchar_t kCmdRenameChromeExe[];
extern const wchar_t kCmdOnOsUpgrade[];
extern const wchar_t kCmdRotateDeviceTrustKey[];
extern const wchar_t kCmdStoreDMToken[];
extern const wchar_t kCmdDeleteDMToken[];
extern const wchar_t kEulaSentinelFile[];
extern const wchar_t kInstallBinaryDir[];
extern const wchar_t kInstallerDir[];
extern const wchar_t kInstallTempDir[];
extern const wchar_t kLnkExt[];
extern const wchar_t kNotificationHelperExe[];
extern const wchar_t kRegDowngradeVersion[];
extern const wchar_t kSetupExe[];
extern const wchar_t kUninstallArgumentsField[];
extern const wchar_t kUninstallDisplayNameField[];
extern const wchar_t kUninstallInstallationDate[];
extern const wchar_t kUninstallStringField[];
extern const wchar_t kWerDll[];

// Elevation Service constants.
extern const base::FilePath::CharType kElevationServiceExe[];

// Google Update installer result API.
extern const wchar_t kInstallerError[];
extern const wchar_t kInstallerExtraCode1[];
extern const wchar_t kInstallerResult[];
extern const wchar_t kInstallerResultUIString[];
extern const wchar_t kInstallerSuccessLaunchCmdLine[];

// Chrome channel display names.
// NOTE: Canary is not strictly a 'channel', but rather a separate product
//     installed side-by-side. However, GoogleUpdateSettings::GetChromeChannel
//     will return "canary" for that product.
extern const wchar_t kChromeChannelUnknown[];
extern const wchar_t kChromeChannelCanary[];
extern const wchar_t kChromeChannelDev[];
extern const wchar_t kChromeChannelBeta[];
extern const wchar_t kChromeChannelStable[];
extern const wchar_t kChromeChannelStableExplicit[];

extern const size_t kMaxAppModelIdLength;
enum : size_t { kMaxDMTokenLength = 4096 };

// Name of the allocator (and associated file) for storing histograms to be
// reported by Chrome during its next upload.
extern const char kSetupHistogramAllocatorName[];

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_UTIL_CONSTANTS_H_
