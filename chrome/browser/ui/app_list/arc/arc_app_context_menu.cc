// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_context_menu.h"

#include <utility>

#include "ash/public/cpp/tablet_mode.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/arc/app_shortcuts/arc_app_shortcuts_menu_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_dialog.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/settings/chromeos/app_management/app_management_uma.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"

ArcAppContextMenu::ArcAppContextMenu(app_list::AppContextMenuDelegate* delegate,
                                     Profile* profile,
                                     const std::string& app_id,
                                     AppListControllerDelegate* controller)
    : app_list::AppContextMenu(delegate, profile, app_id, controller) {}

ArcAppContextMenu::~ArcAppContextMenu() = default;

void ArcAppContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  BuildMenu(menu_model.get());
  BuildAppShortcutsMenu(std::move(menu_model), std::move(callback));
}

void ArcAppContextMenu::BuildMenu(ui::SimpleMenuModel* menu_model) {
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id());
  if (!app_info) {
    LOG(ERROR) << "App " << app_id() << " is not available.";
    return;
  }

  if (!controller()->IsAppOpen(app_id()) && !app_info->suspended) {
    AddContextMenuOption(menu_model, ash::LAUNCH_NEW,
                         IDS_APP_CONTEXT_MENU_ACTIVATE_ARC);
  }
  // Create default items.
  app_list::AppContextMenu::BuildMenu(menu_model);

  if (arc_prefs->IsShortcut(app_id())) {
    AddContextMenuOption(menu_model, ash::UNINSTALL,
                         IDS_APP_LIST_REMOVE_SHORTCUT);
  } else if (!app_info->sticky) {
    AddContextMenuOption(menu_model, ash::UNINSTALL,
                         IDS_APP_LIST_UNINSTALL_ITEM);
  }

  // App Info item.
  AddContextMenuOption(menu_model, ash::SHOW_APP_INFO,
                       IDS_APP_CONTEXT_MENU_SHOW_INFO);
}

bool ArcAppContextMenu::IsCommandIdEnabled(int command_id) const {
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id());

  switch (command_id) {
    case ash::UNINSTALL:
      return app_info && !app_info->sticky &&
             (app_info->ready || app_info->shortcut);
    case ash::SHOW_APP_INFO:
      return app_info && app_info->ready;
    default:
      return app_list::AppContextMenu::IsCommandIdEnabled(command_id);
  }

  return false;
}

void ArcAppContextMenu::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == ash::LAUNCH_NEW) {
    delegate()->ExecuteLaunchCommand(event_flags);
  } else if (command_id == ash::UNINSTALL) {
    apps::AppServiceProxy* proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile());
    DCHECK(proxy);
    proxy->Uninstall(app_id(),
                     controller() ? controller()->GetAppListWindow() : nullptr);
  } else if (command_id == ash::SHOW_APP_INFO) {
    ShowPackageInfo();
  } else if (command_id >= ash::LAUNCH_APP_SHORTCUT_FIRST &&
             command_id <= ash::LAUNCH_APP_SHORTCUT_LAST) {
    DCHECK(app_shortcuts_menu_builder_);
    app_shortcuts_menu_builder_->ExecuteCommand(command_id);
  } else {
    app_list::AppContextMenu::ExecuteCommand(command_id, event_flags);
  }
}

void ArcAppContextMenu::BuildAppShortcutsMenu(
    std::unique_ptr<ui::SimpleMenuModel> menu_model,
    GetMenuModelCallback callback) {
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id());
  if (!app_info) {
    LOG(ERROR) << "App " << app_id() << " is not available.";
    std::move(callback).Run(std::move(menu_model));
    return;
  }

  DCHECK(!app_shortcuts_menu_builder_);
  app_shortcuts_menu_builder_ =
      std::make_unique<arc::ArcAppShortcutsMenuBuilder>(
          profile(), app_id(), controller()->GetAppListDisplayId(),
          ash::LAUNCH_APP_SHORTCUT_FIRST, ash::LAUNCH_APP_SHORTCUT_LAST);
  app_shortcuts_menu_builder_->BuildMenu(
      app_info->package_name, std::move(menu_model), std::move(callback));
}

void ArcAppContextMenu::ShowPackageInfo() {
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id());
  if (!app_info) {
    VLOG(2) << "Requesting AppInfo for package that does not exist: "
            << app_id() << ".";
    return;
  }
  if (base::FeatureList::IsEnabled(chromeos::features::kSplitSettings) &&
      base::FeatureList::IsEnabled(features::kAppManagement)) {
    chrome::ShowAppManagementPage(profile(), app_id());
    base::UmaHistogramEnumeration(
        kAppManagementEntryPointsHistogramName,
        AppManagementEntryPoint::kAppListContextMenuAppInfoArc);
    return;
  }
  if (arc::ShowPackageInfo(app_info->package_name,
                           arc::mojom::ShowPackageInfoPage::MAIN,
                           controller()->GetAppListDisplayId()) &&
      !(ash::TabletMode::Get() && ash::TabletMode::Get()->InTabletMode())) {
    controller()->DismissView();
  }
}
