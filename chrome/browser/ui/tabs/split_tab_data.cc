// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_data.h"

#include "chrome/browser/ui/tabs/split_tab_collection.h"

namespace split_tabs {

SplitTabData::SplitTabData(tabs::SplitTabCollection* controller,
                           const split_tabs::SplitTabId& id,
                           tabs::SplitTabLayout split_layout)
    : controller_(controller), split_layout_(split_layout), id_(id) {}

SplitTabData::~SplitTabData() = default;

std::vector<tabs::TabModel*> SplitTabData::ListTabs() const {
  return controller_->GetTabsRecursive();
}

}  // namespace split_tabs
