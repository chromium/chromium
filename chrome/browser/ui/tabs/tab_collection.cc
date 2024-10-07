// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_collection.h"

namespace tabs {

void TabCollection::OnCollectionAddedToTree(TabCollection* collection) {
  recursive_tab_count_ += collection->TabCountRecursive();

  if (parent_) {
    parent_->OnCollectionAddedToTree(collection);
  }
}

void TabCollection::OnCollectionRemovedFromTree(TabCollection* collection) {
  recursive_tab_count_ -= collection->TabCountRecursive();

  if (parent_) {
    parent_->OnCollectionRemovedFromTree(collection);
  }
}

void TabCollection::OnTabAddedToTree() {
  recursive_tab_count_++;

  if (parent_) {
    parent_->OnTabAddedToTree();
  }
}

void TabCollection::OnTabRemovedFromTree() {
  recursive_tab_count_--;

  if (parent_) {
    parent_->OnTabRemovedFromTree();
  }
}

}  // namespace tabs
