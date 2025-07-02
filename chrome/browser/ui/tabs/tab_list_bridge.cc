// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_list_bridge.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

DEFINE_USER_DATA(TabListBridge);

TabListBridge::TabListBridge(TabStripModel& tab_strip_model,
                             UnownedUserDataHost& unowned_user_data_host)
    : tab_strip_(tab_strip_model),
      scoped_data_holder_(unowned_user_data_host, *this) {}

TabListBridge::~TabListBridge() = default;

// static
TabListInterface* TabListBridge::From(
    BrowserWindowInterface* browser_window_interface) {
  return ScopedUnownedUserData<TabListBridge>::Get(
      browser_window_interface->GetUnownedUserDataHost());
}

void TabListBridge::OpenTab(const GURL& url, int index) {}

void TabListBridge::DiscardTab(tabs::TabHandle tab) {}

void TabListBridge::DuplicateTab(tabs::TabHandle tab) {}

tabs::TabInterface* TabListBridge::GetTab(int index) {
  return tab_strip_->GetTabAtIndex(index);
}

void TabListBridge::HighlightTabs(const std::set<tabs::TabHandle>& tabs) {}

void TabListBridge::MoveTab(tabs::TabHandle tab, int index) {}

void TabListBridge::CloseTab(tabs::TabHandle tab) {}

std::vector<tabs::TabInterface*> TabListBridge::GetAllTabs() {
  return {};
}

void TabListBridge::PinTab(tabs::TabHandle tab) {}

void TabListBridge::UnpinTab(tabs::TabHandle tab) {}

std::optional<tab_groups::TabGroupId> TabListBridge::AddTabsToGroup(
    std::optional<tab_groups::TabGroupId> group_id,
    const std::set<tabs::TabHandle>& tabs) {
  return std::nullopt;
}

void TabListBridge::Ungroup(const std::set<tabs::TabHandle>& tabs) {}

void TabListBridge::MoveGroupTo(tab_groups::TabGroupId group_id, int index) {}
