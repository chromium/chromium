// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/split_tab_data.h"

#include <memory>

#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace split_tabs {

SplitTabData::SplitTabData(tabs::SplitTabCollection* collection,
                           const split_tabs::SplitTabId& id,
                           const SplitTabVisualData& visual_data)
    : collection_(collection), visual_data_(visual_data), id_(id) {}

SplitTabData::~SplitTabData() = default;

std::vector<tabs::TabInterface*> SplitTabData::ListTabs() const {
  return collection_->GetTabsRecursive();
}

gfx::Range SplitTabData::GetIndexRange() const {
  std::vector<tabs::TabInterface*> split_tabs = ListTabs();

  if (split_tabs.empty()) {
    return gfx::Range();
  }

  // Find the TabStripCollection (for which indexes are based off of.)
  tabs::TabCollection* root_collection = collection_;
  while (root_collection &&
         root_collection->type() != tabs::TabCollection::Type::TABSTRIP) {
    root_collection = root_collection->GetParentCollection();
  }

  if (!root_collection) {
    return gfx::Range();
  }

  int start_index =
      root_collection->GetIndexOfTabRecursive(split_tabs[0]).value();
  return gfx::Range(start_index, start_index + split_tabs.size());
}

}  // namespace split_tabs
