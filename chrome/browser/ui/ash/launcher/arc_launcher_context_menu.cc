// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_launcher_context_menu.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/arc/app_shortcuts/arc_app_shortcuts_menu_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_dialog.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/settings/chromeos/app_management/app_management_uma.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"

ArcLauncherContextMenu::ArcLauncherContextMenu(
    ChromeLauncherController* controller,
    const ash::ShelfItem* item,
    int64_t display_id)
    : LauncherContextMenu(controller, item, display_id) {}

ArcLauncherContextMenu::~ArcLauncherContextMenu() = default;

void ArcLauncherContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  BuildMenu(std::make_unique<ui::SimpleMenuModel>(this), std::move(callback));
}

bool ArcLauncherContextMenu::IsCommandIdEnabled(int command_id) const {
  const ArcAppListPrefs* arc_prefs =
      ArcAppListPrefs::Get(controller()->profile());

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs ? arc_prefs->GetApp(item().id.app_id) : nullptr;

  switch (command_id) {
    case ash::UNINSTALL:
      return app_info && !app_info->sticky &&
             (app_info->ready || app_info->shortcut);
    case ash::SHOW_APP_INFO:
      return app_info && app_info->ready;
    default:
      return LauncherContextMenu::IsCommandIdEnabled(command_id);
  }
  NOTREACHED();
  return false;
}

void ArcLauncherContextMenu::ExecuteCommand(int command_id, int event_flags) {
  if (command_id >= ash::LAUNCH_APP_SHORTCUT_FIRST &&
      command_id <= ash::LAUNCH_APP_SHORTCUT_LAST) {
    DCHECK(app_shortcuts_menu_builder_);
    app_shortcuts_menu_builder_->ExecuteCommand(command_id);
    return;
  }
  if (command_id == ash::SHOW_APP_INFO) {
    ShowPackageInfo();
    return;
  }
  if (command_id == ash::UNINSTALL) {
    apps::AppServiceProxy* proxy =
        apps::AppServiceProxyFactory::GetForProfile(controller()->profile());
    DCHECK(proxy);
    proxy->Uninstall(item().id.app_id, nullptr /* parent_window */);
    return;
  }

  LauncherContextMenu::ExecuteCommand(command_id, event_flags);
}

void ArcLauncherContextMenu::BuildMenu(
    std::unique_ptr<ui::SimpleMenuModel> menu_model,
    GetMenuModelCallback callback) {
  const ArcAppListPrefs* arc_list_prefs =
      ArcAppListPrefs::Get(controller()->profile());
  DCHECK(arc_list_prefs);

  const arc::ArcAppShelfId& app_id =
      arc::ArcAppShelfId::FromString(item().id.app_id);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_list_prefs->GetApp(app_id.app_id());
  if (!app_info && !app_id.has_shelf_group_id()) {
    NOTREACHED();
    std::move(callback).Run(std::move(menu_model));
    return;
  }

  const bool app_is_open = controller()->IsOpen(item().id);
  if (!app_is_open && !app_info->suspended) {
    DCHECK(app_info->launchable);
    AddContextMenuOption(menu_model.get(), ash::MENU_OPEN_NEW,
                         IDS_APP_CONTEXT_MENU_ACTIVATE_ARC);
  }

  if (!app_id.has_shelf_group_id() && app_info->launchable)
    AddPinMenu(menu_model.get());

  if (!app_info->sticky) {
    AddContextMenuOption(menu_model.get(), ash::UNINSTALL,
                         IDS_APP_LIST_UNINSTALL_ITEM);
  }

  AddContextMenuOption(menu_model.get(), ash::SHOW_APP_INFO,
                       IDS_APP_CONTEXT_MENU_SHOW_INFO);

  if (app_is_open) {
    AddContextMenuOption(menu_model.get(), ash::MENU_CLOSE,
                         IDS_LAUNCHER_CONTEXT_MENU_CLOSE);
  }

  DCHECK(!app_shortcuts_menu_builder_);
  app_shortcuts_menu_builder_ =
      std::make_unique<arc::ArcAppShortcutsMenuBuilder>(
          controller()->profile(), item().id.app_id, display_id(),
          ash::LAUNCH_APP_SHORTCUT_FIRST, ash::LAUNCH_APP_SHORTCUT_LAST);
  app_shortcuts_menu_builder_->BuildMenu(
      app_info->package_name, std::move(menu_model), std::move(callback));
}

void ArcLauncherContextMenu::ShowPackageInfo() {
  const ArcAppListPrefs* arc_prefs =
      ArcAppListPrefs::Get(controller()->profile());
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(item().id.app_id);
  if (!app_info) {
    VLOG(2) << "Requesting AppInfo for package that does not exist: "
            << item().id.app_id << ".";
    return;
  }
  if (base::FeatureList::IsEnabled(features::kAppManagement)) {
    chrome::ShowAppManagementPage(controller()->profile(), item().id.app_id);
    base::UmaHistogramEnumeration(
        kAppManagementEntryPointsHistogramName,
        AppManagementEntryPoint::kLauncherContextMenuAppInfoArc);
    return;
  }
  arc::ShowPackageInfo(app_info->package_name,
                       arc::mojom::ShowPackageInfoPage::MAIN, display_id());
}
