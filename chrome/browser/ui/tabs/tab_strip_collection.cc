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

bool TabStripCollection::ContainsCollection(TabCollection* collection) const {
  return impl_->ContainsCollection(collection);
}

std::optional<size_t> TabStripCollection::GetIndexOfTabRecursive(
    TabModel* tab_model) const {
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

}  // namespace tabs
