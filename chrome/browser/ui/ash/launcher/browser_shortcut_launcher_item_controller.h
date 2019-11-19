// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_BROWSER_SHORTCUT_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_BROWSER_SHORTCUT_LAUNCHER_ITEM_CONTROLLER_H_

#include <memory>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "base/macros.h"
#include "chrome/browser/ui/browser_list_observer.h"

namespace ash {
class ShelfModel;
}

namespace content {
class WebContents;
}

class LauncherContextMenu;

// Shelf item delegate for a browser shortcut; only one such item should exist.
// This item shows an application menu that lists open browser windows or tabs.
class BrowserShortcutLauncherItemController : public ash::ShelfItemDelegate,
                                              public BrowserListObserver {
 public:
  explicit BrowserShortcutLauncherItemController(ash::ShelfModel* shelf_model);

  ~BrowserShortcutLauncherItemController() override;

  // Updates the activation state of the Broswer item.
  void UpdateBrowserItemState();

  // Sets the shelf id for the browser window if the browser is represented.
  void SetShelfIDForBrowserWindowContents(Browser* browser,
                                          content::WebContents* web_contents);

  // Check if there is any active browsers windows.
  static bool IsListOfActiveBrowserEmpty();

  // ash::ShelfItemDelegate overrides:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback) override;
  AppMenuItems GetAppMenuItems(int event_flags) override;
  void GetContextMenu(int64_t display_id,
                      GetContextMenuCallback callback) override;
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override;
  void Close() override;

 private:
  // Activate a browser - or advance to the next one on the list.
  // Returns the action performed. Should be one of SHELF_ACTION_NONE,
  // SHELF_ACTION_WINDOW_ACTIVATED, or SHELF_ACTION_NEW_WINDOW_CREATED.
  ash::ShelfAction ActivateOrAdvanceToNextBrowser();

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserClosing(Browser* browser) override;

  ash::ShelfModel* shelf_model_;

  // The cached browser windows and tab indices shown in an application menu.
  std::vector<std::pair<Browser*, size_t>> app_menu_items_;

  std::unique_ptr<LauncherContextMenu> context_menu_;

  DISALLOW_COPY_AND_ASSIGN(BrowserShortcutLauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_BROWSER_SHORTCUT_LAUNCHER_ITEM_CONTROLLER_H_
