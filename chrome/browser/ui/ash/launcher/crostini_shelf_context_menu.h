// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CROSTINI_SHELF_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CROSTINI_SHELF_CONTEXT_MENU_H_

#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"

// Class for context menu which is shown for Crostini app in the shelf.
class CrostiniShelfContextMenu : public LauncherContextMenu {
 public:
  CrostiniShelfContextMenu(ChromeLauncherController* controller,
                           const ash::ShelfItem* item,
                           int64_t display_id);
  ~CrostiniShelfContextMenu() override;

  // LauncherContextMenu:
  void GetMenuModel(GetMenuModelCallback callback) override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  bool IsUninstallable() const;
  void BuildMenu(ui::SimpleMenuModel* menu_model);

  DISALLOW_COPY_AND_ASSIGN(CrostiniShelfContextMenu);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CROSTINI_SHELF_CONTEXT_MENU_H_
