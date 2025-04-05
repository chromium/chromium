// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_UNPINNED_TAB_COLLECTION_H_
#define CHROME_BROWSER_UI_TABS_UNPINNED_TAB_COLLECTION_H_

#include <optional>

#include "chrome/browser/ui/tabs/tab_collection.h"

namespace tabs {

class TabModel;
class TabGroupTabCollection;

class UnpinnedTabCollection : public TabCollection {
 public:
  UnpinnedTabCollection();
  ~UnpinnedTabCollection() override;
  UnpinnedTabCollection(const UnpinnedTabCollection&) = delete;
  UnpinnedTabCollection& operator=(const UnpinnedTabCollection&) = delete;

  // Returns the direct child index of the collection containing the tab or the
  // direct child index of the tab if it is a direct child of the unpinned
  // collection.
  std::optional<size_t> GetDirectChildIndexOfCollectionContainingTab(
      const TabModel* tab_model) const;

  void MoveGroupToRecursive(int index, TabGroupTabCollection* collection);
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_UNPINNED_TAB_COLLECTION_H_
