// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_context_menu.h"

#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/vector_icons.h"

namespace app_list {

AppContextMenu::AppContextMenu(AppContextMenuDelegate* delegate,
                               Profile* profile,
                               const std::string& app_id,
                               AppListControllerDelegate* controller)
    : delegate_(delegate),
      profile_(profile),
      app_id_(app_id),
      controller_(controller) {}

AppContextMenu::~AppContextMenu() = default;

void AppContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  BuildMenu(menu_model.get());
  std::move(callback).Run(std::move(menu_model));
}

void AppContextMenu::BuildMenu(ui::SimpleMenuModel* menu_model) {
  // Show Pin/Unpin option if shelf is available.
  if (controller_->GetPinnable(app_id()) != AppListControllerDelegate::NO_PIN) {
    AddContextMenuOption(menu_model, ash::TOGGLE_PIN,
                         controller_->IsAppPinned(app_id_)
                             ? IDS_APP_LIST_CONTEXT_MENU_UNPIN
                             : IDS_APP_LIST_CONTEXT_MENU_PIN);
  }
}

bool AppContextMenu::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == ash::TOGGLE_PIN;
}

base::string16 AppContextMenu::GetLabelForCommandId(int command_id) const {
  DCHECK_EQ(command_id, ash::TOGGLE_PIN);
  // Return "{Pin to, Unpin from} shelf" or "Pinned by administrator".
  // Note this only exists on Ash desktops.
  if (controller_->GetPinnable(app_id()) ==
      AppListControllerDelegate::PIN_FIXED) {
    return l10n_util::GetStringUTF16(
        IDS_APP_LIST_CONTEXT_MENU_PIN_ENFORCED_BY_POLICY);
  }
  return controller_->IsAppPinned(app_id_)
             ? l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_UNPIN)
             : l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_PIN);
}

bool AppContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id == ash::TOGGLE_PIN) {
    return controller_->GetPinnable(app_id_) ==
           AppListControllerDelegate::PIN_EDITABLE;
  }
  return true;
}

void AppContextMenu::TogglePin(const std::string& shelf_app_id) {
  DCHECK_EQ(AppListControllerDelegate::PIN_EDITABLE,
            controller_->GetPinnable(shelf_app_id));
  if (controller_->IsAppPinned(shelf_app_id))
    controller_->UnpinApp(shelf_app_id);
  else
    controller_->PinApp(shelf_app_id);
}

void AppContextMenu::AddContextMenuOption(ui::SimpleMenuModel* menu_model,
                                          ash::CommandId command_id,
                                          int string_id) {
  // Do not include disabled items.
  if (!IsCommandIdEnabled(command_id))
    return;

  const gfx::VectorIcon& icon = GetMenuItemVectorIcon(command_id, string_id);
  if (!icon.is_empty()) {
    menu_model->AddItemWithStringIdAndIcon(command_id, string_id, icon);
    return;
  }
  // Check items use default icons.
  if (command_id == ash::USE_LAUNCH_TYPE_PINNED ||
      command_id == ash::USE_LAUNCH_TYPE_REGULAR ||
      command_id == ash::USE_LAUNCH_TYPE_FULLSCREEN ||
      command_id == ash::USE_LAUNCH_TYPE_WINDOW) {
    menu_model->AddCheckItemWithStringId(command_id, string_id);
    return;
  }
  if (command_id == ash::NOTIFICATION_CONTAINER) {
    NOTREACHED()
        << "NOTIFICATION_CONTAINER is added by NotificationMenuController.";
    return;
  }
  menu_model->AddItemWithStringId(command_id, string_id);
}

const gfx::VectorIcon& AppContextMenu::GetMenuItemVectorIcon(
    int command_id,
    int string_id) const {
  switch (command_id) {
    case ash::LAUNCH_NEW:
      if (string_id == IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW)
        return views::kNewWindowIcon;
      if (string_id == IDS_APP_LIST_CONTEXT_MENU_NEW_TAB)
        return views::kNewTabIcon;
      // The LAUNCH_NEW command is for an ARC app.
      return views::kLaunchIcon;
    case ash::TOGGLE_PIN:
      return string_id == IDS_APP_LIST_CONTEXT_MENU_PIN ? views::kPinIcon
                                                        : views::kUnpinIcon;
    case ash::SHOW_APP_INFO:
      return views::kInfoIcon;
    case ash::OPTIONS:
      return views::kOptionsIcon;
    case ash::UNINSTALL:
      return views::kUninstallIcon;
    case ash::APP_CONTEXT_MENU_NEW_WINDOW:
      return views::kNewWindowIcon;
    case ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW:
      return views::kNewIncognitoWindowIcon;
    case ash::INSTALL:
      // Deprecated.
      return gfx::kNoneIcon;
    case ash::USE_LAUNCH_TYPE_PINNED:
    case ash::USE_LAUNCH_TYPE_REGULAR:
    case ash::USE_LAUNCH_TYPE_FULLSCREEN:
    case ash::USE_LAUNCH_TYPE_WINDOW:
      // Check items use the default icon.
      return gfx::kNoneIcon;
    case ash::NOTIFICATION_CONTAINER:
      NOTREACHED() << "NOTIFICATION_CONTAINER does not have an icon, and it is "
                      "added to the model by NotificationMenuController.";
      return gfx::kNoneIcon;
    case ash::STOP_APP:
      if (string_id == IDS_CROSTINI_SHUT_DOWN_LINUX_MENU_ITEM)
        return views::kLinuxShutdownIcon;
      return gfx::kNoneIcon;
    default:
      NOTREACHED();
      return gfx::kNoneIcon;
  }
}

void AppContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case ash::TOGGLE_PIN:
      TogglePin(app_id_);
      break;
  }
}

const gfx::VectorIcon* AppContextMenu::GetVectorIconForCommandId(
    int command_id) const {
  DCHECK_EQ(command_id, ash::TOGGLE_PIN);
  const gfx::VectorIcon& icon =
      GetMenuItemVectorIcon(command_id, controller_->IsAppPinned(app_id_)
                                            ? IDS_APP_LIST_CONTEXT_MENU_UNPIN
                                            : IDS_APP_LIST_CONTEXT_MENU_PIN);
  return &icon;
}

}  // namespace app_list
