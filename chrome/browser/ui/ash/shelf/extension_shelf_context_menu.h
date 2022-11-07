// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_EXTENSION_SHELF_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_ASH_SHELF_EXTENSION_SHELF_CONTEXT_MENU_H_

#include <memory>

#include "chrome/browser/ui/ash/shelf/shelf_context_menu.h"
#include "extensions/common/constants.h"

namespace extensions {
class ContextMenuMatcher;
}

// Context menu shown for an extension item in the shelf.
class ExtensionShelfContextMenu : public ShelfContextMenu {
 public:
  ExtensionShelfContextMenu(ChromeShelfController* controller,
                            const ash::ShelfItem* item,
                            int64_t display_id);

  ExtensionShelfContextMenu(const ExtensionShelfContextMenu&) = delete;
  ExtensionShelfContextMenu& operator=(const ExtensionShelfContextMenu&) =
      delete;

  ~ExtensionShelfContextMenu() override;

  // ShelfContextMenu overrides:
  void GetMenuModel(GetMenuModelCallback callback) override;

  // ui::SimpleMenuModel::Delegate overrides:
  ui::ImageModel GetIconForCommandId(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsItemForCommandIdDynamic(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // Creates the actionable submenu for MENU_OPEN_NEW.
  void CreateOpenNewSubmenu(ui::SimpleMenuModel* menu_model);

  // Helpers to get and set the launch type for the extension item.
  extensions::LaunchType GetLaunchType() const;
  void SetLaunchType(extensions::LaunchType launch_type);

  // Helper to get the launch type string id.
  int GetLaunchTypeStringId() const;

  // The MenuModel used to control MENU_OPEN_NEW's icon, label, and
  // execution when touchable app context menus are enabled.
  std::unique_ptr<ui::SimpleMenuModel> open_new_submenu_model_;

  std::unique_ptr<extensions::ContextMenuMatcher> extension_items_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_EXTENSION_SHELF_CONTEXT_MENU_H_
