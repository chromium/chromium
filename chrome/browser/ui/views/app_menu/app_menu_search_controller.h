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
struct FuzzySearchResult;

namespace actions {
class ActionItem;
class BaseAction;
}  // namespace actions

// Controller class responsible for managing the search lifecycle in the
// ChroMenu. Handles extracting searchable items from the app menu hierarchy,
// querying FuzzyFinder, and constructing an ephemeral ActionItem results tree
// for the UI to render.
class AppMenuSearchController {
 public:
  explicit AppMenuSearchController(actions::ActionItem* menu_root);
  AppMenuSearchController(const AppMenuSearchController&) = delete;
  AppMenuSearchController& operator=(const AppMenuSearchController&) = delete;
  ~AppMenuSearchController();

  // Initializes the searchable index of items.
  // Must only be called once during the lifetime of this controller.
  void InitializeSearchIndex();

  // Runs query against the search index and returns the root of the ephemeral
  // ActionItem results tree for the caller to render. Returns null when query
  // is below the minimum length threshold, meaning the normal app menu should
  // be shown instead. InitializeSearchIndex() must be called before calling
  // this.
  actions::ActionItem* Search(std::u16string_view query);

  // Accessors for testing.
  bool is_search_index_initialized_for_testing() const {
    return fuzzy_finder_ != nullptr;
  }
  const std::vector<std::unique_ptr<AppMenuSearchItem>>&
  search_items_for_testing() const {
    return search_items_;
  }

 private:
  // Intermediate data structure holding categorized and capped search results
  // before they are transformed into an ephemeral ActionItem tree.
  // Each vector corresponds to a distinct section in the search results UI
  // and is capped to a maximum number of items (e.g. kMaxActions).
  struct SearchResults {
    struct Item {
      raw_ptr<actions::ActionItem> action_item = nullptr;

      // Secondary context text (e.g. parent submenu or folder name) displayed
      // as a breadcrumb beneath the item's title in search results.
      std::u16string_view breadcrumb;
    };

    SearchResults();
    SearchResults(const SearchResults&) = delete;
    SearchResults& operator=(const SearchResults&) = delete;
    SearchResults(SearchResults&&);
    SearchResults& operator=(SearchResults&&);
    ~SearchResults();

    std::vector<Item> actions;
    std::vector<Item> recent_tabs;
    std::vector<Item> bookmarks;
    std::vector<Item> tab_groups;

    // Returns true if none of the sections contain any matched items.
    bool empty() const {
      return actions.empty() && recent_tabs.empty() && bookmarks.empty() &&
             tab_groups.empty();
    }
  };

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

  // Processes a list of FuzzySearchResult by grouping matches into category
  // sections and capping each section to its maximum allowed count.
  SearchResults ProcessSearchResults(
      const std::vector<FuzzySearchResult>& matches) const;

  // Transforms categorized search results into an ephemeral ActionItem tree
  // organized by sections (Actions, Bookmarks, Recent Tabs, Tab Groups)
  // wrapping matched items in IndirectActionItems, and returns its root.
  actions::ActionItem* BuildSearchResultsTree(SearchResults results);

  // The root ActionItem of the app menu.
  raw_ptr<actions::ActionItem> menu_root_;

  // The root ActionItem of the search results tree.
  std::unique_ptr<actions::ActionItem> search_results_root_;

  // Flattened list of searchable items extracted from the app menu hierarchy.
  std::vector<std::unique_ptr<AppMenuSearchItem>> search_items_;

  // Fuzzy search engine.
  std::unique_ptr<FuzzyFinder> fuzzy_finder_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_SEARCH_CONTROLLER_H_
