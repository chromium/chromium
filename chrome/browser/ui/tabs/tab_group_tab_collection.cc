// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_tab_collection.h"

#include <memory>
#include <optional>

#include "chrome/browser/ui/tabs/tab_collection_storage.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "components/tab_groups/tab_group_id.h"

namespace tabs {

TabGroupTabCollection::TabGroupTabCollection(
    tab_groups::TabGroupId group_id,
    tab_groups::TabGroupVisualData visual_data,
    TabGroupController* controller)
    : TabCollection(TabCollection::Type::GROUP,
                    {TabCollection::Type::SPLIT},
                    /*supports_tabs=*/true) {
  group_ = std::make_unique<TabGroup>(controller, group_id, visual_data);
}

TabGroupTabCollection::~TabGroupTabCollection() = default;

}  // namespace tabs
