// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/standalone_browser_extension_app_shelf_item_controller.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps_factory.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/app_window_base.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/standalone_browser_extension_app_context_menu.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/widget/widget.h"

StandaloneBrowserExtensionAppShelfItemController::
    StandaloneBrowserExtensionAppShelfItemController(
        const ash::ShelfID& shelf_id)
    : ash::ShelfItemDelegate(shelf_id) {
  shelf_model_observation_.Observe(ash::ShelfModel::Get());

  auto* activation_client =
      wm::GetActivationClient(ash::Shell::Get()->GetPrimaryRootWindow());
  if (activation_client)
    activation_client_observation_.Observe(activation_client);

  // Lacros is mutually exclusive with multi-signin. As such, there can only be
  // a single ash profile active. We grab it from the profile manager.
  apps::AppServiceProxy* proxy = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetPrimaryUserProfile());

  icon_loader_releaser_ = proxy->LoadIconWithIconEffects(
      shelf_id.app_id, apps::IconEffects::kNone, apps::IconType::kStandard,
      /*size_hint_in_dip=*/48,
      /*allow_placeholder_icon=*/false,
      base::BindOnce(
          &StandaloneBrowserExtensionAppShelfItemController::OnLoadIcon,
          weak_factory_.GetWeakPtr()));

  context_menu_ = std::make_unique<StandaloneBrowserExtensionAppContextMenu>(
      shelf_id.app_id,
      StandaloneBrowserExtensionAppContextMenu::Source::kShelf);
}

StandaloneBrowserExtensionAppShelfItemController::
    StandaloneBrowserExtensionAppShelfItemController(
        const ash::ShelfID& shelf_id,
        aura::Window* window)
    : StandaloneBrowserExtensionAppShelfItemController(shelf_id) {
  // We intentionally avoid going through StartTrackingInstance since no item
  // exists in the shelf yet.
  windows_.push_back(window);
  InitWindowStatus(window);
  window_observations_.AddObservation(window);
}

StandaloneBrowserExtensionAppShelfItemController::
    ~StandaloneBrowserExtensionAppShelfItemController() {}

void StandaloneBrowserExtensionAppShelfItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback,
    const ItemFilterPredicate& filter_predicate) {
  // The behavior of this function matches the behavior of other shelf apps.
  // This is not specific anywhere, so we record it here:
  // First we filter matching windows -- this trims windows on inactive desks.
  // If there's no matching windows, then we launch the app.
  // If there's 1, we focus or minimize it.
  // If there's more than 1, we create menu items and pass them back in
  // ItemSelectedCallback, to be shown by the shelf view as a context menu.
  //
  // We currently do not implement any functionality akin to
  // ActivateOrAdvanceToNextBrowser() as it's not clear how that logic can be
  // triggered.
  //
  // We intentionally do not special case logic for source !=
  // ash::LAUNCH_FROM_SHELF since that path is never triggered.
  DCHECK_EQ(source, ash::LAUNCH_FROM_SHELF);

  std::vector<raw_ptr<aura::Window, VectorExperimental>> filtered_windows;
  for (aura::Window* window : windows_) {
    if (filter_predicate.is_null() || filter_predicate.Run(window)) {
      filtered_windows.push_back(window);
    }
  }

  if (filtered_windows.size() == 0) {
    apps::AppServiceProxy* proxy = apps::AppServiceProxyFactory::GetForProfile(
        ProfileManager::GetPrimaryUserProfile());
    proxy->Launch(app_id(), event->flags(),
                  ShelfLaunchSourceToAppsLaunchSource(source),
                  /*window_info=*/nullptr);

    std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});
    return;
  }

  if (filtered_windows.size() == 1) {
    views::Widget* widget =
        views::Widget::GetWidgetForNativeWindow(filtered_windows[0]);
    AppWindowBase app_window(shelf_id(), widget);
    ash::ShelfAction action =
        ChromeShelfController::instance()->ActivateWindowOrMinimizeIfActive(
            &app_window, /*can_minimize=*/true);
    std::move(callback).Run(action, {});
    return;
  }

  // Show a context menu. This is done by passing back items in callback
  // alongside SHELF_ACTION_NONE.
  context_menu_windows_ = filtered_windows;
  AppMenuItems items;
  for (aura::Window* window : filtered_windows) {
    // The command id is the index into the array of filtered windows.
    items.push_back({/*command_id=*/static_cast<int>(items.size()),
                     window->GetTitle(),
                     icon_ ? icon_.value() : gfx::ImageSkia()});
  }
  std::move(callback).Run(ash::SHELF_ACTION_NONE, std::move(items));
}

void StandaloneBrowserExtensionAppShelfItemController::GetContextMenu(
    int64_t display_id,
    GetContextMenuCallback callback) {
  context_menu_->GetMenuModel(std::move(callback));
}

void StandaloneBrowserExtensionAppShelfItemController::ExecuteCommand(
    bool from_context_menu,
    int64_t command_id,
    int32_t event_flags,
    int64_t display_id) {
  // The current API for showing existing windows in a context menu, and then
  // later receiving a callback here is intrinsically racy. There is no way to
  // encode all relevant information in |command_id|.
  // We do our best by recording the last context menu shown, and then tracking
  // the aura::Windows for destruction. This should almost always work. In rare
  // edge cases, this will cause the wrong window to be selected, but will not
  // cause undefined behavior.
  if (command_id >= 0 &&
      command_id < base::checked_cast<int32_t>(context_menu_windows_.size())) {
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
        context_menu_windows_[command_id]);
    AppWindowBase app_window(shelf_id(), widget);
    ChromeShelfController::instance()->ActivateWindowOrMinimizeIfActive(
        &app_window, /*can_minimize=*/false);
    return;
  }

  // This must be from the context menu, or there has been a race condition and
  // context_menu_windows_ is smaller than it used to be. Either way forward the
  // command.
  context_menu_->ExecuteCommand(command_id, event_flags);
}

void StandaloneBrowserExtensionAppShelfItemController::Close() {
  // There can only be a single active ash profile when Lacros is running.
  apps::AppServiceProxy* proxy = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetPrimaryUserProfile());
  proxy->StopApp(app_id());
}

void StandaloneBrowserExtensionAppShelfItemController::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* new_active,
    aura::Window* old_active) {
  SetWindowActivated(old_active, /*is_active=*/false);
  SetWindowActivated(new_active, /*is_active=*/true);
}

void StandaloneBrowserExtensionAppShelfItemController::ShelfItemAdded(
    int index) {
  ShelfItem item = ash::ShelfModel::Get()->items()[index];
  if (item.id != shelf_id())
    return;

  // When the item is first added, if the running state was not set, set it.
  if (item.type == ash::TYPE_UNDEFINED) {
    item.type = windows_.empty() ? ash::TYPE_PINNED_APP : ash::TYPE_APP;
  }
  item.status = windows_.empty() ? ash::STATUS_CLOSED : ash::STATUS_RUNNING;

  if (icon_) {
    item.image = icon_.value();
  }

  // TODO(crbug.com/40188614): title, policy_pinned_state

  ash::ShelfModel::Get()->Set(index, item);

  // Now that the initial state is set the shelf model observation is no longer
  // required.
  shelf_model_observation_.Reset();
}

void StandaloneBrowserExtensionAppShelfItemController::StartTrackingInstance(
    aura::Window* window) {
  windows_.push_back(window);
  InitWindowStatus(window);
  window_observations_.AddObservation(window);

  if (windows_.size() == 1) {
    int index = GetShelfIndex();

    ShelfItem item = ash::ShelfModel::Get()->items()[index];
    // No other entity should be setting the RUNNING status for apps, since no
    // other entity knows about the active instances.
    DCHECK_NE(item.status, ash::STATUS_RUNNING);
    item.status = ash::STATUS_RUNNING;
    ash::ShelfModel::Get()->Set(index, item);
  }
}

size_t StandaloneBrowserExtensionAppShelfItemController::WindowCount() {
  return windows_.size();
}

void StandaloneBrowserExtensionAppShelfItemController::OnLoadIcon(
    apps::IconValuePtr icon_value) {
  if (!icon_value || icon_value->icon_type != apps::IconType::kStandard)
    return;

  icon_ = icon_value->uncompressed;

  if (ItemAddedToShelf()) {
    int index = GetShelfIndex();
    ShelfItem item = ash::ShelfModel::Get()->items()[index];
    item.image = icon_value->uncompressed;
    ash::ShelfModel::Get()->Set(index, item);
    return;
  }
}

void StandaloneBrowserExtensionAppShelfItemController::
    OnWindowVisibilityChanged(aura::Window* window, bool visible) {
  auto it = window_status_.find(window);
  if (it == window_status_.end())
    return;

  if (visible) {
    it->second = static_cast<apps::InstanceState>(
        it->second | apps::InstanceState::kVisible);
  } else {
    it->second = static_cast<apps::InstanceState>(
        it->second & ~apps::InstanceState::kVisible);
  }
  UpdateInstance(window, it->second);
}

void StandaloneBrowserExtensionAppShelfItemController::OnWindowDestroying(
    aura::Window* window) {
  size_t erased = std::erase(windows_, window);
  DCHECK_EQ(erased, 1u);
  window_observations_.RemoveObservation(window);

  // If a window is destroyed, also remove it from the list used to show context
  // menu items.
  std::erase(context_menu_windows_, window);

  // Remove `window` from InstanceRegistry.
  UpdateInstance(window, apps::InstanceState::kDestroyed);
  window_status_.erase(window);

  // There are still instances left. Nothing to change.
  if (windows_.size() != 0)
    return;

  // If this was the last instance, and the item is not pinned, remove it.
  int index = GetShelfIndex();
  ShelfItem item = ash::ShelfModel::Get()->items()[index];
  if (!ash::IsPinnedShelfItemType(item.type)) {
    // Remove the item. That will also invoke destruction of |this|.
    ash::ShelfModel::Get()->RemoveItemAt(index);
    return;
  }

  // The item was pinned. Update its status.
  DCHECK_NE(item.status, ash::STATUS_CLOSED);
  item.status = ash::STATUS_CLOSED;
  ash::ShelfModel::Get()->Set(index, item);
}

void StandaloneBrowserExtensionAppShelfItemController::SetWindowActivated(
    aura::Window* window,
    bool is_active) {
  auto it = window_status_.find(window);
  if (it == window_status_.end())
    return;

  if (is_active) {
    it->second = static_cast<apps::InstanceState>(it->second |
                                                  apps::InstanceState::kActive);
  } else {
    it->second = static_cast<apps::InstanceState>(
        it->second & ~apps::InstanceState::kActive);
  }

  UpdateInstance(window, it->second);
}

void StandaloneBrowserExtensionAppShelfItemController::InitWindowStatus(
    aura::Window* window) {
  apps::InstanceState state = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  if (window->IsVisible()) {
    state =
        static_cast<apps::InstanceState>(state | apps::InstanceState::kVisible);
  }
  if (window == ash::window_util::GetActiveWindow()) {
    state =
        static_cast<apps::InstanceState>(state | apps::InstanceState::kActive);
  }
  window_status_[window] = state;
  UpdateInstance(window, state);

  // Also update the window properties in exo.
  window->SetProperty(ash::kAppIDKey, shelf_id().app_id);
  window->SetProperty(ash::kShelfIDKey, shelf_id().Serialize());
}

void StandaloneBrowserExtensionAppShelfItemController::UpdateInstance(
    aura::Window* window,
    apps::InstanceState instance_state) {
  if (!crosapi::browser_util::IsLacrosChromeAppsEnabled() || !window)
    return;

  auto* app_service_proxy = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetPrimaryUserProfile());
  DCHECK(app_service_proxy);

  auto& instance_registry = app_service_proxy->InstanceRegistry();

  // If the current state is not changed, we don't need to update.
  if (instance_registry.GetState(window) == instance_state)
    return;

  apps::InstanceParams params(shelf_id().app_id, window);
  params.state = std::make_pair(instance_state, base::Time::Now());
  instance_registry.CreateOrUpdateInstance(std::move(params));
}

int StandaloneBrowserExtensionAppShelfItemController::GetShelfIndex() {
  DCHECK(ItemAddedToShelf());
  int index = ash::ShelfModel::Get()->ItemIndexByID(shelf_id());

  // This instance exists if and only if there's an entry in the shelf.
  DCHECK_GE(index, 0);

  return index;
}

bool StandaloneBrowserExtensionAppShelfItemController::ItemAddedToShelf() {
  return !shelf_model_observation_.IsObserving();
}
