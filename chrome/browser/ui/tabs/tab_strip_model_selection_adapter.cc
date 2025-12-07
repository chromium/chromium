// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_selection_adapter.h"

#include <algorithm>
#include <cstddef>
#include <unordered_set>

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_selection_state.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/models/list_selection_model.h"

// TabStripModelSelectionStateAdapter
TabStripModelSelectionStateAdapter::TabStripModelSelectionStateAdapter(
    TabStripModel* model)
    : tab_strip_model_(model) {}

TabStripModelSelectionStateAdapter::~TabStripModelSelectionStateAdapter() =
    default;

void TabStripModelSelectionStateAdapter::set_anchor(
    std::optional<size_t> anchor) {
  if (anchor.has_value()) {
    selection_state_.SetAnchorTab(
        tab_strip_model_->GetTabAtIndex(anchor.value()));
  } else {
    selection_state_.SetAnchorTab(nullptr);
  }
}

std::optional<size_t> TabStripModelSelectionStateAdapter::anchor() const {
  const int index =
      tab_strip_model_->GetIndexOfTab(selection_state_.anchor_tab());
  if (index != TabStripModel::kNoTab) {
    return static_cast<size_t>(index);
  }
  return std::nullopt;
}

void TabStripModelSelectionStateAdapter::set_active(
    std::optional<size_t> active) {
  if (active.has_value()) {
    selection_state_.SetActiveTab(
        tab_strip_model_->GetTabAtIndex(active.value()));
  } else {
    selection_state_.SetActiveTab(nullptr);
  }
}

std::optional<size_t> TabStripModelSelectionStateAdapter::active() const {
  if (selection_state_.active_tab()) {
    const int index =
        tab_strip_model_->GetIndexOfTab(selection_state_.active_tab());
    if (index != TabStripModel::kNoTab) {
      return static_cast<size_t>(index);
    }
  }
  return std::nullopt;
}

bool TabStripModelSelectionStateAdapter::empty() const {
  return selection_state_.empty();
}

size_t TabStripModelSelectionStateAdapter::size() const {
  return selection_state_.size();
}

void TabStripModelSelectionStateAdapter::IncrementFrom(size_t increment_index) {
}

void TabStripModelSelectionStateAdapter::DecrementFrom(
    size_t decrement_index,  // unused
    tabs::TabInterface* tab_being_removed) {
  if (tab_being_removed == selection_state_.active_tab()) {
    selection_state_.SetActiveTab(nullptr);
  }
  if (tab_being_removed == selection_state_.anchor_tab()) {
    selection_state_.SetAnchorTab(nullptr);
  }
  selection_state_.RemoveTabFromSelection(tab_being_removed);
}

void TabStripModelSelectionStateAdapter::Move(size_t old_index,
                                              size_t new_index,
                                              size_t length) {}

void TabStripModelSelectionStateAdapter::SetSelectedIndex(
    std::optional<size_t> index) {
  Clear();
  if (index.has_value()) {
    AddIndexToSelection(index.value());
    set_active(index);
    set_anchor(index);
  }
}

bool TabStripModelSelectionStateAdapter::IsSelected(size_t index) const {
  if (tab_strip_model_->ContainsIndex(index)) {
    return selection_state_.IsSelected(tab_strip_model_->GetTabAtIndex(index));
  }
  return false;
}

void TabStripModelSelectionStateAdapter::AddIndexToSelection(size_t index) {
  if (tab_strip_model_->ContainsIndex(index)) {
    selection_state_.AddTabToSelection(tab_strip_model_->GetTabAtIndex(index));
  }
}

void TabStripModelSelectionStateAdapter::AddIndexRangeToSelection(
    size_t index_start,
    size_t index_end) {
  int index = 0;
  auto tab_iter = tab_strip_model_->begin();
  const auto tab_iter_end = tab_strip_model_->end();

  while (tab_iter != tab_iter_end && index <= static_cast<int>(index_end)) {
    if (index >= static_cast<int>(index_start)) {
      selection_state_.AddTabToSelection(*tab_iter);
    }

    tab_iter++;
    index++;
  }
}

void TabStripModelSelectionStateAdapter::RemoveIndexFromSelection(
    size_t index) {
  if (tab_strip_model_->ContainsIndex(index)) {
    selection_state_.RemoveTabFromSelection(
        tab_strip_model_->GetTabAtIndex(index));
  }
}

void TabStripModelSelectionStateAdapter::SetSelectionFromAnchorTo(
    size_t index) {
  std::optional<size_t> current_anchor = anchor();
  Clear();
  set_anchor(current_anchor);
  AddSelectionFromAnchorTo(index);
}

void TabStripModelSelectionStateAdapter::AddSelectionFromAnchorTo(
    size_t index) {
  std::optional<size_t> anchor_index = anchor();
  if (!anchor_index.has_value()) {
    SetSelectedIndex(index);
    return;
  }

  size_t start = std::min(anchor_index.value(), index);
  size_t end = std::max(anchor_index.value(), index);
  AddIndexRangeToSelection(start, end);
  set_active(index);
}

void TabStripModelSelectionStateAdapter::Clear() {
  selection_state_.SetSelectedTabs({});
  selection_state_.SetActiveTab(nullptr);
  selection_state_.SetAnchorTab(nullptr);
}

ui::ListSelectionModel
TabStripModelSelectionStateAdapter::ToListSelectionModel() const {
  ui::ListSelectionModel list_selection_model;
  list_selection_model.set_active(active());
  list_selection_model.set_anchor(anchor());
  for (size_t index : selected_indices()) {
    list_selection_model.AddIndexToSelection(index);
  }
  return list_selection_model;
}

ui::ListSelectionModel::SelectedIndices
TabStripModelSelectionStateAdapter::selected_indices() const {
  ui::ListSelectionModel::SelectedIndices indices;
  int index = 0;
  auto tab_iter = tab_strip_model_->begin();
  const auto tab_iter_end = tab_strip_model_->end();
  while (tab_iter != tab_iter_end) {
    if (selection_state_.IsSelected(*tab_iter)) {
      indices.insert(index);
    }

    tab_iter++;
    index++;
  }

  // the selected indices are already sorted due to iterating through the tabs.
  return indices;
}

// ListSelectionModelAdapter
ListSelectionModelAdapter::ListSelectionModelAdapter() = default;

ListSelectionModelAdapter::ListSelectionModelAdapter(
    const ui::ListSelectionModel& model)
    : model_(model) {}

ListSelectionModelAdapter::~ListSelectionModelAdapter() = default;

void ListSelectionModelAdapter::set_anchor(std::optional<size_t> anchor) {
  model_.set_anchor(anchor);
}

std::optional<size_t> ListSelectionModelAdapter::anchor() const {
  return model_.anchor();
}

void ListSelectionModelAdapter::set_active(std::optional<size_t> active) {
  model_.set_active(active);
}

std::optional<size_t> ListSelectionModelAdapter::active() const {
  return model_.active();
}

bool ListSelectionModelAdapter::empty() const {
  return model_.empty();
}

size_t ListSelectionModelAdapter::size() const {
  return model_.size();
}

void ListSelectionModelAdapter::IncrementFrom(size_t index) {
  model_.IncrementFrom(index);
}

void ListSelectionModelAdapter::DecrementFrom(
    size_t index,
    tabs::TabInterface* tab_being_removed) {
  model_.DecrementFrom(index);
}

void ListSelectionModelAdapter::SetSelectedIndex(std::optional<size_t> index) {
  model_.SetSelectedIndex(index);
}

bool ListSelectionModelAdapter::IsSelected(size_t index) const {
  return model_.IsSelected(index);
}

void ListSelectionModelAdapter::AddIndexToSelection(size_t index) {
  model_.AddIndexToSelection(index);
}

void ListSelectionModelAdapter::AddIndexRangeToSelection(size_t index_start,
                                                         size_t index_end) {
  model_.AddIndexRangeToSelection(index_start, index_end);
}

void ListSelectionModelAdapter::RemoveIndexFromSelection(size_t index) {
  model_.RemoveIndexFromSelection(index);
}

void ListSelectionModelAdapter::SetSelectionFromAnchorTo(size_t index) {
  model_.SetSelectionFromAnchorTo(index);
}

void ListSelectionModelAdapter::AddSelectionFromAnchorTo(size_t index) {
  model_.AddSelectionFromAnchorTo(index);
}

void ListSelectionModelAdapter::Move(size_t old_index,
                                     size_t new_index,
                                     size_t length) {
  model_.Move(old_index, new_index, length);
}

void ListSelectionModelAdapter::Clear() {
  model_.Clear();
}

ui::ListSelectionModel ListSelectionModelAdapter::ToListSelectionModel() const {
  return model_;
}

ui::ListSelectionModel::SelectedIndices
ListSelectionModelAdapter::selected_indices() const {
  return model_.selected_indices();
}
