// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_shelf_item_factory.h"

#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/arc_playstore_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/browser_app_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/standalone_browser_extension_app_shelf_item_controller.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "components/services/app_service/public/cpp/types_util.h"

ChromeShelfItemFactory::ChromeShelfItemFactory() = default;

ChromeShelfItemFactory::~ChromeShelfItemFactory() = default;

bool ChromeShelfItemFactory::CreateShelfItemForAppId(
    const std::string& app_id,
    ash::ShelfItem* item,
    std::unique_ptr<ash::ShelfItemDelegate>* delegate) {
  ash::ShelfID shelf_id = ash::ShelfID(app_id);
  ash::ShelfItem shelf_item;
  shelf_item.id = shelf_id;
  *item = shelf_item;

  if (app_id == arc::kPlayStoreAppId) {
    *delegate = std::make_unique<ArcPlaystoreShortcutShelfItemController>();
    return true;
  }

  Profile* profile = GetPrimaryProfile();
  auto* proxy =
      apps::AppServiceProxyFactory::GetInstance()->GetForProfile(profile);

  // TODO(crbug.com/1412708): Update the calling methods naming to avoid the
  // usage of app, to indicate that we could also create a shortcut shelf item
  // using the shortcut id.
  if ((base::FeatureList::IsEnabled(features::kCrosWebAppShortcutUiUpdate)) &&
      proxy->ShortcutRegistryCache()->HasShortcut(apps::ShortcutId(app_id))) {
    *delegate =
        std::make_unique<AppServiceShortcutShelfItemController>(shelf_id);
    return true;
  }

  auto app_type = proxy->AppRegistryCache().GetAppType(app_id);

  // Note: In addition to other kinds of web apps, standalone browser hosted
  // apps are also handled by browser app shelf item controller.
  if (BrowserAppShelfControllerShouldHandleApp(app_id, profile)) {
    *delegate =
        std::make_unique<BrowserAppShelfItemController>(shelf_id, profile);
    return true;
  }

  // Standalone browser platform apps are handled by standalone browser
  // extension app shelf item controller.
  if (app_type == apps::AppType::kStandaloneBrowserChromeApp) {
    *delegate =
        std::make_unique<StandaloneBrowserExtensionAppShelfItemController>(
            shelf_id);
    return true;
  }

  *delegate = std::make_unique<AppShortcutShelfItemController>(shelf_id);
  return true;
}

Profile* ChromeShelfItemFactory::GetPrimaryProfile() {
  return ProfileManager::GetPrimaryUserProfile();
}
