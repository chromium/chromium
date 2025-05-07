// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_SPLIT_TAB_COLLECTION_H_
#define COMPONENTS_TABS_PUBLIC_SPLIT_TAB_COLLECTION_H_

#include "components/tabs/public/split_tab_id.h"
#include "components/tabs/public/tab_collection.h"

namespace split_tabs {
class SplitTabData;
class SplitTabVisualData;
}  // namespace split_tabs

namespace tabs {

// A collection for split tabs.
class SplitTabCollection : public TabCollection {
 public:
  explicit SplitTabCollection(split_tabs::SplitTabId split_id,
                              split_tabs::SplitTabVisualData visual_data);
  ~SplitTabCollection() override;
  SplitTabCollection(const SplitTabCollection&) = delete;
  SplitTabCollection& operator=(const SplitTabCollection&) = delete;

  split_tabs::SplitTabData* data() const { return split_tab_data_.get(); }

  // Returns the SplitTabId this collection is associated with.
  split_tabs::SplitTabId GetSplitTabId() const;

 private:
  std::unique_ptr<split_tabs::SplitTabData> split_tab_data_;
};

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_SPLIT_TAB_COLLECTION_H_
