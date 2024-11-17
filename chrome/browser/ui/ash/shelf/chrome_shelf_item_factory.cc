// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_shelf_item_factory.h"

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/arc_playstore_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/browser_app_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/browser/ui/ash/shelf/standalone_browser_extension_app_shelf_item_controller.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/types_util.h"

ChromeShelfItemFactory::ChromeShelfItemFactory() = default;

ChromeShelfItemFactory::~ChromeShelfItemFactory() = default;

std::unique_ptr<ash::ShelfItem> ChromeShelfItemFactory::CreateShelfItemForApp(
    const ash::ShelfID& shelf_id,
    ash::ShelfItemStatus status,
    ash::ShelfItemType shelf_item_type,
    const std::u16string& title) {
  auto item = std::make_unique<ash::ShelfItem>();
  item->id = shelf_id;
  item->status = status;
  item->type = shelf_item_type;
  item->title = title;

  const std::string& app_id = shelf_id.app_id;
  item->app_status = ShelfControllerHelper::GetAppStatus(profile_, app_id);

  if (ash::features::ArePromiseIconsEnabled()) {
    item->progress =
        ShelfControllerHelper::GetPromiseAppProgress(profile_, app_id);
    item->is_promise_app =
        ShelfControllerHelper::IsPromiseApp(profile_, app_id);
    item->package_id = ShelfControllerHelper::GetAppPackageId(profile_, app_id);
  }

  return item;
}

std::unique_ptr<ash::ShelfItemDelegate>
ChromeShelfItemFactory::CreateShelfItemDelegateForAppId(
    const std::string& app_id) {
  if (app_id == arc::kPlayStoreAppId) {
    return std::make_unique<ArcPlaystoreShortcutShelfItemController>();
  }

  auto* proxy =
      apps::AppServiceProxyFactory::GetInstance()->GetForProfile(profile_);
  auto app_type = proxy->AppRegistryCache().GetAppType(app_id);

  // Note: In addition to other kinds of web apps, standalone browser hosted
  // apps are also handled by browser app shelf item controller.
  if (BrowserAppShelfControllerShouldHandleApp(app_id, profile_)) {
    return std::make_unique<BrowserAppShelfItemController>(ash::ShelfID(app_id),
                                                           profile_);
  }

  // Standalone browser platform apps are handled by standalone browser
  // extension app shelf item controller.
  if (app_type == apps::AppType::kStandaloneBrowserChromeApp) {
    return std::make_unique<StandaloneBrowserExtensionAppShelfItemController>(
        ash::ShelfID(app_id));
  }

  return std::make_unique<AppShortcutShelfItemController>(ash::ShelfID(app_id));
}
