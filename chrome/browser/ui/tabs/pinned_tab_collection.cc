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
  CHECK(index <= ChildCount() && index >= 0);
  CHECK(tab_model);

  TabModel* inserted_tab_model = impl_->AddTab(std::move(tab_model), index);
  inserted_tab_model->set_pinned(/*pinned=*/true);
  inserted_tab_model->OnReparented(this, GetPassKey());
}

void PinnedTabCollection::AppendTab(std::unique_ptr<TabModel> tab_model) {
  CHECK(tab_model);
  AddTab(std::move(tab_model), ChildCount());
}

void PinnedTabCollection::MoveTab(TabModel* tab_model, size_t dst_index) {
  CHECK(dst_index < ChildCount() && dst_index >= 0);
  impl_->MoveTab(tab_model, dst_index);
}

void PinnedTabCollection::CloseTab(TabModel* tab_model) {
  CHECK(tab_model);
  impl_->CloseTab(tab_model);
}

tabs::TabModel* PinnedTabCollection::GetTabAtIndex(size_t index) const {
  CHECK(index < ChildCount() && index >= 0);
  return impl_->GetTabAtIndex(index);
}

bool PinnedTabCollection::ContainsTab(TabModel* tab_model) const {
  CHECK(tab_model);
  return impl_->ContainsTab(tab_model);
}

bool PinnedTabCollection::ContainsTabRecursive(TabModel* tab_model) const {
  CHECK(tab_model);
  return impl_->ContainsTab(tab_model);
}

bool PinnedTabCollection::ContainsCollection(TabCollection* collection) const {
  CHECK(collection);
  return false;
}

std::optional<size_t> PinnedTabCollection::GetIndexOfTabRecursive(
    const TabModel* tab_model) const {
  CHECK(tab_model);
  return impl_->GetIndexOfTab(tab_model);
}

std::optional<size_t> PinnedTabCollection::GetIndexOfCollection(
    TabCollection* collection) const {
  CHECK(collection);
  return std::nullopt;
}

std::unique_ptr<TabModel> PinnedTabCollection::MaybeRemoveTab(
    TabModel* tab_model) {
  CHECK(tab_model);

  std::unique_ptr<TabModel> removed_tab_model = impl_->RemoveTab(tab_model);
  removed_tab_model->set_pinned(/*pinned=*/false);
  removed_tab_model->OnReparented(nullptr, GetPassKey());
  return removed_tab_model;
}

size_t PinnedTabCollection::ChildCount() const {
  return impl_->GetChildrenCount();
}

std::unique_ptr<TabCollection> PinnedTabCollection::MaybeRemoveCollection(
    TabCollection* collection) {
  CHECK(collection);
  return nullptr;
}

}  // namespace tabs
