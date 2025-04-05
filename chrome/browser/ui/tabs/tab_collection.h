// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_COLLECTION_H_
#define CHROME_BROWSER_UI_TABS_TAB_COLLECTION_H_

#include <cstddef>
#include <list>
#include <memory>
#include <optional>
#include <unordered_set>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/tab_collection_storage.h"

namespace tabs {

class TabInterface;
class TabModel;

// This is an interface that representing the hierarchical storage of tabs.
// This can be used to access and manipulate tabs and the state of the tabstrip.
// Different types of collections should implement this base class based on how
// their feature works. For example, a pinned collection can implement tab
// collection that does not store any collection.
// TODO(crbug.com/392950857): Use TabInterface instead of TabModel here and in
// collections inheriting from this.
class TabCollection {
 public:
  // Type describes the various kinds of tab collections:
  // - TABSTRIP:  The main container for tabs in a browser window.
  // - PINNED:    A container for pinned tabs.
  // - UNPINNED:  A container for unpinned tabs.
  // - GROUP:     A container to grouped tabs.
  // - SPLIT:     A container for split tabs.
  enum class Type { TABSTRIP, PINNED, UNPINNED, GROUP, SPLIT };

  virtual ~TabCollection();
  TabCollection(const TabCollection&) = delete;
  TabCollection& operator=(const TabCollection&) = delete;

  // Returns true is the tab collection contains the collection. This is a
  // non-recursive check.
  bool ContainsCollection(TabCollection* collection) const;

  // Non-recursively get the index of a tab.
  std::optional<size_t> GetIndexOfTab(const TabInterface* tab) const;

  // Recursively get the index of the tab among all the leaf tabs.
  std::optional<size_t> GetIndexOfTabRecursive(const TabInterface* tab) const;

  // Recursively get the tab at the index.
  TabModel* GetTabAtIndexRecursive(size_t index) const;

  // Returns a flattened list of all tabs in this collection and its
  // subcollections. Prefer using the result of this function instead of looping
  // over an index and using GetTabAtIndexRecursive, as this function will only
  // do one pass over the subcollections.
  std::list<TabModel*> GetTabsRecursive() const;

  // Non-recursively get the index of a collection.
  std::optional<size_t> GetIndexOfCollection(TabCollection* collection) const;

  // Convert a recursive index to a direct index. Fails a CHECK if the tab at
  // the recursive index lies within a subcollection.
  size_t ToDirectIndex(size_t index);

  // Total number of children that directly have this collection as their
  // parent.
  size_t ChildCount() const;

  TabCollectionStorage* GetTabCollectionStorageForTesting() {
    return impl_.get();
  }

  Type type() { return type_; }

  // Total number of tabs the collection contains.
  size_t TabCountRecursive() const { return recursive_tab_count_; }

  // Callbacks that are triggered by the impl_ when tabs/collections are added
  // or removed from the tree.
  void OnCollectionAddedToTree(TabCollection* collection);
  void OnCollectionRemovedFromTree(TabCollection* collection);
  void OnTabAddedToTree();
  void OnTabRemovedFromTree();

  // Manipulate direct child tabs.
  TabModel* AddTab(std::unique_ptr<TabModel> tab_model, size_t index);
  // Removes the tab if it is a direct child of this collection. This is then
  // returned to the caller as an unique_ptr. If the tab is not present it will
  // crash. This may overridden to return nullptr if the collection does not
  // support removing tabs.
  [[nodiscard]] virtual std::unique_ptr<TabModel> MaybeRemoveTab(TabModel* tab);

  // Manipulate direct child collections.
  // Adds a collection as a direct child of this collection. If this succeeds it
  // will return a pointer to the collection, otherwise it will return nullptr.
  template <std::derived_from<TabCollection> T>
  T* AddCollection(std::unique_ptr<T> collection, size_t index) {
    CHECK(collection);
    CHECK(supported_child_collections_.contains(collection->type()));
    CHECK(index <= ChildCount());

    TabCollection* added_collection =
        impl_->AddCollection(std::move(collection), index);
    added_collection->OnReparented(this);
    return static_cast<T*>(added_collection);
  }
  // Removes the collection if it is a direct child of this collection. This is
  // then returned to the caller as an unique_ptr. If the collection is not
  // present it will crash. This may be overridden to return nullptr if the
  // collection does not support removing collections.
  [[nodiscard]] virtual std::unique_ptr<TabCollection> MaybeRemoveCollection(
      TabCollection* collection);

  TabCollection* GetParentCollection() const { return parent_; }

  // This should be called either when this collection is added to another
  // collection or it is removed from another collection. The child collection
  // should not try to call this internally and set its parent.
  void OnReparented(TabCollection* new_parent);

 protected:
  explicit TabCollection(Type type,
                         std::unordered_set<Type> supported_child_collections,
                         bool supports_tabs);

  // Returns the pass key to be used by derived classes as operations such as
  // setting the parent of a tab can only be performed by a `TabCollection`.
  base::PassKey<TabCollection> GetPassKey() const {
    return base::PassKey<TabCollection>();
  }

  // Total number of tabs in the collection.
  size_t recursive_tab_count_ = 0;

 private:
  raw_ptr<TabCollection> parent_ = nullptr;
  Type type_;
  std::unordered_set<Type> supported_child_collections_;
  bool supports_tabs_;

  // Underlying implementation for the storage of children.
  std::unique_ptr<TabCollectionStorage> impl_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_COLLECTION_H_
