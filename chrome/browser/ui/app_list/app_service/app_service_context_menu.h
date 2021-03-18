// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_CONTEXT_MENU_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_shortcut_item.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class AppContextMenuDelegate;
class AppListControllerDelegate;
class Profile;

namespace extensions {
class ContextMenuMatcher;
}

class AppServiceContextMenu : public app_list::AppContextMenu {
 public:
  AppServiceContextMenu(app_list::AppContextMenuDelegate* delegate,
                        Profile* profile,
                        const std::string& app_id,
                        AppListControllerDelegate* controller);
  ~AppServiceContextMenu() override;

  AppServiceContextMenu(const AppServiceContextMenu&) = delete;
  AppServiceContextMenu& operator=(const AppServiceContextMenu&) = delete;

  // AppContextMenu overrides:
  void GetMenuModel(GetMenuModelCallback callback) override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;

 private:
  void OnGetMenuModel(GetMenuModelCallback callback,
                      apps::mojom::MenuItemsPtr menu_items);

  // Build additional extension app menu items.
  void BuildExtensionAppShortcutsMenu(ui::SimpleMenuModel* menu_model);

  void ShowAppInfo();

  void SetLaunchType(int command_id);

  void ExecutePublisherContextMenuCommand(int command_id);

  apps::mojom::AppType app_type_;

  // The SimpleMenuModel used to hold the submenu items.
  std::unique_ptr<ui::SimpleMenuModel> submenu_;

  std::unique_ptr<extensions::ContextMenuMatcher> extension_menu_items_;

  // Caches the app shortcut items.
  std::unique_ptr<apps::AppShortcutItems> app_shortcut_items_;

  base::WeakPtrFactory<AppServiceContextMenu> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_CONTEXT_MENU_H_
