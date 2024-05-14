// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/extension_shelf_context_menu.h"

#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "base/functional/bind.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/app_list/extension_app_utils.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/browser_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/app_constants/constants.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "content/public/browser/context_menu_params.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// A helper used to filter which menu items added by the extension are shown.
bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
}

// Whether the user has permission to modify the given app's settings.
bool UninstallAllowed(const std::string& app_id, Profile* profile) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  const extensions::ManagementPolicy* policy =
      extensions::ExtensionSystem::Get(profile)->management_policy();
  return extension && policy->UserMayModifySettings(extension, nullptr) &&
         !policy->MustRemainInstalled(extension, nullptr);
}

}  // namespace

ExtensionShelfContextMenu::ExtensionShelfContextMenu(
    ChromeShelfController* controller,
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

  if (item().type == ash::TYPE_PINNED_APP || item().type == ash::TYPE_APP) {
    CreateOpenNewSubmenu(menu_model.get());
    AddPinMenu(menu_model.get());

    if (controller()->IsOpen(item().id)) {
      AddContextMenuOption(menu_model.get(), ash::MENU_CLOSE,
                           IDS_SHELF_CONTEXT_MENU_CLOSE);
    }
  } else if (item().type == ash::TYPE_BROWSER_SHORTCUT ||
             item().type == ash::TYPE_UNPINNED_BROWSER_SHORTCUT) {
    // TODO(crbug.com/40177234): Consider how to support Lacros.
    // Lacros is provided from AppService, so here is not reached.
    AddContextMenuOption(menu_model.get(), ash::APP_CONTEXT_MENU_NEW_WINDOW,
                         IDS_APP_LIST_NEW_WINDOW);
    if (!profile->IsGuestSession()) {
      AddContextMenuOption(menu_model.get(),
                           ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW,
                           IDS_APP_LIST_NEW_INCOGNITO_WINDOW);
    }
    if (!BrowserShortcutShelfItemController::IsListOfActiveBrowserEmpty(
            controller()->shelf_model()) ||
        item().type == ash::TYPE_DIALOG || controller()->IsOpen(item().id)) {
      AddContextMenuOption(menu_model.get(), ash::MENU_CLOSE,
                           IDS_SHELF_CONTEXT_MENU_CLOSE);
    }
  }
  if (app_id != app_constants::kChromeAppId) {
    AddContextMenuOption(menu_model.get(), ash::UNINSTALL,
                         IDS_APP_LIST_EXTENSIONS_UNINSTALL);
  }

  if (controller()->CanDoShowAppInfoFlow(app_id)) {
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

ui::ImageModel ExtensionShelfContextMenu::GetIconForCommandId(
    int command_id) const {
  if (command_id == ash::LAUNCH_NEW) {
    const gfx::VectorIcon& icon =
        GetCommandIdVectorIcon(command_id, GetLaunchTypeStringId());
    return ui::ImageModel::FromVectorIcon(
        icon, apps::GetColorIdForMenuItemIcon(), ash::kAppContextMenuIconSize);
  }
  return ShelfContextMenu::GetIconForCommandId(command_id);
}

std::u16string ExtensionShelfContextMenu::GetLabelForCommandId(
    int command_id) const {
  if (command_id == ash::LAUNCH_NEW)
    return l10n_util::GetStringUTF16(GetLaunchTypeStringId());

  return ShelfContextMenu::GetLabelForCommandId(command_id);
}

bool ExtensionShelfContextMenu::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case ash::USE_LAUNCH_TYPE_REGULAR:
      return GetLaunchType() == extensions::LAUNCH_TYPE_REGULAR;
    case ash::USE_LAUNCH_TYPE_WINDOW:
      return GetLaunchType() == extensions::LAUNCH_TYPE_WINDOW;
    case ash::USE_LAUNCH_TYPE_TABBED_WINDOW:
      // Tabbed window is not supported for extension based items.
      [[fallthrough]];
    case ash::DEPRECATED_USE_LAUNCH_TYPE_PINNED:
    case ash::DEPRECATED_USE_LAUNCH_TYPE_FULLSCREEN:
      NOTREACHED_IN_MIGRATION();
      return false;
    default:
      if (command_id < ash::COMMAND_ID_COUNT)
        return ShelfContextMenu::IsCommandIdChecked(command_id);
      return (extension_items_ &&
              extension_items_->IsCommandIdChecked(command_id));
  }
}

bool ExtensionShelfContextMenu::IsCommandIdEnabled(int command_id) const {
  Profile* profile = controller()->profile();
  switch (command_id) {
    case ash::UNINSTALL:
      return UninstallAllowed(item().id.app_id, profile);
    case ash::APP_CONTEXT_MENU_NEW_WINDOW:
      // "Normal" windows are not allowed when incognito is enforced.
      return IncognitoModePrefs::GetAvailability(profile->GetPrefs()) !=
             policy::IncognitoModeAvailability::kForced;
    case ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW:
      // Incognito windows are not allowed when incognito is disabled.
      return IncognitoModePrefs::GetAvailability(profile->GetPrefs()) !=
             policy::IncognitoModeAvailability::kDisabled;
    default:
      if (command_id < ash::COMMAND_ID_COUNT)
        return ShelfContextMenu::IsCommandIdEnabled(command_id);
      return (extension_items_ &&
              extension_items_->IsCommandIdEnabled(command_id));
  }
}

bool ExtensionShelfContextMenu::IsItemForCommandIdDynamic(
    int command_id) const {
  return command_id == ash::LAUNCH_NEW ||
         ShelfContextMenu::IsItemForCommandIdDynamic(command_id);
}

void ExtensionShelfContextMenu::ExecuteCommand(int command_id,
                                               int event_flags) {
  if (ExecuteCommonCommand(command_id, event_flags))
    return;

  // Place new windows on the same display as the context menu.
  display::ScopedDisplayForNewWindows scoped_display(display_id());

  switch (static_cast<ash::CommandId>(command_id)) {
    case ash::SHOW_APP_INFO:
      controller()->DoShowAppInfoFlow(item().id.app_id);
      break;
    case ash::USE_LAUNCH_TYPE_REGULAR:
      SetLaunchType(extensions::LAUNCH_TYPE_REGULAR);
      break;
    case ash::USE_LAUNCH_TYPE_WINDOW: {
      // Hosted apps can only toggle between LAUNCH_WINDOW and LAUNCH_REGULAR.
      extensions::LaunchType launch_type =
          GetLaunchType() == extensions::LAUNCH_TYPE_WINDOW
              ? extensions::LAUNCH_TYPE_REGULAR
              : extensions::LAUNCH_TYPE_WINDOW;
      SetLaunchType(launch_type);
      break;
    }
    case ash::USE_LAUNCH_TYPE_TABBED_WINDOW:
      // Tabbed window is not supported for extension based items.
      [[fallthrough]];
    case ash::DEPRECATED_USE_LAUNCH_TYPE_PINNED:
    case ash::DEPRECATED_USE_LAUNCH_TYPE_FULLSCREEN:
      NOTREACHED_IN_MIGRATION();
      break;
    case ash::APP_CONTEXT_MENU_NEW_WINDOW:
      ash::NewWindowDelegate::GetInstance()->NewWindow(
          /*incognito=*/false,
          /*should_trigger_session_restore=*/false);
      break;
    case ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW:
      ash::NewWindowDelegate::GetInstance()->NewWindow(
          /*incognito=*/true,
          /*should_trigger_session_restore=*/false);
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
  // LAUNCH_NEW.
  const int kGroupId = 1;
  open_new_submenu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  open_new_submenu_model_->AddRadioItemWithStringId(
      ash::USE_LAUNCH_TYPE_REGULAR, IDS_APP_LIST_CONTEXT_MENU_NEW_TAB,
      kGroupId);
  open_new_submenu_model_->AddRadioItemWithStringId(
      ash::USE_LAUNCH_TYPE_WINDOW, IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW,
      kGroupId);
  menu_model->AddActionableSubMenu(
      ash::LAUNCH_NEW, l10n_util::GetStringUTF16(GetLaunchTypeStringId()),
      open_new_submenu_model_.get());
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
