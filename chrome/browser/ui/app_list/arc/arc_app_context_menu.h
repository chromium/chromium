// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_CONTEXT_MENU_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"

class AppListControllerDelegate;
class Profile;

namespace app_list {
class AppContextMenuDelegate;
}

namespace arc {
class ArcAppShortcutsMenuBuilder;
}

class ArcAppContextMenu : public app_list::AppContextMenu {
 public:
  ArcAppContextMenu(app_list::AppContextMenuDelegate* delegate,
                    Profile* profile,
                    const std::string& app_id,
                    AppListControllerDelegate* controller);
  ~ArcAppContextMenu() override;

  // AppContextMenu overrides:
  void GetMenuModel(GetMenuModelCallback callback) override;
  void BuildMenu(ui::SimpleMenuModel* menu_model) override;

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // Build additional app shortcuts menu items.
  // TODO(warx): consider merging into BuildMenu.
  void BuildAppShortcutsMenu(std::unique_ptr<ui::SimpleMenuModel> menu_model,
                             GetMenuModelCallback callback);

  void ShowPackageInfo();

  std::unique_ptr<arc::ArcAppShortcutsMenuBuilder> app_shortcuts_menu_builder_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppContextMenu);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_CONTEXT_MENU_H_
