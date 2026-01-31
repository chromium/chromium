// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_selection_state.h"

#include "base/check.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace tabs {

TabStripModelSelectionState::TabStripModelSelectionState(TabStripModel* model)
    : selected_tabs_({}),
      active_tab_(nullptr),
      anchor_tab_(nullptr),
      model_(model) {}

TabStripModelSelectionState::TabStripModelSelectionState(
    std::unordered_set<raw_ptr<TabInterface>> selected_tabs,
    raw_ptr<TabInterface> active_tab,
    raw_ptr<TabInterface> anchor_tab)
    : selected_tabs_(std::move(selected_tabs)),
      active_tab_(active_tab),
      anchor_tab_(anchor_tab) {}

TabStripModelSelectionState::~TabStripModelSelectionState() = default;
TabStripModelSelectionState::TabStripModelSelectionState(
    const TabStripModelSelectionState&) = default;
TabStripModelSelectionState& TabStripModelSelectionState::operator=(
    const TabStripModelSelectionState&) = default;

bool TabStripModelSelectionState::operator==(
    const TabStripModelSelectionState& other) const {
  return selected_tabs_ == other.selected_tabs_ &&
         active_tab_ == other.active_tab_ && anchor_tab_ == other.anchor_tab_;
}

void TabStripModelSelectionState::Clear() {
  InvalidateListSelectionModel();
  selected_tabs_.clear();
  active_tab_ = nullptr;
  anchor_tab_ = nullptr;
}

bool TabStripModelSelectionState::IsSelected(TabInterface* tab) const {
  return selected_tabs_.contains(tab);
}

void TabStripModelSelectionState::AddTabToSelection(TabInterface* tab) {
  if (tab) {
    InvalidateListSelectionModel();
    selected_tabs_.insert(tab);
  }
}

void TabStripModelSelectionState::RemoveTabFromSelection(TabInterface* tab) {
  InvalidateListSelectionModel();

  if (tab == active_tab_) {
    active_tab_ = nullptr;
  }

  if (tab == anchor_tab_) {
    anchor_tab_ = nullptr;
  }

  selected_tabs_.erase(tab);
}

void TabStripModelSelectionState::SetActiveTab(TabInterface* tab) {
  InvalidateListSelectionModel();
  active_tab_ = tab;
  if (!selected_tabs_.contains(tab)) {
    AddTabToSelection(tab);
  }
}

void TabStripModelSelectionState::SetAnchorTab(TabInterface* tab) {
  InvalidateListSelectionModel();
  anchor_tab_ = tab;
  if (!selected_tabs_.contains(tab)) {
    AddTabToSelection(tab);
  }
}

bool TabStripModelSelectionState::AppendTabsToSelection(
    const std::unordered_set<TabInterface*>& tabs) {
  InvalidateListSelectionModel();
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
  InvalidateListSelectionModel();
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

bool TabStripModelSelectionState::Valid() const {
  if (selected_tabs_.empty()) {
    return active_tab_ == nullptr && anchor_tab_ == nullptr;
  }
  return active_tab_ && anchor_tab_;
}

const ui::ListSelectionModel&
TabStripModelSelectionState::GetListSelectionModel() const {
  if (!list_selection_model_.has_value()) {
    UpdateListSelectionModel();
    CHECK(list_selection_model_.has_value());
  }

  return list_selection_model_.value();
}

void TabStripModelSelectionState::InvalidateListSelectionModel(
    base::PassKey<TabStripModel>) const {
  InvalidateListSelectionModel();
}

void TabStripModelSelectionState::UpdateListSelectionModel(
    base::PassKey<TabStripModel>) const {
  UpdateListSelectionModel();
}

void TabStripModelSelectionState::InvalidateListSelectionModel() const {
  list_selection_model_ = std::nullopt;
}

ui::ListSelectionModel::SelectedIndices
TabStripModelSelectionState::ComputeSelectedIndices() const {
  ui::ListSelectionModel::SelectedIndices indices;
  if (empty()) {
    return indices;
  }

  CHECK(model_);

  const auto tab_it_end = model_->end();

  for (auto it = model_->begin(); it != tab_it_end; ++it) {
    tabs::TabInterface* tab = *it;
    if (IsSelected(tab)) {
      size_t index = std::distance(model_->begin(), it);
      indices.insert(index);
    }
  }

  // The selected indices are already sorted.
  return indices;
}

void TabStripModelSelectionState::UpdateListSelectionModel() const {
  list_selection_model_ = ui::ListSelectionModel();

  if (empty()) {
    return;
  }

  CHECK(model_);

  if (active_tab()) {
    int active_index = model_->GetIndexOfTab(active_tab());
    CHECK(active_index != TabStripModel::kNoTab);
    list_selection_model_.value().set_active(active_index);
  }

  if (anchor_tab()) {
    int anchor_index = model_->GetIndexOfTab(anchor_tab());
    CHECK(anchor_index != TabStripModel::kNoTab);
    list_selection_model_.value().set_anchor(anchor_index);
  }

  for (size_t i : ComputeSelectedIndices()) {
    list_selection_model_.value().AddIndexToSelection(i);
  }
}

}  // namespace tabs
