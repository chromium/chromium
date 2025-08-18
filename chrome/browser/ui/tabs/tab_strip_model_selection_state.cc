// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_selection_state.h"

#include "base/check.h"

namespace tabs {

TabStripModelSelectionState::TabStripModelSelectionState()
    : TabStripModelSelectionState({}, nullptr, nullptr) {}

TabStripModelSelectionState::TabStripModelSelectionState(
    std::unordered_set<raw_ptr<TabInterface>> selected_tabs,
    raw_ptr<TabInterface> active_tab,
    raw_ptr<TabInterface> anchor_tab)
    : selected_tabs_(std::move(selected_tabs)),
      active_tab_(active_tab),
      anchor_tab_(anchor_tab) {}

TabStripModelSelectionState::~TabStripModelSelectionState() = default;

bool TabStripModelSelectionState::operator==(
    const TabStripModelSelectionState& other) const {
  return selected_tabs_ == other.selected_tabs_ &&
         active_tab_ == other.active_tab_ && anchor_tab_ == other.anchor_tab_;
}

bool TabStripModelSelectionState::IsSelected(TabInterface* tab) const {
  return selected_tabs_.contains(tab);
}

void TabStripModelSelectionState::AddTabToSelection(TabInterface* tab) {
  if (tab) {
    selected_tabs_.insert(tab);
  }
}

void TabStripModelSelectionState::RemoveTabFromSelection(TabInterface* tab) {
  selected_tabs_.erase(tab);
}

void TabStripModelSelectionState::SetActiveTab(TabInterface* tab) {
  active_tab_ = tab;
  if (tab) {
    AddTabToSelection(tab);
  }
}

void TabStripModelSelectionState::SetAnchorTab(TabInterface* tab) {
  anchor_tab_ = tab;
  if (tab) {
    AddTabToSelection(tab);
  }
}

bool TabStripModelSelectionState::AppendTabsToSelection(
    std::unordered_set<TabInterface*> tabs) {
  bool selection_changed = false;
  for (TabInterface* tab : tabs) {
    if (!IsSelected(tab)) {
      selection_changed = true;
      AddTabToSelection(tab);
    }
  }
  return selection_changed;
}

void TabStripModelSelectionState::SetSelectedTabs(
    std::unordered_set<TabInterface*> tabs,
    TabInterface* active_tab,
    TabInterface* anchor_tab) {
  selected_tabs_.clear();
  for (TabInterface* tab : tabs) {
    selected_tabs_.insert(tab);
  }

  if (active_tab) {
    CHECK(IsSelected(active_tab));
    active_tab_ = active_tab;
  } else {
    active_tab_ = selected_tabs_.empty() ? nullptr : *selected_tabs_.begin();
  }

  if (anchor_tab) {
    CHECK(IsSelected(anchor_tab));
    anchor_tab_ = anchor_tab;
  } else {
    anchor_tab_ = selected_tabs_.empty() ? nullptr : *selected_tabs_.begin();
  }
  CHECK(Valid());
}

bool TabStripModelSelectionState::Valid() {
  if (selected_tabs_.empty()) {
    return active_tab_ == nullptr && anchor_tab_ == nullptr;
  }
  return active_tab_ && anchor_tab_ && IsSelected(active_tab_) &&
         IsSelected(anchor_tab_);
}

}  // namespace tabs
