// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/standalone_browser_extension_app_context_menu.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/extension_apps_utils.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"
#include "chrome/browser/ui/ash/shelf/standalone_browser_extension_app_shelf_item_controller.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/ash/settings/app_management/app_management_uma.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/vector_icons.h"

namespace {

// Create an appropriately sized ImageModel for a menu item icon.
ui::ImageModel GetMenuItemIcon(const gfx::VectorIcon& icon) {
  return ui::ImageModel::FromVectorIcon(
      icon,
      /*color_id=*/ui::kColorAshSystemUIMenuIcon, ash::kAppContextMenuIconSize);
}

}  // namespace

StandaloneBrowserExtensionAppContextMenu::
    StandaloneBrowserExtensionAppContextMenu(const std::string& app_id,
                                             Source source)
    : app_id_(app_id), source_(source) {}
StandaloneBrowserExtensionAppContextMenu::
    ~StandaloneBrowserExtensionAppContextMenu() = default;

void StandaloneBrowserExtensionAppContextMenu::GetMenuModel(
    GetMenuModelCallback callback) {
  // Always invoke the callback asynchronously to avoid re-entrancy.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneBrowserExtensionAppContextMenu::OnGetMenuModel,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void StandaloneBrowserExtensionAppContextMenu::ExecuteCommand(int command_id,
                                                              int event_flags) {
  ash::ShelfModel* model = ash::ShelfModel::Get();
  const int item_index = model->ItemIndexByAppID(app_id_);
  const bool item_in_shelf = item_index >= 0;

  switch (static_cast<ash::CommandId>(command_id)) {
    case ash::SWAP_WITH_NEXT:
      if (!item_in_shelf)
        return;
      model->Swap(item_index, /*with_next=*/true);
      return;
    case ash::SWAP_WITH_PREVIOUS:
      if (!item_in_shelf)
        return;
      model->Swap(item_index, /*with_next=*/false);
      return;
    case ash::TOGGLE_PIN:
      if (item_in_shelf && model->IsAppPinned(app_id_)) {
        UnpinAppWithIDFromShelf(app_id_);
      } else {
        PinAppWithIDToShelf(app_id_);
      }
      return;
    case ash::MENU_CLOSE: {
      // There can only be a single active ash profile when Lacros is running.
      apps::AppServiceProxy* proxy =
          apps::AppServiceProxyFactory::GetForProfile(
              ProfileManager::GetPrimaryUserProfile());
      proxy->StopApp(app_id_);
      return;
    }

    case ash::UNINSTALL: {
      apps::UninstallSource uninstall_source =
          (source_ == Source::kShelf) ? apps::UninstallSource::kShelf
                                      : apps::UninstallSource::kAppList;
      aura::Window* window =
          (source_ == Source::kShelf)
              ? AppListClientImpl::GetInstance()->GetAppListWindow()
              : nullptr;
      apps::AppServiceProxy* proxy =
          apps::AppServiceProxyFactory::GetForProfile(
              ProfileManager::GetPrimaryUserProfile());
      proxy->Uninstall(app_id_, uninstall_source, /*parent_window=*/window);
      return;
    }
    case ash::SHOW_APP_INFO: {
      ash::settings::AppManagementEntryPoint entry =
          (source_ == Source::kShelf) ? ash::settings::AppManagementEntryPoint::
                                            kShelfContextMenuAppInfoChromeApp
                                      : ash::settings::AppManagementEntryPoint::
                                            kAppListContextMenuAppInfoChromeApp;
      chrome::ShowAppManagementPage(
          ProfileManager::GetPrimaryUserProfile(),
          apps::GetEscapedAppId(app_id_,
                                apps::AppType::kStandaloneBrowserChromeApp),
          entry);
      return;
    }
    default:
      return;
  }
}

void StandaloneBrowserExtensionAppContextMenu::OnGetMenuModel(
    GetMenuModelCallback callback) {
  bool allow_uninstall = false;
  bool allow_app_info = false;
  apps::AppServiceProxy* proxy = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetPrimaryUserProfile());
  proxy->AppRegistryCache().ForOneApp(
      app_id_,
      [&allow_app_info, &allow_uninstall](const apps::AppUpdate& update) {
        allow_app_info = update.ShowInManagement().value_or(false);
        allow_uninstall = update.AllowUninstall().value_or(false);
      });

  bool allow_pin_unpin =
      GetPinnableForAppID(app_id_, ProfileManager::GetPrimaryUserProfile()) ==
      AppListControllerDelegate::PIN_EDITABLE;

  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);

  ash::ShelfModel* model = ash::ShelfModel::Get();
  const int item_index = model->ItemIndexByAppID(app_id_);
  const bool item_in_shelf = item_index >= 0;

  // Only show commands to reorder shelf items when ChromeVox or SwitchAccess
  // are enabled and the item is in the shelf.
  ash::AccessibilityManager* manager = ash::AccessibilityManager::Get();
  bool show_swap = manager &&
                   (manager->IsSpokenFeedbackEnabled() ||
                    manager->IsSwitchAccessEnabled()) &&
                   item_in_shelf;
  if (show_swap) {
    if (model->CanSwap(item_index, /*with_next=*/true)) {
      menu_model->AddItemWithStringId(ash::SWAP_WITH_NEXT,
                                      IDS_SHELF_CONTEXT_MENU_SWAP_WITH_NEXT);
    }
    if (model->CanSwap(item_index, /*with_next=*/false)) {
      menu_model->AddItemWithStringId(
          ash::SWAP_WITH_PREVIOUS, IDS_SHELF_CONTEXT_MENU_SWAP_WITH_PREVIOUS);
    }
  }

  if (allow_pin_unpin) {
    // This context menu is used by both the shelf and app list. We choose to
    // use the app list IDS string since it's clearer.
    bool currently_pinned = item_in_shelf && model->IsAppPinned(app_id_);
    const gfx::VectorIcon& icon =
        currently_pinned ? views::kUnpinIcon : views::kPinIcon;
    int ids = currently_pinned ? IDS_APP_LIST_CONTEXT_MENU_UNPIN
                               : IDS_APP_LIST_CONTEXT_MENU_PIN;

    menu_model->AddItemWithStringIdAndIcon(ash::TOGGLE_PIN, ids,
                                           GetMenuItemIcon(icon));
  }

  // Add a close menu item if there is at least one open window.
  if (item_in_shelf) {
    ash::ShelfItemDelegate* existing_delegate =
        ash::ShelfModel::Get()->GetShelfItemDelegate(ash::ShelfID(app_id_));
    StandaloneBrowserExtensionAppShelfItemController* controller =
        static_cast<StandaloneBrowserExtensionAppShelfItemController*>(
            existing_delegate);
    if (controller->WindowCount() >= 1) {
      menu_model->AddItemWithStringIdAndIcon(
          ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE,
          GetMenuItemIcon(views::kCloseIcon));
    }
  }

  // Add an uninstall menu item.
  if (allow_uninstall) {
    menu_model->AddItemWithStringIdAndIcon(
        ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM,
        GetMenuItemIcon(views::kUninstallIcon));
  }

  // Add a show app info menu item.
  if (allow_app_info) {
    menu_model->AddItemWithStringIdAndIcon(ash::SHOW_APP_INFO,
                                           IDS_APP_CONTEXT_MENU_SHOW_INFO,
                                           GetMenuItemIcon(views::kInfoIcon));
  }

  // TODO(crbug.com/40188614): Custom chrome app context menu items.

  std::move(callback).Run(std::move(menu_model));
}
