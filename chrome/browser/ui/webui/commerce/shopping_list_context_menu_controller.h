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

// Helper class for shopping-related items in side panel context menu. This is
// created when the context menu is opened and destroyed when the side panel is
// closed.
class ShoppingListContextMenuController {
 public:
  ShoppingListContextMenuController(
      bookmarks::BookmarkModel* bookmark_model,
      ShoppingService* shopping_service,
      const bookmarks::BookmarkNode* bookmark_node,
      ui::SimpleMenuModel* menu_model);
  ShoppingListContextMenuController(const ShoppingListContextMenuController&) =
      delete;
  ShoppingListContextMenuController& operator=(
      const ShoppingListContextMenuController&) = delete;
  ~ShoppingListContextMenuController() = default;

  // Add menu item that will track or untrack price for this product bookmark
  // based on whether it's been tracked now.
  void AddPriceTrackingItemForBookmark();
  // Execute the context menu action represented by |command_id|.
  bool ExecuteCommand(int command_id);

 private:
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<ShoppingService> shopping_service_;
  raw_ptr<const bookmarks::BookmarkNode> bookmark_node_;
  raw_ptr<ui::SimpleMenuModel> menu_model_;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_LIST_CONTEXT_MENU_CONTROLLER_H_
