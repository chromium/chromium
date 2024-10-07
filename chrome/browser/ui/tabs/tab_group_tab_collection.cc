// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "chrome/browser/ui/tabs/tab_collection_storage.h"
#include "chrome/browser/ui/tabs/tab_group_tab_collection.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "components/tab_groups/tab_group_id.h"

namespace tabs {

TabGroupTabCollection::TabGroupTabCollection(tab_groups::TabGroupId group_id)
    : group_id_(group_id),
      impl_(std::make_unique<TabCollectionStorage>(*this)) {}

TabGroupTabCollection::~TabGroupTabCollection() = default;

void TabGroupTabCollection::AddTab(std::unique_ptr<TabModel> tab_model,
                                   size_t index) {
  CHECK(index <= ChildCount() && index >= 0);
  CHECK(tab_model);

  TabModel* inserted_tab_model = impl_->AddTab(std::move(tab_model), index);
  inserted_tab_model->set_group(/*group=*/group_id_);
  inserted_tab_model->OnReparented(this, GetPassKey());
}

void TabGroupTabCollection::AppendTab(std::unique_ptr<TabModel> tab_model) {
  CHECK(tab_model);
  AddTab(std::move(tab_model), ChildCount());
}

void TabGroupTabCollection::MoveTab(TabModel* tab_model, size_t dst_index) {
  CHECK(dst_index < ChildCount() && dst_index >= 0);
  impl_->MoveTab(tab_model, dst_index);
}

void TabGroupTabCollection::CloseTab(TabModel* tab_model) {
  CHECK(tab_model);
  impl_->CloseTab(tab_model);
}

tabs::TabModel* TabGroupTabCollection::GetTabAtIndex(size_t index) const {
  CHECK(index < ChildCount() && index >= 0);
  return impl_->GetTabAtIndex(index);
}

bool TabGroupTabCollection::ContainsTab(TabModel* tab_model) const {
  CHECK(tab_model);
  return impl_->ContainsTab(tab_model);
}

bool TabGroupTabCollection::ContainsTabRecursive(TabModel* tab_model) const {
  CHECK(tab_model);
  return impl_->ContainsTab(tab_model);
}

bool TabGroupTabCollection::ContainsCollection(
    TabCollection* collection) const {
  CHECK(collection);
  return false;
}

std::optional<size_t> TabGroupTabCollection::GetIndexOfTabRecursive(
    const TabModel* tab_model) const {
  CHECK(tab_model);
  return impl_->GetIndexOfTab(tab_model);
}

std::optional<size_t> TabGroupTabCollection::GetIndexOfCollection(
    TabCollection* collection) const {
  CHECK(collection);
  return std::nullopt;
}

std::unique_ptr<TabModel> TabGroupTabCollection::MaybeRemoveTab(
    TabModel* tab_model) {
  CHECK(tab_model);

  std::unique_ptr<TabModel> removed_tab_model = impl_->RemoveTab(tab_model);
  removed_tab_model->set_group(/*group=*/std::nullopt);
  removed_tab_model->OnReparented(nullptr, GetPassKey());
  return removed_tab_model;
}

size_t TabGroupTabCollection::ChildCount() const {
  return impl_->GetChildrenCount();
}

std::unique_ptr<TabCollection> TabGroupTabCollection::MaybeRemoveCollection(
    TabCollection* collection) {
  return nullptr;
}

}  // namespace tabs
