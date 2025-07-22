// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_list_bridge.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

DEFINE_USER_DATA(TabListBridge);

TabListBridge::TabListBridge(TabStripModel& tab_strip_model,
                             ui::UnownedUserDataHost& unowned_user_data_host)
    : tab_strip_(tab_strip_model),
      scoped_data_holder_(unowned_user_data_host, *this) {
  tab_strip_->AddObserver(this);
}

// Note: TabStripObserver already implements RemoveObserver() calls; no need to
// remove this object as an observer here.
TabListBridge::~TabListBridge() = default;

void TabListBridge::AddTabListInterfaceObserver(
    TabListInterfaceObserver* observer) {
  observers_.AddObserver(observer);
}

void TabListBridge::RemoveTabListInterfaceObserver(
    TabListInterfaceObserver* observer) {
  observers_.RemoveObserver(observer);
}

int TabListBridge::GetTabCount() const {
  return tab_strip_->count();
}

int TabListBridge::GetActiveIndex() const {
  return tab_strip_->active_index();
}

tabs::TabInterface* TabListBridge::GetActiveTab() {
  return tab_strip_->GetActiveTab();
}

void TabListBridge::OpenTab(const GURL& url, int index) {}

void TabListBridge::DiscardTab(tabs::TabHandle tab) {}

void TabListBridge::DuplicateTab(tabs::TabHandle tab) {}

tabs::TabInterface* TabListBridge::GetTab(int index) {
  return tab_strip_->GetTabAtIndex(index);
}

int TabListBridge::GetIndexOfTab(tabs::TabHandle tab) {
  return tab_strip_->GetIndexOfTab(tab.Get());
}

void TabListBridge::HighlightTabs(tabs::TabHandle tab_to_activate,
                                  const std::set<tabs::TabHandle>& tabs) {}

void TabListBridge::MoveTab(tabs::TabHandle tab, int index) {}

void TabListBridge::CloseTab(tabs::TabHandle tab) {}

std::vector<tabs::TabInterface*> TabListBridge::GetAllTabs() {
  std::vector<tabs::TabInterface*> all_tabs;
  size_t tab_count = tab_strip_->count();
  all_tabs.reserve(tab_count);
  for (size_t i = 0; i < tab_count; ++i) {
    all_tabs.push_back(tab_strip_->GetTabAtIndex(i));
  }
  return all_tabs;
}

void TabListBridge::PinTab(tabs::TabHandle tab) {
  int index = GetIndexOfTab(tab);
  CHECK_NE(index, TabStripModel::kNoTab)
      << "Trying to pin a tab that doesn't exist in this tab list.";
  tab_strip_->SetTabPinned(index, true);
}

void TabListBridge::UnpinTab(tabs::TabHandle tab) {
  int index = GetIndexOfTab(tab);
  CHECK_NE(index, TabStripModel::kNoTab)
      << "Trying to unpin a tab that doesn't exist in this tab list.";
  tab_strip_->SetTabPinned(index, false);
}

std::optional<tab_groups::TabGroupId> TabListBridge::AddTabsToGroup(
    std::optional<tab_groups::TabGroupId> group_id,
    const std::set<tabs::TabHandle>& tabs) {
  return std::nullopt;
}

void TabListBridge::Ungroup(const std::set<tabs::TabHandle>& tabs) {}

void TabListBridge::MoveGroupTo(tab_groups::TabGroupId group_id, int index) {}

void TabListBridge::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kInserted: {
      // See comment on TabStripModelChange::Insert for notes about the format
      // of `contents`.
      // NOTE: This might be *unsafe* if callers mutate the tab strip model
      // synchronously from this event.
      for (const auto& web_contents_and_index : change.GetInsert()->contents) {
        // This will (correctly) crash if `tab` is not found. Since we just
        // inserted the tab, we know it should exist.
        tabs::TabInterface* tab = web_contents_and_index.tab.get();
        for (auto& observer : observers_) {
          observer.OnTabAdded(tab, web_contents_and_index.index);
        }
      }
      break;
    }
    case TabStripModelChange::kRemoved:
    case TabStripModelChange::kMoved:
    case TabStripModelChange::kReplaced:
    case TabStripModelChange::kSelectionOnly:
      break;
  }
}

// static
// From //chrome/browser/ui/tabs/tab_list_interface.h
TabListInterface* TabListInterface::From(
    BrowserWindowInterface* browser_window_interface) {
  return ui::ScopedUnownedUserData<TabListBridge>::Get(
      browser_window_interface->GetUnownedUserDataHost());
}
