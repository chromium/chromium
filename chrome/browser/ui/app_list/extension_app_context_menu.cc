// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_app_context_menu.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/context_menu_params.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/vector_icons.h"

namespace app_list {

namespace {

bool disable_installed_extension_check_for_testing = false;

bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
}

}  // namespace

ExtensionAppContextMenu::ExtensionAppContextMenu(
    AppContextMenuDelegate* delegate,
    Profile* profile,
    const std::string& app_id,
    AppListControllerDelegate* controller,
    bool is_platform_app)
    : AppContextMenu(delegate, profile, app_id, controller),
      is_platform_app_(is_platform_app) {}

ExtensionAppContextMenu::~ExtensionAppContextMenu() {}

// static
void ExtensionAppContextMenu::DisableInstalledExtensionCheckForTesting(
    bool disable) {
  disable_installed_extension_check_for_testing = disable;
}

int ExtensionAppContextMenu::GetLaunchStringId() const {
  bool launch_in_window = IsCommandIdChecked(ash::USE_LAUNCH_TYPE_WINDOW);
  return launch_in_window ? IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW
                          : IDS_APP_LIST_CONTEXT_MENU_NEW_TAB;
}

void ExtensionAppContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  if (!disable_installed_extension_check_for_testing &&
      !controller()->IsExtensionInstalled(profile(), app_id())) {
    std::move(callback).Run(nullptr);
    return;
  }

  AppContextMenu::GetMenuModel(std::move(callback));
}

void ExtensionAppContextMenu::BuildMenu(ui::SimpleMenuModel* menu_model) {
  if (app_id() == extension_misc::kChromeAppId) {
    AddContextMenuOption(menu_model, ash::APP_CONTEXT_MENU_NEW_WINDOW,
                         IDS_APP_LIST_NEW_WINDOW);
    if (!profile()->IsOffTheRecord()) {
      AddContextMenuOption(menu_model,
                           ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW,
                           IDS_APP_LIST_NEW_INCOGNITO_WINDOW);
    }
    if (controller()->CanDoShowAppInfoFlow()) {
      AddContextMenuOption(menu_model, ash::SHOW_APP_INFO,
                           IDS_APP_CONTEXT_MENU_SHOW_INFO);
    }
  } else {
    extension_menu_items_ = std::make_unique<extensions::ContextMenuMatcher>(
        profile(), this, menu_model,
        base::Bind(MenuItemHasLauncherContext));

    bool is_system_web_app = web_app::WebAppProvider::Get(profile())
                                 ->system_web_app_manager()
                                 .IsSystemWebApp(app_id());

    // First, add the primary actions.
    if (!is_platform_app_ && !is_system_web_app)
      CreateOpenNewSubmenu(menu_model);

    // Create default items.
    AppContextMenu::BuildMenu(menu_model);

    // Assign unique IDs to commands added by the app itself.
    int index = ash::USE_LAUNCH_TYPE_COMMAND_END;
    extension_menu_items_->AppendExtensionItems(
        extensions::MenuItem::ExtensionKey(app_id()),
        base::string16(),
        &index,
        false);  // is_action_menu

    const int appended_count = index - ash::USE_LAUNCH_TYPE_COMMAND_END;
    AddMenuItemIconsForSystemApps(app_id(), menu_model,
                                  menu_model->GetItemCount() - appended_count,
                                  appended_count);

    if (!is_platform_app_)
      AddContextMenuOption(menu_model, ash::OPTIONS, IDS_NEW_TAB_APP_OPTIONS);

    AddContextMenuOption(menu_model, ash::UNINSTALL,
                         is_platform_app_ ? IDS_APP_LIST_UNINSTALL_ITEM
                                          : IDS_APP_LIST_EXTENSIONS_UNINSTALL);

    if (controller()->CanDoShowAppInfoFlow() && !is_system_web_app) {
      AddContextMenuOption(menu_model, ash::SHOW_APP_INFO,
                           IDS_APP_CONTEXT_MENU_SHOW_INFO);
    }
  }
}

base::string16 ExtensionAppContextMenu::GetLabelForCommandId(
    int command_id) const {
  if (command_id == ash::LAUNCH_NEW)
    return l10n_util::GetStringUTF16(GetLaunchStringId());

  return AppContextMenu::GetLabelForCommandId(command_id);
}

const gfx::VectorIcon* ExtensionAppContextMenu::GetVectorIconForCommandId(
    int command_id) const {
  if (command_id == ash::LAUNCH_NEW)
    return &GetMenuItemVectorIcon(ash::LAUNCH_NEW, GetLaunchStringId());

  return AppContextMenu::GetVectorIconForCommandId(command_id);
}

bool ExtensionAppContextMenu::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == ash::LAUNCH_NEW ||
         AppContextMenu::IsItemForCommandIdDynamic(command_id);
}

bool ExtensionAppContextMenu::IsCommandIdChecked(int command_id) const {
  if (command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
      command_id < ash::USE_LAUNCH_TYPE_COMMAND_END) {
    return static_cast<int>(
               controller()->GetExtensionLaunchType(profile(), app_id())) +
               ash::USE_LAUNCH_TYPE_COMMAND_START ==
           command_id;
  } else if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
                 command_id)) {
    return extension_menu_items_->IsCommandIdChecked(command_id);
  }
  return AppContextMenu::IsCommandIdChecked(command_id);
}

bool ExtensionAppContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id == ash::OPTIONS) {
    return controller()->HasOptionsPage(profile(), app_id());
  } else if (command_id == ash::UNINSTALL) {
    return controller()->UninstallAllowed(profile(), app_id());
  } else if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
                 command_id)) {
    return extension_menu_items_->IsCommandIdEnabled(command_id);
  } else if (command_id == ash::APP_CONTEXT_MENU_NEW_WINDOW) {
    // "Normal" windows are not allowed when incognito is enforced.
    return IncognitoModePrefs::GetAvailability(profile()->GetPrefs()) !=
        IncognitoModePrefs::FORCED;
  } else if (command_id == ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW) {
    // Incognito windows are not allowed when incognito is disabled.
    return IncognitoModePrefs::GetAvailability(profile()->GetPrefs()) !=
        IncognitoModePrefs::DISABLED;
  }
  return AppContextMenu::IsCommandIdEnabled(command_id);
}

void ExtensionAppContextMenu::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == ash::LAUNCH_NEW) {
    delegate()->ExecuteLaunchCommand(event_flags);
  } else if (command_id == ash::SHOW_APP_INFO) {
    controller()->DoShowAppInfoFlow(profile(), app_id());
  } else if (command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
             command_id < ash::USE_LAUNCH_TYPE_COMMAND_END) {
    // Hosted apps can only toggle between LAUNCH_TYPE_WINDOW and
    // LAUNCH_TYPE_REGULAR.
    extensions::LaunchType launch_type =
        (controller()->GetExtensionLaunchType(profile(), app_id()) ==
         extensions::LAUNCH_TYPE_WINDOW)
            ? extensions::LAUNCH_TYPE_REGULAR
            : extensions::LAUNCH_TYPE_WINDOW;
    controller()->SetExtensionLaunchType(profile(), app_id(), launch_type);
  } else if (command_id == ash::OPTIONS) {
    controller()->ShowOptionsPage(profile(), app_id());
  } else if (command_id == ash::UNINSTALL) {
    controller()->UninstallApp(profile(), app_id());
  } else if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
                 command_id)) {
    extension_menu_items_->ExecuteCommand(command_id, nullptr, nullptr,
                                          content::ContextMenuParams());
  } else if (command_id == ash::APP_CONTEXT_MENU_NEW_WINDOW) {
    controller()->CreateNewWindow(profile(), false);
  } else if (command_id == ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW) {
    controller()->CreateNewWindow(profile(), true);
  } else {
    AppContextMenu::ExecuteCommand(command_id, event_flags);
  }
}

void ExtensionAppContextMenu::CreateOpenNewSubmenu(
    ui::SimpleMenuModel* menu_model) {
  // Touchable extension context menus use an actionable submenu for LAUNCH_NEW.
  const int kGroupId = 1;
  open_new_submenu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  open_new_submenu_model_->AddRadioItemWithStringId(
      ash::USE_LAUNCH_TYPE_REGULAR, IDS_APP_LIST_CONTEXT_MENU_NEW_TAB,
      kGroupId);
  open_new_submenu_model_->AddRadioItemWithStringId(
      ash::USE_LAUNCH_TYPE_WINDOW, IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW,
      kGroupId);
  menu_model->AddActionableSubmenuWithStringIdAndIcon(
      ash::LAUNCH_NEW, GetLaunchStringId(), open_new_submenu_model_.get(),
      GetMenuItemVectorIcon(ash::LAUNCH_NEW, GetLaunchStringId()));
}

}  // namespace app_list
