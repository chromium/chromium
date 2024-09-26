// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/shelf_context_menu.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/metrics/user_metrics.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_shelf_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_promise_app_shelf_context_menu.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_shelf_context_menu.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/extension_shelf_context_menu.h"
#include "chrome/browser/ui/ash/shelf/extension_uninstaller.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/vector_icons.h"

namespace {

using ::ash::AccessibilityManager;

void UninstallApp(Profile* profile, const std::string& app_id) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  if (proxy->AppRegistryCache().GetAppType(app_id) != apps::AppType::kUnknown) {
    proxy->Uninstall(app_id, apps::UninstallSource::kShelf,
                     nullptr /* parent_window */);
    return;
  }

  // Runs the extension uninstall flow for for extensions. ExtensionUninstall
  // deletes itself when done or aborted.
  ExtensionUninstaller* extension_uninstaller =
      new ExtensionUninstaller(profile, app_id, nullptr /* parent_window */);
  extension_uninstaller->Run();
}

}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kShelfCloseMenuItem);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kShelfContextMenuNewWindowMenuItem);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kShelfContextMenuIncognitoWindowMenuItem);

// static
std::unique_ptr<ShelfContextMenu> ShelfContextMenu::Create(
    ChromeShelfController* controller,
    const ash::ShelfItem* item,
    int64_t display_id) {
  DCHECK(controller);
  DCHECK(item);
  DCHECK(!item->id.IsNull());

  if (ash::features::ArePromiseIconsEnabled() &&
      apps::AppServiceProxyFactory::GetForProfile(controller->profile())
          ->PromiseAppRegistryCache()
          ->GetPromiseAppForStringPackageId(item->id.app_id)) {
    return std::make_unique<AppServicePromiseAppShelfContextMenu>(
        controller, item, display_id);
  }

  auto app_type =
      apps::AppServiceProxyFactory::GetForProfile(controller->profile())
          ->AppRegistryCache()
          .GetAppType(item->id.app_id);
  // AppServiceShelfContextMenu supports context menus for apps registered in
  // AppService, Arc shortcuts and Crostini apps with the prefix "crostini:".
  if ((app_type != apps::AppType::kUnknown &&
       app_type != apps::AppType::kExtension) ||
      guest_os::IsUnregisteredCrostiniShelfAppId(item->id.app_id) ||
      arc::IsArcItem(controller->profile(), item->id.app_id)) {
    return std::make_unique<AppServiceShelfContextMenu>(controller, item,
                                                        display_id);
  }

  // Create an ExtensionShelfContextMenu for other items, including browser
  // extensions.
  return std::make_unique<ExtensionShelfContextMenu>(controller, item,
                                                     display_id);
}

ShelfContextMenu::ShelfContextMenu(ChromeShelfController* controller,
                                   const ash::ShelfItem* item,
                                   int64_t display_id)
    : controller_(controller),
      item_(item ? *item : ash::ShelfItem()),
      display_id_(display_id) {
  DCHECK_NE(display_id, display::kInvalidDisplayId);
}

ShelfContextMenu::~ShelfContextMenu() = default;

std::unique_ptr<ui::SimpleMenuModel> ShelfContextMenu::GetBaseMenuModel() {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  AddContextMenuOption(menu_model.get(), ash::SWAP_WITH_NEXT,
                       IDS_SHELF_CONTEXT_MENU_SWAP_WITH_NEXT);
  AddContextMenuOption(menu_model.get(), ash::SWAP_WITH_PREVIOUS,
                       IDS_SHELF_CONTEXT_MENU_SWAP_WITH_PREVIOUS);
  return menu_model;
}

bool ShelfContextMenu::IsCommandIdChecked(int command_id) const {
  DCHECK(command_id < ash::COMMAND_ID_COUNT);
  return false;
}

bool ShelfContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id == ash::TOGGLE_PIN) {
    // Users cannot modify apps with a forced pinned state.
    return !item_.IsPinStateForced() &&
           (item_.type == ash::TYPE_PINNED_APP || item_.type == ash::TYPE_APP);
  }

  if (command_id == ash::SWAP_WITH_NEXT ||
      command_id == ash::SWAP_WITH_PREVIOUS) {
    // Only show commands to reorder shelf items when ChromeVox or SwitchAccess
    // are enabled.
    if (!AccessibilityManager::Get() ||
        (!AccessibilityManager::Get()->IsSpokenFeedbackEnabled() &&
         !AccessibilityManager::Get()->IsSwitchAccessEnabled())) {
      return false;
    }
    const ash::ShelfModel* model = controller_->shelf_model();
    return model->CanSwap(model->ItemIndexByID(item_.id),
                          /*with_next=*/command_id == ash::SWAP_WITH_NEXT);
  }

  DCHECK(command_id < ash::COMMAND_ID_COUNT);
  return true;
}

void ShelfContextMenu::ExecuteCommand(int command_id, int event_flags) {
  ash::ShelfModel::ScopedUserTriggeredMutation user_triggered(
      controller_->shelf_model());
  ash::ShelfModel* model = controller_->shelf_model();
  const int item_index = model->ItemIndexByID(item_.id);
  switch (static_cast<ash::CommandId>(command_id)) {
    case ash::SWAP_WITH_NEXT:
      model->Swap(item_index, /*with_next=*/true);
      break;
    case ash::SWAP_WITH_PREVIOUS:
      model->Swap(item_index, /*with_next=*/false);
      break;
    case ash::LAUNCH_NEW:
      // Use a copy of the id to avoid crashes, as this menu's owner will be
      // destroyed if LaunchApp replaces the ShelfItemDelegate instance.
      controller_->LaunchApp(ash::ShelfID(item_.id), ash::LAUNCH_FROM_SHELF,
                             ui::EF_NONE, display_id_, /*new_window=*/true);
      break;
    case ash::MENU_CLOSE:
      if (item_.type == ash::TYPE_DIALOG) {
        ash::ShelfItemDelegate* item_delegate =
            model->GetShelfItemDelegate(item_.id);
        DCHECK(item_delegate);
        item_delegate->Close();
      } else {
        // TODO(simonhong): Use ShelfItemDelegate::Close().
        controller_->Close(item_.id);
      }
      base::RecordAction(base::UserMetricsAction("CloseFromContextMenu"));
      if (display::Screen::GetScreen()->InTabletMode()) {
        base::RecordAction(
            base::UserMetricsAction("Tablet_WindowCloseFromContextMenu"));
      }
      break;
    case ash::TOGGLE_PIN:
      if (controller_->IsAppPinned(item_.id.app_id))
        controller_->UnpinAppWithID(item_.id.app_id);
      else
        controller_->shelf_model()->PinExistingItemWithID(item_.id.app_id);
      break;
    case ash::UNINSTALL:
      UninstallApp(controller_->profile(), item_.id.app_id);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

const gfx::VectorIcon& ShelfContextMenu::GetCommandIdVectorIcon(
    int type,
    int string_id) const {
  switch (type) {
    case ash::LAUNCH_NEW:
      if (string_id == IDS_APP_LIST_CONTEXT_MENU_NEW_TAB)
        return views::kNewTabIcon;
      if (string_id == IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW)
        return views::kNewWindowIcon;
      return views::kOpenIcon;
    case ash::MENU_CLOSE:
      return views::kCloseIcon;
    case ash::SHOW_APP_INFO:
      return views::kInfoIcon;
    case ash::UNINSTALL:
      return views::kUninstallIcon;
    case ash::SETTINGS:
      return vector_icons::kSettingsIcon;
    case ash::TOGGLE_PIN:
      return controller_->IsPinned(item_.id) ? views::kUnpinIcon
                                             : views::kPinIcon;
    case ash::APP_CONTEXT_MENU_NEW_WINDOW:
      return views::kNewWindowIcon;
    case ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW:
      return views::kNewIncognitoWindowIcon;
    case ash::USE_LAUNCH_TYPE_REGULAR:
    case ash::USE_LAUNCH_TYPE_WINDOW:
    case ash::USE_LAUNCH_TYPE_TABBED_WINDOW:
      // Check items use a default icon in touchable and default context menus.
      return gfx::kNoneIcon;
    case ash::DEPRECATED_USE_LAUNCH_TYPE_PINNED:
    case ash::DEPRECATED_USE_LAUNCH_TYPE_FULLSCREEN:
      NOTREACHED_IN_MIGRATION();
      return gfx::kNoneIcon;
    case ash::NOTIFICATION_CONTAINER:
      NOTREACHED_IN_MIGRATION()
          << "NOTIFICATION_CONTAINER does not have an icon, and it is "
             "added to the model by NotificationMenuController.";
      return gfx::kNoneIcon;
    case ash::SHUTDOWN_GUEST_OS:
      return kShutdownGuestOsIcon;
    case ash::SHUTDOWN_BRUSCHETTA_OS:
      return kShutdownGuestOsIcon;
    case ash::CROSTINI_USE_HIGH_DENSITY:
      return views::kLinuxHighDensityIcon;
    case ash::CROSTINI_USE_LOW_DENSITY:
      return views::kLinuxLowDensityIcon;
    case ash::SWAP_WITH_NEXT:
    case ash::SWAP_WITH_PREVIOUS:
      return gfx::kNoneIcon;
    case ash::LAUNCH_APP_SHORTCUT_FIRST:
    case ash::LAUNCH_APP_SHORTCUT_LAST:
    case ash::COMMAND_ID_COUNT:
      NOTREACHED_IN_MIGRATION();
      return gfx::kNoneIcon;
    default:
      NOTREACHED_IN_MIGRATION();
      return gfx::kNoneIcon;
  }
}

void ShelfContextMenu::AddPinMenu(ui::SimpleMenuModel* menu_model) {
  // Expect a valid ShelfID to add pin/unpin menu item.
  DCHECK(!item_.id.IsNull());
  int menu_pin_string_id;
  switch (GetPinnableForAppID(item_.id.app_id, controller_->profile())) {
    case AppListControllerDelegate::PIN_EDITABLE:
      menu_pin_string_id = controller_->IsPinned(item_.id)
                               ? IDS_SHELF_CONTEXT_MENU_UNPIN
                               : IDS_SHELF_CONTEXT_MENU_PIN;
      break;
    case AppListControllerDelegate::PIN_FIXED:
      menu_pin_string_id = IDS_SHELF_CONTEXT_MENU_PIN_ENFORCED_BY_POLICY;
      break;
    case AppListControllerDelegate::NO_PIN:
      return;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  AddContextMenuOption(menu_model, ash::TOGGLE_PIN, menu_pin_string_id);
}

bool ShelfContextMenu::ExecuteCommonCommand(int command_id, int event_flags) {
  switch (command_id) {
    case ash::LAUNCH_NEW: {
      if (auto* full_restore_service =
              ash::full_restore::FullRestoreServiceFactory::GetForProfile(
                  controller()->profile())) {
        full_restore_service->MaybeCloseNotification();
      }
      [[fallthrough]];
    }
    case ash::MENU_CLOSE:
    case ash::TOGGLE_PIN:
    case ash::SWAP_WITH_NEXT:
    case ash::SWAP_WITH_PREVIOUS:
    case ash::UNINSTALL:
      ShelfContextMenu::ExecuteCommand(command_id, event_flags);
      return true;
    default:
      return false;
  }
}

void ShelfContextMenu::AddContextMenuOption(ui::SimpleMenuModel* menu_model,
                                            ash::CommandId type,
                                            int string_id) {
  // Do not include disabled items.
  if (!IsCommandIdEnabled(type))
    return;

  const gfx::VectorIcon& icon = GetCommandIdVectorIcon(type, string_id);
  if (!icon.is_empty()) {
    menu_model->AddItemWithStringIdAndIcon(
        type, string_id,
        ui::ImageModel::FromVectorIcon(icon, ui::kColorAshSystemUIMenuIcon,
                                       ash::kAppContextMenuIconSize));
    MaybeSetElementIdentifier(menu_model, type);
    return;
  }
  // If the MenuType is a check item.
  if (type >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
      type < ash::USE_LAUNCH_TYPE_COMMAND_END) {
    menu_model->AddCheckItemWithStringId(type, string_id);
    return;
  }
  // NOTIFICATION_CONTAINER is added by NotificationMenuController.
  if (type == ash::NOTIFICATION_CONTAINER) {
    NOTREACHED_IN_MIGRATION()
        << "NOTIFICATION_CONTAINER is added by NotificationMenuController.";
    return;
  }
  menu_model->AddItemWithStringId(type, string_id);
}

void ShelfContextMenu::MaybeSetElementIdentifier(
    ui::SimpleMenuModel* menu_model,
    ash::CommandId type) {
  ui::ElementIdentifier id;
  switch (type) {
    case ash::MENU_CLOSE:
      id = kShelfCloseMenuItem;
      break;
    case ash::APP_CONTEXT_MENU_NEW_WINDOW:
      id = kShelfContextMenuNewWindowMenuItem;
      break;
    case ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW:
      id = kShelfContextMenuIncognitoWindowMenuItem;
      break;
    default:
      break;
  }

  if (!id || !menu_model) {
    return;
  }
  menu_model->SetElementIdentifierAt(
      menu_model->GetIndexOfCommandId(type).value(), id);
}
