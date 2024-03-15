// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/unpinned_tab_collection.h"

#include <cstddef>
#include <memory>
#include <optional>

#include "chrome/browser/ui/tabs/tab_collection.h"
#include "chrome/browser/ui/tabs/tab_collection_storage.h"
#include "chrome/browser/ui/tabs/tab_group_tab_collection.h"
#include "chrome/browser/ui/tabs/tab_model.h"

namespace tabs {

UnpinnedTabCollection::UnpinnedTabCollection() {
  impl_ = std::make_unique<TabCollectionStorage>(*this);
}

UnpinnedTabCollection::~UnpinnedTabCollection() = default;

void UnpinnedTabCollection::AddTab(std::unique_ptr<TabModel> tab_model,
                                   size_t direct_child_index) {
  TabModel* inserted_tab_model =
      impl_->AddTab(std::move(tab_model), direct_child_index);
  inserted_tab_model->OnReparented(this, GetPassKey());
}

void UnpinnedTabCollection::AppendTab(std::unique_ptr<TabModel> tab_model) {
  AddTab(std::move(tab_model), ChildCount());
}

void UnpinnedTabCollection::MoveTab(TabModel* tab_model,
                                    size_t direct_child_dst_index) {
  impl_->MoveTab(tab_model, direct_child_dst_index);
}

void UnpinnedTabCollection::CloseTab(TabModel* tab_model) {
  impl_->CloseTab(tab_model);
}

bool UnpinnedTabCollection::ContainsTab(TabModel* tab_model) const {
  return impl_->ContainsTab(tab_model);
}

bool UnpinnedTabCollection::ContainsTabRecursive(TabModel* tab_model) const {
  return GetIndexOfTabRecursive(tab_model).has_value();
}

bool UnpinnedTabCollection::ContainsCollection(
    TabCollection* collection) const {
  return impl_->ContainsCollection(collection);
}

std::optional<size_t> UnpinnedTabCollection::GetIndexOfTabRecursive(
    TabModel* tab_model) const {
  size_t current_index = 0;

  // If the child is a `tab_model` check if it is the the desired tab, otherwise
  // increase the current_index by 1.
  // Otherwise the child is a group. If the tab is present in the group, use
  // the relative index and the `current_index` and return the result.
  // Otherwise, update the `current_index` by the number of tabs in the
  // group.
  for (const auto& child : impl_->GetChildren()) {
    if (std::holds_alternative<std::unique_ptr<tabs::TabModel>>(child)) {
      if (std::get<std::unique_ptr<tabs::TabModel>>(child).get() == tab_model) {
        return current_index;
      }
      current_index++;
    } else if (std::holds_alternative<std::unique_ptr<tabs::TabCollection>>(
                   child)) {
      const TabCollection* const group_collection =
          std::get<std::unique_ptr<tabs::TabCollection>>(child).get();

      if (std::optional<size_t> index_within_group_collection =
              group_collection->GetIndexOfTabRecursive(tab_model);
          index_within_group_collection.has_value()) {
        return current_index + index_within_group_collection.value();
      } else {
        current_index += group_collection->TabCountRecursive();
      }
    }
  }

  return std::nullopt;
}

std::optional<size_t> UnpinnedTabCollection::GetIndexOfCollection(
    TabCollection* collection) const {
  return impl_->GetIndexOfCollection(collection);
}

std::unique_ptr<TabModel> UnpinnedTabCollection::MaybeRemoveTab(
    TabModel* tab_model) {
  if (!ContainsTab(tab_model)) {
    return nullptr;
  }

  std::unique_ptr<TabModel> removed_tab_model = impl_->RemoveTab(tab_model);
  removed_tab_model->OnReparented(nullptr, GetPassKey());
  return removed_tab_model;
}

size_t UnpinnedTabCollection::ChildCount() const {
  return impl_->GetChildrenCount();
}

size_t UnpinnedTabCollection::TabCountRecursive() const {
  size_t count = 0;
  for (const auto& child : impl_->GetChildren()) {
    if (std::holds_alternative<std::unique_ptr<tabs::TabModel>>(child)) {
      count++;
    } else if (std::holds_alternative<std::unique_ptr<tabs::TabCollection>>(
                   child)) {
      const TabCollection* const group_collection =
          std::get<std::unique_ptr<tabs::TabCollection>>(child).get();
      count += group_collection->ChildCount();
    }
  }
  return count;
}

std::unique_ptr<TabCollection> UnpinnedTabCollection::MaybeRemoveCollection(
    TabCollection* collection) {
  if (!ContainsCollection(collection)) {
    return nullptr;
  }

  std::unique_ptr<TabCollection> removed_tab_collection =
      impl_->RemoveCollection(collection);
  removed_tab_collection->OnReparented(nullptr);
  return removed_tab_collection;
}

void UnpinnedTabCollection::AddTabGroup(
    std::unique_ptr<TabGroupTabCollection> group,
    size_t index) {
  TabCollection* added_collection =
      impl_->AddCollection(std::move(group), index);
  added_collection->OnReparented(this);
}

void UnpinnedTabCollection::MoveTabGroup(TabGroupTabCollection* group,
                                         size_t direct_child_dst_index) {
  impl_->MoveCollection(group, direct_child_dst_index);
}

void UnpinnedTabCollection::CloseTabGroup(TabGroupTabCollection* group) {
  impl_->CloseCollection(group);
}

std::unique_ptr<TabGroupTabCollection> UnpinnedTabCollection::RemoveGroup(
    TabGroupTabCollection* group) {
  std::unique_ptr<TabCollection> removed_group = impl_->RemoveCollection(group);
  removed_group->OnReparented(nullptr);
  return base::WrapUnique(
      static_cast<TabGroupTabCollection*>(removed_group.release()));
}

TabGroupTabCollection* UnpinnedTabCollection::GetTabGroupCollection(
    tab_groups::TabGroupId group_id_) {
  for (const auto& child : impl_->GetChildren()) {
    if (std::holds_alternative<std::unique_ptr<tabs::TabCollection>>(child)) {
      TabGroupTabCollection* group_collection =
          static_cast<TabGroupTabCollection*>(
              std::get<std::unique_ptr<tabs::TabCollection>>(child).get());

      if (group_id_ == group_collection->GetTabGroupId()) {
        return group_collection;
      }
    }
  }

  return nullptr;
}

}  // namespace tabs
