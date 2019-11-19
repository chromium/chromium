// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/internal_app/internal_app_context_menu.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/grit/generated_resources.h"

InternalAppContextMenu::InternalAppContextMenu(
    Profile* profile,
    const std::string& app_id,
    AppListControllerDelegate* controller)
    : app_list::AppContextMenu(nullptr, profile, app_id, controller) {}

InternalAppContextMenu::~InternalAppContextMenu() = default;

bool InternalAppContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id == ash::STOP_APP) {
    DCHECK_EQ(app_list::FindInternalApp(app_id())->internal_app_name,
              apps::BuiltInAppName::kPluginVm);
    return plugin_vm::IsPluginVmRunning(profile());
  }
  return app_list::AppContextMenu::IsCommandIdEnabled(command_id);
}

void InternalAppContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case ash::STOP_APP:
      DCHECK_EQ(app_list::FindInternalApp(app_id())->internal_app_name,
                apps::BuiltInAppName::kPluginVm);
      plugin_vm::PluginVmManager::GetForProfile(profile())->StopPluginVm(
          plugin_vm::kPluginVmName);
      return;
  }
  app_list::AppContextMenu::ExecuteCommand(command_id, event_flags);
}

void InternalAppContextMenu::BuildMenu(ui::SimpleMenuModel* menu_model) {
  app_list::AppContextMenu::BuildMenu(menu_model);

  const auto* internal_app = app_list::FindInternalApp(app_id());
  DCHECK(internal_app);
  if (internal_app->internal_app_name == apps::BuiltInAppName::kPluginVm) {
    AddContextMenuOption(menu_model, ash::STOP_APP,
                         IDS_PLUGIN_VM_SHUT_DOWN_MENU_ITEM);
  }
}
