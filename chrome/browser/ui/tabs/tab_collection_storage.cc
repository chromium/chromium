// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/notimplemented.h"
#include "chrome/browser/ui/tabs/tab_collection.h"
#include "chrome/browser/ui/tabs/tab_collection_storage.h"
#include "chrome/browser/ui/tabs/tab_model.h"

namespace tabs {

TabCollectionStorage::TabCollectionStorage(TabCollection& owner)
    : owning_collection_(owner) {}

TabCollectionStorage::~TabCollectionStorage() = default;

bool TabCollectionStorage::ContainsTab(TabModel* tab_model) const {
  return GetIndexOfTab(tab_model).has_value();
}

TabModel* TabCollectionStorage::AddTab(std::unique_ptr<TabModel> tab_model,
                                       size_t index) {
  CHECK(index <= GetChildrenCount());
  CHECK(tab_model);

  TabModel* tab_model_ptr = tab_model.get();
  children_.insert(children_.begin() + index, std::move(tab_model));
  return tab_model_ptr;
}

void TabCollectionStorage::MoveTab(TabModel* tab_model, size_t dst_index) {
  CHECK(tab_model);
  std::unique_ptr<TabModel> tab_model_to_move = RemoveTab(tab_model);
  CHECK(tab_model_to_move);
  AddTab(std::move(tab_model_to_move), dst_index);
}

std::unique_ptr<TabModel> TabCollectionStorage::RemoveTab(TabModel* tab_model) {
  CHECK(tab_model);
  for (size_t i = 0; i < children_.size(); ++i) {
    if (std::holds_alternative<std::unique_ptr<TabModel>>(children_[i])) {
      auto& stored_tab_model =
          std::get<std::unique_ptr<TabModel>>(children_[i]);
      if (stored_tab_model.get() == tab_model) {
        auto removed_tab_model = std::move(stored_tab_model);
        children_.erase(children_.begin() + i);
        return removed_tab_model;
      }
    }
  }
  NOTREACHED_NORETURN();
}

void TabCollectionStorage::CloseTab(TabModel* tab) {
  std::unique_ptr<TabModel> removed_tab_model = RemoveTab(tab);
  removed_tab_model.reset();
}

TabCollection* TabCollectionStorage::AddCollection(
    std::unique_ptr<TabCollection> collection,
    size_t index) {
  NOTIMPLEMENTED();
  return nullptr;
}

void TabCollectionStorage::MoveCollection(TabCollection* collection,
                                          size_t dst_index) {
  NOTIMPLEMENTED();
}

std::unique_ptr<TabCollection> TabCollectionStorage::RemoveCollection(
    TabCollection* collection) {
  NOTIMPLEMENTED();
  return nullptr;
}

void TabCollectionStorage::CloseCollection(TabCollection* collection) {
  // This should free all the children as well.
  NOTIMPLEMENTED();
}

std::optional<size_t> TabCollectionStorage::GetIndexOfTab(
    TabModel* tab_model) const {
  const auto it = std::find_if(
      children_.begin(), children_.end(), [tab_model](const auto& child) {
        return std::holds_alternative<std::unique_ptr<TabModel>>(child) &&
               std::get<std::unique_ptr<TabModel>>(child).get() == tab_model;
      });
  return it == children_.end() ? std::nullopt
                               : std::optional<size_t>(it - children_.begin());
}

size_t TabCollectionStorage::GetChildrenCount() const {
  return children_.size();
}
}  // namespace tabs
