// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_window_shelf_item_controller.h"

#include <iterator>
#include <utility>

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/wm/window_animations.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ui/ash/shelf/app_window_base.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_context_menu.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chromeos/ui/wm/desks/desks_helper.h"
#include "components/app_constants/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/wm/core/window_util.h"

namespace {

// Activates |app_window|. If |allow_minimize| is true and the system allows it,
// the the window will get minimized instead.
// Returns the action performed. Should be one of SHELF_ACTION_NONE,
// SHELF_ACTION_WINDOW_ACTIVATED, or SHELF_ACTION_WINDOW_MINIMIZED.
ash::ShelfAction ShowAndActivateOrMinimize(ui::BaseWindow* app_window,
                                           bool allow_minimize) {
  // Either show or minimize windows when shown from the shelf.
  return ChromeShelfController::instance()->ActivateWindowOrMinimizeIfActive(
      app_window, allow_minimize);
}

// Activate the given |window_to_show|, or - if already selected - advance to
// the next window of similar type using the given |windows| list.
// Returns the action performed. Should be one of SHELF_ACTION_NONE,
// SHELF_ACTION_WINDOW_ACTIVATED, or SHELF_ACTION_WINDOW_MINIMIZED.
ash::ShelfAction ActivateOrAdvanceToNextAppWindow(
    AppWindowBase* window_to_show,
    const AppWindowShelfItemController::WindowList& windows) {
  DCHECK(window_to_show);

  auto i = base::ranges::find(windows, window_to_show);
  if (i != windows.end()) {
    if (++i != windows.end())
      window_to_show = *i;
    else
      window_to_show = windows.front();
  }
  if (window_to_show->IsActive()) {
    // Coming here, only a single window is active. For keyboard activations
    // the window gets animated.
    ash::BounceWindow(window_to_show->GetNativeWindow());
  } else {
    return ShowAndActivateOrMinimize(window_to_show, windows.size() == 1);
  }
  return ash::SHELF_ACTION_NONE;
}

// Launches a new lacros window if there isn't already one on the active desk,
// or the icon is clicked with CTRL.
bool ShouldLaunchNewLacrosWindow(
    const ui::Event& event,
    const std::list<raw_ptr<AppWindowBase, CtnExperimental>>& app_windows) {
  // If the icon is clicked with holding the CTRL, launch a new window.
  if (event.IsControlDown())
    return true;

  // Do not launch a new window if there is already a lacros window on the
  // current desk.
  for (AppWindowBase* window : app_windows) {
    aura::Window* aura_window = window->GetNativeWindow();
    if (crosapi::browser_util::IsLacrosWindow(aura_window) &&
        chromeos::DesksHelper::Get(aura_window)
            ->BelongsToActiveDesk(aura_window)) {
      return false;
    }
  }

  return true;
}

}  // namespace

AppWindowShelfItemController::AppWindowShelfItemController(
    const ash::ShelfID& shelf_id)
    : ash::ShelfItemDelegate(shelf_id) {}

AppWindowShelfItemController::~AppWindowShelfItemController() {
  WindowList windows(windows_);
  for (AppWindowBase* window : hidden_windows_) {
    windows.push_back(window);
  }

  for (AppWindowBase* window : windows) {
    window->SetController(nullptr);
  }
}

void AppWindowShelfItemController::AddWindow(AppWindowBase* app_window) {
  aura::Window* window = app_window->GetNativeWindow();
  if (window && !observed_windows_.IsObservingSource(window))
    observed_windows_.AddObservation(window);
  if (window && window->GetProperty(ash::kHideInShelfKey))
    hidden_windows_.push_front(app_window);
  else
    windows_.push_front(app_window);
  UpdateShelfItemIcon();
}

AppWindowShelfItemController::WindowList::iterator
AppWindowShelfItemController::GetFromNativeWindow(aura::Window* window,
                                                  WindowList& list) {
  return base::ranges::find(list, window, &AppWindowBase::GetNativeWindow);
}

void AppWindowShelfItemController::RemoveWindow(AppWindowBase* app_window) {
  DCHECK(app_window);
  aura::Window* window = app_window->GetNativeWindow();
  if (window && observed_windows_.IsObservingSource(window))
    observed_windows_.RemoveObservation(window);
  if (app_window == last_active_window_)
    last_active_window_ = nullptr;
  auto iter = base::ranges::find(windows_, app_window);
  if (iter != windows_.end()) {
    windows_.erase(iter);
  } else {
    iter = base::ranges::find(hidden_windows_, app_window);
    if (iter == hidden_windows_.end())
      return;
    hidden_windows_.erase(iter);
  }
  UpdateShelfItemIcon();
}

AppWindowBase* AppWindowShelfItemController::GetAppWindow(aura::Window* window,
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

void AppWindowShelfItemController::SetActiveWindow(aura::Window* window) {
  // If the window is hidden, do not set it as last_active_window
  AppWindowBase* app_window = GetAppWindow(window, false);
  if (app_window)
    last_active_window_ = app_window;
  UpdateShelfItemIcon();
}

AppWindowShelfItemController*
AppWindowShelfItemController::AsAppWindowShelfItemController() {
  return this;
}

void AppWindowShelfItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback,
    const ItemFilterPredicate& filter_predicate) {
  WindowList filtered_windows;
  for (AppWindowBase* window : windows_) {
    if (filter_predicate.is_null() ||
        filter_predicate.Run(window->GetNativeWindow())) {
      filtered_windows.push_back(window);
    }
  }

  if (filtered_windows.empty()) {
    std::move(callback).Run(ash::SHELF_ACTION_NONE, {});
    return;
  }

  // If this app is the lacros browser, create a new window if there isn't a
  // lacros window on the current workspace, or the icon is clicked with CTRL.
  // Otherwise, fallthrough to minimize or activate or advance.
  // TODO(sammiequon): This feature should only be for lacros browser and not
  // lacros PWAs. Revisit when there is a way to differentiate the two.
  if (app_id() == app_constants::kLacrosAppId &&
      ShouldLaunchNewLacrosWindow(*event, filtered_windows)) {
    crosapi::BrowserManager::Get()->NewWindow(
        /*incognito=*/false, /*should_trigger_session_restore=*/true);
    std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});
    return;
  }

  auto* last_active = last_active_window_.get();
  if (last_active && !filter_predicate.is_null() &&
      !filter_predicate.Run(last_active->GetNativeWindow())) {
    last_active = nullptr;
  }

  AppWindowBase* window_to_show =
      last_active ? last_active : filtered_windows.front().get();
  // If the event was triggered by a keystroke, we try to advance to the next
  // item if the window we are trying to activate is already active.
  ash::ShelfAction action = ash::SHELF_ACTION_NONE;
  if (filtered_windows.size() >= 1 && window_to_show->IsActive() && event &&
      event->type() == ui::EventType::kKeyReleased) {
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
AppWindowShelfItemController::GetAppMenuItems(
    int event_flags,
    const ItemFilterPredicate& filter_predicate) {
  AppMenuItems items;
  std::u16string app_title = ShelfControllerHelper::GetAppTitle(
      ChromeShelfController::instance()->profile(), app_id());
  int command_id = -1;
  for (const AppWindowBase* it : windows()) {
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

void AppWindowShelfItemController::GetContextMenu(
    int64_t display_id,
    GetContextMenuCallback callback) {
  ChromeShelfController* controller = ChromeShelfController::instance();
  const ash::ShelfItem* item = controller->GetItem(shelf_id());
  context_menu_ = ShelfContextMenu::Create(controller, item, display_id);
  context_menu_->GetMenuModel(std::move(callback));
}

void AppWindowShelfItemController::Close() {
  for (AppWindowBase* window : windows_) {
    window->Close();
  }
  for (AppWindowBase* window : hidden_windows_) {
    window->Close();
  }
}

void AppWindowShelfItemController::ActivateIndexedApp(size_t index) {
  if (index >= windows_.size())
    return;
  auto it = windows_.begin();
  std::advance(it, index);
  ShowAndActivateOrMinimize(*it, /*allow_minimize=*/windows_.size() == 1);
}

void AppWindowShelfItemController::OnWindowPropertyChanged(aura::Window* window,
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
    ChromeShelfController::instance()->SetItemStatus(shelf_id(), status);
  } else if (key == aura::client::kAppIconKey) {
    UpdateShelfItemIcon();
  } else if (key == ash::kHideInShelfKey) {
    UpdateWindowInLists(window);
  }
}

AppWindowBase* AppWindowShelfItemController::GetLastActiveWindow() {
  if (last_active_window_)
    return last_active_window_;
  if (windows_.empty())
    return nullptr;
  return windows_.front();
}

void AppWindowShelfItemController::UpdateShelfItemIcon() {
  // Set the shelf item icon from the kAppIconKey property of the current
  // (or most recently) active window. If there is no valid icon, ask
  // ChromeShelfController to update the icon.
  const gfx::ImageSkia* app_icon = nullptr;
  AppWindowBase* last_active_window = GetLastActiveWindow();
  if (last_active_window && last_active_window->GetNativeWindow()) {
    app_icon = last_active_window->GetNativeWindow()->GetProperty(
        aura::client::kAppIconKey);
  }
  // TODO(khmel): Remove using image_set_by_controller
  if (app_icon && !app_icon->isNull() &&
      ChromeShelfController::instance()->GetItem(shelf_id())) {
    set_image_set_by_controller(true);
    ChromeShelfController::instance()->SetItemImage(shelf_id(), *app_icon);
  } else if (image_set_by_controller()) {
    set_image_set_by_controller(false);
    ChromeShelfController::instance()->UpdateItemImage(shelf_id().app_id);
  }
}

void AppWindowShelfItemController::UpdateWindowInLists(aura::Window* window) {
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

void AppWindowShelfItemController::ExecuteCommand(bool from_context_menu,
                                                  int64_t command_id,
                                                  int32_t event_flags,
                                                  int64_t display_id) {
  DCHECK(!from_context_menu);

  ActivateIndexedApp(command_id);
}
