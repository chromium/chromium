// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_collection.h"

#include <memory>
#include <optional>

#include "chrome/browser/ui/tabs/tab_collection_storage.h"
#include "chrome/browser/ui/tabs/tab_model.h"

namespace tabs {

SplitTabCollection::SplitTabCollection(split_tabs::SplitTabId split_id)
    : TabCollection(TabCollection::Type::SPLIT),
      split_id_(split_id),
      impl_(std::make_unique<TabCollectionStorage>(*this)) {}

SplitTabCollection::~SplitTabCollection() = default;

void SplitTabCollection::AddTab(std::unique_ptr<TabModel> tab_model,
                                size_t index) {
  CHECK(index <= ChildCount() && index >= 0);
  CHECK(tab_model);

  TabModel* inserted_tab_model = impl_->AddTab(std::move(tab_model), index);
  inserted_tab_model->set_split(split_id_);
  inserted_tab_model->OnReparented(this, GetPassKey());
}

void SplitTabCollection::AppendTab(std::unique_ptr<TabModel> tab_model) {
  CHECK(tab_model);
  AddTab(std::move(tab_model), ChildCount());
}

void SplitTabCollection::MoveTab(TabModel* tab_model, size_t dst_index) {
  CHECK(dst_index < ChildCount() && dst_index >= 0);
  impl_->MoveTab(tab_model, dst_index);
}

void SplitTabCollection::CloseTab(TabModel* tab_model) {
  CHECK(tab_model);
  impl_->CloseTab(tab_model);
}

tabs::TabModel* SplitTabCollection::GetTabAtIndex(size_t index) const {
  CHECK(index < ChildCount() && index >= 0);
  return impl_->GetTabAtIndex(index);
}

bool SplitTabCollection::ContainsTab(const TabInterface* tab) const {
  CHECK(tab);
  return impl_->ContainsTab(tab);
}

bool SplitTabCollection::ContainsTabRecursive(const TabInterface* tab) const {
  CHECK(tab);
  return impl_->ContainsTab(tab);
}

bool SplitTabCollection::ContainsCollection(TabCollection* collection) const {
  CHECK(collection);
  return false;
}

std::optional<size_t> SplitTabCollection::GetIndexOfTabRecursive(
    const TabInterface* tab) const {
  CHECK(tab);
  return impl_->GetIndexOfTab(tab);
}

std::optional<size_t> SplitTabCollection::GetIndexOfCollection(
    TabCollection* collection) const {
  CHECK(collection);
  return std::nullopt;
}

std::unique_ptr<TabModel> SplitTabCollection::MaybeRemoveTab(
    TabModel* tab_model) {
  CHECK(tab_model);

  std::unique_ptr<TabModel> removed_tab_model = impl_->RemoveTab(tab_model);
  removed_tab_model->set_split(/*split=*/std::nullopt);
  removed_tab_model->OnReparented(nullptr, GetPassKey());
  return removed_tab_model;
}

size_t SplitTabCollection::ChildCount() const {
  return impl_->GetChildrenCount();
}

std::unique_ptr<TabCollection> SplitTabCollection::MaybeRemoveCollection(
    TabCollection* collection) {
  return nullptr;
}

}  // namespace tabs
