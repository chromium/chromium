// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_MENU_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_TAB_MENU_BRIDGE_H_

#include "base/gtest_prod_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

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
// To use this class, construct an instance and call BuildMenu() on it.
class TabMenuBridge : public TabStripModelObserver {
 public:
  // The |menu_item| contains the actual menu this class manages.
  TabMenuBridge(TabStripModel* model, NSMenuItem* menu_item);
  ~TabMenuBridge() override;

  // It's legal to call this method more than once - it will clear all the
  // existing dynamic items added by this instance before adding any new ones,
  // so multiple calls are idempotent.
  void BuildMenu();

 private:
  FRIEND_TEST_ALL_PREFIXES(TabMenuBridgeTest, ClickingMenuActivatesTab);

  // These methods are used to make batch changes to the menu.
  void RemoveAllDynamicItems();
  void AddDynamicItemsFromModel();

  // This method exists to be called back into from the Cocoa part of this
  // bridge class (TabMenuListener).
  void OnDynamicItemChosen(NSMenuItem* item);

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void OnTabStripModelDestroyed(TabStripModel* model) override;

  TabStripModel* model_;
  NSMenuItem* menu_item_;  // weak
  base::scoped_nsobject<TabMenuListener> menu_listener_;

  // When created, this class remembers how many items were present in the
  // non-dynamic section of the menu. This offset is used to map menu items to
  // their underlying tabs.
  int dynamic_items_start_;

  DISALLOW_COPY_AND_ASSIGN(TabMenuBridge);
};

#endif  // CHROME_BROWSER_UI_COCOA_TAB_MENU_BRIDGE_H_
