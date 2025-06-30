// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/tab_group_tab_collection.h"

#include <memory>
#include <optional>

#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_collection_storage.h"
#include "components/tabs/public/tab_group.h"

namespace tabs {

TabGroupTabCollection::TabGroupTabCollection(
    TabGroup::Factory& group_factory,
    tab_groups::TabGroupId group_id,
    tab_groups::TabGroupVisualData visual_data)
    : TabCollection(TabCollection::Type::GROUP,

                    {TabCollection::Type::SPLIT},
                    /*supports_tabs=*/true),
      group_(group_factory.Create(this, group_id, visual_data)) {}

TabGroupTabCollection::~TabGroupTabCollection() = default;

const tab_groups::TabGroupId& TabGroupTabCollection::GetTabGroupId() const {
  return group_->id();
}

std::pair<std::vector<int>, std::vector<int>>
TabGroupTabCollection::SeparateTabsByVisualPosition(
    const std::vector<int>& indices) {
  std::vector<int> left_of_group;
  std::vector<int> right_of_group;
  const size_t midpoint = ChildCount() / 2;
  for (int index : indices) {
    size_t direct_index = GetDirectChildIndexOfCollectionContainingTab(
                              GetTabAtIndexRecursive(index))
                              .value();
    if (direct_index < midpoint) {
      left_of_group.push_back(index);
    } else {
      right_of_group.push_back(index);
    }
  }
  return {left_of_group, right_of_group};
}

}  // namespace tabs
