// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/app_shortcut_manager.h"

#include <utility>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

namespace {

// UMA metric name for shortcuts creation result.
constexpr const char* kCreationResultMetric =
    "WebApp.Shortcuts.Creation.Result";

// Result of shortcuts creation process.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CreationResult {
  kSuccess = 0,
  kFailToCreateShortcut = 1,
  kMaxValue = kFailToCreateShortcut
};

AppShortcutManager::ShortcutCallback& GetShortcutUpdateCallbackForTesting() {
  static base::NoDestructor<AppShortcutManager::ShortcutCallback> callback;
  return *callback;
}

}  // namespace

AppShortcutManager::AppShortcutManager(Profile* profile) : profile_(profile) {}

AppShortcutManager::~AppShortcutManager() = default;

void AppShortcutManager::SetSubsystems(AppIconManager* icon_manager,
                                       AppRegistrar* registrar) {
  icon_manager_ = icon_manager;
  registrar_ = registrar;
}

void AppShortcutManager::UpdateShortcuts(const AppId& app_id,
                                         base::StringPiece old_name) {
  if (!CanCreateShortcuts())
    return;

  GetShortcutInfoForApp(
      app_id, base::BindOnce(
                  &AppShortcutManager::OnShortcutInfoRetrievedUpdateShortcuts,
                  weak_ptr_factory_.GetWeakPtr(), base::UTF8ToUTF16(old_name)));
}

void AppShortcutManager::SetShortcutUpdateCallbackForTesting(
    base::OnceCallback<void(const ShortcutInfo*)> callback) {
  GetShortcutUpdateCallbackForTesting() = std::move(callback);
}

bool AppShortcutManager::CanCreateShortcuts() const {
#if defined(OS_CHROMEOS)
  return false;
#else
  return true;
#endif
}

void AppShortcutManager::SuppressShortcutsForTesting() {
  suppress_shortcuts_for_testing_ = true;
}

void AppShortcutManager::CreateShortcuts(const AppId& app_id,
                                         bool add_to_desktop,
                                         CreateShortcutsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(CanCreateShortcuts());

  GetShortcutInfoForApp(
      app_id, base::BindOnce(
                  &AppShortcutManager::OnShortcutInfoRetrievedCreateShortcuts,
                  weak_ptr_factory_.GetWeakPtr(), add_to_desktop,
                  base::BindOnce(&AppShortcutManager::OnShortcutsCreated,
                                 weak_ptr_factory_.GetWeakPtr(), app_id,
                                 std::move(callback))));
}

void AppShortcutManager::ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
    const AppId& app_id,
    RegisterShortcutsMenuCallback callback) {
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu)) {
    icon_manager_->ReadAllShortcutsMenuIcons(
        app_id,
        base::BindOnce(
            &AppShortcutManager::OnShortcutsMenuIconsReadRegisterShortcutsMenu,
            weak_ptr_factory_.GetWeakPtr(), app_id, std::move(callback)));
  } else {
    std::move(callback).Run(/*shortcuts_menu_registered=*/true);
  }
}

void AppShortcutManager::RegisterShortcutsMenuWithOs(
    const AppId& app_id,
    const std::vector<WebApplicationShortcutsMenuItemInfo>&
        shortcuts_menu_item_infos,
    const ShortcutsMenuIconsBitmaps& shortcuts_menu_icons_bitmaps) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_app::ShouldRegisterShortcutsMenuWithOs() ||
      suppress_shortcuts_for_testing()) {
    return;
  }

  std::unique_ptr<ShortcutInfo> shortcut_info = BuildShortcutInfo(app_id);
  if (!shortcut_info)
    return;

  // |shortcut_data_dir| is located in per-app OS integration resources
  // directory. See GetOsIntegrationResourcesDirectoryForApp function for more
  // info.
  base::FilePath shortcut_data_dir =
      internals::GetShortcutDataDir(*shortcut_info);
  web_app::RegisterShortcutsMenuWithOs(
      shortcut_info->extension_id, shortcut_info->profile_path,
      shortcut_data_dir, shortcuts_menu_item_infos,
      shortcuts_menu_icons_bitmaps);
}

void AppShortcutManager::UnregisterShortcutsMenuWithOs(const AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_app::ShouldRegisterShortcutsMenuWithOs())
    return;

  web_app::UnregisterShortcutsMenuWithOs(app_id, profile_->GetPath());
}

void AppShortcutManager::OnShortcutsCreated(const AppId& app_id,
                                            CreateShortcutsCallback callback,
                                            bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  UMA_HISTOGRAM_ENUMERATION(kCreationResultMetric,
                            success ? CreationResult::kSuccess
                                    : CreationResult::kFailToCreateShortcut);
  std::move(callback).Run(success);
}

void AppShortcutManager::OnShortcutInfoRetrievedCreateShortcuts(
    bool add_to_desktop,
    CreateShortcutsCallback callback,
    std::unique_ptr<ShortcutInfo> info) {
  if (suppress_shortcuts_for_testing_) {
    std::move(callback).Run(/*shortcut_created=*/true);
    return;
  }

  if (info == nullptr) {
    std::move(callback).Run(/*shortcut_created=*/false);
    return;
  }

  base::FilePath shortcut_data_dir = internals::GetShortcutDataDir(*info);

  ShortcutLocations locations;
  locations.on_desktop = add_to_desktop;
  locations.applications_menu_location = APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;

  // Remove any previously created App Icon Shortcuts Menu.
  if (!base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu) &&
      web_app::ShouldRegisterShortcutsMenuWithOs()) {
    web_app::UnregisterShortcutsMenuWithOs(info->extension_id,
                                           info->profile_path);
  }

  internals::ScheduleCreatePlatformShortcuts(
      std::move(shortcut_data_dir), locations, SHORTCUT_CREATION_BY_USER,
      std::move(info), std::move(callback));
}

void AppShortcutManager::OnShortcutsMenuIconsReadRegisterShortcutsMenu(
    const AppId& app_id,
    RegisterShortcutsMenuCallback callback,
    ShortcutsMenuIconsBitmaps shortcuts_menu_icons_bitmaps) {
  std::vector<WebApplicationShortcutsMenuItemInfo> shortcuts_menu_item_infos =
      registrar_->GetAppShortcutsMenuItemInfos(app_id);
  if (!shortcuts_menu_item_infos.empty()) {
    RegisterShortcutsMenuWithOs(app_id, shortcuts_menu_item_infos,
                                shortcuts_menu_icons_bitmaps);
  }

  std::move(callback).Run(/*shortcuts_menu_registered=*/true);
}

void AppShortcutManager::OnShortcutInfoRetrievedUpdateShortcuts(
    base::string16 old_name,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  if (GetShortcutUpdateCallbackForTesting())
    std::move(GetShortcutUpdateCallbackForTesting()).Run(shortcut_info.get());

  if (suppress_shortcuts_for_testing() || !shortcut_info)
    return;

  base::FilePath shortcut_data_dir =
      internals::GetShortcutDataDir(*shortcut_info);
  internals::PostShortcutIOTask(
      base::BindOnce(&internals::UpdatePlatformShortcuts,
                     std::move(shortcut_data_dir), std::move(old_name)),
      std::move(shortcut_info));
}

}  // namespace web_app
