// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_SHELF_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_SHELF_CONTEXT_MENU_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_shortcut_item.h"
#include "chrome/browser/ui/ash/shelf/shelf_context_menu.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/menu.h"
#include "extensions/common/constants.h"

class ChromeShelfController;

namespace extensions {
class ContextMenuMatcher;
}

class AppServiceShelfContextMenu : public ShelfContextMenu {
 public:
  AppServiceShelfContextMenu(ChromeShelfController* controller,
                             const ash::ShelfItem* item,
                             int64_t display_id);
  ~AppServiceShelfContextMenu() override;

  AppServiceShelfContextMenu(const AppServiceShelfContextMenu&) = delete;
  AppServiceShelfContextMenu& operator=(const AppServiceShelfContextMenu&) =
      delete;

  // ShelfContextMenu:
  ui::ImageModel GetIconForCommandId(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  void GetMenuModel(GetMenuModelCallback callback) override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsItemForCommandIdDynamic(int command_id) const override;

 private:
  void OnGetMenuModel(GetMenuModelCallback callback,
                      apps::MenuItems menu_items);

  // Build additional extension app menu items.
  void BuildExtensionAppShortcutsMenu(ui::SimpleMenuModel* menu_model);

  // Build additional app shortcuts menu items.
  void BuildAppShortcutsMenu(apps::MenuItems menu_items,
                             std::unique_ptr<ui::SimpleMenuModel> menu_model,
                             GetMenuModelCallback callback,
                             size_t shortcut_index);

  // Build ARC-specific app shortcuts menu items.
  void BuildArcAppShortcutsMenu(apps::MenuItems menu_items,
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

  void ExecutePublisherContextMenuCommand(int command_id);

  apps::AppType app_type_;

  // The SimpleMenuModel used to hold the submenu items.
  std::unique_ptr<ui::SimpleMenuModel> submenu_;

  // Caches the app shortcut items.
  std::unique_ptr<apps::AppShortcutItems> app_shortcut_items_;

  std::unique_ptr<extensions::ContextMenuMatcher> extension_menu_items_;

  // String id for the `LAUNCH_NEW` menu item tracked so the menu icon and label
  // can be changed dynamically after the app launch type changes using the
  // launch new item submenu.
  int launch_new_string_id_ = 0;

  base::WeakPtrFactory<AppServiceShelfContextMenu> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_SHELF_CONTEXT_MENU_H_
