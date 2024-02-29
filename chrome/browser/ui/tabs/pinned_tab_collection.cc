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
  NOTIMPLEMENTED();
}

void PinnedTabCollection::AppendTab(std::unique_ptr<TabModel> tab_model) {
  NOTIMPLEMENTED();
}

void PinnedTabCollection::MoveTab(TabModel* tab_model, size_t index) {
  NOTIMPLEMENTED();
}

void PinnedTabCollection::CloseTab(TabModel* tab_model) {
  NOTIMPLEMENTED();
}

bool PinnedTabCollection::ContainsTabRecursive(TabModel* tab_model) const {
  NOTIMPLEMENTED();
  return false;
}

bool PinnedTabCollection::ContainsCollection(TabCollection* collection) const {
  NOTIMPLEMENTED();
  return false;
}

std::optional<size_t> PinnedTabCollection::GetIndexOfTabRecursive(
    TabModel* tab_model) const {
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::optional<size_t> PinnedTabCollection::GetIndexOfCollection(
    TabCollection* collection) const {
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::unique_ptr<TabModel> PinnedTabCollection::MaybeRemoveTab(
    TabModel* tab_model) {
  NOTIMPLEMENTED();
  return nullptr;
}

size_t PinnedTabCollection::ChildCount() const {
  NOTIMPLEMENTED();
  return 0;
}

size_t PinnedTabCollection::TabCountRecursive() const {
  NOTIMPLEMENTED();
  return 0;
}

std::unique_ptr<TabCollection> PinnedTabCollection::MaybeRemoveCollection(
    TabCollection* collection) {
  return nullptr;
}

}  // namespace tabs
