// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_SEARCH_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_item.h"
#include "ui/base/models/image_model.h"

class AppMenuSearchBarView;
class FuzzyFinder;
struct FuzzySearchResult;

namespace actions {
class ActionItem;
class BaseAction;
}  // namespace actions

// Controller class responsible for managing the search lifecycle in the
// ChroMenu. Handles extracting searchable items from the app menu hierarchy,
// querying FuzzyFinder, and constructing an ephemeral ActionItem results tree
// for the UI to render. It searches whenever the search bar's query changes,
// owns the results tree, and has its `Delegate` show the results. When the
// back button is pressed, it has the `Delegate` show the normal menu again.
class AppMenuSearchController {
 public:
  // Implemented by the app menu that shows the search results.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Shows `results` in place of the rows below the search bar.
    virtual void ShowSearchResults(actions::ActionItem* results) = 0;

    // Restores the normal menu rows below the search bar.
    virtual void RestoreMenu() = 0;
  };

  // `search_bar` and `delegate` must outlive this controller.
  AppMenuSearchController(actions::ActionItem* menu_root,
                          AppMenuSearchBarView* search_bar,
                          Delegate* delegate);
  AppMenuSearchController(const AppMenuSearchController&) = delete;
  AppMenuSearchController& operator=(const AppMenuSearchController&) = delete;
  ~AppMenuSearchController();

  // Initializes the searchable index of items.
  // Must only be called once during the lifetime of this controller.
  void InitializeSearchIndex();

  // Runs query against the search index and returns a new ephemeral ActionItem
  // results tree for the caller to render. Returns null when query is below the
  // minimum length threshold.
  // InitializeSearchIndex() must be called before calling this.
  std::unique_ptr<actions::ActionItem> Search(std::u16string_view query);

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

      // The title search matched (see ExtractItemTitle()), displayed as the
      // item's title in search results. It can differ from the action's own
      // text, e.g. a text override on the menu row or the action's tooltip.
      std::u16string_view title;

      // Secondary context text (e.g. parent submenu or folder name) displayed
      // as a breadcrumb beneath the item's title in search results.
      std::u16string_view breadcrumb;

      // The icon override of the indexed menu row, if any, displayed as the
      // item's icon in search results.
      std::optional<ui::ImageModel> icon_override;

      // Whether the indexed menu row is a checkbox item, so the search result
      // is rendered as one too, bound to the action's checked state.
      bool is_checkable = false;
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

  // Searches for the search bar's query and has `delegate_` show the results.
  // Leaves the menu as it is when the query is below the minimum query length
  // threshold.
  void OnQueryChanged();

  // Has `delegate_` restore the normal menu again.
  void OnBackPressed();

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
  std::unique_ptr<actions::ActionItem> BuildSearchResultsTree(
      SearchResults results) const;

  // The root ActionItem of the app menu.
  raw_ptr<actions::ActionItem> menu_root_;

  // The search bar whose query is searched.
  raw_ptr<AppMenuSearchBarView> search_bar_;

  // Shows the search results or the normal menu rows.
  raw_ptr<Delegate> delegate_;

  // Flattened list of searchable items extracted from the app menu hierarchy.
  std::vector<std::unique_ptr<AppMenuSearchItem>> search_items_;

  // Fuzzy search engine.
  std::unique_ptr<FuzzyFinder> fuzzy_finder_;

  // The search results shown below the search bar.
  std::unique_ptr<actions::ActionItem> search_results_;

  base::CallbackListSubscription query_changed_subscription_;
  base::CallbackListSubscription back_pressed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_SEARCH_CONTROLLER_H_
