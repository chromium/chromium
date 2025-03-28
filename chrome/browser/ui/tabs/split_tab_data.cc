// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_data.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tab_collections/public/tab_interface.h"

namespace split_tabs {

SplitTabData::SplitTabData(TabStripModel* controller,
                           const split_tabs::SplitTabId& id,
                           tabs::SplitTabLayout split_layout)
    : controller_(controller), split_layout_(split_layout), id_(id) {}

SplitTabData::~SplitTabData() = default;

std::vector<tabs::TabInterface*> SplitTabData::ListTabs() const {
  std::vector<tabs::TabInterface*> tabs;
  for (int i = 0; i < controller_->GetTabCount(); i++) {
    tabs::TabInterface* tab = controller_->GetTabAtIndex(i);
    if (tab->GetSplit() == id_) {
      tabs.emplace_back(tab);
    }
  }
  return tabs;
}

}  // namespace split_tabs
