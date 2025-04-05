// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_TAB_COLLECTION_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_TAB_COLLECTION_H_

#include "chrome/browser/ui/tabs/split_tab_id.h"
#include "chrome/browser/ui/tabs/tab_collection.h"

namespace tabs {

enum class SplitTabLayout { kHorizontal, kVertical };

// A collection for split tabs.
class SplitTabCollection : public TabCollection {
 public:
  explicit SplitTabCollection(split_tabs::SplitTabId split_id);
  ~SplitTabCollection() override;
  SplitTabCollection(const SplitTabCollection&) = delete;
  SplitTabCollection& operator=(const SplitTabCollection&) = delete;

  // Returns the `split_id_` this collection is associated with.
  split_tabs::SplitTabId GetSplitTabId() const { return split_id_; }

 private:
  // The split identifier of this collection.
  const split_tabs::SplitTabId split_id_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_TAB_COLLECTION_H_
