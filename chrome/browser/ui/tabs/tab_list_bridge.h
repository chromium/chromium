// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_LIST_BRIDGE_H_
#define CHROME_BROWSER_UI_TABS_TAB_LIST_BRIDGE_H_

#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/browser/ui/tabs/tab_list_interface_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

// This class is a bridge between the TabListInterface, a cross-platform
// abstract class representing a collection of tabs, and the implementations
// that exist on classic desktop platforms -- specifically, TabStripModel.
// TabListInterface is designed to be used on code that is shared between
// classic desktop platforms and the experimental desktop android platform;
// this allows us to use a single interface for code that runs on both.
//
// When possible, the behavior of this class should closely match the behavior
// Android's implementation (in TabModel), and, ideally, the existing analogous
// methods in TabStripModel. This makes migration easier and allows for similar
// behaviors between platforms.
//
// One exception to the above is that this class assumes (and CHECK()s) that
// tab operations are valid. This is different from Android's implementation,
// which is more forgiving. The stricter approach is used because it matches
// the classic desktop implementation and also because it leads to more
// deterministic behavior. Callers should validate the presence of a tab before
// trying to perform operations on it, and callers can gracefully handle the
// case of a tab being missing (if it's expected).
class TabListBridge : public TabListInterface, public TabStripModelObserver {
 public:
  DECLARE_USER_DATA(TabListBridge);

  TabListBridge(TabStripModel& tab_strip_model,
                ui::UnownedUserDataHost& unowned_data_host);
  TabListBridge(const TabListBridge&) = delete;
  TabListBridge& operator=(const TabListBridge&) = delete;
  ~TabListBridge() override;

  // TabListInterface:
  void AddTabListInterfaceObserver(TabListInterfaceObserver* observer) override;
  void RemoveTabListInterfaceObserver(
      TabListInterfaceObserver* observer) override;
  int GetTabCount() const override;
  int GetActiveIndex() const override;
  tabs::TabInterface* GetActiveTab() override;
  tabs::TabInterface* OpenTab(const GURL& url, int index) override;
  void DiscardTab(tabs::TabHandle tab) override;
  tabs::TabInterface* DuplicateTab(tabs::TabHandle tab) override;
  tabs::TabInterface* GetTab(int index) override;
  int GetIndexOfTab(tabs::TabHandle tab) override;
  void HighlightTabs(tabs::TabHandle tab_to_activate,
                     const std::set<tabs::TabHandle>& tabs) override;
  void MoveTab(tabs::TabHandle tab, int index) override;
  void CloseTab(tabs::TabHandle tab) override;
  std::vector<tabs::TabInterface*> GetAllTabs() override;
  void PinTab(tabs::TabHandle tab) override;
  void UnpinTab(tabs::TabHandle tab) override;
  std::optional<tab_groups::TabGroupId> AddTabsToGroup(
      std::optional<tab_groups::TabGroupId> group_id,
      const std::set<tabs::TabHandle>& tabs) override;
  void Ungroup(const std::set<tabs::TabHandle>& tabs) override;
  void MoveGroupTo(tab_groups::TabGroupId group_id, int index) override;
  void MoveTabToWindow(tabs::TabHandle tab,
                       SessionID destination_window_id,
                       int destination_index) override;
  void MoveTabGroupToWindow(tab_groups::TabGroupId group_id,
                            SessionID destination_window_id,
                            int destination_index) override;

 private:
  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // The underlying TabStripModel that this serves as a bridge for.
  // Must outlive this object.
  raw_ref<TabStripModel> tab_strip_;

  base::ObserverList<TabListInterfaceObserver> observers_;

  ui::ScopedUnownedUserData<TabListBridge> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_LIST_BRIDGE_H_
