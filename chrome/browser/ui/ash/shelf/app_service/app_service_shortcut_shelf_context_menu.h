// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_SHORTCUT_SHELF_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_SHORTCUT_SHELF_CONTEXT_MENU_H_

#include "chrome/browser/ui/ash/shelf/shelf_context_menu.h"
#include "components/services/app_service/public/cpp/menu.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"

class ChromeShelfController;

class AppServiceShortcutShelfContextMenu : public ShelfContextMenu {
 public:
  AppServiceShortcutShelfContextMenu(ChromeShelfController* controller,
                                     const ash::ShelfItem* item,
                                     int64_t display_id);
  ~AppServiceShortcutShelfContextMenu() override;

  AppServiceShortcutShelfContextMenu(
      const AppServiceShortcutShelfContextMenu&) = delete;
  AppServiceShortcutShelfContextMenu& operator=(
      const AppServiceShortcutShelfContextMenu&) = delete;

  // ShelfContextMenu:
  void GetMenuModel(GetMenuModelCallback callback) override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  void OnGetMenuModel(GetMenuModelCallback callback,
                      apps::MenuItems menu_items);

  apps::ShortcutId shortcut_id_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_SHORTCUT_SHELF_CONTEXT_MENU_H_
