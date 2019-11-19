// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_EXTENSION_APP_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_APP_LIST_EXTENSION_APP_CONTEXT_MENU_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"

class AppListControllerDelegate;
class Profile;

namespace extensions {
class ContextMenuMatcher;
}

namespace app_list {

class AppContextMenuDelegate;

class ExtensionAppContextMenu : public AppContextMenu {
 public:
  ExtensionAppContextMenu(AppContextMenuDelegate* delegate,
                          Profile* profile,
                          const std::string& app_id,
                          AppListControllerDelegate* controller,
                          bool is_platform_app);
  ~ExtensionAppContextMenu() override;

  static void DisableInstalledExtensionCheckForTesting(bool disable);

  // Returns the string id based on launch type.
  int GetLaunchStringId() const;

  // AppContextMenu overrides:
  void GetMenuModel(GetMenuModelCallback callback) override;
  void BuildMenu(ui::SimpleMenuModel* menu_model) override;

  // ui::SimpleMenuModel::Delegate overrides:
  base::string16 GetLabelForCommandId(int command_id) const override;
  const gfx::VectorIcon* GetVectorIconForCommandId(
      int command_id) const override;
  bool IsItemForCommandIdDynamic(int command_id) const override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // Creates the actionable submenu for LAUNCH_NEW.
  void CreateOpenNewSubmenu(ui::SimpleMenuModel* menu_model);

  bool is_platform_app_;

  // The MenuModel used to control LAUNCH_NEW's icon, label, and
  // execution when touchable app context menus are enabled.
  std::unique_ptr<ui::SimpleMenuModel> open_new_submenu_model_;

  std::unique_ptr<extensions::ContextMenuMatcher> extension_menu_items_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppContextMenu);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_EXTENSION_APP_CONTEXT_MENU_H_
