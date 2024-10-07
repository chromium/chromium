// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_COLLECTION_H_
#define CHROME_BROWSER_UI_TABS_TAB_COLLECTION_H_

#include <cstddef>
#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"

namespace tabs {

class TabModel;

// This is an interface that representing the hierarchical storage of tabs.
// This can be used to access and manipulate tabs and the state of the tabstrip.
// Different types of collections should implement this base class based on how
// their feature works. For example, a pinned collection can implement tab
// collection that does not store any collection.
class TabCollection {
 public:
  TabCollection() = default;
  virtual ~TabCollection() = default;
  TabCollection(const TabCollection&) = delete;
  TabCollection& operator=(const TabCollection&) = delete;

  // Returns true if the tab model is a direct child of the collection.
  virtual bool ContainsTab(TabModel* tab_model) const = 0;

  // Returns true if the tab collection tree contains the tab.
  virtual bool ContainsTabRecursive(TabModel* tab_model) const = 0;

  // Returns true is the tab collection contains the collection. This is a
  // non-recursive check.
  virtual bool ContainsCollection(TabCollection* collection) const = 0;

  // Recursively get the index of the tab_model among all the leaf tab_models.
  virtual std::optional<size_t> GetIndexOfTabRecursive(
      const TabModel* tab_model) const = 0;

  // Non-recursively get the index of a collection.
  virtual std::optional<size_t> GetIndexOfCollection(
      TabCollection* collection) const = 0;

  // Total number of children that directly have this collection as their
  // parent.
  virtual size_t ChildCount() const = 0;

  // Total number of tabs the collection contains.
  size_t TabCountRecursive() const { return recursive_tab_count_; }

  void OnCollectionAddedToTree(TabCollection* collection);
  void OnCollectionRemovedFromTree(TabCollection* collection);
  void OnTabAddedToTree();
  void OnTabRemovedFromTree();

  // Removes the tab if it is a direct child of this collection. This is then
  // returned to the caller as an unique_ptr. If the tab is not present it will
  // return a nullptr.
  [[nodiscard]] virtual std::unique_ptr<TabModel> MaybeRemoveTab(
      TabModel* tab) = 0;

  // Removes the collection if it is a direct child of this collection. This is
  // then returned to the caller as an unique_ptr. If the collection is not
  // present it will return a nullptr.
  [[nodiscard]] virtual std::unique_ptr<TabCollection> MaybeRemoveCollection(
      TabCollection* collection) = 0;

  TabCollection* GetParentCollection() const { return parent_; }

  // This should be called either when this collection is added to another
  // collection or it is removed from another collection. The child collection
  // should not try to call this internally and set its parent.
  void OnReparented(TabCollection* new_parent) { parent_ = new_parent; }

 protected:
  // Returns the pass key to be used by derived classes as operations such as
  // setting the parent of a tab can only be performed by a `TabCollection`.
  base::PassKey<TabCollection> GetPassKey() const {
    return base::PassKey<TabCollection>();
  }

  // Total number of tabs in the collection.
  size_t recursive_tab_count_ = 0;

 private:
  raw_ptr<TabCollection> parent_ = nullptr;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_COLLECTION_H_
