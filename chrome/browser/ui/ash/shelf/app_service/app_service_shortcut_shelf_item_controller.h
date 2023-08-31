// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_SHORTCUT_SHELF_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_SHORTCUT_SHELF_ITEM_CONTROLLER_H_

#include <memory>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/ui/ash/shelf/shelf_context_menu.h"

// Item controller for an app service shortcut pinned to the shelf.
class AppServiceShortcutShelfItemController : public ash::ShelfItemDelegate {
 public:
  explicit AppServiceShortcutShelfItemController(const ash::ShelfID& shelf_id);

  AppServiceShortcutShelfItemController(
      const AppServiceShortcutShelfItemController&) = delete;
  AppServiceShortcutShelfItemController& operator=(
      const AppServiceShortcutShelfItemController&) = delete;

  ~AppServiceShortcutShelfItemController() override;

  // ash::ShelfItemDelegate overrides:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override;
  void GetContextMenu(int64_t display_id,
                      GetContextMenuCallback callback) override;
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override;
  void Close() override;

 private:
  std::unique_ptr<ShelfContextMenu> context_menu_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_SHORTCUT_SHELF_ITEM_CONTROLLER_H_
