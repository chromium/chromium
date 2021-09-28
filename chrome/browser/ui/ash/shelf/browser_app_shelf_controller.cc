// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/browser_app_shelf_controller.h"

#include <memory>

#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_instance.h"
#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ui/ash/shelf/browser_app_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_item_factory.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "extensions/common/constants.h"
#include "ui/aura/window.h"

namespace {

void MaybeUpdateStringProperty(aura::Window* window,
                               const ui::ClassProperty<std::string*>* property,
                               const std::string& value) {
  std::string* old_value = window->GetProperty(ash::kAppIDKey);
  if (!old_value || *old_value != value) {
    window->SetProperty(property, value);
  }
}

}  // namespace

BrowserAppShelfController::BrowserAppShelfController(
    Profile* profile,
    ash::ShelfModel& model,
    ChromeShelfItemFactory& shelf_item_factory,
    ShelfSpinnerController& shelf_spinner_controller)
    : profile_(profile),
      model_(model),
      shelf_item_factory_(shelf_item_factory),
      shelf_spinner_controller_(shelf_spinner_controller),
      browser_app_instance_registry_(
          *apps::AppServiceProxyFactory::GetForProfile(profile)
               ->BrowserAppInstanceRegistry()) {
  registry_observation_.Observe(&browser_app_instance_registry_);
}

BrowserAppShelfController::~BrowserAppShelfController() = default;

void BrowserAppShelfController::OnBrowserWindowAdded(
    const apps::BrowserWindowInstance& instance) {
  ash::ShelfID id(crosapi::browser_util::IsLacrosWindow(instance.window)
                      ? extension_misc::kLacrosAppId
                      : extension_misc::kChromeAppId);
  CreateOrUpdateShelfItem(id, ash::STATUS_RUNNING);
  MaybeUpdateBrowserWindowProperties(instance.window);
}

void BrowserAppShelfController::OnBrowserWindowRemoved(
    const apps::BrowserWindowInstance& instance) {
  bool is_running =
      crosapi::browser_util::IsLacrosWindow(instance.window)
          ? browser_app_instance_registry_.IsLacrosBrowserRunning()
          : browser_app_instance_registry_.IsAshBrowserRunning();
  if (!is_running) {
    ash::ShelfID id(crosapi::browser_util::IsLacrosWindow(instance.window)
                        ? extension_misc::kLacrosAppId
                        : extension_misc::kChromeAppId);
    SetShelfItemClosed(id);
  }
}

void BrowserAppShelfController::OnBrowserAppAdded(
    const apps::BrowserAppInstance& instance) {
  ash::ShelfID id(instance.app_id);
  switch (instance.type) {
    case apps::BrowserAppInstance::Type::kAppWindow: {
      shelf_spinner_controller_.CloseSpinner(instance.app_id);
      CreateOrUpdateShelfItem(id, ash::STATUS_RUNNING);
      break;
    }
    case apps::BrowserAppInstance::Type::kAppTab:
      // New shelf item is not automatically created for unpinned tabbed apps.
      if (const ash::ShelfItem* item = model_.ItemByID(id)) {
        UpdateShelfItemStatus(*item, ash::STATUS_RUNNING);
      }
      break;
  }
  MaybeUpdateBrowserWindowProperties(instance.window);
}

void BrowserAppShelfController::OnBrowserAppUpdated(
    const apps::BrowserAppInstance& instance) {
  MaybeUpdateBrowserWindowProperties(instance.window);
}

void BrowserAppShelfController::OnBrowserAppRemoved(
    const apps::BrowserAppInstance& instance) {
  MaybeUpdateBrowserWindowProperties(instance.window);
  if (!browser_app_instance_registry_.IsAppRunning(instance.app_id)) {
    ash::ShelfID id(instance.app_id);
    SetShelfItemClosed(id);
  }
}

void BrowserAppShelfController::UpdateShelfItemStatus(
    const ash::ShelfItem& item,
    ash::ShelfItemStatus status) {
  auto new_item = item;
  new_item.status = status;
  model_.Set(model_.ItemIndexByID(item.id), new_item);
}

void BrowserAppShelfController::CreateOrUpdateShelfItem(
    const ash::ShelfID& id,
    ash::ShelfItemStatus status) {
  const ash::ShelfItem* item = model_.ItemByID(id);
  if (item) {
    UpdateShelfItemStatus(*item, status);
    return;
  }

  ash::ShelfItem new_item;
  std::unique_ptr<ash::ShelfItemDelegate> delegate;
  shelf_item_factory_.CreateShelfItemForAppId(id.app_id, &new_item, &delegate);
  new_item.type = ash::TYPE_APP;
  new_item.status = status;
  new_item.app_status =
      ShelfControllerHelper::GetAppStatus(profile_, id.app_id);
  model_.AddAt(model_.item_count(), new_item, std::move(delegate));
}

void BrowserAppShelfController::SetShelfItemClosed(const ash::ShelfID& id) {
  const ash::ShelfItem* item = model_.ItemByID(id);
  if (!item) {
    // There is no shelf item for unpinned apps running in a browser tab.
    return;
  }

  if (ash::IsPinnedShelfItemType(item->type)) {
    UpdateShelfItemStatus(*item, ash::STATUS_CLOSED);
  } else {
    int index = model_.ItemIndexByID(id);
    model_.RemoveItemAt(index);
  }
}

void BrowserAppShelfController::MaybeUpdateBrowserWindowProperties(
    aura::Window* window) {
  // If it's a browser window, this app ID will be used as app ID and/or shelf
  // ID.
  std::string browser_app_id = crosapi::browser_util::IsLacrosWindow(window)
                                   ? extension_misc::kLacrosAppId
                                   : extension_misc::kChromeAppId;

  const apps::BrowserAppInstance* active_instance =
      browser_app_instance_registry_.GetActiveAppInstanceForWindow(window);
  // App ID of the window is set to the app ID of the active tab. If the active
  // tab has no app, window's app ID is set to Chrome's ID.
  std::string app_id =
      active_instance ? active_instance->app_id : browser_app_id;
  MaybeUpdateStringProperty(window, ash::kAppIDKey, app_id);

  // Shelf ID is set to match the app's item on the shelf, if it exists.
  const ash::ShelfItem* item = model_.ItemByID(ash::ShelfID(app_id));
  // There is no shelf item for unpinned apps running in a browser tab.
  ash::ShelfID shelf_id = item ? item->id : ash::ShelfID(browser_app_id);
  MaybeUpdateStringProperty(window, ash::kShelfIDKey, shelf_id.Serialize());
}
