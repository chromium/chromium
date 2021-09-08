// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/standalone_browser_extension_app_shelf_item_controller.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy_chromeos.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"

StandaloneBrowserExtensionAppShelfItemController::
    StandaloneBrowserExtensionAppShelfItemController(
        const ash::ShelfID& shelf_id)
    : ash::ShelfItemDelegate(shelf_id) {
  shelf_model_observation_.Observe(ash::ShelfModel::Get());

  // Lacros is mutually exclusive with multi-signin. As such, there can only be
  // a single ash profile active. We grab it from the shelf.
  apps::AppServiceProxyChromeOs* proxy =
      apps::AppServiceProxyFactory::GetForProfile(
          ChromeShelfController::instance()->profile());

  apps::mojom::IconKeyPtr icon_key = apps::mojom::IconKey::New();
  constexpr bool kAllowPlaceholderIcon = false;
  constexpr int32_t kIconSize = 48;
  auto icon_type = apps::mojom::IconType::kStandard;
  icon_loader_releaser_ = proxy->LoadIconFromIconKey(
      apps::mojom::AppType::kStandaloneBrowserExtension, shelf_id.app_id,
      std::move(icon_key), icon_type, kIconSize, kAllowPlaceholderIcon,
      base::BindOnce(
          &StandaloneBrowserExtensionAppShelfItemController::DidLoadIcon,
          weak_factory_.GetWeakPtr()));
}

StandaloneBrowserExtensionAppShelfItemController::
    StandaloneBrowserExtensionAppShelfItemController(
        const ash::ShelfID& shelf_id,
        aura::Window* window)
    : StandaloneBrowserExtensionAppShelfItemController(shelf_id) {
  // We intentionally avoid going through StartTrackingInstance since no item
  // exists in the shelf yet.
  windows_.insert(window);
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
  // TODO(https://crbug.com/1225848): implement
}

void StandaloneBrowserExtensionAppShelfItemController::GetContextMenu(
    int64_t display_id,
    GetContextMenuCallback callback) {
  // TODO(https://crbug.com/1225848): implement
}

void StandaloneBrowserExtensionAppShelfItemController::ExecuteCommand(
    bool from_context_menu,
    int64_t command_id,
    int32_t event_flags,
    int64_t display_id) {
  // TODO(https://crbug.com/1225848): implement
}

void StandaloneBrowserExtensionAppShelfItemController::Close() {
  // TODO(https://crbug.com/1225848): implement
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

  // TODO(https://crbug.com/1225848): title, policy_pinned_state

  ash::ShelfModel::Get()->Set(index, item);

  // Now that the initial state is set the shelf model observation is no longer
  // required.
  shelf_model_observation_.Reset();
}

void StandaloneBrowserExtensionAppShelfItemController::StartTrackingInstance(
    aura::Window* window) {
  DCHECK(windows_.find(window) == windows_.end());
  windows_.insert(window);
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

void StandaloneBrowserExtensionAppShelfItemController::DidLoadIcon(
    apps::mojom::IconValuePtr icon_value) {
  if (ItemAddedToShelf()) {
    int index = GetShelfIndex();
    ShelfItem item = ash::ShelfModel::Get()->items()[index];
    item.image = icon_value->uncompressed;
    ash::ShelfModel::Get()->Set(index, item);
    return;
  }

  icon_ = icon_value->uncompressed;
}

void StandaloneBrowserExtensionAppShelfItemController::OnWindowDestroying(
    aura::Window* window) {
  DCHECK(windows_.find(window) != windows_.end());
  windows_.erase(window);
  window_observations_.RemoveObservation(window);

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
