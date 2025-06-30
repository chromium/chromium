// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_LIST_BRIDGE_H_
#define CHROME_BROWSER_UI_TABS_TAB_LIST_BRIDGE_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/browser/ui/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class TabStripModel;

class TabListBridge : public TabListInterface {
 public:
  DECLARE_USER_DATA(TabListBridge);

  TabListBridge(TabStripModel& tab_strip_model,
                UnownedUserDataHost& unowned_data_host);
  TabListBridge(const TabListBridge&) = delete;
  TabListBridge& operator=(const TabListBridge&) = delete;
  ~TabListBridge() override;

  // TODO(devlin): This should be accessible from a BrowserWindowInterface
  // or the TabListInterface so that it can be shared in all builds that use
  // the TabListInterface.
  static TabListInterface* From(
      BrowserWindowInterface* browser_window_interface);

  // TabListInterface:
  void OpenTab(const GURL& url, int index) override;
  void DiscardTab(tabs::TabHandle tab) override;
  void DuplicateTab(int index) override;
  tabs::TabInterface* GetTab(int index) override;
  void HighlightTabs(const std::set<tabs::TabHandle>& tabs) override;
  void MoveTab(int from_index, int to_index) override;
  void CloseTab(int index) override;
  std::vector<tabs::TabInterface*> GetAllTabs() override;
  void PinTab(tabs::TabHandle tab) override;
  void UnpinTab(tabs::TabHandle tab) override;
  std::optional<tab_groups::TabGroupId> AddTabsToGroup(
      std::optional<tab_groups::TabGroupId> group_id,
      const std::set<tabs::TabHandle>& tabs) override;
  void Ungroup(const std::set<tabs::TabHandle>& tabs) override;
  void MoveGroupTo(tab_groups::TabGroupId group_id, int index) override;

 private:
  raw_ref<TabStripModel> tab_strip_;
  ScopedUnownedUserData<TabListBridge> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_LIST_BRIDGE_H_
