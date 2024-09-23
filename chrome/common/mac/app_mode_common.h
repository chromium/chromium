// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_APP_MODE_COMMON_H_
#define CHROME_COMMON_MAC_APP_MODE_COMMON_H_

#include <CoreServices/CoreServices.h>
#include <string>

#include "base/files/file_path.h"
#include "base/macros/concat.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/strings/stringize_macros.h"

#ifdef __OBJC__
@class NSString;
#endif

// This file contains constants, interfaces, etc. which are common to the
// browser application and the app mode loader (a.k.a. shim).

// The version of the ChromeAppModeInfo struct below. If the format of the
// struct ever changes, be sure to update the APP_SHIM_VERSION_NUMBER here and
// the corresponding lines in //chrome/app/framework.order and
// //chrome/app/framework.exports .
#define APP_SHIM_VERSION_NUMBER 7

// All the other macro magic to make APP_SHIM_VERSION_NUMBER usable.
#define APP_SHIM_ENTRY_POINT_NAME \
  BASE_CONCAT(ChromeAppModeStart_v, APP_SHIM_VERSION_NUMBER)
#define APP_SHIM_ENTRY_POINT_NAME_STRING STRINGIZE(APP_SHIM_ENTRY_POINT_NAME)

namespace app_mode {

// Mach message ID used by the shim to connect to Chrome.
constexpr mach_msg_id_t kBootstrapMsgId = 'apps';

// Name fragment of the Mach server endpoint published in the bootstrap
// namespace. The full name is "<bundle-id>.apps.<profile_path_hash>".
// <bundle-id> is the BaseBundleID() and <profile_path_hash> is an MD5 hash
// of the full profile directory path.
extern const char kAppShimBootstrapNameFragment[];

// A symlink used to store the version string of the currently running Chrome,
// along with any other necessary configuration. The shim will read this to
// determine which version of the framework to load.
extern const char kRunningChromeVersionSymlinkName[];

// A file used to store feature and field trial state of the currently or most
// recently running Chrome. The shim will read this to determine what features
// to enable if it wasn't launched by Chrome, until it can get the current state
// from Chrome.
extern const char kFeatureStateFileName[];

// The process ID of the Chrome process that launched the app shim.
// The presence of this switch instructs the app shim to send LaunchApp with
// launch_now = false. This associates the shim without launching the app.
extern const char kLaunchedByChromeProcessId[];

// The main bundle path of the Chrome process that launched the app shim.
extern const char kLaunchedByChromeBundlePath[];

// The framework bundle path of the Chrome process that launched the app shim.
extern const char kLaunchedByChromeFrameworkBundlePath[];

// The framework dylib path of the Chrome process that launched the app shim.
extern const char kLaunchedByChromeFrameworkDylibPath[];

// Indicates to the shim that it was launched for a test, so don't attempt to
// launch Chrome.
extern const char kLaunchedForTest[];

// Indicates to the shim that this Chrome has rebuilt it once already, i.e. if
// it fails to launch again, don't trigger another rebuild.
extern const char kLaunchedAfterRebuild[];

// Indicates to the shim that even if `kLaunchedByChromeProcessId` was also
// specified, this should still be considered a "normal" launch as opposed to a
// "register only" launch. This is used by tests to launch a shim as if the user
// launched it, while still making sure it connects to the correct chrome
// process.
extern const char kIsNormalLaunch[];

// Normally when running tests app shims are not supposed to try to launch
// Chrome. Pass this flag to specify the executable to launch when the app shim
// would normally launch Chrome.
extern const char kLaunchChromeForTest[];

#ifdef __OBJC__

// Keys for specifying the file types handled by an app.
extern NSString* const kCFBundleDocumentTypesKey;
extern NSString* const kCFBundleTypeExtensionsKey;
extern NSString* const kCFBundleTypeIconFileKey;
extern NSString* const kCFBundleTypeNameKey;
extern NSString* const kCFBundleTypeMIMETypesKey;
extern NSString* const kCFBundleTypeRoleKey;
extern NSString* const kCFBundleURLNameKey;
extern NSString* const kCFBundleURLSchemesKey;
extern NSString* const kCFBundleURLTypesKey;
extern NSString* const kBundleTypeRoleViewer;

// The display name of the bundle as shown in Finder and the Dock. For localized
// bundles, this overrides the bundle's file name.
extern NSString* const kCFBundleDisplayNameKey;

// When Chrome is built, any app bundles (e.g. the app shim template bundle)
// will have their CFBundleShortVersionString set to the full version string of
// that build. Since, this string is used by OSX when displaying an app bundle's
// version, we override it in app shim bundles to show the app's version
// instead.
extern NSString* const kCFBundleShortVersionStringKey;

// Key for the Chrome version that built the app shim bundle. This needs to be
// added since we override CFBundleShortVersionString with the version of the
// app.
extern NSString* const kCrBundleVersionKey;

// The key specifying whether the display name should be localized. This makes
// Finder look in localization folders in the app bundle for a display name.
// (e.g. Content/Resources/en.lproj/)
extern NSString* const kLSHasLocalizedDisplayNameKey;

// Key specifying whether or not high DPI display is supported at all. If this
// is not set to true then all graphics (including system dialogs and display
// property queries) will behave as though all displays are low DPI.
extern NSString* const kNSHighResolutionCapableKey;

// The key under which the browser's bundle ID will be stored in the
// app mode launcher bundle's Info.plist.
extern NSString* const kBrowserBundleIDKey;

// Key for the shortcut ID.
extern NSString* const kCrAppModeShortcutIDKey;

// Key for the app's name.
extern NSString* const kCrAppModeShortcutNameKey;

// Key for the app's URL.
extern NSString* const kCrAppModeShortcutURLKey;

// Key for the app user data directory.
extern NSString* const kCrAppModeUserDataDirKey;

// Key for the app's extension path.
extern NSString* const kCrAppModeProfileDirKey;

// Key for the app's profile display name.
extern NSString* const kCrAppModeProfileNameKey;

// When the Chrome browser is run, it stores its location in the defaults
// system using this key.
extern NSString* const kLastRunAppBundlePathPrefsKey;

// The key for the major and minor version of an app.
extern NSString* const kCrAppModeMajorVersionKey;
extern NSString* const kCrAppModeMinorVersionKey;

// Info.plist key that indicates whether a PWA is ad-hoc signed.
// Intended for use by `app_mode_loader` prior to checking in with the browser.
extern NSString* const kCrAppModeIsAdHocSignedKey;

// Placeholders used in the app mode loader bundle' Info.plist:
extern NSString* const kShortcutIdPlaceholder; // Extension shortcut ID.
extern NSString* const kShortcutNamePlaceholder; // Extension name.
extern NSString* const kShortcutURLPlaceholder;
// Bundle ID of the Chrome browser bundle.
extern NSString* const kShortcutBrowserBundleIDPlaceholder;

#endif  // __OBJC__

// Indicates the MojoIpcz feature configuration for a launched shim process.
enum class MojoIpczConfig {
  // MojoIpcz is enabled.
  kEnabled,

  // MojoIpcz is disabled.
  kDisabled,

  // The MojoIpcz configuration should be determined by feature flags on the
  // CommandLine once parsed by the shim.
  kUseCommandLineFeatures,
};

// The structure used to pass information from the app mode loader to the
// (browser) framework via the entry point ChromeAppModeStart_vN.
//
// As long as the name of the entry point is kept constant and
// APP_SHIM_VERSION_NUMBER does not change, the layout of this structure
// **MUST NOT CHANGE**, even across Chromium versions. This implies that no
// base/ or std:: types may be used in this structure.
//
// However, this structure *may* be changed as long as the
// APP_SHIM_VERSION_NUMBER above is updated; don't forget to also update the
// corresponding line in //chrome/app/framework.order .
struct ChromeAppModeInfo {
  // Original |argc| and |argv| of the App Mode shortcut.
  int argc;
  // This field is not a raw_ptr<> because this struct is part of separate
  // binary and must be a POD.
  RAW_PTR_EXCLUSION char** argv;

  // Path of the Chromium Framework, as UTF-8. This will be the input to
  // SetOverrideFrameworkBundlePath().
  const char* chrome_framework_path;

  // Path to Chromium app bundle, as UTF-8.
  const char* chrome_outer_bundle_path;

  // Information about the App Mode shortcut:

  // Path to the App Mode Loader application bundle that launched the process,
  // as UTF-8.
  const char* app_mode_bundle_path;

  // Short UTF-8 ID string, preferably derived from |app_mode_short_name|.
  // Should be safe for the file system.
  const char* app_mode_id;

  // Unrestricted (e.g., several-word) UTF-8-encoded name for the shortcut.
  const char* app_mode_name;

  // URL for the shortcut. Must be a valid UTF-8-encoded URL.
  const char* app_mode_url;

  // Path to the app's user data directory, as UTF-8.
  const char* user_data_dir;

  // Directory of the profile associated with the app, as UTF-8.
  const char* profile_dir;

  // Indicates whether MojoIpcz must be enabled in the shim.
  MojoIpczConfig mojo_ipcz_config;
};

// Conveys the configuration for a connection to be established between a shim
// process and a running Chrome process.
struct ChromeConnectionConfig {
  // The version of the Chromium framework to use.
  std::string framework_version;

  // Indicates whether or not the MojoIpcz feature must be enabled.
  bool is_mojo_ipcz_enabled;

  // Returns a new configuration appropriate for the calling Chrome process to
  // encode and convey to a shim.
  static ChromeConnectionConfig GenerateForCurrentProcess();

  // Generates a path value which encodes the contents of this structure.
  base::FilePath EncodeAsPath() const;

  // Parses a path value into a configuration.
  static ChromeConnectionConfig DecodeFromPath(const base::FilePath& path);
};

}  // namespace app_mode

#endif  // CHROME_COMMON_MAC_APP_MODE_COMMON_H_
