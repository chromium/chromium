// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/unpinned_tab_collection.h"

#include <cstddef>
#include <memory>
#include <optional>

#include "base/not_fatal_until.h"
#include "base/notreached.h"
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
  CHECK(direct_child_index <= ChildCount() && direct_child_index >= 0);
  CHECK(tab_model);

  TabModel* inserted_tab_model =
      impl_->AddTab(std::move(tab_model), direct_child_index);
  inserted_tab_model->OnReparented(this, GetPassKey());
}

void UnpinnedTabCollection::AddTabRecursive(
    std::unique_ptr<TabModel> tab_model,
    size_t dst_index,
    std::optional<tab_groups::TabGroupId> new_group_id) {
  size_t curr_index = 0;
  size_t direct_child_index = 0;

  // `index` can be equal to the tab count as at this point the tab has not yet
  // been added.
  CHECK(dst_index >= 0 && dst_index <= TabCountRecursive());

  for (const auto& child : impl_->GetChildren()) {
    CHECK(curr_index <= dst_index);
    if (curr_index == dst_index && !new_group_id.has_value()) {
      return AddTab(std::move(tab_model), direct_child_index);
    }
    if (std::holds_alternative<std::unique_ptr<tabs::TabModel>>(child)) {
      curr_index++;
    } else if (std::holds_alternative<std::unique_ptr<tabs::TabCollection>>(
                   child)) {
      TabGroupTabCollection* group_collection =
          static_cast<TabGroupTabCollection*>(
              std::get<std::unique_ptr<tabs::TabCollection>>(child).get());
      const size_t num_of_tabs_in_sub_collection =
          group_collection->TabCountRecursive();

      // Check if the tab should be added to the subcollection.
      if (new_group_id.has_value() &&
          new_group_id.value() == group_collection->GetTabGroupId() &&
          dst_index <= curr_index + num_of_tabs_in_sub_collection) {
        return group_collection->AddTab(std::move(tab_model),
                                        dst_index - curr_index);
      } else {
        curr_index += num_of_tabs_in_sub_collection;
      }
    }
    direct_child_index++;
  }

  // Case when we want to add a tab to the end of this collection as a direct
  // child.
  CHECK(curr_index == dst_index && !new_group_id.has_value());
  AddTab(std::move(tab_model), direct_child_index);
}

tabs::TabModel* UnpinnedTabCollection::GetTabAtIndexRecursive(size_t index) {
  CHECK(index >= 0);

  size_t curr_index = 0;
  size_t direct_child_index = 0;

  for (auto& child : impl_->GetChildren()) {
    if (std::holds_alternative<std::unique_ptr<tabs::TabModel>>(child)) {
      if (curr_index == index) {
        return impl_->GetTabAtIndex(direct_child_index);
      } else {
        curr_index++;
      }
    } else if (std::holds_alternative<std::unique_ptr<tabs::TabCollection>>(
                   child)) {
      TabGroupTabCollection* group_collection =
          static_cast<TabGroupTabCollection*>(
              std::get<std::unique_ptr<tabs::TabCollection>>(child).get());
      size_t num_of_tabs_in_sub_collection =
          group_collection->TabCountRecursive();

      if (index < curr_index + num_of_tabs_in_sub_collection) {
        return group_collection->GetTabAtIndex(index - curr_index);
      } else {
        curr_index += num_of_tabs_in_sub_collection;
      }
    }
    direct_child_index++;
  }
  NOTREACHED();
}

void UnpinnedTabCollection::AppendTab(std::unique_ptr<TabModel> tab_model) {
  CHECK(tab_model);
  AddTab(std::move(tab_model), ChildCount());
}

void UnpinnedTabCollection::MoveTab(TabModel* tab_model,
                                    size_t direct_child_dst_index) {
  CHECK(direct_child_dst_index < ChildCount() && direct_child_dst_index >= 0);
  impl_->MoveTab(tab_model, direct_child_dst_index);
}

void UnpinnedTabCollection::CloseTab(TabModel* tab_model) {
  CHECK(tab_model);
  impl_->CloseTab(tab_model);
}

std::optional<size_t>
UnpinnedTabCollection::GetDirectChildIndexOfCollectionContainingTab(
    const TabModel* tab_model) const {
  CHECK(tab_model);
  if (tab_model->GetParentCollection(GetPassKey()) == this) {
    return GetIndexOfTab(tab_model).value();
  } else {
    TabCollection* parent_collection =
        tab_model->GetParentCollection(GetPassKey());
    while (parent_collection && !ContainsCollection(parent_collection)) {
      parent_collection = parent_collection->GetParentCollection();
    }

    return GetIndexOfCollection(parent_collection);
  }
}

bool UnpinnedTabCollection::ContainsTab(TabModel* tab_model) const {
  CHECK(tab_model);
  return impl_->ContainsTab(tab_model);
}

bool UnpinnedTabCollection::ContainsTabRecursive(TabModel* tab_model) const {
  CHECK(tab_model);
  return GetIndexOfTabRecursive(tab_model).has_value();
}

bool UnpinnedTabCollection::ContainsCollection(
    TabCollection* collection) const {
  CHECK(collection);
  return impl_->ContainsCollection(collection);
}

std::optional<size_t> UnpinnedTabCollection::GetIndexOfTabRecursive(
    const TabModel* tab_model) const {
  CHECK(tab_model);
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
  CHECK(collection);
  return impl_->GetIndexOfCollection(collection);
}

std::unique_ptr<TabModel> UnpinnedTabCollection::MaybeRemoveTab(
    TabModel* tab_model) {
  CHECK(tab_model);

  std::unique_ptr<TabModel> removed_tab_model = impl_->RemoveTab(tab_model);
  removed_tab_model->OnReparented(nullptr, GetPassKey());
  return removed_tab_model;
}

size_t UnpinnedTabCollection::ChildCount() const {
  return impl_->GetChildrenCount();
}

std::unique_ptr<TabCollection> UnpinnedTabCollection::MaybeRemoveCollection(
    TabCollection* collection) {
  CHECK(collection);

  std::unique_ptr<TabCollection> removed_tab_collection =
      impl_->RemoveCollection(collection);
  removed_tab_collection->OnReparented(nullptr);
  return removed_tab_collection;
}

TabGroupTabCollection* UnpinnedTabCollection::AddTabGroup(
    std::unique_ptr<TabGroupTabCollection> group,
    size_t index) {
  CHECK(group);
  CHECK(index <= ChildCount() && index >= 0);

  TabCollection* added_collection =
      impl_->AddCollection(std::move(group), index);
  added_collection->OnReparented(this);
  return static_cast<TabGroupTabCollection*>(added_collection);
}

void UnpinnedTabCollection::MoveTabGroup(TabGroupTabCollection* group,
                                         size_t direct_child_dst_index) {
  CHECK(group);
  CHECK(direct_child_dst_index < ChildCount() && direct_child_dst_index >= 0);

  impl_->MoveCollection(group, direct_child_dst_index);
}

void UnpinnedTabCollection::CloseTabGroup(TabGroupTabCollection* group) {
  CHECK(group);
  impl_->CloseCollection(group);
}

std::unique_ptr<TabGroupTabCollection> UnpinnedTabCollection::RemoveGroup(
    TabGroupTabCollection* group) {
  CHECK(group);

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

std::optional<size_t> UnpinnedTabCollection::GetIndexOfTab(
    const TabModel* tab_model) const {
  CHECK(tab_model);
  return impl_->GetIndexOfTab(tab_model);
}

void UnpinnedTabCollection::MoveGroupToRecursive(
    int index,
    TabGroupTabCollection* collection) {
  CHECK(collection);
  CHECK(index >= 0);

  std::unique_ptr<tabs::TabGroupTabCollection> removed_collection =
      RemoveGroup(collection);
  if (index == static_cast<int>(TabCountRecursive())) {
    AddTabGroup(std::move(removed_collection), ChildCount());
  } else {
    const tabs::TabModel* const tab_at_destination =
        GetTabAtIndexRecursive(index);
    const size_t index_to_move =
        GetDirectChildIndexOfCollectionContainingTab(tab_at_destination)
            .value();
    AddTabGroup(std::move(removed_collection), index_to_move);
  }
}

void UnpinnedTabCollection::ValidateCollections() const {
#if DCHECK_IS_ON()
  for (const auto& child : impl_->GetChildren()) {
    if (std::holds_alternative<std::unique_ptr<tabs::TabCollection>>(child)) {
      TabGroupTabCollection* group_collection =
          static_cast<TabGroupTabCollection*>(
              std::get<std::unique_ptr<tabs::TabCollection>>(child).get());
      CHECK(group_collection->TabCountRecursive() > 0,
            base::NotFatalUntil::M130);
    }
  }
#endif
}

}  // namespace tabs
