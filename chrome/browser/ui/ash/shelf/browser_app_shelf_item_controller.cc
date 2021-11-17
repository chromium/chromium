// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/browser_app_shelf_item_controller.h"

#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_context_menu.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"

BrowserAppShelfItemController::BrowserAppShelfItemController(
    const ash::ShelfID& shelf_id,
    Profile* profile)
    : ash::ShelfItemDelegate(shelf_id),
      profile_(profile),
      registry_(*apps::AppServiceProxyFactory::GetForProfile(profile_)
                     ->BrowserAppInstanceRegistry()) {
  registry_observation_.Observe(&registry_);
  // Registers all running instances that started before this shelf item was
  // created, for example if a running app is later pinned to the shelf.
  registry_.NotifyExistingInstances(this);
  LoadAppMenuIcon();
}

BrowserAppShelfItemController::~BrowserAppShelfItemController() = default;

void BrowserAppShelfItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback,
    const ItemFilterPredicate& filter_predicate) {
  auto instances = GetMatchingInstances(filter_predicate);
  switch (instances.size()) {
    case 0:
      // Nothing is running, launch.
      std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});
      ChromeShelfController::instance()->LaunchApp(
          ash::ShelfID(shelf_id()), source, ui::EF_NONE, display_id);
      break;
    case 1: {
      // One instance is running, activate it.
      base::UnguessableToken id = instances[0].second;
      const bool can_minimize = source != ash::LAUNCH_FROM_APP_LIST &&
                                source != ash::LAUNCH_FROM_APP_LIST_SEARCH;
      ash::ShelfAction action;
      if (registry_.IsInstanceActive(id) && can_minimize) {
        registry_.MinimizeInstance(id);
        action = ash::SHELF_ACTION_WINDOW_MINIMIZED;
      } else {
        registry_.ActivateInstance(id);
        action = ash::SHELF_ACTION_WINDOW_ACTIVATED;
      }
      std::move(callback).Run(action, {});
      break;
    }
    default:
      // Multiple instances activated, show the list of running instances.
      std::move(callback).Run(
          ash::SHELF_ACTION_NONE,
          GetAppMenuItems(event ? event->flags() : ui::EF_NONE,
                          filter_predicate));
      break;
  }
}

BrowserAppShelfItemController::AppMenuItems
BrowserAppShelfItemController::GetAppMenuItems(
    int event_flags,
    const ItemFilterPredicate& filter_predicate) {
  AppMenuItems items;
  for (const auto& pair : GetMatchingInstances(filter_predicate)) {
    int command_id = pair.first;
    base::UnguessableToken id = pair.second;
    if (shelf_id().app_id == extension_misc::kLacrosAppId) {
      const apps::BrowserWindowInstance* instance =
          registry_.GetBrowserWindowInstanceById(id);
      DCHECK(instance);
      items.push_back({command_id, instance->window->GetTitle(), menu_icon_});
    } else {
      const apps::BrowserAppInstance* instance =
          registry_.GetAppInstanceById(id);
      DCHECK(instance);
      items.push_back(
          {command_id, base::UTF8ToUTF16(instance->title), menu_icon_});
    }
  }
  return items;
}

void BrowserAppShelfItemController::GetContextMenu(
    int64_t display_id,
    GetContextMenuCallback callback) {
  ChromeShelfController* controller = ChromeShelfController::instance();
  const ash::ShelfItem* item = controller->GetItem(shelf_id());
  context_menu_ = ShelfContextMenu::Create(controller, item, display_id);
  context_menu_->GetMenuModel(std::move(callback));
}

void BrowserAppShelfItemController::ExecuteCommand(bool from_context_menu,
                                                   int64_t command_id,
                                                   int32_t event_flags,
                                                   int64_t display_id) {
  // Item selected from menu.
  auto it = command_to_instance_map_.find(command_id);
  if (it != command_to_instance_map_.end()) {
    registry_.ActivateInstance(it->second);
  }
}

void BrowserAppShelfItemController::Close() {
  apps::AppServiceProxyFactory::GetForProfile(profile_)->StopApp(
      shelf_id().app_id);
}

void BrowserAppShelfItemController::OnBrowserWindowAdded(
    const apps::BrowserWindowInstance& instance) {
  if (!(shelf_id().app_id == extension_misc::kLacrosAppId &&
        crosapi::browser_util::IsLacrosWindow(instance.window))) {
    // Only handle Lacros browser windows.
    return;
  }
  int command = ++last_command_id_;
  command_to_instance_map_[command] = instance.id;
}

void BrowserAppShelfItemController::OnBrowserWindowRemoved(
    const apps::BrowserWindowInstance& instance) {
  if (!(shelf_id().app_id == extension_misc::kLacrosAppId &&
        crosapi::browser_util::IsLacrosWindow(instance.window))) {
    // Only handle Lacros browser windows.
    return;
  }
  int command = GetInstanceCommand(instance.id);
  command_to_instance_map_.erase(command);
}

void BrowserAppShelfItemController::OnBrowserAppAdded(
    const apps::BrowserAppInstance& instance) {
  if (shelf_id().app_id != instance.app_id) {
    return;
  }
  int command = ++last_command_id_;
  command_to_instance_map_[command] = instance.id;
}

void BrowserAppShelfItemController::OnBrowserAppRemoved(
    const apps::BrowserAppInstance& instance) {
  if (shelf_id().app_id != instance.app_id) {
    return;
  }
  int command = GetInstanceCommand(instance.id);
  command_to_instance_map_.erase(command);
}

std::vector<std::pair<int, base::UnguessableToken>>
BrowserAppShelfItemController::GetMatchingInstances(
    const ItemFilterPredicate& filter_predicate) {
  // Iterating the map keyed by command ID, so the instances are automatically
  // sorted by launch order.
  std::vector<std::pair<int, base::UnguessableToken>> result;
  for (const auto& pair : command_to_instance_map_) {
    base::UnguessableToken id = pair.second;
    aura::Window* window = nullptr;
    if (shelf_id().app_id == extension_misc::kLacrosAppId) {
      const apps::BrowserWindowInstance* instance =
          registry_.GetBrowserWindowInstanceById(id);
      DCHECK(instance);
      window = instance->window;
    } else {
      const apps::BrowserAppInstance* instance =
          registry_.GetAppInstanceById(id);
      DCHECK(instance);
      window = instance->window;
    }
    if (filter_predicate.is_null() || filter_predicate.Run(window)) {
      result.push_back(pair);
    }
  }
  return result;
}

int BrowserAppShelfItemController::GetInstanceCommand(
    const base::UnguessableToken& id) {
  auto it = base::ranges::find_if(
      command_to_instance_map_,
      [&id](const auto& pair) { return pair.second == id; });
  DCHECK(it != command_to_instance_map_.end());
  return it->first;
}

void BrowserAppShelfItemController::LoadAppMenuIcon() {
  const std::string& app_id = shelf_id().app_id;
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  auto app_type = proxy->AppRegistryCache().GetAppType(app_id);
  if (base::FeatureList::IsEnabled(features::kAppServiceLoadIconWithoutMojom)) {
    icon_loader_releaser_ = proxy->LoadIcon(
        apps::ConvertMojomAppTypToAppType(app_type), app_id,
        apps::IconType::kStandard,
        // matches favicon size
        /* size_hint_in_dip= */ extension_misc::EXTENSION_ICON_BITTY,
        /* allow_placeholder_icon= */ false,
        base::BindOnce(&BrowserAppShelfItemController::OnLoadAppMenuIcon,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    icon_loader_releaser_ = proxy->LoadIcon(
        app_type, app_id, apps::mojom::IconType::kStandard,
        // matches favicon size
        /* size_hint_in_dip= */ extension_misc::EXTENSION_ICON_BITTY,
        /* allow_placeholder_icon= */ false,
        apps::MojomIconValueToIconValueCallback(
            base::BindOnce(&BrowserAppShelfItemController::OnLoadAppMenuIcon,
                           weak_ptr_factory_.GetWeakPtr())));
  }
}

void BrowserAppShelfItemController::OnLoadAppMenuIcon(
    apps::IconValuePtr icon_value) {
  if (icon_value && icon_value->icon_type == apps::IconType::kStandard) {
    menu_icon_ = icon_value->uncompressed;
  }
}
