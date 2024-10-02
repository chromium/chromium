// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/app_service_shelf_context_menu.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/extension_apps_utils.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/extension_app_utils.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_shelf_utils.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/arc_app_shelf_id.h"
#include "chrome/browser/ui/ash/shelf/browser_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/shelf_context_menu.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/crostini/crostini_app_restart_dialog.h"
#include "chrome/browser/ui/webui/ash/settings/app_management/app_management_uma.h"
#include "chrome/grit/generated_resources.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/browser/context_menu_params.h"
#include "extensions/browser/extension_prefs.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/gfx/vector_icon_types.h"

namespace {

bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
}

apps::WindowMode ConvertLaunchTypeCommandToWindowMode(int command_id) {
  switch (command_id) {
    case ash::USE_LAUNCH_TYPE_REGULAR:
      return apps::WindowMode::kBrowser;
    case ash::USE_LAUNCH_TYPE_WINDOW:
      return apps::WindowMode::kWindow;
    case ash::USE_LAUNCH_TYPE_TABBED_WINDOW:
      return apps::WindowMode::kTabbedWindow;
    default:
      return apps::WindowMode::kUnknown;
  }
}

extensions::LaunchType ConvertLaunchTypeCommandToExtensionLaunchType(
    int command_id) {
  switch (command_id) {
    case ash::USE_LAUNCH_TYPE_REGULAR:
      return extensions::LAUNCH_TYPE_REGULAR;
    case ash::USE_LAUNCH_TYPE_WINDOW:
      return extensions::LAUNCH_TYPE_WINDOW;
    case ash::USE_LAUNCH_TYPE_TABBED_WINDOW:
      // Not supported for extensions.
      [[fallthrough]];
    case ash::DEPRECATED_USE_LAUNCH_TYPE_PINNED:
    case ash::DEPRECATED_USE_LAUNCH_TYPE_FULLSCREEN:
      [[fallthrough]];
    default:
      NOTREACHED_IN_MIGRATION();
      return extensions::LAUNCH_TYPE_INVALID;
  }
}

std::string GetAppId(const ash::ShelfID& shelf_id) {
  // Remove the ARC shelf group prefix.
  const arc::ArcAppShelfId arc_shelf_id =
      arc::ArcAppShelfId::FromString(shelf_id.app_id);
  if (arc_shelf_id.valid())
    return arc_shelf_id.app_id();

  return shelf_id.app_id;
}

void MaybeCloseFullRestoreServiceNotification(Profile* profile) {
  if (auto* full_restore_service =
          ash::full_restore::FullRestoreServiceFactory::GetForProfile(
              profile)) {
    full_restore_service->MaybeCloseNotification();
  }
}

}  // namespace

AppServiceShelfContextMenu::AppServiceShelfContextMenu(
    ChromeShelfController* controller,
    const ash::ShelfItem* item,
    int64_t display_id)
    : ShelfContextMenu(controller, item, display_id) {
  if (guest_os::IsUnregisteredCrostiniShelfAppId(item->id.app_id) ||
      borealis::BorealisWindowManager::IsAnonymousAppId(item->id.app_id)) {
    // Sometimes GuestOS runs applications that are not registered with the apps
    // service. These "anonymous" apps should not be pinnable, so we set type
    // "unknown" to avoid the ARC check below.
    app_type_ = apps::AppType::kUnknown;
    return;
  }

  app_type_ = apps::AppServiceProxyFactory::GetForProfile(controller->profile())
                  ->AppRegistryCache()
                  .GetAppType(GetAppId(item->id));
}

AppServiceShelfContextMenu::~AppServiceShelfContextMenu() = default;

ui::ImageModel AppServiceShelfContextMenu::GetIconForCommandId(
    int command_id) const {
  if (command_id == ash::LAUNCH_NEW ||
      command_id == ash::SHUTDOWN_BRUSCHETTA_OS) {
    const gfx::VectorIcon& icon =
        GetCommandIdVectorIcon(command_id, launch_new_string_id_);
    return ui::ImageModel::FromVectorIcon(
        icon, apps::GetColorIdForMenuItemIcon(), ash::kAppContextMenuIconSize);
  }
  return ShelfContextMenu::GetIconForCommandId(command_id);
}

std::u16string AppServiceShelfContextMenu::GetLabelForCommandId(
    int command_id) const {
  if (command_id == ash::LAUNCH_NEW) {
    CHECK_GT(launch_new_string_id_, 0)
        << "Unexpected `launch_new_string_id_` value. App id = "
        << item().id.app_id << "; app type = " << apps::EnumToString(app_type_)
        << "; submenu items count = " << submenu_->GetItemCount();
    return l10n_util::GetStringUTF16(launch_new_string_id_);
  } else if (command_id == ash::SHUTDOWN_BRUSCHETTA_OS) {
    return l10n_util::GetStringFUTF16(
        IDS_BRUSCHETTA_SHUT_DOWN_LINUX_MENU_ITEM,
        base::UTF8ToUTF16(
            bruschetta::GetBruschettaDisplayName(controller()->profile())));
  }
  return ShelfContextMenu::GetLabelForCommandId(command_id);
}

void AppServiceShelfContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  apps::AppServiceProxyFactory::GetForProfile(controller()->profile())
      ->GetMenuModel(
          item().id.app_id, apps::MenuType::kShelf, display_id(),
          base::BindOnce(&AppServiceShelfContextMenu::OnGetMenuModel,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AppServiceShelfContextMenu::ExecuteCommand(int command_id,
                                                int event_flags) {
  // Place new windows on the same display as the context menu.
  display::ScopedDisplayForNewWindows scoped_display(display_id());
  if (ExecuteCommonCommand(command_id, event_flags))
    return;

  switch (command_id) {
    case ash::SHOW_APP_INFO:
      ShowAppInfo();
      MaybeCloseFullRestoreServiceNotification(controller()->profile());
      break;

    case ash::APP_CONTEXT_MENU_NEW_WINDOW:
      if (app_type_ == apps::AppType::kCrostini ||
          app_type_ == apps::AppType::kBruschetta) {
        ShelfContextMenu::ExecuteCommand(ash::LAUNCH_NEW, event_flags);
      } else if (app_type_ == apps::AppType::kStandaloneBrowser) {
        crosapi::BrowserManager::Get()->NewWindow(
            /*incognito=*/false, /*should_trigger_session_restore=*/false);
      } else {
        ash::NewWindowDelegate::GetInstance()->NewWindow(
            /*incognito=*/false,
            /*should_trigger_session_restore=*/false);
      }
      MaybeCloseFullRestoreServiceNotification(controller()->profile());
      break;

    case ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW:
      if (app_type_ == apps::AppType::kStandaloneBrowser) {
        crosapi::BrowserManager::Get()->NewWindow(
            /*incognito=*/true, /*should_trigger_session_restore=*/false);
      } else {
        ash::NewWindowDelegate::GetInstance()->NewWindow(
            /*incognito=*/true,
            /*should_trigger_session_restore=*/false);
      }
      MaybeCloseFullRestoreServiceNotification(controller()->profile());
      break;

    case ash::SHUTDOWN_GUEST_OS:
      if (item().id.app_id == guest_os::kTerminalSystemAppId) {
        crostini::CrostiniManager::GetForProfile(controller()->profile())
            ->StopRunningVms(base::DoNothing());
      } else if (item().id.app_id == plugin_vm::kPluginVmShelfAppId) {
        plugin_vm::PluginVmManagerFactory::GetForProfile(
            controller()->profile())
            ->StopPluginVm(plugin_vm::kPluginVmName, /*force=*/false);
      } else {
        LOG(ERROR) << "App " << item().id.app_id
                   << " should not have a shutdown guest OS command.";
      }
      break;

    case ash::SHUTDOWN_BRUSCHETTA_OS:
      if (item().id.app_id == guest_os::kTerminalSystemAppId) {
        bruschetta::BruschettaServiceFactory::GetForProfile(
            controller()->profile())
            ->StopRunningVms();
      } else {
        LOG(ERROR) << "App " << item().id.app_id
                   << " should not have a shutdown Bruschetta command.";
      }
      break;

    case ash::USE_LAUNCH_TYPE_TABBED_WINDOW:
      [[fallthrough]];
    case ash::USE_LAUNCH_TYPE_REGULAR:
      [[fallthrough]];
    case ash::USE_LAUNCH_TYPE_WINDOW:
      launch_new_string_id_ = apps::StringIdForUseLaunchTypeCommand(command_id);
      SetLaunchType(command_id);
      break;

    case ash::DEPRECATED_USE_LAUNCH_TYPE_FULLSCREEN:
    case ash::DEPRECATED_USE_LAUNCH_TYPE_PINNED:
      NOTREACHED_IN_MIGRATION();
      break;

    case ash::CROSTINI_USE_LOW_DENSITY:
    case ash::CROSTINI_USE_HIGH_DENSITY: {
      auto* registry_service =
          guest_os::GuestOsRegistryServiceFactory::GetForProfile(
              controller()->profile());
      const bool scaled = command_id == ash::CROSTINI_USE_LOW_DENSITY;
      registry_service->SetAppScaled(item().id.app_id, scaled);
      if (controller()->IsOpen(item().id))
        crostini::ShowAppRestartDialog(display_id());
      return;
    }

    case ash::SETTINGS:
      if (item().id.app_id == guest_os::kTerminalSystemAppId) {
        guest_os::LaunchTerminalSettings(controller()->profile(), display_id());
        MaybeCloseFullRestoreServiceNotification(controller()->profile());
      }
      return;

    default:
      if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
              command_id)) {
        extension_menu_items_->ExecuteCommand(command_id, nullptr, nullptr,
                                              content::ContextMenuParams());
        return;
      }

      if (command_id >= ash::LAUNCH_APP_SHORTCUT_FIRST &&
          command_id <= ash::LAUNCH_APP_SHORTCUT_LAST) {
        ExecutePublisherContextMenuCommand(command_id);
        return;
      }

      ShelfContextMenu::ExecuteCommand(command_id, event_flags);
  }
}

bool AppServiceShelfContextMenu::IsCommandIdChecked(int command_id) const {
  switch (app_type_) {
    case apps::AppType::kStandaloneBrowserChromeApp:
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb: {
      if (command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
          command_id < ash::USE_LAUNCH_TYPE_COMMAND_END) {
        auto user_window_mode = apps::WindowMode::kUnknown;
        apps::AppServiceProxyFactory::GetForProfile(controller()->profile())
            ->AppRegistryCache()
            .ForOneApp(item().id.app_id,
                       [&user_window_mode](const apps::AppUpdate& update) {
                         user_window_mode = update.WindowMode();
                       });
        return user_window_mode != apps::WindowMode::kUnknown &&
               user_window_mode ==
                   ConvertLaunchTypeCommandToWindowMode(command_id);
      }
      return ShelfContextMenu::IsCommandIdChecked(command_id);
    }
    case apps::AppType::kChromeApp:
      if (command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
          command_id < ash::USE_LAUNCH_TYPE_COMMAND_END) {
        return GetExtensionLaunchType() ==
               ConvertLaunchTypeCommandToExtensionLaunchType(command_id);
      } else if (command_id < ash::COMMAND_ID_COUNT) {
        return ShelfContextMenu::IsCommandIdChecked(command_id);
      } else {
        return (extension_menu_items_ &&
                extension_menu_items_->IsCommandIdChecked(command_id));
      }
    case apps::AppType::kArc:
      [[fallthrough]];
    case apps::AppType::kCrostini:
      [[fallthrough]];
    case apps::AppType::kBuiltIn:
      [[fallthrough]];
    case apps::AppType::kPluginVm:
      [[fallthrough]];
    case apps::AppType::kBorealis:
      [[fallthrough]];
    default:
      return ShelfContextMenu::IsCommandIdChecked(command_id);
  }
}

bool AppServiceShelfContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id < ash::COMMAND_ID_COUNT)
    return ShelfContextMenu::IsCommandIdEnabled(command_id);
  if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(command_id) &&
      extension_menu_items_) {
    return extension_menu_items_->IsCommandIdEnabled(command_id);
  }
  return true;
}

bool AppServiceShelfContextMenu::IsItemForCommandIdDynamic(
    int command_id) const {
  return command_id == ash::LAUNCH_NEW ||
         command_id == ash::SHUTDOWN_BRUSCHETTA_OS ||
         ShelfContextMenu::IsItemForCommandIdDynamic(command_id);
}

void AppServiceShelfContextMenu::OnGetMenuModel(GetMenuModelCallback callback,
                                                apps::MenuItems menu_items) {
  auto menu_model = GetBaseMenuModel();
  submenu_ = std::make_unique<ui::SimpleMenuModel>(this);
  size_t index = 0;

  if (!menu_items.items.empty() &&
      menu_items.items[0]->command_id == ash::LAUNCH_NEW) {
    apps::PopulateLaunchNewItemFromMenuItem(menu_items.items[0],
                                            menu_model.get(), submenu_.get(),
                                            &launch_new_string_id_);
    ++index;
  }

  // The special rule to ensure that FilesManager's first menu item is "New
  // window".
  const bool build_extension_menu_before_pin =
      (app_type_ == apps::AppType::kChromeApp &&
       item().id.app_id == extension_misc::kFilesManagerAppId);

  if (build_extension_menu_before_pin)
    BuildExtensionAppShortcutsMenu(menu_model.get());

  // "New Window" should go above "Pin".
  if (menu_items.items.size() > index &&
      menu_items.items[index]->command_id == ash::APP_CONTEXT_MENU_NEW_WINDOW) {
    AddContextMenuOption(menu_model.get(), ash::APP_CONTEXT_MENU_NEW_WINDOW,
                         menu_items.items[index]->string_id);
    ++index;
  }

  if (IsAppPinEditable(app_type_, item().id.app_id, controller()->profile())) {
    AddPinMenu(menu_model.get());
  }

  size_t shortcut_index = menu_items.items.size();
  for (size_t i = index; i < menu_items.items.size(); i++) {
    // For Chrome browser, add the close item before the app info item.
    if ((item().id.app_id == app_constants::kChromeAppId ||
         item().id.app_id == app_constants::kLacrosAppId) &&
        menu_items.items[i]->command_id == ash::SHOW_APP_INFO) {
      BuildChromeAppMenu(menu_model.get());
    }

    if (menu_items.items[i]->command_id == ash::LAUNCH_NEW) {
      // Crostini apps have `LAUNCH_NEW` menu item at non-0 position.
      apps::PopulateLaunchNewItemFromMenuItem(menu_items.items[i],
                                              menu_model.get(), submenu_.get(),
                                              &launch_new_string_id_);
    } else if (menu_items.items[i]->type == apps::MenuItemType::kCommand) {
      AddContextMenuOption(
          menu_model.get(),
          static_cast<ash::CommandId>(menu_items.items[i]->command_id),
          menu_items.items[i]->string_id);
    } else {
      // All shortcut menu items are appended at the end, so break out
      // of the loop and continue processing shortcut menu items in
      // BuildAppShortcutsMenu and BuildArcAppShortcutsMenu.
      shortcut_index = i;
      break;
    }
  }

  if (app_type_ == apps::AppType::kArc) {
    BuildArcAppShortcutsMenu(std::move(menu_items), std::move(menu_model),
                             std::move(callback), shortcut_index);
    return;
  }

  if (app_type_ == apps::AppType::kWeb ||
      app_type_ == apps::AppType::kSystemWeb ||
      app_type_ == apps::AppType::kCrostini) {
    BuildAppShortcutsMenu(std::move(menu_items), std::move(menu_model),
                          std::move(callback), shortcut_index);
    return;
  }

  if (!build_extension_menu_before_pin)
    BuildExtensionAppShortcutsMenu(menu_model.get());

  // When Crostini generates shelf id with the prefix "crostini:", AppService
  // can't generate the menu items, because the app_id doesn't match, so add the
  // menu items at UI side, based on the app running status.
  if (guest_os::IsUnregisteredCrostiniShelfAppId(item().id.app_id)) {
    BuildCrostiniAppMenu(menu_model.get());
  }

  std::move(callback).Run(std::move(menu_model));
}

void AppServiceShelfContextMenu::BuildExtensionAppShortcutsMenu(
    ui::SimpleMenuModel* menu_model) {
  extension_menu_items_ = std::make_unique<extensions::ContextMenuMatcher>(
      controller()->profile(), this, menu_model,
      base::BindRepeating(MenuItemHasLauncherContext));

  int index = 0;
  extension_menu_items_->AppendExtensionItems(
      extensions::MenuItem::ExtensionKey(item().id.app_id), std::u16string(),
      &index, false /*is_action_menu*/);

  app_list::AddMenuItemIconsForSystemApps(
      item().id.app_id, menu_model, menu_model->GetItemCount() - index, index);
}

void AppServiceShelfContextMenu::BuildAppShortcutsMenu(
    apps::MenuItems menu_items,
    std::unique_ptr<ui::SimpleMenuModel> menu_model,
    GetMenuModelCallback callback,
    size_t shortcut_index) {
  app_shortcut_items_ = std::make_unique<apps::AppShortcutItems>();
  for (size_t i = shortcut_index; i < menu_items.items.size(); i++) {
    apps::PopulateItemFromMenuItem(menu_items.items[i], menu_model.get(),
                                   app_shortcut_items_.get());
  }
  std::move(callback).Run(std::move(menu_model));
}

void AppServiceShelfContextMenu::BuildArcAppShortcutsMenu(
    apps::MenuItems menu_items,
    std::unique_ptr<ui::SimpleMenuModel> menu_model,
    GetMenuModelCallback callback,
    size_t arc_shortcut_index) {
  const ArcAppListPrefs* arc_prefs =
      ArcAppListPrefs::Get(controller()->profile());
  DCHECK(arc_prefs);

  const arc::ArcAppShelfId& arc_shelf_id =
      arc::ArcAppShelfId::FromString(item().id.app_id);
  DCHECK(arc_shelf_id.valid());
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(arc_shelf_id.app_id());
  if (!app_info && !arc_shelf_id.has_shelf_group_id()) {
    LOG(ERROR) << "App " << item().id.app_id << " is not available.";
    std::move(callback).Run(std::move(menu_model));
    return;
  }

  if (arc_shelf_id.has_shelf_group_id()) {
    const bool app_is_open = controller()->IsOpen(item().id);
    if (!app_is_open && !app_info->suspended) {
      DCHECK(app_info->launchable);
      AddContextMenuOption(menu_model.get(), ash::LAUNCH_NEW,
                           IDS_APP_CONTEXT_MENU_ACTIVATE_ARC);
      launch_new_string_id_ = IDS_APP_CONTEXT_MENU_ACTIVATE_ARC;
    }

    if (app_is_open) {
      AddContextMenuOption(menu_model.get(), ash::MENU_CLOSE,
                           IDS_SHELF_CONTEXT_MENU_CLOSE);
    }
  }

  BuildAppShortcutsMenu(std::move(menu_items), std::move(menu_model),
                        std::move(callback), arc_shortcut_index);
}

void AppServiceShelfContextMenu::BuildCrostiniAppMenu(
    ui::SimpleMenuModel* menu_model) {
  if (controller()->IsOpen(item().id)) {
    AddContextMenuOption(menu_model, ash::MENU_CLOSE,
                         IDS_SHELF_CONTEXT_MENU_CLOSE);
  } else {
    AddContextMenuOption(menu_model, ash::LAUNCH_NEW,
                         IDS_APP_CONTEXT_MENU_ACTIVATE_ARC);
    launch_new_string_id_ = IDS_APP_CONTEXT_MENU_ACTIVATE_ARC;
  }
}

void AppServiceShelfContextMenu::BuildChromeAppMenu(
    ui::SimpleMenuModel* menu_model) {
  // Don't check list of active browsers for lacros app because the list of
  // browsers only tracks in-process browsers (i.e. instances of ash-chrome).
  const bool has_active_browsers =
      item().id.app_id != app_constants::kLacrosAppId &&
      !BrowserShortcutShelfItemController::IsListOfActiveBrowserEmpty(
          controller()->shelf_model());
  if (has_active_browsers || item().type == ash::TYPE_DIALOG ||
      controller()->IsOpen(item().id)) {
    AddContextMenuOption(menu_model, ash::MENU_CLOSE,
                         IDS_SHELF_CONTEXT_MENU_CLOSE);
  }
}

void AppServiceShelfContextMenu::ShowAppInfo() {
  if (app_type_ == apps::AppType::kArc) {
    chrome::ShowAppManagementPage(
        controller()->profile(), item().id.app_id,
        ash::settings::AppManagementEntryPoint::kShelfContextMenuAppInfoArc);
    return;
  }

  // TODO(crbug.com/40176571): If this comes from Lacros app, it shows the
  // top "Apps" settings page. This is fallback, because Lacros app is not
  // registered. This is short term workaround to keep the relative
  // compatibility for Lacros Primary. We should figure out what should be shown
  // by this.
  controller()->DoShowAppInfoFlow(item().id.app_id);
}

void AppServiceShelfContextMenu::SetLaunchType(int command_id) {
  switch (app_type_) {
    case apps::AppType::kStandaloneBrowserChromeApp:
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb: {
      // Web apps can only toggle between kWindow, kTabbed and kBrowser.
      apps::WindowMode user_window_mode =
          ConvertLaunchTypeCommandToWindowMode(command_id);
      if (user_window_mode != apps::WindowMode::kUnknown) {
        apps::AppServiceProxyFactory::GetForProfile(controller()->profile())
            ->SetWindowMode(item().id.app_id, user_window_mode);
      }
      return;
    }
    case apps::AppType::kChromeApp:
      SetExtensionLaunchType(command_id);
      return;
    case apps::AppType::kArc:
      [[fallthrough]];
    case apps::AppType::kCrostini:
      [[fallthrough]];
    case apps::AppType::kBuiltIn:
      [[fallthrough]];
    case apps::AppType::kPluginVm:
      [[fallthrough]];
    case apps::AppType::kBorealis:
      [[fallthrough]];
    default:
      return;
  }
}

void AppServiceShelfContextMenu::SetExtensionLaunchType(int command_id) {
  switch (static_cast<ash::CommandId>(command_id)) {
    case ash::USE_LAUNCH_TYPE_REGULAR:
      extensions::SetLaunchType(controller()->profile(), item().id.app_id,
                                extensions::LAUNCH_TYPE_REGULAR);
      break;
    case ash::USE_LAUNCH_TYPE_WINDOW: {
      // Hosted apps can only toggle between LAUNCH_WINDOW and LAUNCH_REGULAR.
      extensions::LaunchType launch_type =
          GetExtensionLaunchType() == extensions::LAUNCH_TYPE_WINDOW
              ? extensions::LAUNCH_TYPE_REGULAR
              : extensions::LAUNCH_TYPE_WINDOW;

      extensions::SetLaunchType(controller()->profile(), item().id.app_id,
                                launch_type);
      break;
    }
    case ash::USE_LAUNCH_TYPE_TABBED_WINDOW:
    case ash::DEPRECATED_USE_LAUNCH_TYPE_FULLSCREEN:
    case ash::DEPRECATED_USE_LAUNCH_TYPE_PINNED:
      NOTREACHED_IN_MIGRATION();
      break;
    default:
      return;
  }
}

extensions::LaunchType AppServiceShelfContextMenu::GetExtensionLaunchType()
    const {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(controller()->profile())
          ->GetExtensionById(item().id.app_id,
                             extensions::ExtensionRegistry::EVERYTHING);
  if (!extension)
    return extensions::LAUNCH_TYPE_DEFAULT;

  return extensions::GetLaunchType(
      extensions::ExtensionPrefs::Get(controller()->profile()), extension);
}

void AppServiceShelfContextMenu::ExecutePublisherContextMenuCommand(
    int command_id) {
  DCHECK(command_id >= ash::LAUNCH_APP_SHORTCUT_FIRST &&
         command_id <= ash::LAUNCH_APP_SHORTCUT_LAST);
  size_t index = command_id - ash::LAUNCH_APP_SHORTCUT_FIRST;
  DCHECK(app_shortcut_items_);
  DCHECK_LT(index, app_shortcut_items_->size());

  apps::AppServiceProxyFactory::GetForProfile(controller()->profile())
      ->ExecuteContextMenuCommand(item().id.app_id, command_id,
                                  app_shortcut_items_->at(index).shortcut_id,
                                  display_id());
}
