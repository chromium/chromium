// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_PROMISE_APP_SHELF_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_PROMISE_APP_SHELF_CONTEXT_MENU_H_

#include "chrome/browser/ui/ash/shelf/shelf_context_menu.h"
#include "components/services/app_service/public/cpp/menu.h"

class ChromeShelfController;

class AppServicePromiseAppShelfContextMenu : public ShelfContextMenu {
 public:
  AppServicePromiseAppShelfContextMenu(ChromeShelfController* controller,
                                       const ash::ShelfItem* item,
                                       int64_t display_id);
  ~AppServicePromiseAppShelfContextMenu() override;

  AppServicePromiseAppShelfContextMenu(
      const AppServicePromiseAppShelfContextMenu&) = delete;
  AppServicePromiseAppShelfContextMenu& operator=(
      const AppServicePromiseAppShelfContextMenu&) = delete;

  // ShelfContextMenu:
  void GetMenuModel(GetMenuModelCallback callback) override;

 private:
  void OnGetMenuModel(GetMenuModelCallback callback,
                      apps::MenuItems menu_items);
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_PROMISE_APP_SHELF_CONTEXT_MENU_H_
