// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/extension_shelf_context_menu.h"

#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "base/bind.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/context_menu_params.h"
#include "extensions/browser/extension_prefs.h"
#include "ui/base/models/image_model.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/display/screen.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// A helper used to filter which menu items added by the extension are shown.
bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
}

}  // namespace

ExtensionShelfContextMenu::ExtensionShelfContextMenu(
    ChromeLauncherController* controller,
    const ash::ShelfItem* item,
    int64_t display_id)
    : ShelfContextMenu(controller, item, display_id) {}

ExtensionShelfContextMenu::~ExtensionShelfContextMenu() = default;

void ExtensionShelfContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  auto menu_model = GetBaseMenuModel();
  Profile* profile = controller()->profile();
  const std::string app_id = item().id.app_id;

  extension_items_ = std::make_unique<extensions::ContextMenuMatcher>(
      profile, this, menu_model.get(),
      base::BindRepeating(MenuItemHasLauncherContext));
  // V1 apps can be started from the menu - but V2 apps and system web apps
  // should not.
  bool is_system_web_app = web_app::WebAppProvider::Get(profile)
                               ->system_web_app_manager()
                               .IsSystemWebApp(app_id);
  const bool is_platform_app = controller()->IsPlatformApp(item().id);

  if (item().type == ash::TYPE_PINNED_APP || item().type == ash::TYPE_APP) {
    if (!is_platform_app && !is_system_web_app)
      CreateOpenNewSubmenu(menu_model.get());
    AddPinMenu(menu_model.get());

    if (controller()->IsOpen(item().id)) {
      AddContextMenuOption(menu_model.get(), ash::MENU_CLOSE,
                           IDS_SHELF_CONTEXT_MENU_CLOSE);
    }
  } else if (item().type == ash::TYPE_BROWSER_SHORTCUT ||
             item().type == ash::TYPE_UNPINNED_BROWSER_SHORTCUT) {
    AddContextMenuOption(menu_model.get(), ash::MENU_NEW_WINDOW,
                         IDS_APP_LIST_NEW_WINDOW);
    if (!profile->IsGuestSession()) {
      AddContextMenuOption(menu_model.get(), ash::MENU_NEW_INCOGNITO_WINDOW,
                           IDS_APP_LIST_NEW_INCOGNITO_WINDOW);
    }
    if (!BrowserShortcutLauncherItemController::IsListOfActiveBrowserEmpty(
            controller()->shelf_model()) ||
        item().type == ash::TYPE_DIALOG || controller()->IsOpen(item().id)) {
      AddContextMenuOption(menu_model.get(), ash::MENU_CLOSE,
                           IDS_SHELF_CONTEXT_MENU_CLOSE);
    }
  }
  if (app_id != extension_misc::kChromeAppId) {
    AddContextMenuOption(menu_model.get(), ash::UNINSTALL,
                         is_platform_app ? IDS_APP_LIST_UNINSTALL_ITEM
                                         : IDS_APP_LIST_EXTENSIONS_UNINSTALL);
  }

  if (controller()->CanDoShowAppInfoFlow(profile, app_id)) {
    AddContextMenuOption(menu_model.get(), ash::SHOW_APP_INFO,
                         IDS_APP_CONTEXT_MENU_SHOW_INFO);
  }

  if (item().type == ash::TYPE_PINNED_APP || item().type == ash::TYPE_APP) {
    const extensions::MenuItem::ExtensionKey app_key(app_id);
    if (!app_key.empty()) {
      int index = 0;
      extension_items_->AppendExtensionItems(app_key, std::u16string(), &index,
                                             false);  // is_action_menu
      app_list::AddMenuItemIconsForSystemApps(
          app_id, menu_model.get(), menu_model.get()->GetItemCount() - index,
          index);
    }
  }
  std::move(callback).Run(std::move(menu_model));
}

bool ExtensionShelfContextMenu::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case ash::LAUNCH_TYPE_PINNED_TAB:
      return GetLaunchType() == extensions::LAUNCH_TYPE_PINNED;
    case ash::LAUNCH_TYPE_REGULAR_TAB:
      return GetLaunchType() == extensions::LAUNCH_TYPE_REGULAR;
    case ash::LAUNCH_TYPE_WINDOW:
      return GetLaunchType() == extensions::LAUNCH_TYPE_WINDOW;
    case ash::LAUNCH_TYPE_FULLSCREEN:
      return GetLaunchType() == extensions::LAUNCH_TYPE_FULLSCREEN;
    default:
      if (command_id < ash::COMMAND_ID_COUNT)
        return ShelfContextMenu::IsCommandIdChecked(command_id);
      return (extension_items_ &&
              extension_items_->IsCommandIdChecked(command_id));
  }
}

bool ExtensionShelfContextMenu::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case ash::UNINSTALL:
      return controller()->UninstallAllowed(item().id.app_id);
    case ash::MENU_NEW_WINDOW:
      // "Normal" windows are not allowed when incognito is enforced.
      return IncognitoModePrefs::GetAvailability(
                 controller()->profile()->GetPrefs()) !=
             IncognitoModePrefs::FORCED;
    case ash::MENU_NEW_INCOGNITO_WINDOW:
      // Incognito windows are not allowed when incognito is disabled.
      return IncognitoModePrefs::GetAvailability(
                 controller()->profile()->GetPrefs()) !=
             IncognitoModePrefs::DISABLED;
    default:
      if (command_id < ash::COMMAND_ID_COUNT)
        return ShelfContextMenu::IsCommandIdEnabled(command_id);
      return (extension_items_ &&
              extension_items_->IsCommandIdEnabled(command_id));
  }
}

void ExtensionShelfContextMenu::ExecuteCommand(int command_id,
                                               int event_flags) {
  if (ExecuteCommonCommand(command_id, event_flags))
    return;

  // Place new windows on the same display as the context menu.
  display::ScopedDisplayForNewWindows scoped_display(display_id());

  switch (static_cast<ash::CommandId>(command_id)) {
    case ash::SHOW_APP_INFO:
      controller()->DoShowAppInfoFlow(controller()->profile(),
                                      item().id.app_id);
      break;
    case ash::LAUNCH_TYPE_PINNED_TAB:
      SetLaunchType(extensions::LAUNCH_TYPE_PINNED);
      break;
    case ash::LAUNCH_TYPE_REGULAR_TAB:
      SetLaunchType(extensions::LAUNCH_TYPE_REGULAR);
      break;
    case ash::LAUNCH_TYPE_WINDOW: {
      // Hosted apps can only toggle between LAUNCH_WINDOW and LAUNCH_REGULAR.
      extensions::LaunchType launch_type =
          GetLaunchType() == extensions::LAUNCH_TYPE_WINDOW
              ? extensions::LAUNCH_TYPE_REGULAR
              : extensions::LAUNCH_TYPE_WINDOW;
      SetLaunchType(launch_type);
      break;
    }
    case ash::LAUNCH_TYPE_FULLSCREEN:
      SetLaunchType(extensions::LAUNCH_TYPE_FULLSCREEN);
      break;
    case ash::MENU_NEW_WINDOW:
      ash::NewWindowDelegate::GetInstance()->NewWindow(/*incognito=*/false);
      break;
    case ash::MENU_NEW_INCOGNITO_WINDOW:
      ash::NewWindowDelegate::GetInstance()->NewWindow(/*incognito=*/true);
      break;
    default:
      if (extension_items_) {
        extension_items_->ExecuteCommand(command_id, nullptr, nullptr,
                                         content::ContextMenuParams());
      }
  }
}

void ExtensionShelfContextMenu::CreateOpenNewSubmenu(
    ui::SimpleMenuModel* menu_model) {
  // Touchable extension context menus use an actionable submenu for
  // MENU_OPEN_NEW.
  const int kGroupId = 1;
  open_new_submenu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  open_new_submenu_model_->AddRadioItemWithStringId(
      ash::LAUNCH_TYPE_REGULAR_TAB, IDS_APP_LIST_CONTEXT_MENU_NEW_TAB,
      kGroupId);
  open_new_submenu_model_->AddRadioItemWithStringId(
      ash::LAUNCH_TYPE_WINDOW, IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW, kGroupId);
  menu_model->AddActionableSubmenuWithStringIdAndIcon(
      ash::MENU_OPEN_NEW, GetLaunchTypeStringId(),
      open_new_submenu_model_.get(),
      ui::ImageModel::FromVectorIcon(
          GetCommandIdVectorIcon(ash::MENU_OPEN_NEW, GetLaunchTypeStringId()),
          /*color_id=*/-1, ash::kAppContextMenuIconSize));
}

extensions::LaunchType ExtensionShelfContextMenu::GetLaunchType() const {
  const extensions::Extension* extension =
      GetExtensionForAppID(item().id.app_id, controller()->profile());

  // An extension can be unloaded/updated/unavailable at any time.
  if (!extension)
    return extensions::LAUNCH_TYPE_DEFAULT;

  return extensions::GetLaunchType(
      extensions::ExtensionPrefs::Get(controller()->profile()), extension);
}

void ExtensionShelfContextMenu::SetLaunchType(extensions::LaunchType type) {
  extensions::SetLaunchType(controller()->profile(), item().id.app_id, type);
}

int ExtensionShelfContextMenu::GetLaunchTypeStringId() const {
  return (GetLaunchType() == extensions::LAUNCH_TYPE_PINNED ||
          GetLaunchType() == extensions::LAUNCH_TYPE_REGULAR)
             ? IDS_APP_LIST_CONTEXT_MENU_NEW_TAB
             : IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW;
}
