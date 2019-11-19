// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"

#include <memory>
#include <string>

#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/extension_uninstaller.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/ash/launcher/arc_launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/crostini_shelf_context_menu.h"
#include "chrome/browser/ui/ash/launcher/extension_launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/internal_app_shelf_context_menu.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/vector_icons.h"

namespace {

void UninstallApp(Profile* profile, const std::string& app_id) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(proxy);
  proxy->Uninstall(app_id, nullptr /* parent_window */);
  return;
}

}  // namespace

// static
std::unique_ptr<LauncherContextMenu> LauncherContextMenu::Create(
    ChromeLauncherController* controller,
    const ash::ShelfItem* item,
    int64_t display_id) {
  DCHECK(controller);
  DCHECK(item);
  DCHECK(!item->id.IsNull());
  // Create an ArcLauncherContextMenu if the item is an ARC app.
  if (arc::IsArcItem(controller->profile(), item->id.app_id)) {
    return std::make_unique<ArcLauncherContextMenu>(controller, item,
                                                    display_id);
  }

  // Create an CrostiniShelfContextMenu if the item is Crostini app.
  crostini::CrostiniRegistryService* crostini_registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(
          controller->profile());
  if (crostini_registry_service &&
      crostini_registry_service->IsCrostiniShelfAppId(item->id.app_id)) {
    return std::make_unique<CrostiniShelfContextMenu>(controller, item,
                                                      display_id);
  }

  if (app_list::IsInternalApp(item->id.app_id)) {
    return std::make_unique<InternalAppShelfContextMenu>(controller, item,
                                                         display_id);
  }

  // Create an ExtensionLauncherContextMenu for other items.
  return std::make_unique<ExtensionLauncherContextMenu>(controller, item,
                                                        display_id);
}

LauncherContextMenu::LauncherContextMenu(ChromeLauncherController* controller,
                                         const ash::ShelfItem* item,
                                         int64_t display_id)
    : controller_(controller),
      item_(item ? *item : ash::ShelfItem()),
      display_id_(display_id) {
  DCHECK_NE(display_id, display::kInvalidDisplayId);
}

LauncherContextMenu::~LauncherContextMenu() = default;

bool LauncherContextMenu::IsCommandIdChecked(int command_id) const {
  DCHECK(command_id < ash::COMMAND_ID_COUNT);
  return false;
}

bool LauncherContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id == ash::MENU_PIN) {
    // Users cannot modify the pinned state of apps pinned by policy.
    return !item_.pinned_by_policy &&
           (item_.type == ash::TYPE_PINNED_APP || item_.type == ash::TYPE_APP);
  }

  DCHECK(command_id < ash::COMMAND_ID_COUNT);
  return true;
}

void LauncherContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (static_cast<ash::CommandId>(command_id)) {
    case ash::MENU_OPEN_NEW:
      // Use a copy of the id to avoid crashes, as this menu's owner will be
      // destroyed if LaunchApp replaces the ShelfItemDelegate instance.
      controller_->LaunchApp(ash::ShelfID(item_.id), ash::LAUNCH_FROM_SHELF,
                             ui::EF_NONE, display_id_);
      break;
    case ash::MENU_CLOSE:
      if (item_.type == ash::TYPE_DIALOG) {
        ash::ShelfItemDelegate* item_delegate =
            controller_->shelf_model()->GetShelfItemDelegate(item_.id);
        DCHECK(item_delegate);
        item_delegate->Close();
      } else {
        // TODO(simonhong): Use ShelfItemDelegate::Close().
        controller_->Close(item_.id);
      }
      base::RecordAction(base::UserMetricsAction("CloseFromContextMenu"));
      if (ash::TabletMode::Get()->InTabletMode()) {
        base::RecordAction(
            base::UserMetricsAction("Tablet_WindowCloseFromContextMenu"));
      }
      break;
    case ash::MENU_PIN:
      if (controller_->IsAppPinned(item_.id.app_id))
        controller_->UnpinAppWithID(item_.id.app_id);
      else
        controller_->PinAppWithID(item_.id.app_id);
      break;
    case ash::UNINSTALL:
      UninstallApp(controller_->profile(), item_.id.app_id);
      break;
    default:
      NOTREACHED();
  }
}

void LauncherContextMenu::AddPinMenu(ui::SimpleMenuModel* menu_model) {
  // Expect a valid ShelfID to add pin/unpin menu item.
  DCHECK(!item_.id.IsNull());
  int menu_pin_string_id;
  switch (GetPinnableForAppID(item_.id.app_id, controller_->profile())) {
    case AppListControllerDelegate::PIN_EDITABLE:
      menu_pin_string_id = controller_->IsPinned(item_.id)
                               ? IDS_LAUNCHER_CONTEXT_MENU_UNPIN
                               : IDS_LAUNCHER_CONTEXT_MENU_PIN;
      break;
    case AppListControllerDelegate::PIN_FIXED:
      menu_pin_string_id = IDS_LAUNCHER_CONTEXT_MENU_PIN_ENFORCED_BY_POLICY;
      break;
    case AppListControllerDelegate::NO_PIN:
      return;
    default:
      NOTREACHED();
      return;
  }
  AddContextMenuOption(menu_model, ash::MENU_PIN, menu_pin_string_id);
}

bool LauncherContextMenu::ExecuteCommonCommand(int command_id,
                                               int event_flags) {
  switch (command_id) {
    case ash::MENU_OPEN_NEW:
    case ash::MENU_CLOSE:
    case ash::MENU_PIN:
    case ash::UNINSTALL:
      LauncherContextMenu::ExecuteCommand(command_id, event_flags);
      return true;
    default:
      return false;
  }
}

void LauncherContextMenu::AddContextMenuOption(ui::SimpleMenuModel* menu_model,
                                               ash::CommandId type,
                                               int string_id) {
  // Do not include disabled items.
  if (!IsCommandIdEnabled(type))
    return;

  const gfx::VectorIcon& icon = GetCommandIdVectorIcon(type, string_id);
  if (!icon.is_empty()) {
    menu_model->AddItemWithStringIdAndIcon(type, string_id, icon);
    return;
  }
  // If the MenuType is a check item.
  if (type == ash::LAUNCH_TYPE_REGULAR_TAB ||
      type == ash::LAUNCH_TYPE_PINNED_TAB || type == ash::LAUNCH_TYPE_WINDOW ||
      type == ash::LAUNCH_TYPE_FULLSCREEN) {
    menu_model->AddCheckItemWithStringId(type, string_id);
    return;
  }
  // NOTIFICATION_CONTAINER is added by NotificationMenuController.
  if (type == ash::NOTIFICATION_CONTAINER) {
    NOTREACHED()
        << "NOTIFICATION_CONTAINER is added by NotificationMenuController.";
    return;
  }
  menu_model->AddItemWithStringId(type, string_id);
}

const gfx::VectorIcon& LauncherContextMenu::GetCommandIdVectorIcon(
    ash::CommandId type,
    int string_id) const {
  switch (type) {
    case ash::MENU_OPEN_NEW:
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
    case ash::MENU_PIN:
      return controller_->IsPinned(item_.id) ? views::kUnpinIcon
                                             : views::kPinIcon;
    case ash::MENU_NEW_WINDOW:
      return views::kNewWindowIcon;
    case ash::MENU_NEW_INCOGNITO_WINDOW:
      return views::kNewIncognitoWindowIcon;
    case ash::LAUNCH_TYPE_PINNED_TAB:
    case ash::LAUNCH_TYPE_REGULAR_TAB:
    case ash::LAUNCH_TYPE_FULLSCREEN:
    case ash::LAUNCH_TYPE_WINDOW:
      // Check items use a default icon in touchable and default context menus.
      return gfx::kNoneIcon;
    case ash::NOTIFICATION_CONTAINER:
      NOTREACHED() << "NOTIFICATION_CONTAINER does not have an icon, and it is "
                      "added to the model by NotificationMenuController.";
      return gfx::kNoneIcon;
    case ash::STOP_APP:
      if (string_id == IDS_CROSTINI_SHUT_DOWN_LINUX_MENU_ITEM)
        return views::kLinuxShutdownIcon;
      return gfx::kNoneIcon;
    case ash::CROSTINI_USE_HIGH_DENSITY:
      return views::kLinuxHighDensityIcon;
    case ash::CROSTINI_USE_LOW_DENSITY:
      return views::kLinuxLowDensityIcon;
    case ash::LAUNCH_APP_SHORTCUT_FIRST:
    case ash::LAUNCH_APP_SHORTCUT_LAST:
    case ash::COMMAND_ID_COUNT:
      NOTREACHED();
      return gfx::kNoneIcon;
    default:
      NOTREACHED();
      return gfx::kNoneIcon;
  }
}
