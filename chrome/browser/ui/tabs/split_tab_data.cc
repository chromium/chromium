// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_data.h"

#include <memory>

#include "chrome/browser/ui/tabs/split_tab_collection.h"
#include "chrome/browser/ui/tabs/split_tab_visual_data.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "components/tabs/public/tab_interface.h"

namespace split_tabs {

SplitTabData::SplitTabData(tabs::SplitTabCollection* controller,
                           const split_tabs::SplitTabId& id,
                           const SplitTabVisualData& visual_data)
    : controller_(controller), visual_data_(visual_data), id_(id) {}

SplitTabData::~SplitTabData() = default;

std::vector<tabs::TabModel*> SplitTabData::ListTabs() const {
  return controller_->GetTabsRecursive();
}

SplitTabActiveLocation SplitTabData::GetActiveTabLocation() {
  std::vector<tabs::TabModel*> tabs_in_split = ListTabs();
  CHECK_EQ(tabs_in_split.size(), 2U);

  const bool first_tab_activated = tabs_in_split[0]->IsActivated();
  const bool second_tab_activated = tabs_in_split[1]->IsActivated();

  if (!first_tab_activated && !second_tab_activated) {
    return SplitTabActiveLocation::kNone;
  }

  if (visual_data_.split_layout() == SplitTabLayout::kHorizontal) {
    return first_tab_activated ? SplitTabActiveLocation::kLeft
                               : SplitTabActiveLocation::kRight;
  } else {
    return first_tab_activated ? SplitTabActiveLocation::kTop
                               : SplitTabActiveLocation::kBottom;
  }
}

}  // namespace split_tabs
