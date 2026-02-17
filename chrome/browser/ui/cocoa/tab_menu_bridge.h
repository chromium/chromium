// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_MENU_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_TAB_MENU_BRIDGE_H_

#include <map>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tabs/public/tab_interface.h"

@class NSMutableArray;
@class NSMenuItem;
@class TabMenuListener;
class TabStripModel;

// A TabMenuBridge bidirectionally connects a list of tabs (represented by a
// TabStripModel) to an NSMenu, for use in the system menu bar. Specifically,
// the TabStripBridge appends items to the bottom of the provided NSMenu
// corresponding to tabs in the TabStripModel, and keeps those items
// synchronized with changes to the TabStripModel. Clicking one of these items
// activates the corresponding tab in the TabStripModel. This class assumes
// that:
//   1) It owns the bottom "dynamic" part of of the provided menu
//   2) The number of items not in the "dynamic" part does not change after the
//      TabMenuBridge is constructed
//
// To use this class, construct an instance and call SetTabStripModel() on it.
class TabMenuBridge : public TabStripModelObserver {
 public:
  // The |menu_item| contains the actual menu this class manages.
  explicit TabMenuBridge(NSMenuItem* menu_item);

  TabMenuBridge(const TabMenuBridge&) = delete;
  TabMenuBridge& operator=(const TabMenuBridge&) = delete;

  ~TabMenuBridge() override;

  // Called to update with a new tab strip model associated with the active
  // browser. This method is idempotent. If there is a new model, it will
  // clear all the existing dynamic items added before adding new ones for
  // the new model if `model` isn't nullptr.
  void SetTabStripModel(TabStripModel* model);

  // Test hook to force rebuilding menu immediately on modal change instead of
  // doing so during menu show.
  void SetForceRebuildMenuForTesting(bool force);

 private:
  FRIEND_TEST_ALL_PREFIXES(TabMenuBridgeTest, ClickingMenuActivatesTab);
  FRIEND_TEST_ALL_PREFIXES(TabMenuBridgeTest,
                           ClickingStaleMenuItemDoesNotCrash);
  FRIEND_TEST_ALL_PREFIXES(TabMenuBridgeTest,
                           ClickingMenuItemAfterReorderActivatesCorrectTab);

  // These methods are used manage the dynamic menu items.
  NSMutableArray* DynamicMenuItems();
  void AddDynamicItemsFromModel();

  // This method exists to be called back into from the Cocoa part of this
  // bridge class (TabMenuListener).
  void OnDynamicItemChosen(NSMenuItem* item);

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabChangedAt(tabs::TabInterface* tab,
                      int index,
                      TabChangeType change_type) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void TabGroupedStateChanged(TabStripModel* tab_strip_model,
                              std::optional<tab_groups::TabGroupId> old_group,
                              std::optional<tab_groups::TabGroupId> new_group,
                              tabs::TabInterface* tab,
                              int index) override;
  void OnTabStripModelDestroyed(TabStripModel* model) override;

  raw_ptr<TabStripModel> model_ = nullptr;
  NSMenuItem* __weak menu_item_;
  TabMenuListener* __strong menu_listener_;

  // When created, this class remembers how many items were present in the
  // non-dynamic section of the menu. This offset is used to map menu items to
  // their underlying tabs.
  int dynamic_items_start_;

  // Maps each dynamic NSMenuItem to the TabInterface it was created for.
  // This allows activating the correct tab even if the menu becomes stale
  // (e.g., a tab is closed or reordered while the menu is open).
  std::map<NSMenuItem*, raw_ptr<tabs::TabInterface>> menu_item_to_tab_;

  // Flag when set forces the menu to be immediately rebuilt on modal change
  // instead of only doing so during menu show.
  bool force_rebuild_menu_ = false;
};

#endif  // CHROME_BROWSER_UI_COCOA_TAB_MENU_BRIDGE_H_
