// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_collection.h"

#include <cstddef>
#include <memory>
#include <optional>

#include "base/containers/adapters.h"
#include "chrome/browser/ui/tabs/pinned_tab_collection.h"
#include "chrome/browser/ui/tabs/tab_collection.h"
#include "chrome/browser/ui/tabs/tab_collection_storage.h"
#include "chrome/browser/ui/tabs/tab_group_tab_collection.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/unpinned_tab_collection.h"

namespace tabs {

TabStripCollection::TabStripCollection() {
  impl_ = std::make_unique<TabCollectionStorage>(*this);
  pinned_collection_ = static_cast<PinnedTabCollection*>(
      impl_->AddCollection(std::make_unique<PinnedTabCollection>(), 0));
  unpinned_collection_ = static_cast<UnpinnedTabCollection*>(
      impl_->AddCollection(std::make_unique<UnpinnedTabCollection>(), 1));

  pinned_collection_->OnReparented(this);
  unpinned_collection_->OnReparented(this);
}

TabStripCollection::~TabStripCollection() = default;

bool TabStripCollection::ContainsTab(TabModel* tab_model) const {
  return false;
}

bool TabStripCollection::ContainsTabRecursive(TabModel* tab_model) const {
  return pinned_collection_->ContainsTabRecursive(tab_model) ||
         unpinned_collection_->ContainsTabRecursive(tab_model);
}

void TabStripCollection::AddTabRecursive(
    std::unique_ptr<TabModel> tab_model,
    size_t index,
    std::optional<tab_groups::TabGroupId> new_group_id,
    bool new_pinned_state) {
  CHECK(index >= 0 && index <= TabCountRecursive());
  if (new_pinned_state) {
    CHECK(!new_group_id.has_value());
    pinned_collection_->AddTab(std::move(tab_model), index);
  } else {
    if (new_group_id.has_value()) {
      MaybeCreateNewGroupCollectionForTab(index, new_group_id.value());
    }
    unpinned_collection_->AddTabRecursive(
        std::move(tab_model), index - pinned_collection_->TabCountRecursive(),
        new_group_id);
  }
}

void TabStripCollection::MoveTabRecursive(
    size_t initial_index,
    size_t final_index,
    std::optional<tab_groups::TabGroupId> new_group_id,
    bool new_pinned_state) {
  TabModel* tab_model = GetTabAtIndexRecursive(initial_index);
  CHECK(tab_model);
  const std::optional<tab_groups::TabGroupId> old_group = tab_model->group();
  TabGroupTabCollection* old_group_collection =
      old_group.has_value()
          ? unpinned_collection_->GetTabGroupCollection(old_group.value())
          : nullptr;
  const bool is_only_tab_in_group =
      old_group.has_value() && old_group_collection->ChildCount() == 1;

  if (old_group == new_group_id && new_group_id.has_value() &&
      is_only_tab_in_group) {
    unpinned_collection_->MoveGroupToRecursive(
        final_index - pinned_collection_->TabCountRecursive(),
        old_group_collection);
  } else {
    std::unique_ptr<tabs::TabModel> moved_data =
        RemoveTabRecursive(tab_model, old_group != new_group_id);
    AddTabRecursive(std::move(moved_data), final_index, new_group_id,
                    new_pinned_state);
  }
}

void TabStripCollection::MoveTabsRecursive(
    const std::vector<int>& tab_indices,
    size_t destination_index,
    std::optional<tab_groups::TabGroupId> new_group_id,
    bool new_pinned_state) {
  std::vector<std::unique_ptr<tabs::TabModel>> moved_datas;
  // Remove all the tabs from the model.
  for (int tab_index : base::Reversed(tab_indices)) {
    std::unique_ptr<tabs::TabModel> moved_data =
        RemoveTabAtIndexRecursive(tab_index);
    moved_datas.insert(moved_datas.begin(), std::move(moved_data));
  }

  //  Add all the tabs back to the model.
  for (size_t i = 0; i < moved_datas.size(); i++) {
    AddTabRecursive(std::move(moved_datas[i]), destination_index + i,
                    new_group_id, new_pinned_state);
  }
}

void TabStripCollection::MoveGroupTo(const TabGroupModel* group_model,
                                     const tab_groups::TabGroupId& group,
                                     int to_index) {
  tabs::TabGroupTabCollection* group_collection =
      unpinned_collection_->GetTabGroupCollection(group);
  unpinned_collection_->MoveGroupToRecursive(
      to_index - pinned_collection_->TabCountRecursive(), group_collection);
}

tabs::TabModel* TabStripCollection::GetTabAtIndexRecursive(size_t index) const {
  const size_t pinned_count = pinned_collection_->TabCountRecursive();

  if (index < pinned_count) {
    return pinned_collection_->GetTabAtIndex(index);
  } else {
    // Adjust the index for the unpinned collection (subtract the count of
    // pinned tabs)
    const size_t unpinned_index = index - pinned_count;
    return unpinned_collection_->GetTabAtIndexRecursive(unpinned_index);
  }
}

bool TabStripCollection::ContainsCollection(TabCollection* collection) const {
  return impl_->ContainsCollection(collection);
}

std::optional<size_t> TabStripCollection::GetIndexOfTabRecursive(
    const TabModel* tab_model) const {
  // Check if the tab is present in the pinned collection first and return the
  // index if it is present.
  std::optional<size_t> pinned_index =
      pinned_collection_->GetIndexOfTabRecursive(tab_model);
  if (pinned_index.has_value()) {
    return pinned_index.value();
  }

  // Check the unpinned tab collection only if the tab is not present in the
  // pinned tab collection and return the correct index taking pinned tabs into
  // account.
  std::optional<size_t> unpinned_index =
      unpinned_collection_->GetIndexOfTabRecursive(tab_model);
  if (unpinned_index.has_value()) {
    return pinned_collection_->TabCountRecursive() + unpinned_index.value();
  }

  return std::nullopt;
}

std::unique_ptr<TabModel> TabStripCollection::RemoveTabAtIndexRecursive(
    size_t index) {
  TabModel* tab_to_be_removed = GetTabAtIndexRecursive(index);
  return RemoveTabRecursive(tab_to_be_removed);
}

std::unique_ptr<TabModel> TabStripCollection::RemoveTabRecursive(
    TabModel* tab,
    bool close_empty_group_collection) {
  CHECK(tab);

  TabCollection* parent_collection = tab->GetParentCollection(GetPassKey());
  const std::optional<tab_groups::TabGroupId> group = tab->group();

  std::unique_ptr<TabModel> removed_tab =
      parent_collection->MaybeRemoveTab(tab);
  CHECK(removed_tab);

  if (group.has_value() && close_empty_group_collection) {
    MaybeRemoveGroupCollection(group.value());
  }

  return removed_tab;
}

std::optional<size_t> TabStripCollection::GetIndexOfCollection(
    TabCollection* collection) const {
  return impl_->GetIndexOfCollection(collection);
}

std::unique_ptr<TabModel> TabStripCollection::MaybeRemoveTab(
    TabModel* tab_model) {
  return nullptr;
}

std::unique_ptr<TabCollection> TabStripCollection::MaybeRemoveCollection(
    TabCollection* collection) {
  return nullptr;
}

size_t TabStripCollection::ChildCount() const {
  return impl_->GetChildrenCount();
}

size_t TabStripCollection::TabCountRecursive() const {
  return pinned_collection_->TabCountRecursive() +
         unpinned_collection_->TabCountRecursive();
}

TabGroupTabCollection* TabStripCollection::MaybeCreateNewGroupCollectionForTab(
    int index,
    const tab_groups::TabGroupId& new_group) {
  // Do not create a collection if the group is already present.
  if (unpinned_collection_->GetTabGroupCollection(new_group)) {
    return nullptr;
  }

  // Add it to the end.
  if (index == static_cast<int>(TabCountRecursive())) {
    return unpinned_collection_->AddTabGroup(
        std::make_unique<TabGroupTabCollection>(new_group),
        unpinned_collection_->ChildCount());
  }

  tabs::TabModel* tab_model = GetTabAtIndexRecursive(index);
  return unpinned_collection_->AddTabGroup(
      std::make_unique<TabGroupTabCollection>(new_group),
      unpinned_collection_
          ->GetDirectChildIndexOfCollectionContainingTab(tab_model)
          .value());
}

void TabStripCollection::MaybeRemoveGroupCollection(
    const tab_groups::TabGroupId& group) {
  TabGroupTabCollection* group_collection =
      unpinned_collection_->GetTabGroupCollection(group);

  if (group_collection && group_collection->TabCountRecursive() == 0) {
    unpinned_collection_->CloseTabGroup(group_collection);
  }
}

}  // namespace tabs
