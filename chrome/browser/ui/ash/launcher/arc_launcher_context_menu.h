// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_LAUNCHER_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_LAUNCHER_CONTEXT_MENU_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"

namespace arc {
class ArcAppShortcutsMenuBuilder;
}  // namespace arc

// Class for context menu which is shown for ARC app in the shelf.
class ArcLauncherContextMenu : public LauncherContextMenu {
 public:
  ArcLauncherContextMenu(ChromeLauncherController* controller,
                         const ash::ShelfItem* item,
                         int64_t display_id);
  ~ArcLauncherContextMenu() override;

  // LauncherContextMenu:
  void GetMenuModel(GetMenuModelCallback callback) override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // Launches App Info UI for ARC apps.
  void ShowPackageInfo();

  void BuildMenu(std::unique_ptr<ui::SimpleMenuModel> menu_model,
                 GetMenuModelCallback callback);

  std::unique_ptr<arc::ArcAppShortcutsMenuBuilder> app_shortcuts_menu_builder_;

  DISALLOW_COPY_AND_ASSIGN(ArcLauncherContextMenu);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_LAUNCHER_CONTEXT_MENU_H_
