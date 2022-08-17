// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_CONTEXT_MENU_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/app_shortcut_item.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class AppContextMenuDelegate;
class AppListControllerDelegate;
class Profile;
class StandaloneBrowserExtensionAppContextMenu;

namespace ash {
enum class AppListItemContext;
}

namespace extensions {
class ContextMenuMatcher;
}

// Creates context menus for app items in the app list.
class AppServiceContextMenu : public app_list::AppContextMenu {
 public:
  AppServiceContextMenu(app_list::AppContextMenuDelegate* delegate,
                        Profile* profile,
                        const std::string& app_id,
                        AppListControllerDelegate* controller,
                        ash::AppListItemContext item_context);
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

  apps::AppType app_type_ = apps::AppType::kUnknown;
  bool is_platform_app_ = false;

  // The SimpleMenuModel used to hold the submenu items.
  std::unique_ptr<ui::SimpleMenuModel> submenu_;

  // The SimpleMenuModel that contains reorder options. Could be nullptr if
  // sorting is not available.
  std::unique_ptr<ui::SimpleMenuModel> reorder_submenu_;

  std::unique_ptr<extensions::ContextMenuMatcher> extension_menu_items_;

  // This member holds all logic for context menus associated with standalone
  // browser extension apps.
  std::unique_ptr<StandaloneBrowserExtensionAppContextMenu>
      standalone_browser_extension_menu_;

  // Caches the app shortcut items.
  std::unique_ptr<apps::AppShortcutItems> app_shortcut_items_;

  apps::AppServiceProxy* const proxy_;

  // Where this item is being shown (e.g. the apps grid or recent apps).
  const ash::AppListItemContext item_context_;

  base::WeakPtrFactory<AppServiceContextMenu> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_CONTEXT_MENU_H_
