// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mac/app_mode_common.h"

#import <Foundation/Foundation.h>

#include <type_traits>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "components/version_info/version_info.h"
#include "mojo/core/embedder/embedder.h"

namespace app_mode {

const char kAppShimBootstrapNameFragment[] = "apps";

const char kRunningChromeVersionSymlinkName[] = "RunningChromeVersion";
const char kFeatureStateFileName[] = "ChromeFeatureState";

const char kLaunchedByChromeProcessId[] = "launched-by-chrome-process-id";
const char kLaunchedByChromeBundlePath[] = "launched-by-chrome-bundle-path";
const char kLaunchedByChromeFrameworkBundlePath[] =
    "launched-by-chrome-framework-bundle-path";
const char kLaunchedByChromeFrameworkDylibPath[] =
    "launched-by-chrome-framework-dylib-path";
const char kLaunchedForTest[] = "launched-for-test";
const char kLaunchedAfterRebuild[] = "launched-after-rebuild";
const char kIsNormalLaunch[] = "is-normal-launch";
const char kLaunchChromeForTest[] = "launch-chrome-for-test";

NSString* const kCFBundleDocumentTypesKey = @"CFBundleDocumentTypes";
NSString* const kCFBundleTypeExtensionsKey = @"CFBundleTypeExtensions";
NSString* const kCFBundleTypeIconFileKey = @"CFBundleTypeIconFile";
NSString* const kCFBundleTypeNameKey = @"CFBundleTypeName";
NSString* const kCFBundleTypeMIMETypesKey = @"CFBundleTypeMIMETypes";
NSString* const kCFBundleTypeRoleKey = @"CFBundleTypeRole";
NSString* const kCFBundleURLNameKey = @"CFBundleURLName";
NSString* const kCFBundleURLSchemesKey = @"CFBundleURLSchemes";
NSString* const kCFBundleURLTypesKey = @"CFBundleURLTypes";
NSString* const kBundleTypeRoleViewer = @"Viewer";

NSString* const kCFBundleDisplayNameKey = @"CFBundleDisplayName";
NSString* const kCFBundleShortVersionStringKey = @"CFBundleShortVersionString";
NSString* const kCrBundleVersionKey = @"CrBundleVersion";
NSString* const kLSHasLocalizedDisplayNameKey = @"LSHasLocalizedDisplayName";
NSString* const kNSHighResolutionCapableKey = @"NSHighResolutionCapable";
NSString* const kBrowserBundleIDKey = @"CrBundleIdentifier";
NSString* const kCrAppModeShortcutIDKey = @"CrAppModeShortcutID";
NSString* const kCrAppModeShortcutNameKey = @"CrAppModeShortcutName";
NSString* const kCrAppModeShortcutURLKey = @"CrAppModeShortcutURL";
NSString* const kCrAppModeUserDataDirKey = @"CrAppModeUserDataDir";
NSString* const kCrAppModeProfileDirKey = @"CrAppModeProfileDir";
NSString* const kCrAppModeProfileNameKey = @"CrAppModeProfileName";
NSString* const kCrAppModeMajorVersionKey = @"CrAppModeMajorVersionKey";
NSString* const kCrAppModeMinorVersionKey = @"CrAppModeMinorVersionKey";
NSString* const kCrAppModeIsAdHocSignedKey = @"CrAppModeIsAdhocSigned";

NSString* const kLastRunAppBundlePathPrefsKey = @"LastRunAppBundlePath";

NSString* const kShortcutIdPlaceholder = @"APP_MODE_SHORTCUT_ID";
NSString* const kShortcutNamePlaceholder = @"APP_MODE_SHORTCUT_NAME";
NSString* const kShortcutURLPlaceholder = @"APP_MODE_SHORTCUT_URL";
NSString* const kShortcutBrowserBundleIDPlaceholder =
                    @"APP_MODE_BROWSER_BUNDLE_ID";

static_assert(std::is_standard_layout_v<ChromeAppModeInfo> &&
                  std::is_trivial_v<ChromeAppModeInfo>,
              "ChromeAppModeInfo must be a POD type");

// ChromeAppModeInfo is built into the app_shim_loader binary that is not
// updated with Chrome. If the layout of this structure changes, then Chrome
// must rebuild all app shims. See https://crrev.com/362634 as an example.
static_assert(
    offsetof(ChromeAppModeInfo, argc) == 0x0 &&
        offsetof(ChromeAppModeInfo, argv) == 0x8 &&
        offsetof(ChromeAppModeInfo, chrome_framework_path) == 0x10 &&
        offsetof(ChromeAppModeInfo, chrome_outer_bundle_path) == 0x18 &&
        offsetof(ChromeAppModeInfo, app_mode_bundle_path) == 0x20 &&
        offsetof(ChromeAppModeInfo, app_mode_id) == 0x28 &&
        offsetof(ChromeAppModeInfo, app_mode_name) == 0x30 &&
        offsetof(ChromeAppModeInfo, app_mode_url) == 0x38 &&
        offsetof(ChromeAppModeInfo, user_data_dir) == 0x40 &&
        offsetof(ChromeAppModeInfo, profile_dir) == 0x48 &&
        offsetof(ChromeAppModeInfo, mojo_ipcz_config) == 0x50,
    "ChromeAppModeInfo layout has changed; bump the APP_SHIM_VERSION_NUMBER "
    "in chrome/common/mac/app_mode_common.h. (And fix this static_assert.)");

// static
ChromeConnectionConfig ChromeConnectionConfig::GenerateForCurrentProcess() {
  return {
      .framework_version = std::string(version_info::GetVersionNumber()),
      .is_mojo_ipcz_enabled = mojo::core::IsMojoIpczEnabled(),
  };
}

base::FilePath ChromeConnectionConfig::EncodeAsPath() const {
  return base::FilePath(
      base::StrCat({framework_version, ":", is_mojo_ipcz_enabled ? "1" : "0"}));
}

// static
ChromeConnectionConfig ChromeConnectionConfig::DecodeFromPath(
    const base::FilePath& path) {
  DCHECK(!path.empty());
  const std::vector<std::string> parts = base::SplitString(
      path.value(), ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK(!parts.empty());
  if (parts.size() == 1) {
    // Assume MojoIpcz is disabled for path values which predate the
    // introduction of the MojoIpcz bit.
    return {.framework_version = parts[0], .is_mojo_ipcz_enabled = false};
  }

  return {.framework_version = parts[0],
          .is_mojo_ipcz_enabled = parts[1] == "1"};
}

}  // namespace app_mode
