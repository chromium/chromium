// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_INTERNAL_APP_SHELF_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_INTERNAL_APP_SHELF_CONTEXT_MENU_H_

#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"

// Class for context menu which is shown for internal app in the shelf.
class InternalAppShelfContextMenu : public LauncherContextMenu {
 public:
  InternalAppShelfContextMenu(ChromeLauncherController* controller,
                              const ash::ShelfItem* item,
                              int64_t display_id);
  ~InternalAppShelfContextMenu() override = default;

  // LauncherContextMenu:
  void GetMenuModel(GetMenuModelCallback callback) override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  void BuildMenu(ui::SimpleMenuModel* menu_model);

  DISALLOW_COPY_AND_ASSIGN(InternalAppShelfContextMenu);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_INTERNAL_APP_SHELF_CONTEXT_MENU_H_
