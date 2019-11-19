// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/crostini_shelf_context_menu.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item.h"
#include "base/bind_helpers.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/views/crostini/crostini_app_restart_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

bool IsDisplayScaleFactorOne(int64_t display_id) {
  display::Display d;
  if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id, &d))
    return false;
  return d.device_scale_factor() == 1.0;
}

bool ShouldShowDisplayDensityMenuItem(
    const base::Optional<crostini::CrostiniRegistryService::Registration>& reg,
    int64_t display_id) {
  // The default terminal app is crosh in a Chrome window and it doesn't run in
  // the Crostini container so it doesn't support display density the same way.
  if (!reg || reg->is_terminal_app())
    return false;
  return !IsDisplayScaleFactorOne(display_id);
}

}  // namespace

CrostiniShelfContextMenu::CrostiniShelfContextMenu(
    ChromeLauncherController* controller,
    const ash::ShelfItem* item,
    int64_t display_id)
    : LauncherContextMenu(controller, item, display_id) {}

CrostiniShelfContextMenu::~CrostiniShelfContextMenu() = default;

void CrostiniShelfContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  BuildMenu(menu_model.get());
  std::move(callback).Run(std::move(menu_model));
}

bool CrostiniShelfContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id == ash::UNINSTALL)
    return IsUninstallable();
  if (command_id == ash::STOP_APP &&
      item().id.app_id == crostini::kCrostiniTerminalId) {
    return crostini::IsCrostiniRunning(controller()->profile());
  }
  return LauncherContextMenu::IsCommandIdEnabled(command_id);
}

void CrostiniShelfContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case ash::UNINSTALL: {
      DCHECK_NE(item().id.app_id, crostini::kCrostiniTerminalId);
      apps::AppServiceProxy* proxy =
          apps::AppServiceProxyFactory::GetForProfile(controller()->profile());
      DCHECK(proxy);
      proxy->Uninstall(item().id.app_id, nullptr /* parent_window */);
      return;
    }
    case ash::STOP_APP:
      if (item().id.app_id == crostini::kCrostiniTerminalId) {
        crostini::CrostiniManager::GetForProfile(controller()->profile())
            ->StopVm(crostini::kCrostiniDefaultVmName, base::DoNothing());
      }
      return;
    case ash::MENU_NEW_WINDOW:
      crostini::LaunchCrostiniApp(controller()->profile(), item().id.app_id,
                                  display_id());
      return;
    case ash::CROSTINI_USE_LOW_DENSITY:
    case ash::CROSTINI_USE_HIGH_DENSITY: {
      crostini::CrostiniRegistryService* registry_service =
          crostini::CrostiniRegistryServiceFactory::GetForProfile(
              controller()->profile());
      const bool scaled = command_id == ash::CROSTINI_USE_LOW_DENSITY;
      registry_service->SetAppScaled(item().id.app_id, scaled);
      if (controller()->IsOpen(item().id))
        CrostiniAppRestartView::Show(item().id, display_id());
      return;
    }
    default:
      ExecuteCommonCommand(command_id, event_flags);
  }
}

bool CrostiniShelfContextMenu::IsUninstallable() const {
  return crostini::IsUninstallable(controller()->profile(), item().id.app_id);
}

void CrostiniShelfContextMenu::BuildMenu(ui::SimpleMenuModel* menu_model) {
  const crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(
          controller()->profile());
  base::Optional<crostini::CrostiniRegistryService::Registration> registration =
      registry_service->GetRegistration(item().id.app_id);

  // For apps which Crostini knows about (i.e. those with .desktop files), we
  // allow the user to pin them and open new windows. Otherwise this window is
  // not associated with an app.
  if (registration) {
    AddPinMenu(menu_model);
    AddContextMenuOption(menu_model, ash::MENU_NEW_WINDOW,
                         IDS_APP_LIST_NEW_WINDOW);
  }

  if (item().id.app_id == crostini::kCrostiniTerminalId &&
      crostini::IsCrostiniRunning(controller()->profile())) {
    AddContextMenuOption(menu_model, ash::STOP_APP,
                         IDS_CROSTINI_SHUT_DOWN_LINUX_MENU_ITEM);
  }

  if (IsUninstallable()) {
    AddContextMenuOption(menu_model, ash::UNINSTALL,
                         IDS_APP_LIST_UNINSTALL_ITEM);
  }

  if (controller()->IsOpen(item().id)) {
    AddContextMenuOption(menu_model, ash::MENU_CLOSE,
                         IDS_LAUNCHER_CONTEXT_MENU_CLOSE);
  } else {
    AddContextMenuOption(menu_model, ash::MENU_OPEN_NEW,
                         IDS_APP_CONTEXT_MENU_ACTIVATE_ARC);
  }

  // Offer users the ability to toggle per-application UI scaling.
  // Some apps have high-density display support and do not require scaling
  // to match the system display density, but others are density-unaware and
  // look better when scaled to match the display density.
  if (ShouldShowDisplayDensityMenuItem(registration, display_id())) {
    if (registration->IsScaled()) {
      AddContextMenuOption(menu_model, ash::CROSTINI_USE_HIGH_DENSITY,
                           IDS_CROSTINI_USE_HIGH_DENSITY);
    } else {
      AddContextMenuOption(menu_model, ash::CROSTINI_USE_LOW_DENSITY,
                           IDS_CROSTINI_USE_LOW_DENSITY);
    }
  }
}
