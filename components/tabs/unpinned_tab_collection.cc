// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/unpinned_tab_collection.h"

#include <memory>
#include <optional>

#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

UnpinnedTabCollection::UnpinnedTabCollection()
    : TabCollection(TabCollection::Type::UNPINNED,
                    {TabCollection::Type::GROUP, TabCollection::Type::SPLIT},
                    /*supports_tabs=*/true) {}

UnpinnedTabCollection::~UnpinnedTabCollection() = default;

std::optional<size_t>
UnpinnedTabCollection::GetDirectChildIndexOfCollectionContainingTab(
    const TabInterface* tab) const {
  CHECK(tab);
  if (tab->GetParentCollection(GetPassKey()) == this) {
    return GetIndexOfTab(tab).value();
  } else {
    TabCollection* parent_collection = tab->GetParentCollection(GetPassKey());
    while (parent_collection && !ContainsCollection(parent_collection)) {
      parent_collection = parent_collection->GetParentCollection();
    }

    return GetIndexOfCollection(parent_collection);
  }
}

void UnpinnedTabCollection::MoveGroupToRecursive(
    int index,
    TabGroupTabCollection* collection) {
  CHECK(collection);
  CHECK(index >= 0);

  std::unique_ptr<tabs::TabCollection> removed_collection =
      MaybeRemoveCollection(collection);
  if (index == static_cast<int>(TabCountRecursive())) {
    AddCollection(std::move(removed_collection), ChildCount());
  } else {
    const tabs::TabInterface* const tab_at_destination =
        GetTabAtIndexRecursive(index);
    const size_t index_to_move =
        GetDirectChildIndexOfCollectionContainingTab(tab_at_destination)
            .value();
    AddCollection(std::move(removed_collection), index_to_move);
  }
}

}  // namespace tabs
