// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/tab_group.h"

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace {
// TODO(crbug.com/418837851) we should be able to ignore this case in the future
// once detach cases are properly checked for use of TabStripModel indices. Find
// the TabStripCollection the group exists in. Iterate through collection
// parents looking for a tabstrip. if none is found, then true indexes cant be
// derived.
bool IsInValidCollectionTree(tabs::TabCollection* collection) {
  while (collection &&
         collection->type() != tabs::TabCollection::Type::TABSTRIP) {
    collection = collection->GetParentCollection();
  }
  return collection != nullptr;
}

}  // anonymous namespace

TabGroup::TabGroup(tabs::TabGroupTabCollection* collection,
                   const tab_groups::TabGroupId& id,
                   const tab_groups::TabGroupVisualData& visual_data)
    : collection_(collection),
      id_(id),
      visual_data_(
          std::make_unique<tab_groups::TabGroupVisualData>(visual_data)) {}

TabGroup::~TabGroup() = default;

void TabGroup::SetVisualData(tab_groups::TabGroupVisualData visual_data,
                             bool is_customized) {
  visual_data_ = std::make_unique<tab_groups::TabGroupVisualData>(visual_data);

  // Is customized is always true after it has been set to true once.
  is_customized_ |= is_customized;
}

void TabGroup::SetGroupIsClosing(bool is_closing) {
  is_closing_ = is_closing;
}

void TabGroup::AddTab() {
  ++tab_count_;
}

void TabGroup::RemoveTab() {
  DCHECK_GT(tab_count_, 0);
  --tab_count_;
}

bool TabGroup::IsEmpty() const {
  return tab_count_ == 0;
}

bool TabGroup::IsCustomized() const {
  return is_customized_;
}

tabs::TabInterface* TabGroup::GetFirstTab() const {
  if (collection_->begin() == collection_->end()) {
    return nullptr;
  }

  if (!IsInValidCollectionTree(collection_)) {
    return nullptr;
  }

  return collection_->GetTabAtIndexRecursive(0);
}

tabs::TabInterface* TabGroup::GetLastTab() const {
  if (collection_->begin() == collection_->end()) {
    return nullptr;
  }

  if (!IsInValidCollectionTree(collection_)) {
    return nullptr;
  }

  std::vector<tabs::TabInterface*> tabs = collection_->GetTabsRecursive();
  return tabs.at(tabs.size() - 1);
}

gfx::Range TabGroup::ListTabs() const {
  tabs::TabInterface* maybe_first_tab = GetFirstTab();
  if (!maybe_first_tab) {
    return gfx::Range();
  }

  // If there's a first tab, there's a last tab.
  tabs::TabInterface* last_tab = GetLastTab();

  // Find the TabStripCollection (for which indexes are based off of.)
  tabs::TabCollection* root_collection = collection_;
  while (root_collection &&
         root_collection->type() != tabs::TabCollection::Type::TABSTRIP) {
    root_collection = root_collection->GetParentCollection();
  }
  if (!root_collection) {
    return gfx::Range();
  }

  return gfx::Range(
      root_collection->GetIndexOfTabRecursive(maybe_first_tab).value(),
      root_collection->GetIndexOfTabRecursive(last_tab).value() + 1);
}
