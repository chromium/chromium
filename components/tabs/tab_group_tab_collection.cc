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
    tab_groups::TabGroupId group_id,
    tab_groups::TabGroupVisualData visual_data)
    : TabCollection(TabCollection::Type::GROUP,
                    {TabCollection::Type::SPLIT},
                    /*supports_tabs=*/true) {
  group_ = std::make_unique<TabGroup>(this, group_id, visual_data);
}

TabGroupTabCollection::~TabGroupTabCollection() = default;

const tab_groups::TabGroupId& TabGroupTabCollection::GetTabGroupId() const {
  return group_->id();
}

}  // namespace tabs
