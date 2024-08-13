// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"

#include <optional>
#include <string>
#include <utility>

#import "base/apple/foundation_util.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/web_applications/os_integration/mac/apps_folder_support.h"
#import "chrome/browser/web_applications/os_integration/mac/bundle_info_plist.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_auto_login_util.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_creator.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"

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

bool UseAdHocSigningForWebAppShims() {
  if (@available(macOS 11.7, *)) {
    // macOS 11.7 and above can code sign at runtime without requiring that the
    // developer tools be installed.

    // A disabled feature flag takes precedence over any enterprise policy.
    if (!base::FeatureList::IsEnabled(
            features::kUseAdHocSigningForWebAppShims)) {
      return false;
    }

    // The browser's local_state can be null in tests. In that case there is no
    // enterprise policy to consider.
    if (PrefService* local_state = g_browser_process->local_state()) {
      // Respect an enterprise policy if one is set.
      if (local_state->IsManagedPreference(
              prefs::kWebAppsUseAdHocCodeSigningForAppShims)) {
        return local_state->GetBoolean(
            prefs::kWebAppsUseAdHocCodeSigningForAppShims);
      }
    }

    return true;
  }

  // Code signing on older macOS versions invokes `codesign_allocate` from the
  // developer tools, so we can't do it at runtime.
  return false;
}

namespace internals {

void CreatePlatformShortcuts_WithUseAdHocSigningForWebAppShims(
    const base::FilePath& app_data_path,
    const ShortcutLocations& creation_locations,
    ShortcutCreationReason creation_reason,
    const ShortcutInfo& shortcut_info,
    CreateShortcutsCallback callback,
    bool use_ad_hoc_signing_for_web_app_shims) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  WebAppShortcutCreator shortcut_creator(app_data_path, GetChromeAppsFolder(),
                                         &shortcut_info,
                                         use_ad_hoc_signing_for_web_app_shims);
  bool created_shortcuts =
      shortcut_creator.CreateShortcuts(creation_reason, creation_locations);
  std::move(callback).Run(created_shortcuts);
}

void CreatePlatformShortcuts(const base::FilePath& app_data_path,
                             const ShortcutLocations& creation_locations,
                             ShortcutCreationReason creation_reason,
                             const ShortcutInfo& shortcut_info,
                             CreateShortcutsCallback callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in
  // `GetChromeAppsFolder()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      web_app::OsIntegrationTestOverride::Get();
  if (AppShimCreationAndLaunchDisabledForTest()) {
    std::move(callback).Run(true);
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(UseAdHocSigningForWebAppShims),
      base::BindOnce(&CreatePlatformShortcuts_WithUseAdHocSigningForWebAppShims,
                     app_data_path, creation_locations, creation_reason,
                     std::cref(shortcut_info), std::move(callback)));
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

  bool has_shortcuts = false;
  const std::string bundle_id = GetBundleIdentifierForShim(
      shortcut_info.app_id, shortcut_info.is_multi_profile
                                ? base::FilePath()
                                : shortcut_info.profile_path);
  if (!BundleInfoPlist::SearchForBundlesById(bundle_id, GetChromeAppsFolder())
           .empty()) {
    has_shortcuts = true;
  } else if (shortcut_info.is_multi_profile) {
    // If in multi-profile mode, search using the profile-scoped bundle id, in
    // case the user has an old shim hanging around.
    const std::string profile_scoped_bundle_id = GetBundleIdentifierForShim(
        shortcut_info.app_id, shortcut_info.profile_path);
    has_shortcuts = !BundleInfoPlist::SearchForBundlesById(
                         profile_scoped_bundle_id, GetChromeAppsFolder())
                         .empty();
  }

  ShortcutLocations locations;
  if (has_shortcuts) {
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

void UpdatePlatformShortcuts_WithUseAdHocSigningForWebAppShims(
    const base::FilePath& app_data_path,
    const std::u16string& old_app_title,
    std::optional<ShortcutLocations> user_specified_locations,
    ResultCallback callback,
    const ShortcutInfo& shortcut_info,
    bool use_ad_hoc_signing_for_web_app_shims) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  WebAppShortcutCreator shortcut_creator(app_data_path, GetChromeAppsFolder(),
                                         &shortcut_info,
                                         use_ad_hoc_signing_for_web_app_shims);
  std::vector<base::FilePath> updated_shim_paths;
  Result result = (shortcut_creator.UpdateShortcuts(/*create_if_needed=*/false,
                                                    &updated_shim_paths)
                       ? Result::kOk
                       : Result::kError);
  std::move(callback).Run(result);
}

void UpdatePlatformShortcuts(
    const base::FilePath& app_data_path,
    const std::u16string& old_app_title,
    std::optional<ShortcutLocations> user_specified_locations,
    ResultCallback callback,
    const ShortcutInfo& shortcut_info) {
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in
  // `GetChromeAppsFolder()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      web_app::OsIntegrationTestOverride::Get();
  if (AppShimCreationAndLaunchDisabledForTest()) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&UseAdHocSigningForWebAppShims),
      base::BindOnce(&UpdatePlatformShortcuts_WithUseAdHocSigningForWebAppShims,
                     app_data_path, old_app_title,
                     std::move(user_specified_locations), std::move(callback),
                     std::cref(shortcut_info)));
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
