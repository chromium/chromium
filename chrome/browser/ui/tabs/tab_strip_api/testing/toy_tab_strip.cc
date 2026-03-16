// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip.h"

#include "base/logging.h"

namespace tabs_api::testing {

ToyTabStrip::ToyTabStrip()
    : tab_strip_collection_(std::make_unique<tabs::TabStripCollection>()),
      root_{tab_strip_collection_->GetHandle(), std::vector<ToyTab>()} {
  // Increment id since tab_strip_collection_ uses the next handle as its id.
  GetNextId();
}

std::optional<ToyTab> ToyTabStrip::GetToyTabFor(tabs::TabHandle handle) const {
  for (auto& tab : root_.tabs) {
    if (tab.tab_handle == handle) {
      return tab;
    }
  }
  return std::nullopt;
}

void ToyTabStrip::AddTab(ToyTab tab) {
  root_.tabs.push_back(tab);
}

std::vector<tabs::TabHandle> ToyTabStrip::GetTabs() {
  std::vector<tabs::TabHandle> result;
  for (auto& tab : root_.tabs) {
    result.push_back(tab.tab_handle);
  }
  return result;
}

void ToyTabStrip::CloseTab(size_t index) {
  if (index < 0 || index >= root_.tabs.size()) {
    LOG(FATAL) << "invalid idx passed in: " << index
               << ", tab size is: " << root_.tabs.size();
  }
  root_.tabs.erase(root_.tabs.begin() + index);
}

std::optional<int> ToyTabStrip::GetIndexForHandle(tabs::TabHandle tab_handle) {
  for (size_t i = 0; i < root_.tabs.size(); ++i) {
    if (root_.tabs.at(i).tab_handle == tab_handle) {
      return i;
    }
  }

  return std::nullopt;
}

tabs::TabHandle ToyTabStrip::AddTabAt(const GURL& url,
                                      std::optional<int> index) {
  auto tab = ToyTab{
      tabs::TabHandle(GetNextId()),
      url,
  };

  if (index.has_value()) {
    root_.tabs.insert(root_.tabs.begin() + index.value(), tab);
  } else {
    root_.tabs.push_back(tab);
  }

  return tab.tab_handle;
}

void ToyTabStrip::ActivateTab(tabs::TabHandle handle) {
  for (auto& tab : root_.tabs) {
    tab.active = tab.tab_handle == handle;
    tab.selected = tab.selected || tab.tab_handle == handle;
  }
}

tabs::TabHandle ToyTabStrip::FindActiveTab() {
  for (auto& tab : root_.tabs) {
    if (tab.active) {
      return tab.tab_handle;
    }
  }
  NOTREACHED() << "toy tab strip does not guarantee one tab is always active, "
                  "did you forget to activate a tab beforehand?";
}

void ToyTabStrip::MoveTab(tabs::TabHandle handle, size_t to) {
  auto idx = GetIndexForHandle(handle).value();
  auto tab = root_.tabs.at(idx);

  root_.tabs.erase(root_.tabs.begin() + idx);
  root_.tabs.insert(root_.tabs.begin() + to, tab);
}

int ToyTabStrip::GetNextId() {
  static int id = 0;
  return id++;
}

std::optional<tab_groups::TabGroupId> ToyTabStrip::GetGroupIdFor(
    const tabs::TabCollectionHandle& handle) const {
  for (const auto& group : groups_with_visuals_) {
    if (group.handle == handle) {
      return group.id;
    }
  }
  return std::nullopt;
}

tabs::TabCollectionHandle ToyTabStrip::AddGroup(
    const tab_groups::TabGroupVisualData& visual_data) {
  return AddGroup(tab_groups::TabGroupId::GenerateNew(), visual_data);
}

tabs::TabCollectionHandle ToyTabStrip::AddGroup(
    const tab_groups::TabGroupId& group_id,
    const tab_groups::TabGroupVisualData& visual_data) {
  ToyTabGroupData new_group{group_id, tabs::TabCollectionHandle(GetNextId()),
                            visual_data};
  auto handle = new_group.handle;
  groups_with_visuals_.push_back(std::move(new_group));
  return handle;
}

const tab_groups::TabGroupVisualData* ToyTabStrip::GetGroupVisualData(
    const tabs::TabCollectionHandle& handle) const {
  for (const auto& group : groups_with_visuals_) {
    if (group.handle == handle) {
      return &group.visuals;
    }
  }
  return nullptr;
}

void ToyTabStrip::UpdateGroupVisuals(
    const tab_groups::TabGroupId& group_id,
    const tab_groups::TabGroupVisualData& new_visuals) {
  for (auto& group : groups_with_visuals_) {
    if (group.id == group_id) {
      group.visuals = new_visuals;
      return;
    }
  }
}

void ToyTabStrip::SetActiveTab(tabs::TabHandle handle) {
  for (auto& tab : root_.tabs) {
    tab.active = tab.tab_handle == handle;
  }
}

void ToyTabStrip::SetTabSelection(std::set<tabs::TabHandle> selection) {
  for (auto& tab : root_.tabs) {
    tab.selected = selection.contains(tab.tab_handle);
  }
}

std::optional<tab_groups::TabGroupId> ToyTabStrip::GetTabGroupForTab(
    int index) const {
  if (index < 0 || static_cast<size_t>(index) >= root_.tabs.size()) {
    return std::nullopt;
  }
  return root_.tabs[index].group_id;
}

tabs::TabCollectionHandle ToyTabStrip::GetCollectionHandleForTabGroupId(
    tab_groups::TabGroupId group_id) const {
  for (const auto& group : groups_with_visuals_) {
    if (group.id == group_id) {
      return group.handle;
    }
  }
  return tabs::TabCollectionHandle::Null();
}

tabs::TabCollectionHandle ToyTabStrip::GetCollectionHandleForSplitTabId(
    split_tabs::SplitTabId split_id) const {
  for (const auto& split : splits_) {
    if (split.id == split_id) {
      return split.handle;
    }
  }
  return tabs::TabCollectionHandle::Null();
}

tabs::TabCollectionHandle ToyTabStrip::AddSplit(
    split_tabs::SplitTabId split_id) {
  ToySplitTabData new_split{split_id, tabs::TabCollectionHandle(GetNextId())};
  auto handle = new_split.handle;
  splits_.push_back(std::move(new_split));
  return handle;
}

}  // namespace tabs_api::testing
