// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_SPLIT_TAB_DATA_H_
#define COMPONENTS_TABS_PUBLIC_SPLIT_TAB_DATA_H_

#include <vector>

#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_id.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/gfx/range/range.h"

namespace split_tabs {

// Contains metadata for a split tab collection such as the id and split layout
// orientation. Also provides a way to access a the list of tabs in the split.
class SplitTabData {
 public:
  SplitTabData(tabs::SplitTabCollection* collection,
               const split_tabs::SplitTabId& id,
               const SplitTabVisualData& visual_data);
  ~SplitTabData();

  const split_tabs::SplitTabId& id() const { return id_; }

  SplitTabVisualData* visual_data() { return &visual_data_; }

  std::vector<tabs::TabInterface*> ListTabs() const;

  // Returns [start, end) where the leftmost tab in the split has index start
  // and the rightmost tab in the split has index end - 1.
  gfx::Range GetIndexRange() const;

 private:
  raw_ptr<tabs::SplitTabCollection> collection_;
  SplitTabVisualData visual_data_;
  split_tabs::SplitTabId id_;
};

}  // namespace split_tabs

#endif  // COMPONENTS_TABS_PUBLIC_SPLIT_TAB_DATA_H_
