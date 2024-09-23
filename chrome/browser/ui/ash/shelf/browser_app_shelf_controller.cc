// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/browser_app_shelf_controller.h"

#include <memory>

#include "ash/public/cpp/window_properties.h"
#include "base/debug/dump_without_crashing.h"
#include "base/strings/string_util.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_registry.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ui/ash/shelf/browser_app_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_item_factory.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "chrome/browser/web_applications/web_app_utils.h"
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

std::string BrowserAppIdForWindow(aura::Window* window) {
  return crosapi::browser_util::IsLacrosWindow(window)
             ? app_constants::kLacrosAppId
             : app_constants::kChromeAppId;
}

}  // namespace

BrowserAppShelfController::BrowserAppShelfController(
    Profile* profile,
    apps::BrowserAppInstanceRegistry& browser_app_instance_registry,
    ash::ShelfModel& model,
    ChromeShelfItemFactory& shelf_item_factory,
    ShelfSpinnerController& shelf_spinner_controller)
    : profile_(profile),
      model_(model),
      shelf_item_factory_(shelf_item_factory),
      shelf_spinner_controller_(shelf_spinner_controller),
      browser_app_instance_registry_(browser_app_instance_registry) {
  CHECK(web_app::IsWebAppsCrosapiEnabled());
  registry_observation_.Observe(&*browser_app_instance_registry_);
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
          ? browser_app_instance_registry_->IsLacrosBrowserRunning()
          : browser_app_instance_registry_->IsAshBrowserRunning();
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
      shelf_spinner_controller_->CloseSpinner(instance.app_id);
      CreateOrUpdateShelfItem(id, ash::STATUS_RUNNING);
      break;
    }
    case apps::BrowserAppInstance::Type::kAppTab:
      // New shelf item is not automatically created for unpinned tabbed apps.
      if (const ash::ShelfItem* item = model_->ItemByID(id)) {
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
  if (!browser_app_instance_registry_->IsAppRunning(instance.app_id)) {
    ash::ShelfID id(instance.app_id);
    SetShelfItemClosed(id);
  }
}

void BrowserAppShelfController::ShelfItemAdded(int index) {
  const ash::ShelfItem& item = model_->items()[index];
  const std::string& app_id = item.id.app_id;
  if (!BrowserAppShelfControllerShouldHandleApp(app_id, profile_)) {
    return;
  }
  bool running = (app_id == app_constants::kLacrosAppId)
                     ? browser_app_instance_registry_->IsLacrosBrowserRunning()
                     : browser_app_instance_registry_->IsAppRunning(app_id);
  UpdateShelfItemStatus(item,
                        running ? ash::STATUS_RUNNING : ash::STATUS_CLOSED);
  MaybeUpdateWindowPropertiesForApp(app_id);
}

void BrowserAppShelfController::UpdateShelfItemStatus(
    const ash::ShelfItem& item,
    ash::ShelfItemStatus status) {
  auto new_item = item;
  new_item.status = status;
  model_->Set(model_->ItemIndexByID(item.id), new_item);
}

void BrowserAppShelfController::CreateOrUpdateShelfItem(
    const ash::ShelfID& id,
    ash::ShelfItemStatus status) {
  const ash::ShelfItem* item = model_->ItemByID(id);
  if (item) {
    UpdateShelfItemStatus(*item, status);
    return;
  }

  std::unique_ptr<ash::ShelfItemDelegate> delegate =
      shelf_item_factory_->CreateShelfItemDelegateForAppId(id.app_id);
  std::unique_ptr<ash::ShelfItem> new_item =
      shelf_item_factory_->CreateShelfItemForApp(id, status, ash::TYPE_APP,
                                                 /*title=*/std::u16string());
  model_->AddAt(model_->item_count(), *new_item, std::move(delegate));
}

void BrowserAppShelfController::SetShelfItemClosed(const ash::ShelfID& id) {
  const ash::ShelfItem* item = model_->ItemByID(id);
  if (!item) {
    // There is no shelf item for unpinned apps running in a browser tab.
    return;
  }

  if (ash::IsPinnedShelfItemType(item->type)) {
    UpdateShelfItemStatus(*item, ash::STATUS_CLOSED);
  } else {
    int index = model_->ItemIndexByID(id);
    model_->RemoveItemAt(index);
  }
}

void BrowserAppShelfController::MaybeUpdateWindowProperties(
    aura::Window* window) {
  // App ID of a window is set to the ID of the app active in this window:
  // 1) for app windows, it's the ID of the app running in this window,
  // 2) for regular tabbed browser windows, it's the ID of the app running in
  //    the active tab of this window. If there is no app in the active tab of a
  //    browser window, the window's app ID is set to the ID of the browser
  //    itself (Ash or Lacros).
  //
  // Shelf ID of a window is set to the ID of the shelf item the app instance
  // running in this window maps to. This is usually the same as window's app
  // ID, except for the cases where the active instance has no shelf item (apps
  // configured to open in a tab that don't have a pinned shelf item): in this
  // case, the window's shelf ID is set to the ID of the browser itself (Ash or
  // Lacros).

  std::string app_id;
  ash::ShelfID shelf_id;

  const apps::BrowserAppInstance* active_instance =
      browser_app_instance_registry_->FindAppInstanceIf(
          [window](const apps::BrowserAppInstance& instance) {
            return instance.window == window && instance.is_web_contents_active;
          });
  if (active_instance) {
    app_id = active_instance->app_id;
    if (const ash::ShelfItem* item = model_->ItemByID(ash::ShelfID(app_id))) {
      shelf_id = item->id;
    }
  } else {
    app_id = BrowserAppIdForWindow(window);
  }
  if (shelf_id.IsNull()) {
    shelf_id = ash::ShelfID(BrowserAppIdForWindow(window));
  }
  MaybeUpdateStringProperty(window, ash::kAppIDKey, app_id);
  MaybeUpdateStringProperty(window, ash::kShelfIDKey, shelf_id.Serialize());
}

void BrowserAppShelfController::MaybeUpdateWindowPropertiesForApp(
    const std::string& app_id) {
  std::set<const apps::BrowserAppInstance*> instances =
      browser_app_instance_registry_->SelectAppInstances(
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
