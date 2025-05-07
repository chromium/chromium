// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/split_tab_data.h"

#include <memory>

#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "components/tabs/public/tab_interface.h"

namespace split_tabs {

SplitTabData::SplitTabData(tabs::SplitTabCollection* controller,
                           const split_tabs::SplitTabId& id,
                           const SplitTabVisualData& visual_data)
    : controller_(controller), visual_data_(visual_data), id_(id) {}

SplitTabData::~SplitTabData() = default;

std::vector<tabs::TabInterface*> SplitTabData::ListTabs() const {
  return controller_->GetTabsRecursive();
}
}  // namespace split_tabs
