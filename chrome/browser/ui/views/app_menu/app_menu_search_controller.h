// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_SEARCH_CONTROLLER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_item.h"

class FuzzyFinder;

namespace actions {
class ActionItem;
class BaseAction;
}  // namespace actions

// Controller class responsible for managing search within the ChroMenu.
// Extracts searchable items from the app menu hierarchy, flattens them,
// and initializes the FuzzyFinder search index.
class AppMenuSearchController {
 public:
  explicit AppMenuSearchController(actions::ActionItem* menu_root);
  AppMenuSearchController(const AppMenuSearchController&) = delete;
  AppMenuSearchController& operator=(const AppMenuSearchController&) = delete;
  ~AppMenuSearchController();

  // Initializes the searchable index of items.
  // Must only be called once during the lifetime of this controller.
  void InitializeSearchIndex();

  // Accessors for testing.
  bool is_search_index_initialized_for_testing() const {
    return fuzzy_finder_ != nullptr;
  }
  const std::vector<std::unique_ptr<AppMenuSearchItem>>&
  search_items_for_testing() const {
    return search_items_;
  }

 private:
  // Recursively traverses action and its children to extract searchable leaf
  // items into search_items_, tracking contextual text and category types.
  void FlattenHierarchyRecursive(actions::BaseAction* action,
                                 AppMenuSearchItem::Type current_type,
                                 std::u16string_view context);

  // Creates and appends an AppMenuSearchItem to search_items_.
  void AddSearchItem(actions::BaseAction* action,
                     AppMenuSearchItem::Type type,
                     std::u16string_view title,
                     std::u16string_view secondary_text);

  // The root ActionItem of the app menu.
  raw_ptr<actions::ActionItem> menu_root_;

  // Flattened list of searchable items extracted from the app menu hierarchy.
  std::vector<std::unique_ptr<AppMenuSearchItem>> search_items_;

  // Fuzzy search engine.
  std::unique_ptr<FuzzyFinder> fuzzy_finder_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_SEARCH_CONTROLLER_H_
