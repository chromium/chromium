// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/pinned_tab_collection.h"

#include <memory>
#include <optional>

#include "chrome/browser/ui/tabs/tab_collection_storage.h"
#include "chrome/browser/ui/tabs/tab_model.h"

namespace tabs {

PinnedTabCollection::PinnedTabCollection() {
  impl_ = std::make_unique<TabCollectionStorage>(*this);
}

PinnedTabCollection::~PinnedTabCollection() = default;

void PinnedTabCollection::AddTab(std::unique_ptr<TabModel> tab_model,
                                 size_t index) {
  TabModel* inserted_tab_model = impl_->AddTab(std::move(tab_model), index);
  inserted_tab_model->set_pinned(/*pinned=*/true);
  inserted_tab_model->OnReparented(this, GetPassKey());
}

void PinnedTabCollection::AppendTab(std::unique_ptr<TabModel> tab_model) {
  AddTab(std::move(tab_model), ChildCount());
}

void PinnedTabCollection::MoveTab(TabModel* tab_model, size_t index) {
  impl_->MoveTab(tab_model, index);
}

void PinnedTabCollection::CloseTab(TabModel* tab_model) {
  impl_->CloseTab(tab_model);
}

tabs::TabModel* PinnedTabCollection::GetTabAtIndex(size_t index) const {
  return impl_->GetTabAtIndex(index);
}

bool PinnedTabCollection::ContainsTab(TabModel* tab_model) const {
  return impl_->ContainsTab(tab_model);
}

bool PinnedTabCollection::ContainsTabRecursive(TabModel* tab_model) const {
  return impl_->ContainsTab(tab_model);
}

bool PinnedTabCollection::ContainsCollection(TabCollection* collection) const {
  return false;
}

std::optional<size_t> PinnedTabCollection::GetIndexOfTabRecursive(
    TabModel* tab_model) const {
  return impl_->GetIndexOfTab(tab_model);
}

std::optional<size_t> PinnedTabCollection::GetIndexOfCollection(
    TabCollection* collection) const {
  return std::nullopt;
}

std::unique_ptr<TabModel> PinnedTabCollection::MaybeRemoveTab(
    TabModel* tab_model) {
  if (!ContainsTab(tab_model)) {
    return nullptr;
  }

  std::unique_ptr<TabModel> removed_tab_model = impl_->RemoveTab(tab_model);
  removed_tab_model->set_pinned(/*pinned=*/false);
  removed_tab_model->OnReparented(nullptr, GetPassKey());
  return removed_tab_model;
}

size_t PinnedTabCollection::ChildCount() const {
  return impl_->GetChildrenCount();
}

size_t PinnedTabCollection::TabCountRecursive() const {
  // Same as total number of children since there are no child collections.
  return ChildCount();
}

std::unique_ptr<TabCollection> PinnedTabCollection::MaybeRemoveCollection(
    TabCollection* collection) {
  return nullptr;
}

}  // namespace tabs
