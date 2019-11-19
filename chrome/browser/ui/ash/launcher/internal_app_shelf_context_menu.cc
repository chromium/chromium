// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/internal_app_shelf_context_menu.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/grit/generated_resources.h"

InternalAppShelfContextMenu::InternalAppShelfContextMenu(
    ChromeLauncherController* controller,
    const ash::ShelfItem* item,
    int64_t display_id)
    : LauncherContextMenu(controller, item, display_id) {}

void InternalAppShelfContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  BuildMenu(menu_model.get());
  std::move(callback).Run(std::move(menu_model));
}

void InternalAppShelfContextMenu::BuildMenu(ui::SimpleMenuModel* menu_model) {
  const bool app_is_open = controller()->IsOpen(item().id);
  if (!app_is_open) {
    AddContextMenuOption(menu_model, ash::MENU_OPEN_NEW,
                         IDS_APP_CONTEXT_MENU_ACTIVATE_ARC);
  }

  const auto* internal_app = app_list::FindInternalApp(item().id.app_id);
  DCHECK(internal_app);
  if (internal_app->show_in_launcher)
    AddPinMenu(menu_model);

  if (app_is_open) {
    AddContextMenuOption(menu_model, ash::MENU_CLOSE,
                         IDS_LAUNCHER_CONTEXT_MENU_CLOSE);

    if (internal_app->internal_app_name == apps::BuiltInAppName::kPluginVm &&
        plugin_vm::IsPluginVmRunning(controller()->profile())) {
      AddContextMenuOption(menu_model, ash::STOP_APP,
                           IDS_PLUGIN_VM_SHUT_DOWN_MENU_ITEM);
    }
  }
}

void InternalAppShelfContextMenu::ExecuteCommand(int command_id,
                                                 int event_flags) {
  if (ExecuteCommonCommand(command_id, event_flags))
    return;

  const auto* internal_app = app_list::FindInternalApp(item().id.app_id);
  DCHECK(internal_app);
  DCHECK_EQ(internal_app->internal_app_name, apps::BuiltInAppName::kPluginVm);
  if (command_id == ash::STOP_APP) {
    plugin_vm::PluginVmManager::GetForProfile(controller()->profile())
        ->StopPluginVm(plugin_vm::kPluginVmName);
    return;
  }
}
