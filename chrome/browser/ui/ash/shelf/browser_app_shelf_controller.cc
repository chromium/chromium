// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/browser_app_shelf_controller.h"

#include <memory>

#include "ash/public/cpp/window_properties.h"
#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_instance.h"
#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ui/ash/shelf/browser_app_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_item_factory.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "components/app_constants/constants.h"
#include "ui/aura/window.h"

namespace {

void MaybeUpdateStringProperty(aura::Window* window,
                               const ui::ClassProperty<std::string*>* property,
                               const std::string& value) {
  std::string* old_value = window->GetProperty(property);
  if (!old_value || *old_value != value) {
    window->SetProperty(property, value);
  }
}

// TODO(crbug.com/1267769): sometimes browser window instance lookup fails when
// it should be present. Remove when we know the root cause.
void BrowserWindowMustBeValid(
    const apps::BrowserWindowInstance* browser_window) {
  if (!browser_window) {
    base::debug::DumpWithoutCrashing();
  } else if (!browser_window->window) {
    base::debug::DumpWithoutCrashing();
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
  shelf_model_observation_.Observe(&model);
}

BrowserAppShelfController::~BrowserAppShelfController() = default;

void BrowserAppShelfController::OnBrowserWindowAdded(
    const apps::BrowserWindowInstance& instance) {
  ash::ShelfID id(instance.GetAppId());
  CreateOrUpdateShelfItem(id, ash::STATUS_RUNNING);
  MaybeUpdateWindowProperties(instance.window);
}

void BrowserAppShelfController::OnBrowserWindowRemoved(
    const apps::BrowserWindowInstance& instance) {
  bool is_running =
      crosapi::browser_util::IsLacrosWindow(instance.window)
          ? browser_app_instance_registry_.IsLacrosBrowserRunning()
          : browser_app_instance_registry_.IsAshBrowserRunning();
  if (!is_running) {
    ash::ShelfID id(instance.GetAppId());
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
  MaybeUpdateWindowProperties(instance.window);
}

void BrowserAppShelfController::OnBrowserAppUpdated(
    const apps::BrowserAppInstance& instance) {
  // Active tab may have changed.
  MaybeUpdateWindowProperties(instance.window);
}

void BrowserAppShelfController::OnBrowserAppRemoved(
    const apps::BrowserAppInstance& instance) {
  if (instance.type == apps::BrowserAppInstance::Type::kAppTab) {
    // If a tab is closed, browser window may still remain, so it needs its
    // properties updated.
    MaybeUpdateWindowProperties(instance.window);
  }
  if (!browser_app_instance_registry_.IsAppRunning(instance.app_id)) {
    ash::ShelfID id(instance.app_id);
    SetShelfItemClosed(id);
  }
}

void BrowserAppShelfController::ShelfItemAdded(int index) {
  const ash::ShelfItem& item = model_.items()[index];
  const std::string& app_id = item.id.app_id;
  if (!BrowserAppShelfControllerShouldHandleApp(app_id, profile_)) {
    return;
  }
  bool running = (app_id == app_constants::kLacrosAppId)
                     ? browser_app_instance_registry_.IsLacrosBrowserRunning()
                     : browser_app_instance_registry_.IsAppRunning(app_id);
  UpdateShelfItemStatus(item,
                        running ? ash::STATUS_RUNNING : ash::STATUS_CLOSED);
  MaybeUpdateWindowPropertiesForApp(app_id);
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

void BrowserAppShelfController::MaybeUpdateWindowProperties(
    aura::Window* window) {
  const apps::BrowserAppInstance* active_instance =
      browser_app_instance_registry_.FindAppInstanceIf(
          [window](const apps::BrowserAppInstance& instance) {
            return instance.window == window && instance.is_web_contents_active;
          });
  const apps::BrowserWindowInstance* browser_window =
      browser_app_instance_registry_.FindWindowInstanceIf(
          [window](const apps::BrowserWindowInstance& instance) {
            return instance.window == window;
          });
  const char* browser_app_id = crosapi::browser_util::IsLacrosWindow(window)
                                   ? app_constants::kLacrosAppId
                                   : app_constants::kChromeAppId;
  // App ID of the window is set to the app ID of the active tab. If the active
  // tab has no app, app ID of the window is set to the browser's ID.
  // Shelf ID of the window is set to the app's item on the shelf, if the item
  // exists, otherwise it's set to the browser's ID (this happens for apps in
  // tabs that aren't pinned).
  std::string app_id;
  ash::ShelfID shelf_id;
  if (active_instance) {
    app_id = active_instance->app_id;
    const ash::ShelfItem* item = model_.ItemByID(ash::ShelfID(app_id));
    if (item) {
      shelf_id = item->id;
    } else {
      // There is no shelf item for unpinned apps running in a browser tab, so
      // they get mapped to the browser's shelf item (app ID and shelf ID are
      // different at this point).
      shelf_id = ash::ShelfID(browser_app_id);
      DCHECK(model_.ItemByID(shelf_id));
      BrowserWindowMustBeValid(browser_window);
    }
  } else {
    // No active app for that window: it's mapped to the browser's shelf item,
    // which must be present.
    app_id = browser_app_id;
    shelf_id = ash::ShelfID(browser_app_id);
    DCHECK(model_.ItemByID(shelf_id));
    BrowserWindowMustBeValid(browser_window);
  }
  MaybeUpdateStringProperty(window, ash::kAppIDKey, app_id);
  MaybeUpdateStringProperty(window, ash::kShelfIDKey, shelf_id.Serialize());
}

void BrowserAppShelfController::MaybeUpdateWindowPropertiesForApp(
    const std::string& app_id) {
  std::set<const apps::BrowserAppInstance*> instances =
      browser_app_instance_registry_.SelectAppInstances(
          [&app_id](const apps::BrowserAppInstance& instance) {
            return instance.app_id == app_id;
          });
  std::set<aura::Window*> windows;
  for (const auto* instance : instances) {
    windows.insert(instance->window);
  }
  for (auto* window : windows) {
    MaybeUpdateWindowProperties(window);
  }
}
