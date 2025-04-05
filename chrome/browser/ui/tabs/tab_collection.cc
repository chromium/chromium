// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_collection.h"

#include "base/check.h"
#include "base/notreached.h"
#include "chrome/browser/ui/tabs/tab_model.h"

namespace tabs {

TabCollection::TabCollection(
    Type type,
    std::unordered_set<Type> supported_child_collections,
    bool supports_tabs)
    : type_(type),
      supported_child_collections_(supported_child_collections),
      supports_tabs_{supports_tabs},
      impl_(std::make_unique<TabCollectionStorage>(*this)) {}

TabCollection::~TabCollection() = default;

bool TabCollection::ContainsCollection(TabCollection* collection) const {
  CHECK(collection);
  return impl_->ContainsCollection(collection);
}

std::optional<size_t> TabCollection::GetIndexOfTab(
    const TabInterface* tab) const {
  CHECK(tab);
  return impl_->GetIndexOfTab(tab);
}

std::optional<size_t> TabCollection::GetIndexOfTabRecursive(
    const TabInterface* tab) const {
  CHECK(tab);
  size_t current_index = 0;

  // If the child is a `tab_model` check if it is the the desired tab, otherwise
  // increase the current_index by 1.
  // Otherwise the child is a collection. If the tab is present in the
  // collection, use the relative index and the `current_index` and return the
  // result. Otherwise, update the `current_index` by the number of tabs in the
  // collection.
  for (const auto& child : impl_->GetChildren()) {
    if (std::holds_alternative<std::unique_ptr<TabModel>>(child)) {
      if (std::get<std::unique_ptr<TabModel>>(child).get() == tab) {
        return current_index;
      }
      current_index++;
    } else if (std::holds_alternative<std::unique_ptr<TabCollection>>(child)) {
      const TabCollection* const collection =
          std::get<std::unique_ptr<TabCollection>>(child).get();

      if (std::optional<size_t> index_within_collection =
              collection->GetIndexOfTabRecursive(tab);
          index_within_collection.has_value()) {
        return current_index + index_within_collection.value();
      } else {
        current_index += collection->TabCountRecursive();
      }
    }
  }

  return std::nullopt;
}

TabModel* TabCollection::GetTabAtIndexRecursive(size_t index) const {
  size_t curr_index = 0;

  for (auto& child : impl_->GetChildren()) {
    if (std::holds_alternative<std::unique_ptr<TabModel>>(child)) {
      if (curr_index == index) {
        return std::get<std::unique_ptr<TabModel>>(child).get();
      } else {
        curr_index++;
      }
    } else if (std::holds_alternative<std::unique_ptr<TabCollection>>(child)) {
      TabCollection* collection =
          std::get<std::unique_ptr<TabCollection>>(child).get();
      size_t num_of_tabs_in_sub_collection = collection->TabCountRecursive();

      if (index < curr_index + num_of_tabs_in_sub_collection) {
        return collection->GetTabAtIndexRecursive(index - curr_index);
      } else {
        curr_index += num_of_tabs_in_sub_collection;
      }
    }
  }
  NOTREACHED();
}

std::list<TabModel*> TabCollection::GetTabsRecursive() const {
  const auto& children = impl_->GetChildren();
  std::list<TabModel*> tab_models;

  for (const auto& child : children) {
    if (std::holds_alternative<std::unique_ptr<TabModel>>(child)) {
      TabModel* tab = std::get<std::unique_ptr<TabModel>>(child).get();
      tab_models.push_back(tab);
    } else {
      std::list<TabModel*> tabs_to_insert =
          std::get<std::unique_ptr<TabCollection>>(child)->GetTabsRecursive();
      tab_models.splice(tab_models.end(), tabs_to_insert);
    }
  }

  return tab_models;
}

std::optional<size_t> TabCollection::GetIndexOfCollection(
    TabCollection* collection) const {
  CHECK(collection);
  return impl_->GetIndexOfCollection(collection);
}

size_t TabCollection::ToDirectIndex(size_t index) {
  CHECK(index <= TabCountRecursive());

  size_t curr_index = 0;
  size_t direct_child_index = 0;
  for (const auto& child : impl_->GetChildren()) {
    CHECK(curr_index <= index);
    if (curr_index == index) {
      return direct_child_index;
    }
    if (std::holds_alternative<std::unique_ptr<tabs::TabModel>>(child)) {
      curr_index++;
    } else if (std::holds_alternative<std::unique_ptr<tabs::TabCollection>>(
                   child)) {
      curr_index += std::get<std::unique_ptr<tabs::TabCollection>>(child)
                        ->TabCountRecursive();
    }
    direct_child_index++;
  }

  CHECK(curr_index == index);
  CHECK(direct_child_index == ChildCount());
  return direct_child_index;
}

size_t TabCollection::ChildCount() const {
  return impl_->GetChildrenCount();
}

void TabCollection::OnCollectionAddedToTree(TabCollection* collection) {
  recursive_tab_count_ += collection->TabCountRecursive();

  if (parent_) {
    parent_->OnCollectionAddedToTree(collection);
  }
}

void TabCollection::OnCollectionRemovedFromTree(TabCollection* collection) {
  recursive_tab_count_ -= collection->TabCountRecursive();

  if (parent_) {
    parent_->OnCollectionRemovedFromTree(collection);
  }
}

void TabCollection::OnTabAddedToTree() {
  recursive_tab_count_++;

  if (parent_) {
    parent_->OnTabAddedToTree();
  }
}

void TabCollection::OnTabRemovedFromTree() {
  recursive_tab_count_--;

  if (parent_) {
    parent_->OnTabRemovedFromTree();
  }
}

TabModel* TabCollection::AddTab(std::unique_ptr<TabModel> tab_model,
                                size_t index) {
  CHECK(tab_model);
  CHECK(supports_tabs_);
  CHECK(index <= ChildCount());

  TabModel* inserted_tab_model = impl_->AddTab(std::move(tab_model), index);
  inserted_tab_model->OnReparented(this, GetPassKey());
  return inserted_tab_model;
}

std::unique_ptr<TabModel> TabCollection::MaybeRemoveTab(TabModel* tab_model) {
  CHECK(tab_model);

  std::unique_ptr<TabModel> removed_tab_model = impl_->RemoveTab(tab_model);
  removed_tab_model->OnReparented(nullptr, GetPassKey());
  return removed_tab_model;
}

std::unique_ptr<TabCollection> TabCollection::MaybeRemoveCollection(
    TabCollection* collection) {
  CHECK(collection);

  std::unique_ptr<TabCollection> removed_tab_collection =
      impl_->RemoveCollection(collection);
  removed_tab_collection->OnReparented(nullptr);
  return removed_tab_collection;
}

void TabCollection::OnReparented(TabCollection* new_parent) {
  parent_ = new_parent;

  for (auto tab : GetTabsRecursive()) {
    tab->OnAncestorChanged(GetPassKey());
  }
}

}  // namespace tabs
