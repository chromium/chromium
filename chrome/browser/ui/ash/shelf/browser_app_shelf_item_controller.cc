// Copyright 2021 The Chromium Authors
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
#include "chrome/grit/theme_resources.h"
#include "chromeos/ui/wm/desks/desks_helper.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/widget/native_widget_aura.h"

namespace {

// Returns true if we are trying to launch a lacros window if there isn't
// already one on the active desk.
bool ShouldLaunchNewLacrosWindow(
    std::string app_id,
    const std::vector<std::pair<int, base::UnguessableToken>>& instances,
    const apps::BrowserAppInstanceRegistry& registry) {
  if (app_id != app_constants::kLacrosAppId) {
    return false;
  }

  for (auto [cmd_id, instance_id] : instances) {
    aura::Window* window = registry.GetWindowByInstanceId(instance_id);
    if (window &&
        chromeos::DesksHelper::Get(window)->BelongsToActiveDesk(window)) {
      return false;
    }
  }

  return true;
}

}  // namespace

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
  LoadIcon(extension_misc::EXTENSION_ICON_BITTY,
           base::BindOnce(&BrowserAppShelfItemController::OnLoadBittyIcon,
                          weak_ptr_factory_.GetWeakPtr()));
}

BrowserAppShelfItemController::~BrowserAppShelfItemController() = default;

void BrowserAppShelfItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback,
    const ItemFilterPredicate& filter_predicate) {
  auto instances = GetMatchingInstances(filter_predicate);
  if (instances.size() == 0 ||
      ShouldLaunchNewLacrosWindow(app_id(), instances, registry_)) {
    // No instances or if this is a lacros window and there isn't already one on
    // the current workspace, launch.
    std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});
    ChromeShelfController::instance()->LaunchApp(
        ash::ShelfID(shelf_id()), source, ui::EF_NONE, display_id);
  } else if (instances.size() == 1) {
    // One instance is running, activate it.
    const base::UnguessableToken id = instances[0].second;
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
  } else {
    // Multiple instances activated, show the list of running instances.
    std::move(callback).Run(
        ash::SHELF_ACTION_NONE,
        GetAppMenuItems(event ? event->flags() : ui::EF_NONE,
                        filter_predicate));
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
    if (shelf_id().app_id == app_constants::kLacrosAppId) {
      const apps::BrowserWindowInstance* instance =
          registry_.GetBrowserWindowInstanceById(id);
      DCHECK(instance);
      const gfx::Image& icon =
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(
              instance->is_incognito ? IDR_ASH_SHELF_LIST_INCOGNITO_BROWSER
                                     : IDR_ASH_SHELF_LIST_BROWSER);
      items.push_back(
          {command_id, instance->window->GetTitle(), icon.AsImageSkia()});
    } else {
      const apps::BrowserAppInstance* instance =
          registry_.GetAppInstanceById(id);
      DCHECK(instance);
      items.push_back(
          {command_id, base::UTF8ToUTF16(instance->title), bitty_icon_});
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
  if (!(shelf_id().app_id == app_constants::kLacrosAppId &&
        crosapi::browser_util::IsLacrosWindow(instance.window))) {
    // Only handle Lacros browser windows.
    return;
  }

  if (!(bitty_icon_.isNull() || medium_icon_.isNull())) {
    views::NativeWidgetAura::AssignIconToAuraWindow(instance.window,
                                                    bitty_icon_, medium_icon_);
  }

  int command = ++last_command_id_;
  command_to_instance_map_[command] = instance.id;
}

void BrowserAppShelfItemController::OnBrowserWindowRemoved(
    const apps::BrowserWindowInstance& instance) {
  if (!(shelf_id().app_id == app_constants::kLacrosAppId &&
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

  // If we are adding a tab to a browser window for the app, then we still want
  // the browser window to maintain its own icon.
  if (instance.type != apps::BrowserAppInstance::Type::kAppTab &&
      !(bitty_icon_.isNull() || medium_icon_.isNull())) {
    views::NativeWidgetAura::AssignIconToAuraWindow(instance.window,
                                                    bitty_icon_, medium_icon_);
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
    if (shelf_id().app_id == app_constants::kLacrosAppId) {
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
  auto it = base::ranges::find(command_to_instance_map_, id,
                               &CommandToInstanceMap::value_type::second);
  DCHECK(it != command_to_instance_map_.end());
  return it->first;
}

void BrowserAppShelfItemController::LoadIcon(int32_t size_hint_in_dip,
                                             apps::LoadIconCallback callback) {
  const std::string& app_id = shelf_id().app_id;
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  icon_loader_releaser_ =
      proxy->LoadIcon(proxy->AppRegistryCache().GetAppType(app_id), app_id,
                      apps::IconType::kStandard,
                      // matches favicon size
                      /* size_hint_in_dip= */ size_hint_in_dip,
                      /* allow_placeholder_icon= */ false, std::move(callback));
}

void BrowserAppShelfItemController::OnLoadMediumIcon(
    apps::IconValuePtr icon_value) {
  if (icon_value && icon_value->icon_type == apps::IconType::kStandard) {
    medium_icon_ = icon_value->uncompressed;

    // At this point, we have loaded both icons needed to assign an icon to the
    // Lacros and Ash windows, so we can assign the icons to the instances that
    // have already been created.
    std::string app_id = shelf_id().app_id;
    if (app_id == app_constants::kLacrosAppId) {
      for (auto* instance : registry_.GetLacrosBrowserWindowInstances()) {
        views::NativeWidgetAura::AssignIconToAuraWindow(
            instance->window, bitty_icon_, medium_icon_);
      }
    } else {
      for (auto* instance : registry_.SelectAppInstances(
               [&app_id](const apps::BrowserAppInstance& instance) {
                 return instance.type ==
                            apps::BrowserAppInstance::Type::kAppWindow &&
                        app_id == instance.app_id;
               })) {
        views::NativeWidgetAura::AssignIconToAuraWindow(
            instance->window, bitty_icon_, medium_icon_);
      }
    }
  }
}

void BrowserAppShelfItemController::OnLoadBittyIcon(
    apps::IconValuePtr icon_value) {
  if (icon_value && icon_value->icon_type == apps::IconType::kStandard) {
    bitty_icon_ = icon_value->uncompressed;
    BrowserAppShelfItemController::LoadIcon(
        extension_misc::EXTENSION_ICON_MEDIUM,
        base::BindOnce(&BrowserAppShelfItemController::OnLoadMediumIcon,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}
