// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/extension_app_window_launcher_controller.h"

#include <memory>

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/extension_app_window_launcher_item_controller.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/base_window.h"
#include "ui/views/widget/widget.h"

using extensions::AppWindow;
using extensions::AppWindowRegistry;

namespace {

// Get the ShelfID for a given |app_window|.
ash::ShelfID GetShelfId(AppWindow* app_window) {
  if (IsCameraApp(app_window->extension_id()))
    return ash::ShelfID(ash::kInternalAppIdCamera);

  // Set launch_id default value to an empty string. If showInShelf parameter
  // is true and the window key is not empty, its value is appended to the
  // launch_id. Otherwise, if the window key is empty, the session_id is used.
  std::string launch_id;
  if (app_window->show_in_shelf()) {
    if (!app_window->window_key().empty())
      launch_id = app_window->window_key();
    else
      launch_id = base::StringPrintf("%d", app_window->session_id().id());
  }
  return ash::ShelfID(app_window->extension_id(), launch_id);
}

}  // namespace

ExtensionAppWindowLauncherController::ExtensionAppWindowLauncherController(
    ChromeLauncherController* owner)
    : AppWindowLauncherController(owner),
      registry_(AppWindowRegistry::Get(owner->profile())) {
  registry_->AddObserver(this);
}

ExtensionAppWindowLauncherController::~ExtensionAppWindowLauncherController() {
  registry_->RemoveObserver(this);

  for (const auto& iter : window_to_shelf_id_map_) {
    iter.first->RemoveObserver(this);
    views::Widget* widget = views::Widget::GetWidgetForNativeView(iter.first);
    DCHECK(widget);  // Extension windows are always backed by Widgets.
    widget->RemoveObserver(this);
  }
}

AppWindowLauncherItemController*
ExtensionAppWindowLauncherController::ControllerForWindow(
    aura::Window* window) {
  const auto window_iter = window_to_shelf_id_map_.find(window);
  if (window_iter == window_to_shelf_id_map_.end())
    return nullptr;
  const auto controller_iter = app_controller_map_.find(window_iter->second);
  if (controller_iter == app_controller_map_.end())
    return nullptr;
  return controller_iter->second;
}

void ExtensionAppWindowLauncherController::OnItemDelegateDiscarded(
    ash::ShelfItemDelegate* delegate) {
  AppControllerMap::iterator it =
      app_controller_map_.find(delegate->shelf_id());
  if (it == app_controller_map_.end() || it->second != delegate)
    return;

  ExtensionAppWindowLauncherItemController* controller = it->second;
  // Existing controller is no longer bound with shelf. We have to clean the
  // cache. See crbug.com/770005.
  VLOG(1) << "Item controller was released externally for the app "
          << delegate->shelf_id().app_id << ".";
  app_controller_map_.erase(it);

  for (ui::BaseWindow* base_window : controller->windows()) {
    aura::Window* window = base_window->GetNativeWindow();
    if (window)
      UnregisterApp(window);
  }
}

void ExtensionAppWindowLauncherController::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  // All work is done in OnWidgetActivationChanged(). This does nothing as the
  // supplied windows are created by ash, which is *not* the same as the windows
  // created by the browser when running in mash.
}

void ExtensionAppWindowLauncherController::OnAppWindowAdded(
    extensions::AppWindow* app_window) {
  RegisterApp(app_window);
}

void ExtensionAppWindowLauncherController::OnAppWindowShown(
    AppWindow* app_window,
    bool was_hidden) {
  aura::Window* window = app_window->GetNativeWindow();
  if (!IsRegisteredApp(window))
    RegisterApp(app_window);
}

void ExtensionAppWindowLauncherController::OnAppWindowHidden(
    AppWindow* app_window) {
  aura::Window* window = app_window->GetNativeWindow();
  if (IsRegisteredApp(window))
    UnregisterApp(window);
}

// Called from aura::Window::~Window(), before delegate_->OnWindowDestroyed()
// which destroys AppWindow, so both |window| and the associated AppWindow
// are valid here.
void ExtensionAppWindowLauncherController::OnWindowDestroying(
    aura::Window* window) {
  UnregisterApp(window);
}

void ExtensionAppWindowLauncherController::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  AppWindowLauncherItemController* controller = nullptr;
  if (active) {
    aura::Window* active_window = widget->GetNativeWindow();
    DCHECK(active_window);
    controller = ControllerForWindow(active_window);
    DCHECK(controller);  // Observer is only added for known controllers.
    controller->SetActiveWindow(active_window);
  }
  if (!active_shelf_id_.IsNull() &&
      (!controller || controller->shelf_id() != active_shelf_id_)) {
    owner()->SetItemStatus(active_shelf_id_, ash::STATUS_RUNNING);
  }
  active_shelf_id_ = controller ? controller->shelf_id() : ash::ShelfID();
}

void ExtensionAppWindowLauncherController::OnWidgetDestroying(
    views::Widget* widget) {
  widget->RemoveObserver(this);
}

void ExtensionAppWindowLauncherController::RegisterApp(AppWindow* app_window) {
  aura::Window* window = app_window->GetNativeWindow();
  const ash::ShelfID shelf_id = GetShelfId(app_window);
  DCHECK(!shelf_id.IsNull());

  window->SetProperty(ash::kAppIDKey, shelf_id.app_id);
  window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
  // TODO(msw): Set shelf item types earlier to avoid ShelfWindowWatcher races.
  window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);

  // Windows created by IME extension should be treated the same way as the
  // virtual keyboard window, which does not register itself in launcher.
  if (app_window->is_ime_window())
    return;

  // Get the app's shelf identifier and add an entry to the map.
  DCHECK_EQ(window_to_shelf_id_map_.count(window), 0u);
  window_to_shelf_id_map_[window] = shelf_id;
  window->AddObserver(this);

  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  DCHECK(widget);  // Extension windows are always backed by Widgets.
  widget->AddObserver(this);

  // Find or create an item controller and launcher item.
  AppControllerMap::iterator app_controller_iter =
      app_controller_map_.find(shelf_id);
  if (app_controller_iter != app_controller_map_.end()) {
    ExtensionAppWindowLauncherItemController* controller =
        app_controller_iter->second;
    DCHECK_EQ(controller->app_id(), shelf_id.app_id);
    controller->AddAppWindow(app_window);
  } else {
    std::unique_ptr<ExtensionAppWindowLauncherItemController> controller =
        std::make_unique<ExtensionAppWindowLauncherItemController>(shelf_id);
    app_controller_map_[shelf_id] = controller.get();

    // Check for any existing pinned shelf item with a matching |shelf_id|.
    if (!owner()->GetItem(shelf_id)) {
      owner()->CreateAppLauncherItem(std::move(controller), ash::STATUS_RUNNING,
                                     app_window->GetTitle());
    } else {
      owner()->shelf_model()->SetShelfItemDelegate(shelf_id,
                                                   std::move(controller));
    }

    // Register the window after a shelf item is created to let the controller
    // set the shelf icon property.
    app_controller_map_[shelf_id]->AddAppWindow(app_window);
  }

  owner()->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
}

void ExtensionAppWindowLauncherController::UnregisterApp(aura::Window* window) {
  const auto window_iter = window_to_shelf_id_map_.find(window);
  DCHECK(window_iter != window_to_shelf_id_map_.end());
  ash::ShelfID shelf_id = window_iter->second;
  window_to_shelf_id_map_.erase(window_iter);
  window->RemoveObserver(this);

  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  DCHECK(widget);  // Extension windows are always backed by Widgets.
  widget->RemoveObserver(this);

  AppControllerMap::iterator app_controller_iter =
      app_controller_map_.find(shelf_id);
  if (app_controller_iter == app_controller_map_.end())
    return;

  ExtensionAppWindowLauncherItemController* controller =
      app_controller_iter->second;

  controller->RemoveWindow(controller->GetAppWindow(window));
  if (controller->window_count() == 0) {
    // If this is the last window associated with the app window shelf id,
    // close the shelf item.
    app_controller_map_.erase(app_controller_iter);
    owner()->CloseLauncherItem(shelf_id);
  }
}

bool ExtensionAppWindowLauncherController::IsRegisteredApp(
    aura::Window* window) {
  return window_to_shelf_id_map_.find(window) != window_to_shelf_id_map_.end();
}
