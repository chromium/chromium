// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_app_context_menu.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind_helpers.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/grit/generated_resources.h"

CrostiniAppContextMenu::CrostiniAppContextMenu(
    Profile* profile,
    const std::string& app_id,
    AppListControllerDelegate* controller)
    : app_list::AppContextMenu(nullptr, profile, app_id, controller) {}

CrostiniAppContextMenu::~CrostiniAppContextMenu() = default;

bool CrostiniAppContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id == ash::UNINSTALL) {
    return IsUninstallable();
  } else if (command_id == ash::STOP_APP) {
    if (app_id() == crostini::kCrostiniTerminalId) {
      return crostini::IsCrostiniRunning(profile());
    }
  }
  return app_list::AppContextMenu::IsCommandIdEnabled(command_id);
}

void CrostiniAppContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case ash::UNINSTALL: {
      DCHECK_NE(app_id(), crostini::kCrostiniTerminalId);
      apps::AppServiceProxy* proxy =
          apps::AppServiceProxyFactory::GetForProfile(profile());
      DCHECK(proxy);
      proxy->Uninstall(app_id(), nullptr /* parent_window */);
      return;
    }
    case ash::STOP_APP:
      if (app_id() == crostini::kCrostiniTerminalId) {
        crostini::CrostiniManager::GetForProfile(profile())->StopVm(
            crostini::kCrostiniDefaultVmName, base::DoNothing());
        return;
      }
      break;
  }
  app_list::AppContextMenu::ExecuteCommand(command_id, event_flags);
}

// TODO(timloh): Add support for "App Info" and possibly actions defined in
// .desktop files.
void CrostiniAppContextMenu::BuildMenu(ui::SimpleMenuModel* menu_model) {
  app_list::AppContextMenu::BuildMenu(menu_model);

  if (IsUninstallable()) {
    AddContextMenuOption(menu_model, ash::UNINSTALL,
                         IDS_APP_LIST_UNINSTALL_ITEM);
  }

  if (app_id() == crostini::kCrostiniTerminalId) {
    AddContextMenuOption(menu_model, ash::STOP_APP,
                         IDS_CROSTINI_SHUT_DOWN_LINUX_MENU_ITEM);
  }
}

bool CrostiniAppContextMenu::IsUninstallable() const {
  return crostini::IsUninstallable(profile(), app_id());
}
