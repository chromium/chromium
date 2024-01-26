// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_LIST_CONTEXT_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_LIST_CONTEXT_MENU_CONTROLLER_H_

#include "base/memory/raw_ptr.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace commerce {

class ShoppingService;
class ShoppingServiceHandler;

// Helper class for shopping-related items in side panel context menu. This
// class is created and owned by BookmarksSidePanelUI so that it can be used for
// context menu controlling in the bookmark side panel.
class ShoppingListContextMenuController {
 public:
  ShoppingListContextMenuController(
      bookmarks::BookmarkModel* bookmark_model,
      ShoppingService* shopping_service,
      ShoppingServiceHandler* shopping_list_hander);
  ShoppingListContextMenuController(const ShoppingListContextMenuController&) =
      delete;
  ShoppingListContextMenuController& operator=(
      const ShoppingListContextMenuController&) = delete;
  ~ShoppingListContextMenuController() = default;

  // Add menu item that will track or untrack price for this product bookmark
  // based on whether it's been tracked now.
  void AddPriceTrackingItemForBookmark(ui::SimpleMenuModel* menu_model,
                                       const bookmarks::BookmarkNode* node);
  // Execute the context menu action represented by |command_id|.
  bool ExecuteCommand(int command_id, const bookmarks::BookmarkNode* node);

 private:
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<ShoppingService> shopping_service_;
  raw_ptr<ShoppingServiceHandler, DanglingUntriaged> shopping_list_hander_;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_LIST_CONTEXT_MENU_CONTROLLER_H_
