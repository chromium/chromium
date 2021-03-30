// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/launcher/app_window_base.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "chrome/browser/ui/ash/launcher/shelf_context_menu.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/wm/core/window_util.h"

namespace {

// Activates |app_window|. If |allow_minimize| is true and the system allows it,
// the the window will get minimized instead.
// Returns the action performed. Should be one of SHELF_ACTION_NONE,
// SHELF_ACTION_WINDOW_ACTIVATED, or SHELF_ACTION_WINDOW_MINIMIZED.
ash::ShelfAction ShowAndActivateOrMinimize(ui::BaseWindow* app_window,
                                           bool allow_minimize) {
  // Either show or minimize windows when shown from the launcher.
  return ChromeLauncherController::instance()->ActivateWindowOrMinimizeIfActive(
      app_window, allow_minimize);
}

// Activate the given |window_to_show|, or - if already selected - advance to
// the next window of similar type using the given |windows| list.
// Returns the action performed. Should be one of SHELF_ACTION_NONE,
// SHELF_ACTION_WINDOW_ACTIVATED, or SHELF_ACTION_WINDOW_MINIMIZED.
ash::ShelfAction ActivateOrAdvanceToNextAppWindow(
    AppWindowBase* window_to_show,
    const AppWindowLauncherItemController::WindowList& windows) {
  DCHECK(window_to_show);

  auto i = std::find(windows.begin(), windows.end(), window_to_show);
  if (i != windows.end()) {
    if (++i != windows.end())
      window_to_show = *i;
    else
      window_to_show = windows.front();
  }
  if (window_to_show->IsActive()) {
    // Coming here, only a single window is active. For keyboard activations
    // the window gets animated.
    ash_util::BounceWindow(window_to_show->GetNativeWindow());
  } else {
    return ShowAndActivateOrMinimize(window_to_show, windows.size() == 1);
  }
  return ash::SHELF_ACTION_NONE;
}

}  // namespace

AppWindowLauncherItemController::AppWindowLauncherItemController(
    const ash::ShelfID& shelf_id)
    : ash::ShelfItemDelegate(shelf_id) {}

AppWindowLauncherItemController::~AppWindowLauncherItemController() {
  WindowList windows(windows_);
  for (auto* window : hidden_windows_)
    windows.push_back(window);

  for (auto* window : windows)
    window->SetController(nullptr);
}

void AppWindowLauncherItemController::AddWindow(AppWindowBase* app_window) {
  aura::Window* window = app_window->GetNativeWindow();
  if (window && !observed_windows_.IsObserving(window))
    observed_windows_.Add(window);
  if (window && window->GetProperty(ash::kHideInShelfKey))
    hidden_windows_.push_front(app_window);
  else
    windows_.push_front(app_window);
  UpdateShelfItemIcon();
}

AppWindowLauncherItemController::WindowList::iterator
AppWindowLauncherItemController::GetFromNativeWindow(aura::Window* window,
                                                     WindowList& list) {
  return std::find_if(list.begin(), list.end(),
                      [window](AppWindowBase* base_window) {
                        return base_window->GetNativeWindow() == window;
                      });
}

void AppWindowLauncherItemController::RemoveWindow(AppWindowBase* app_window) {
  DCHECK(app_window);
  aura::Window* window = app_window->GetNativeWindow();
  if (window && observed_windows_.IsObserving(window))
    observed_windows_.Remove(window);
  if (app_window == last_active_window_)
    last_active_window_ = nullptr;
  auto iter = std::find(windows_.begin(), windows_.end(), app_window);
  if (iter != windows_.end()) {
    windows_.erase(iter);
  } else {
    iter =
        std::find(hidden_windows_.begin(), hidden_windows_.end(), app_window);
    if (iter == hidden_windows_.end())
      return;
    hidden_windows_.erase(iter);
  }
  UpdateShelfItemIcon();
}

AppWindowBase* AppWindowLauncherItemController::GetAppWindow(
    aura::Window* window,
    bool include_hidden) {
  auto iter = GetFromNativeWindow(window, windows_);
  if (iter != windows_.end())
    return *iter;
  if (include_hidden) {
    iter = GetFromNativeWindow(window, hidden_windows_);
    if (iter != hidden_windows_.end())
      return *iter;
  }
  return nullptr;
}

void AppWindowLauncherItemController::SetActiveWindow(aura::Window* window) {
  // If the window is hidden, do not set it as last_active_window
  AppWindowBase* app_window = GetAppWindow(window, false);
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
    ItemSelectedCallback callback,
    const ItemFilterPredicate& filter_predicate) {
  WindowList filtered_windows;
  for (auto* window : windows_) {
    if (filter_predicate.is_null() ||
        filter_predicate.Run(window->GetNativeWindow())) {
      filtered_windows.push_back(window);
    }
  }

  if (filtered_windows.empty()) {
    std::move(callback).Run(ash::SHELF_ACTION_NONE, {});
    return;
  }

  auto* last_active = last_active_window_;
  if (last_active && !filter_predicate.is_null() &&
      !filter_predicate.Run(last_active->GetNativeWindow())) {
    last_active = nullptr;
  }

  AppWindowBase* window_to_show =
      last_active ? last_active : filtered_windows.front();
  // If the event was triggered by a keystroke, we try to advance to the next
  // item if the window we are trying to activate is already active.
  ash::ShelfAction action = ash::SHELF_ACTION_NONE;
  if (filtered_windows.size() >= 1 && window_to_show->IsActive() && event &&
      event->type() == ui::ET_KEY_RELEASED) {
    action = ActivateOrAdvanceToNextAppWindow(window_to_show, filtered_windows);
  } else if (filtered_windows.size() <= 1 || source != ash::LAUNCH_FROM_SHELF) {
    action = ShowAndActivateOrMinimize(
        window_to_show, /*allow_minimize=*/filtered_windows.size() == 1);
  } else {
    // Do nothing if multiple windows are available when launching from shelf -
    // the shelf will show a context menu with available windows.
    action = ash::SHELF_ACTION_NONE;
  }

  std::move(callback).Run(
      action,
      GetAppMenuItems(event ? event->flags() : ui::EF_NONE, filter_predicate));
}

ash::ShelfItemDelegate::AppMenuItems
AppWindowLauncherItemController::GetAppMenuItems(
    int event_flags,
    const ItemFilterPredicate& filter_predicate) {
  AppMenuItems items;
  std::u16string app_title = LauncherControllerHelper::GetAppTitle(
      ChromeLauncherController::instance()->profile(), app_id());
  int command_id = -1;
  for (const auto* it : windows()) {
    ++command_id;
    aura::Window* window = it->GetNativeWindow();
    // Can window be null?
    if (!filter_predicate.is_null() && !filter_predicate.Run(window))
      continue;

    auto title = (window && !window->GetTitle().empty()) ? window->GetTitle()
                                                         : app_title;
    gfx::ImageSkia image;
    if (window) {
      // Prefer the smaller window icon because that fits better inside a menu.
      const gfx::ImageSkia* icon =
          window->GetProperty(aura::client::kWindowIconKey);
      if (!icon || icon->isNull()) {
        // Fall back to the larger app icon.
        icon = window->GetProperty(aura::client::kAppIconKey);
      }
      if (icon && !icon->isNull())
        image = *icon;
    }
    items.push_back({command_id, title, image});
  }
  return items;
}

void AppWindowLauncherItemController::GetContextMenu(
    int64_t display_id,
    GetContextMenuCallback callback) {
  ChromeLauncherController* controller = ChromeLauncherController::instance();
  const ash::ShelfItem* item = controller->GetItem(shelf_id());
  context_menu_ = ShelfContextMenu::Create(controller, item, display_id);
  context_menu_->GetMenuModel(std::move(callback));
}

void AppWindowLauncherItemController::Close() {
  for (auto* window : windows_)
    window->Close();
  for (auto* window : hidden_windows_)
    window->Close();
}

void AppWindowLauncherItemController::ActivateIndexedApp(size_t index) {
  if (index >= windows_.size())
    return;
  auto it = windows_.begin();
  std::advance(it, index);
  ShowAndActivateOrMinimize(*it, /*allow_minimize=*/windows_.size() == 1);
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
  } else if (key == ash::kHideInShelfKey) {
    UpdateWindowInLists(window);
  }
}

AppWindowBase* AppWindowLauncherItemController::GetLastActiveWindow() {
  if (last_active_window_)
    return last_active_window_;
  if (windows_.empty())
    return nullptr;
  return windows_.front();
}

void AppWindowLauncherItemController::UpdateShelfItemIcon() {
  // Set the shelf item icon from the kAppIconKey property of the current
  // (or most recently) active window. If there is no valid icon, ask
  // ChromeLauncherController to update the icon.
  const gfx::ImageSkia* app_icon = nullptr;
  AppWindowBase* last_active_window = GetLastActiveWindow();
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

void AppWindowLauncherItemController::UpdateWindowInLists(
    aura::Window* window) {
  if (window->GetProperty(ash::kHideInShelfKey)) {
    // Hide Window:
    auto it = GetFromNativeWindow(window, windows_);
    if (it != windows_.end()) {
      hidden_windows_.push_front(*it);
      windows_.erase(it);
      UpdateShelfItemIcon();
    }
  } else {
    // Unhide window:
    auto it = GetFromNativeWindow(window, hidden_windows_);
    if (it != hidden_windows_.end()) {
      windows_.push_front(*it);
      hidden_windows_.erase(it);
      UpdateShelfItemIcon();
    }
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
