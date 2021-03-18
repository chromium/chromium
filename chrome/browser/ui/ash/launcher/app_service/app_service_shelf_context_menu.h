// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_SHELF_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_SHELF_CONTEXT_MENU_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_shortcut_item.h"
#include "chrome/browser/ui/ash/launcher/shelf_context_menu.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/common/constants.h"

class ChromeLauncherController;

namespace extensions {
class ContextMenuMatcher;
}

class AppServiceShelfContextMenu : public ShelfContextMenu {
 public:
  AppServiceShelfContextMenu(ChromeLauncherController* controller,
                             const ash::ShelfItem* item,
                             int64_t display_id);
  ~AppServiceShelfContextMenu() override;

  AppServiceShelfContextMenu(const AppServiceShelfContextMenu&) = delete;
  AppServiceShelfContextMenu& operator=(const AppServiceShelfContextMenu&) =
      delete;

  // ShelfContextMenu:
  void GetMenuModel(GetMenuModelCallback callback) override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;

 private:
  void OnGetMenuModel(GetMenuModelCallback callback,
                      apps::mojom::MenuItemsPtr menu_items);

  // Build additional extension app menu items.
  void BuildExtensionAppShortcutsMenu(ui::SimpleMenuModel* menu_model);

  // Build additional app shortcuts menu items.
  void BuildAppShortcutsMenu(apps::mojom::MenuItemsPtr menu_items,
                             std::unique_ptr<ui::SimpleMenuModel> menu_model,
                             GetMenuModelCallback callback,
                             size_t shortcut_index);

  // Build ARC-specific app shortcuts menu items.
  void BuildArcAppShortcutsMenu(apps::mojom::MenuItemsPtr menu_items,
                                std::unique_ptr<ui::SimpleMenuModel> menu_model,
                                GetMenuModelCallback callback,
                                size_t arc_shortcut_index);

  // Build the menu items based on the app running status for the Crostini shelf
  // id with the prefix "crostini:".
  void BuildCrostiniAppMenu(ui::SimpleMenuModel* menu_model);

  // Build additional Chrome app menu items.
  void BuildChromeAppMenu(ui::SimpleMenuModel* menu_model);

  void ShowAppInfo();

  // Helpers to set the launch type.
  void SetLaunchType(int command_id);

  // Helpers to set the launch type for the extension item.
  void SetExtensionLaunchType(int command_id);

  // Helpers to get the launch type for the extension item.
  extensions::LaunchType GetExtensionLaunchType() const;

  bool ShouldAddPinMenu();

  void ExecutePublisherContextMenuCommand(int command_id);

  apps::mojom::AppType app_type_;

  // The SimpleMenuModel used to hold the submenu items.
  std::unique_ptr<ui::SimpleMenuModel> submenu_;

  // Caches the app shortcut items.
  std::unique_ptr<apps::AppShortcutItems> app_shortcut_items_;

  std::unique_ptr<extensions::ContextMenuMatcher> extension_menu_items_;

  base::WeakPtrFactory<AppServiceShelfContextMenu> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_SHELF_CONTEXT_MENU_H_
