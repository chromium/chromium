// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/web_applications/os_integration/web_app_shortcut_mac.h"

#import <Cocoa/Cocoa.h>
#include <stdint.h>

#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "base/apple/bridging.h"
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/base_switches.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#import "base/mac/launch_application.h"
#include "base/mac/mac_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/version.h"
#include "cc/paint/paint_flags.h"
#import "chrome/browser/mac/dock.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/shortcuts/platform_util_mac.h"
#include "chrome/browser/web_applications/os_integration/mac/apps_folder_support.h"
#import "chrome/browser/web_applications/os_integration/mac/bundle_info_plist.h"
#include "chrome/browser/web_applications/os_integration/mac/icns_encoder.h"
#include "chrome/browser/web_applications/os_integration/mac/icon_utils.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_auto_login_util.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_creator.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#import "chrome/common/mac/app_mode_common.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/variations/net/variations_command_line.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/features.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/effects/SkImageFilters.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_family.h"

namespace web_app {

bool AppShimCreationAndLaunchDisabledForTest() {
  // Note: The kTestType switch is only added on browser tests, but not unit
  // tests. Unit tests need to set the test override.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kTestType) &&
         !OsIntegrationTestOverride::Get();
}

// Removes the app shim from the list of Login Items.
void RemoveAppShimFromLoginItems(const std::string& app_id) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const std::string bundle_id = GetBundleIdentifierForShim(app_id);
  auto bundle_infos =
      BundleInfoPlist::SearchForBundlesById(bundle_id, GetChromeAppsFolder());
  for (const auto& bundle_info : bundle_infos) {
    WebAppAutoLoginUtil::GetInstance()->RemoveFromLoginItems(
        bundle_info.bundle_path());
  }
}

std::string GetBundleIdentifierForShim(const std::string& app_id,
                                       const base::FilePath& profile_path) {
  // Note that this matches APP_MODE_APP_BUNDLE_ID in chrome/chrome.gyp.
  if (!profile_path.empty()) {
    // Replace spaces in the profile path with hyphen.
    std::string normalized_profile_path;
    base::ReplaceChars(profile_path.BaseName().value(), " ", "-",
                       &normalized_profile_path);
    return base::apple::BaseBundleID() + std::string(".app.") +
           normalized_profile_path + "-" + app_id;
  }
  return base::apple::BaseBundleID() + std::string(".app.") + app_id;
}

namespace internals {

bool CreatePlatformShortcuts(const base::FilePath& app_data_path,
                             const ShortcutLocations& creation_locations,
                             ShortcutCreationReason creation_reason,
                             const ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in
  // `GetChromeAppsFolder()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      web_app::OsIntegrationTestOverride::Get();
  if (AppShimCreationAndLaunchDisabledForTest()) {
    return true;
  }

  WebAppShortcutCreator shortcut_creator(app_data_path, GetChromeAppsFolder(),
                                         &shortcut_info);
  return shortcut_creator.CreateShortcuts(creation_reason, creation_locations);
}

ShortcutLocations GetAppExistingShortCutLocationImpl(
    const ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in
  // `GetChromeAppsFolder()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      web_app::OsIntegrationTestOverride::Get();
  WebAppShortcutCreator shortcut_creator(
      internals::GetShortcutDataDir(shortcut_info), GetChromeAppsFolder(),
      &shortcut_info);
  ShortcutLocations locations;
  if (!shortcut_creator.GetAppBundlesById().empty()) {
    locations.applications_menu_location = APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
  }
  return locations;
}

void DeletePlatformShortcuts(const base::FilePath& app_data_path,
                             const ShortcutInfo& shortcut_info,
                             scoped_refptr<base::TaskRunner> result_runner,
                             DeleteShortcutsCallback callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in
  // `GetChromeAppsFolder()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      web_app::OsIntegrationTestOverride::Get();

  if (test_override) {
    CHECK_IS_TEST();
    test_override->RegisterProtocolSchemes(shortcut_info.app_id,
                                           std::vector<std::string>());
  }
  const std::string bundle_id = GetBundleIdentifierForShim(
      shortcut_info.app_id, shortcut_info.profile_path);
  auto bundle_infos =
      BundleInfoPlist::SearchForBundlesById(bundle_id, GetChromeAppsFolder());
  bool result = true;
  for (const auto& bundle_info : bundle_infos) {
    WebAppAutoLoginUtil::GetInstance()->RemoveFromLoginItems(
        bundle_info.bundle_path());
    if (!base::DeletePathRecursively(bundle_info.bundle_path())) {
      result = false;
    }
  }
  result_runner->PostTask(FROM_HERE,
                          base::BindOnce(std::move(callback), result));
}

void DeleteMultiProfileShortcutsForApp(const std::string& app_id) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in
  // `GetChromeAppsFolder()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      web_app::OsIntegrationTestOverride::Get();
  const std::string bundle_id = GetBundleIdentifierForShim(app_id);
  auto bundle_infos =
      BundleInfoPlist::SearchForBundlesById(bundle_id, GetChromeAppsFolder());
  for (const auto& bundle_info : bundle_infos) {
    WebAppAutoLoginUtil::GetInstance()->RemoveFromLoginItems(
        bundle_info.bundle_path());
    base::DeletePathRecursively(bundle_info.bundle_path());
  }
}

Result UpdatePlatformShortcuts(
    const base::FilePath& app_data_path,
    const std::u16string& old_app_title,
    std::optional<ShortcutLocations> user_specified_locations,
    const ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in
  // `GetChromeAppsFolder()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      web_app::OsIntegrationTestOverride::Get();
  if (AppShimCreationAndLaunchDisabledForTest()) {
    return Result::kOk;
  }

  WebAppShortcutCreator shortcut_creator(app_data_path, GetChromeAppsFolder(),
                                         &shortcut_info);
  std::vector<base::FilePath> updated_shim_paths;
  return (shortcut_creator.UpdateShortcuts(/*create_if_needed=*/false,
                                           &updated_shim_paths)
              ? Result::kOk
              : Result::kError);
}

void DeleteAllShortcutsForProfile(const base::FilePath& profile_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in
  // `GetChromeAppsFolder()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      web_app::OsIntegrationTestOverride::Get();
  std::list<BundleInfoPlist> bundles_info =
      BundleInfoPlist::GetAllInPath(GetChromeAppsFolder(), /*recursive=*/true);
  for (const auto& info : bundles_info) {
    if (!info.IsForCurrentUserDataDir()) {
      continue;
    }
    if (!info.IsForProfile(profile_path)) {
      continue;
    }
    WebAppAutoLoginUtil::GetInstance()->RemoveFromLoginItems(
        info.bundle_path());
    base::DeletePathRecursively(info.bundle_path());
  }
}

}  // namespace internals

}  // namespace web_app
