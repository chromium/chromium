// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/tab_collection_storage.h"

#include <memory>

#include "base/notimplemented.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

TabCollectionStorage::TabCollectionStorage(TabCollection& owner)
    : owning_collection_(owner) {}

TabCollectionStorage::~TabCollectionStorage() = default;

bool TabCollectionStorage::ContainsTab(const TabInterface* tab) const {
  CHECK(tab);
  return GetIndexOfTab(tab).has_value();
}

TabInterface* TabCollectionStorage::GetTabAtIndex(size_t index) const {
  CHECK(index < GetChildrenCount() && index >= 0);
  CHECK(std::holds_alternative<std::unique_ptr<tabs::TabInterface>>(
      children_[index]));
  const std::unique_ptr<tabs::TabInterface>& tab =
      std::get<std::unique_ptr<tabs::TabInterface>>(children_[index]);
  return tab.get();
}

bool TabCollectionStorage::ContainsCollection(
    TabCollection* tab_collection) const {
  CHECK(tab_collection);
  return GetIndexOfCollection(tab_collection).has_value();
}

TabInterface* TabCollectionStorage::AddTab(std::unique_ptr<TabInterface> tab,
                                           size_t index) {
  CHECK(index <= GetChildrenCount() && index >= 0);
  CHECK(tab);

  TabInterface* tab_ptr = tab.get();
  children_.insert(children_.begin() + index, std::move(tab));
  owning_collection_->OnTabAddedToTree();
  return tab_ptr;
}

void TabCollectionStorage::MoveTab(TabInterface* tab, size_t dst_index) {
  CHECK(tab);
  CHECK(dst_index < GetChildrenCount() && dst_index >= 0);
  std::unique_ptr<TabInterface> tab_to_move = RemoveTab(tab);
  CHECK(tab_to_move);
  AddTab(std::move(tab_to_move), dst_index);
}

std::unique_ptr<TabInterface> TabCollectionStorage::RemoveTab(
    TabInterface* tab) {
  CHECK(tab);
  for (size_t i = 0; i < children_.size(); ++i) {
    if (std::holds_alternative<std::unique_ptr<TabInterface>>(children_[i])) {
      std::unique_ptr<TabInterface>& stored_tab =
          std::get<std::unique_ptr<TabInterface>>(children_[i]);
      if (stored_tab.get() == tab) {
        auto removed_tab = std::move(stored_tab);
        children_.erase(children_.begin() + i);
        owning_collection_->OnTabRemovedFromTree();
        return removed_tab;
      }
    }
  }
  NOTREACHED();
}

TabCollection* TabCollectionStorage::AddCollection(
    std::unique_ptr<TabCollection> collection,
    size_t index) {
  CHECK(index <= GetChildrenCount() && index >= 0);
  CHECK(collection);

  TabCollection* collection_ptr = collection.get();
  children_.insert(children_.begin() + index, std::move(collection));
  owning_collection_->OnCollectionAddedToTree(collection_ptr);
  return collection_ptr;
}

void TabCollectionStorage::MoveCollection(TabCollection* collection,
                                          size_t dst_index) {
  CHECK(collection);
  CHECK(dst_index < GetChildrenCount() && dst_index >= 0);
  std::unique_ptr<TabCollection> tab_collection_to_move =
      RemoveCollection(collection);
  CHECK(tab_collection_to_move);
  AddCollection(std::move(tab_collection_to_move), dst_index);
}

std::unique_ptr<TabCollection> TabCollectionStorage::RemoveCollection(
    TabCollection* collection) {
  CHECK(collection);
  for (size_t i = 0; i < children_.size(); ++i) {
    if (std::holds_alternative<std::unique_ptr<TabCollection>>(children_[i])) {
      std::unique_ptr<TabCollection>& stored_tab_collection =
          std::get<std::unique_ptr<TabCollection>>(children_[i]);
      if (stored_tab_collection.get() == collection) {
        auto removed_tab_collection = std::move(stored_tab_collection);
        children_.erase(children_.begin() + i);
        owning_collection_->OnCollectionRemovedFromTree(
            removed_tab_collection.get());
        return removed_tab_collection;
      }
    }
  }
  NOTREACHED();
}

std::optional<size_t> TabCollectionStorage::GetIndexOfTab(
    const TabInterface* const tab) const {
  CHECK(tab);
  const auto it = std::find_if(
      children_.begin(), children_.end(), [tab](const auto& child) {
        return std::holds_alternative<std::unique_ptr<TabInterface>>(child) &&
               std::get<std::unique_ptr<TabInterface>>(child).get() == tab;
      });
  return it == children_.end() ? std::nullopt
                               : std::optional<size_t>(it - children_.begin());
}

std::optional<size_t> TabCollectionStorage::GetIndexOfCollection(
    TabCollection* tab_collection) const {
  CHECK(tab_collection);
  const auto it = std::find_if(
      children_.begin(), children_.end(), [tab_collection](const auto& child) {
        return std::holds_alternative<std::unique_ptr<TabCollection>>(child) &&
               std::get<std::unique_ptr<TabCollection>>(child).get() ==
                   tab_collection;
      });
  return it == children_.end() ? std::nullopt
                               : std::optional<size_t>(it - children_.begin());
}

size_t TabCollectionStorage::GetChildrenCount() const {
  return children_.size();
}
}  // namespace tabs
