// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_collection.h"

#include <cstddef>
#include <memory>
#include <optional>

#include "chrome/browser/ui/tabs/pinned_tab_collection.h"
#include "chrome/browser/ui/tabs/tab_collection.h"
#include "chrome/browser/ui/tabs/tab_collection_storage.h"
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
    unpinned_collection_->AddTabRecursive(
        std::move(tab_model), index - pinned_collection_->TabCountRecursive(),
        new_group_id);
  }
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
  CHECK(tab_to_be_removed);

  TabCollection* parent_collection =
      tab_to_be_removed->GetParentCollection(GetPassKey());
  std::unique_ptr<TabModel> removed_tab =
      parent_collection->MaybeRemoveTab(tab_to_be_removed);
  CHECK(removed_tab);

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

TabGroupTabCollection* TabStripCollection::CreateNewGroupCollectionForTab(
    const TabModel* tab_model,
    const tab_groups::TabGroupId& new_group) {
  if (tab_model->GetParentCollection(GetPassKey()) == pinned_collection_) {
    return unpinned_collection_->AddTabGroup(
        std::make_unique<TabGroupTabCollection>(new_group), 0);
  } else {
    return unpinned_collection_->AddTabGroup(
        std::make_unique<TabGroupTabCollection>(new_group),
        unpinned_collection_
                ->GetDirectChildIndexOfCollectionContainingTab(tab_model)
                .value() +
            1);
  }
}

}  // namespace tabs
