// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_desktop.h"

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"

TabGroupDesktop::TabGroupDesktop(
    Profile* profile,
    tabs::TabGroupTabCollection* collection,
    const tab_groups::TabGroupId& id,
    const tab_groups::TabGroupVisualData& visual_data)
    : TabGroup(collection, id, visual_data) {
  tab_group_features_ = TabGroupFeatures::CreateTabGroupFeatures();
  tab_group_features_->Init(*this, profile);
}

TabGroupDesktop::~TabGroupDesktop() = default;

TabGroupFeatures* TabGroupDesktop::GetTabGroupFeatures() {
  return tab_group_features_.get();
}

const TabGroupFeatures* TabGroupDesktop::GetTabGroupFeatures() const {
  return tab_group_features_.get();
}

std::unique_ptr<TabGroup> TabGroupDesktop::Factory::Create(
    tabs::TabGroupTabCollection* collection,
    const tab_groups::TabGroupId& id,
    const tab_groups::TabGroupVisualData& visual_data) {
  return std::make_unique<TabGroupDesktop>(profile(), collection, id,
                                           visual_data);
}
