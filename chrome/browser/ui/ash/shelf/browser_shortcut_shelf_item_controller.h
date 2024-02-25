// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_BROWSER_SHORTCUT_SHELF_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_BROWSER_SHORTCUT_SHELF_ITEM_CONTROLLER_H_

#include <memory>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_list_observer.h"

namespace ash {
class ShelfModel;
}

class ShelfContextMenu;

// Shelf item delegate for a browser shortcut; only one such item should exist.
// This item shows an application menu that lists open browser windows or tabs.
class BrowserShortcutShelfItemController : public ash::ShelfItemDelegate,
                                           public BrowserListObserver {
 public:
  explicit BrowserShortcutShelfItemController(ash::ShelfModel* shelf_model);

  BrowserShortcutShelfItemController(
      const BrowserShortcutShelfItemController&) = delete;
  BrowserShortcutShelfItemController& operator=(
      const BrowserShortcutShelfItemController&) = delete;

  ~BrowserShortcutShelfItemController() override;

  // Check if there is any active browsers windows.
  static bool IsListOfActiveBrowserEmpty(const ash::ShelfModel* model);

  // ash::ShelfItemDelegate overrides:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override;
  AppMenuItems GetAppMenuItems(
      int event_flags,
      const ItemFilterPredicate& filter_predicate) override;
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

  raw_ptr<ash::ShelfModel> shelf_model_;

  // The cached browser windows and tab indices shown in an application menu.
  std::vector<std::pair<Browser*, size_t>> app_menu_items_;

  std::unique_ptr<ShelfContextMenu> context_menu_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_BROWSER_SHORTCUT_SHELF_ITEM_CONTROLLER_H_
