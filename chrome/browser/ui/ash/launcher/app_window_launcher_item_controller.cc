// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/base_window.h"
#include "ui/wm/core/window_util.h"

AppWindowLauncherItemController::AppWindowLauncherItemController(
    const ash::ShelfID& shelf_id)
    : ash::ShelfItemDelegate(shelf_id) {}

AppWindowLauncherItemController::~AppWindowLauncherItemController() {}

void AppWindowLauncherItemController::AddWindow(ui::BaseWindow* app_window) {
  windows_.push_front(app_window);
  aura::Window* window = app_window->GetNativeWindow();
  if (window)
    observed_windows_.Add(window);
  UpdateShelfItemIcon();
}

AppWindowLauncherItemController::WindowList::iterator
AppWindowLauncherItemController::GetFromNativeWindow(aura::Window* window) {
  return std::find_if(windows_.begin(), windows_.end(),
                      [window](ui::BaseWindow* base_window) {
                        return base_window->GetNativeWindow() == window;
                      });
}

void AppWindowLauncherItemController::RemoveWindow(ui::BaseWindow* app_window) {
  DCHECK(app_window);
  aura::Window* window = app_window->GetNativeWindow();
  if (window)
    observed_windows_.Remove(window);
  if (app_window == last_active_window_)
    last_active_window_ = nullptr;
  auto iter = std::find(windows_.begin(), windows_.end(), app_window);
  if (iter == windows_.end()) {
    NOTREACHED();
    return;
  }
  windows_.erase(iter);
  UpdateShelfItemIcon();
}

ui::BaseWindow* AppWindowLauncherItemController::GetAppWindow(
    aura::Window* window) {
  const auto iter = GetFromNativeWindow(window);
  if (iter != windows_.end())
    return *iter;
  return nullptr;
}

void AppWindowLauncherItemController::SetActiveWindow(aura::Window* window) {
  ui::BaseWindow* app_window = GetAppWindow(window);
  if (app_window)
    last_active_window_ = app_window;
  UpdateShelfItemIcon();
}

AppWindowLauncherItemController*
AppWindowLauncherItemController::AsAppWindowLauncherItemController() {
  return this;
}

void AppWindowLauncherItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback) {
  if (windows_.empty()) {
    std::move(callback).Run(ash::SHELF_ACTION_NONE, {});
    return;
  }

  ui::BaseWindow* window_to_show =
      last_active_window_ ? last_active_window_ : windows_.front();
  // If the event was triggered by a keystroke, we try to advance to the next
  // item if the window we are trying to activate is already active.
  ash::ShelfAction action = ash::SHELF_ACTION_NONE;
  if (windows_.size() >= 1 && window_to_show->IsActive() && event &&
      event->type() == ui::ET_KEY_RELEASED) {
    action = ActivateOrAdvanceToNextAppWindow(window_to_show);
  } else {
    action = ShowAndActivateOrMinimize(window_to_show);
  }

  std::move(callback).Run(
      action, GetAppMenuItems(event ? event->flags() : ui::EF_NONE));
}

ash::ShelfItemDelegate::AppMenuItems
AppWindowLauncherItemController::GetAppMenuItems(int event_flags) {
  AppMenuItems items;
  base::string16 app_title = LauncherControllerHelper::GetAppTitle(
      ChromeLauncherController::instance()->profile(), app_id());
  for (const auto* it : windows()) {
    // TODO(khmel): resolve correct icon here.
    aura::Window* window = it->GetNativeWindow();
    auto title = (window && !window->GetTitle().empty()) ? window->GetTitle()
                                                         : app_title;
    items.push_back({title, gfx::ImageSkia()});
  }
  return items;
}

void AppWindowLauncherItemController::GetContextMenu(
    int64_t display_id,
    GetContextMenuCallback callback) {
  ChromeLauncherController* controller = ChromeLauncherController::instance();
  const ash::ShelfItem* item = controller->GetItem(shelf_id());
  context_menu_ = LauncherContextMenu::Create(controller, item, display_id);
  context_menu_->GetMenuModel(std::move(callback));
}

void AppWindowLauncherItemController::Close() {
  for (auto* window : windows_)
    window->Close();
}

void AppWindowLauncherItemController::ActivateIndexedApp(size_t index) {
  if (index >= windows_.size())
    return;
  auto it = windows_.begin();
  std::advance(it, index);
  ShowAndActivateOrMinimize(*it);
}

void AppWindowLauncherItemController::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key == aura::client::kDrawAttentionKey) {
    ash::ShelfItemStatus status;
    // Active windows don't draw attention because the user is looking at them.
    if (window->GetProperty(aura::client::kDrawAttentionKey) &&
        !wm::IsActiveWindow(window)) {
      status = ash::STATUS_ATTENTION;
    } else {
      status = ash::STATUS_RUNNING;
    }
    ChromeLauncherController::instance()->SetItemStatus(shelf_id(), status);
  } else if (key == aura::client::kAppIconKey) {
    UpdateShelfItemIcon();
  }
}

ui::BaseWindow* AppWindowLauncherItemController::GetLastActiveWindow() {
  if (last_active_window_)
    return last_active_window_;
  if (windows_.empty())
    return nullptr;
  return windows_.front();
}

ash::ShelfAction AppWindowLauncherItemController::ShowAndActivateOrMinimize(
    ui::BaseWindow* app_window) {
  // Either show or minimize windows when shown from the launcher.
  return ChromeLauncherController::instance()->ActivateWindowOrMinimizeIfActive(
      app_window, windows().size() == 1);
}

ash::ShelfAction
AppWindowLauncherItemController::ActivateOrAdvanceToNextAppWindow(
    ui::BaseWindow* window_to_show) {
  WindowList::iterator i(
      std::find(windows_.begin(), windows_.end(), window_to_show));
  if (i != windows_.end()) {
    if (++i != windows_.end())
      window_to_show = *i;
    else
      window_to_show = windows_.front();
  }
  if (window_to_show->IsActive()) {
    // Coming here, only a single window is active. For keyboard activations
    // the window gets animated.
    ash_util::BounceWindow(window_to_show->GetNativeWindow());
  } else {
    return ShowAndActivateOrMinimize(window_to_show);
  }
  return ash::SHELF_ACTION_NONE;
}

void AppWindowLauncherItemController::UpdateShelfItemIcon() {
  // Set the shelf item icon from the kAppIconKey property of the current
  // (or most recently) active window. If there is no valid icon, ask
  // ChromeLauncherController to update the icon.
  const gfx::ImageSkia* app_icon = nullptr;
  ui::BaseWindow* last_active_window = GetLastActiveWindow();
  if (last_active_window && last_active_window->GetNativeWindow()) {
    app_icon = last_active_window->GetNativeWindow()->GetProperty(
        aura::client::kAppIconKey);
  }
  // TODO(khmel): Remove using image_set_by_controller
  if (app_icon && !app_icon->isNull()) {
    set_image_set_by_controller(true);
    ChromeLauncherController::instance()->SetLauncherItemImage(shelf_id(),
                                                               *app_icon);
  } else if (image_set_by_controller()) {
    set_image_set_by_controller(false);
    ChromeLauncherController::instance()->UpdateLauncherItemImage(
        shelf_id().app_id);
  }
}

void AppWindowLauncherItemController::ExecuteCommand(bool from_context_menu,
                                                     int64_t command_id,
                                                     int32_t event_flags,
                                                     int64_t display_id) {
  if (from_context_menu && ExecuteContextMenuCommand(command_id, event_flags))
    return;

  ActivateIndexedApp(command_id);
}
