// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/internal_app_window_shelf_controller.h"

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/ash/launcher/app_window_base.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "components/exo/shell_surface_util.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/widget/widget.h"

InternalAppWindowShelfController::InternalAppWindowShelfController(
    ChromeLauncherController* owner)
    : AppWindowLauncherController(owner) {
  aura::Env::GetInstance()->AddObserver(this);
}

InternalAppWindowShelfController::~InternalAppWindowShelfController() {
  for (auto* window : observed_windows_)
    window->RemoveObserver(this);
  aura::Env::GetInstance()->RemoveObserver(this);
}

void InternalAppWindowShelfController::ActiveUserChanged(
    const std::string& user_email) {
  for (const auto& w : aura_window_to_app_window_) {
    AppWindowBase* app_window = w.second.get();
    if (app_window->shelf_id().app_id ==
        ash::kInternalAppIdKeyboardShortcutViewer) {
      continue;
    }

    if (MultiUserWindowManagerHelper::GetWindowManager()
            ->GetWindowOwner(w.first)
            .GetUserEmail() == user_email) {
      AddToShelf(app_window);
    } else {
      RemoveFromShelf(app_window);
    }
  }
}

void InternalAppWindowShelfController::OnWindowInitialized(
    aura::Window* window) {
  // An internal app window has type WINDOW_TYPE_NORMAL, a WindowDelegate and
  // is a top level views widget.
  if (window->type() != aura::client::WINDOW_TYPE_NORMAL || !window->delegate())
    return;
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget || !widget->is_top_level())
    return;

  observed_windows_.push_back(window);
  window->AddObserver(this);
}

void InternalAppWindowShelfController::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key != ash::kShelfIDKey)
    return;

  ash::ShelfID shelf_id =
      ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));
  if (!shelf_id.IsNull() && app_list::IsInternalApp(shelf_id.app_id))
    RegisterAppWindow(window, shelf_id);
}

void InternalAppWindowShelfController::OnWindowVisibilityChanging(
    aura::Window* window,
    bool visible) {
  if (!visible)
    return;

  // Skip OnWindowVisibilityChanged for ancestors/descendants.
  if (!base::Contains(observed_windows_, window))
    return;

  ash::ShelfID shelf_id =
      ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));

  if (shelf_id.IsNull()) {
    if (!plugin_vm::IsPluginVmWindow(window))
      return;
    shelf_id = ash::ShelfID(plugin_vm::kPluginVmAppId);
    window->SetProperty(ash::kShelfIDKey,
                        new std::string(shelf_id.Serialize()));
    window->SetProperty(ash::kAppIDKey, new std::string(shelf_id.app_id));
  }

  if (!app_list::IsInternalApp(shelf_id.app_id))
    return;

  RegisterAppWindow(window, shelf_id);
}

void InternalAppWindowShelfController::OnWindowDestroying(
    aura::Window* window) {
  auto it =
      std::find(observed_windows_.begin(), observed_windows_.end(), window);
  DCHECK(it != observed_windows_.end());
  observed_windows_.erase(it);
  window->RemoveObserver(this);

  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it == aura_window_to_app_window_.end())
    return;

  RemoveFromShelf(app_window_it->second.get());

  aura_window_to_app_window_.erase(app_window_it);
}

void InternalAppWindowShelfController::RegisterAppWindow(
    aura::Window* window,
    const ash::ShelfID& shelf_id) {
  // Skip when this window has been handled. This can happen when the window
  // becomes visible again.
  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it != aura_window_to_app_window_.end())
    return;

  // Prevent InternalAppWindow from showing up after user switch.
  // Keyboard Shortcut Viewer has a global instance so it can be shared with
  // different users.
  const user_manager::User* window_owner = nullptr;
  if (shelf_id.app_id != ash::kInternalAppIdKeyboardShortcutViewer) {
    // Plugin VM is only allowed on the primary profile. If the user switches
    // profile while it is launching, we associate it with the primary profile,
    // which hides the window, and don't show it on the shelf.
    if (shelf_id.app_id == plugin_vm::kPluginVmAppId)
      window_owner = user_manager::UserManager::Get()->GetPrimaryUser();
    else
      window_owner = user_manager::UserManager::Get()->GetActiveUser();

    MultiUserWindowManagerHelper::GetWindowManager()->SetWindowOwner(
        window, window_owner->GetAccountId());
  }

  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  auto app_window_ptr = std::make_unique<AppWindowBase>(shelf_id, widget);
  AppWindowBase* app_window = app_window_ptr.get();
  aura_window_to_app_window_[window] = std::move(app_window_ptr);

  if (window_owner &&
      window_owner != user_manager::UserManager::Get()->GetActiveUser()) {
    return;
  }

  AddToShelf(app_window);
}

void InternalAppWindowShelfController::AddToShelf(AppWindowBase* app_window) {
  const ash::ShelfID shelf_id = app_window->shelf_id();
  // Internal Camera app does not have own window. Either ARC or extension
  // window controller would add window to controller.
  if (shelf_id.app_id == ash::kInternalAppIdCamera)
    return;

  AppWindowLauncherItemController* item_controller =
      owner()->shelf_model()->GetAppWindowLauncherItemController(shelf_id);
  if (item_controller == nullptr) {
    auto controller =
        std::make_unique<AppWindowLauncherItemController>(shelf_id);
    item_controller = controller.get();
    if (!owner()->GetItem(shelf_id)) {
      owner()->CreateAppLauncherItem(std::move(controller),
                                     ash::STATUS_RUNNING);
    } else {
      owner()->shelf_model()->SetShelfItemDelegate(shelf_id,
                                                   std::move(controller));
      owner()->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
    }
  }

  item_controller->AddWindow(app_window);
  app_window->SetController(item_controller);
}

void InternalAppWindowShelfController::RemoveFromShelf(
    AppWindowBase* app_window) {
  const ash::ShelfID shelf_id = app_window->shelf_id();
  // Internal Camera app does not have own window. Either ARC or extension
  // window controller would remove window from controller.
  if (shelf_id.app_id == ash::kInternalAppIdCamera)
    return;

  UnregisterAppWindow(app_window);

  // Check if we may close controller now, at this point we can safely remove
  // controllers without window.
  AppWindowLauncherItemController* item_controller =
      owner()->shelf_model()->GetAppWindowLauncherItemController(
          app_window->shelf_id());

  if (item_controller != nullptr && item_controller->window_count() == 0)
    owner()->CloseLauncherItem(item_controller->shelf_id());
}

void InternalAppWindowShelfController::UnregisterAppWindow(
    AppWindowBase* app_window) {
  if (!app_window)
    return;

  AppWindowLauncherItemController* controller = app_window->controller();
  if (controller)
    controller->RemoveWindow(app_window);

  app_window->SetController(nullptr);
}

AppWindowLauncherItemController*
InternalAppWindowShelfController::ControllerForWindow(aura::Window* window) {
  if (!window)
    return nullptr;

  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it == aura_window_to_app_window_.end())
    return nullptr;

  AppWindowBase* app_window = app_window_it->second.get();
  if (app_window == nullptr)
    return nullptr;

  return app_window->controller();
}

void InternalAppWindowShelfController::OnItemDelegateDiscarded(
    ash::ShelfItemDelegate* delegate) {
  for (auto& it : aura_window_to_app_window_) {
    AppWindowBase* app_window = it.second.get();
    if (!app_window || app_window->controller() != delegate)
      continue;

    VLOG(1) << "Item controller was released externally for the app "
            << delegate->shelf_id().app_id << ".";

    UnregisterAppWindow(it.second.get());
  }
}
