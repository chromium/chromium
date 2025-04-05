// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PINNED_TAB_COLLECTION_H_
#define CHROME_BROWSER_UI_TABS_PINNED_TAB_COLLECTION_H_

#include "chrome/browser/ui/tabs/tab_collection.h"

namespace tabs {

class PinnedTabCollection : public TabCollection {
 public:
  PinnedTabCollection();
  ~PinnedTabCollection() override;
  PinnedTabCollection(const PinnedTabCollection&) = delete;
  PinnedTabCollection& operator=(const PinnedTabCollection&) = delete;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_PINNED_TAB_COLLECTION_H_
