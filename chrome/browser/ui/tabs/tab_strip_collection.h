// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_COLLECTION_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_COLLECTION_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_collection.h"

namespace tab_groups {
class TabGroupId;
}  // namespace tab_groups

namespace tabs {

class TabModel;
class TabCollectionStorage;
class UnpinnedTabCollection;
class PinnedTabCollection;

// TabStripCollection is the storage representation of a tabstrip
// in a browser. This contains a pinned collection and an unpinned
// collection which then contain different tabs and group.
class TabStripCollection : public TabCollection {
 public:
  TabStripCollection();
  ~TabStripCollection() override;
  TabStripCollection(const TabStripCollection&) = delete;
  TabStripCollection& operator=(const TabStripCollection&) = delete;

  PinnedTabCollection* GetPinnedCollection() { return pinned_collection_; }
  UnpinnedTabCollection* GetUnpinnedCollection() {
    return unpinned_collection_;
  }

  // Adds a tab to a particular index in the collection in a
  // recursive method. This forwards calls to either the pinned
  // container or the unpinned container. If the inputs are incorrect
  // this method will fail and hit a CHECK.
  void AddTabRecursive(std::unique_ptr<TabModel> tab_model,
                       size_t index,
                       std::optional<tab_groups::TabGroupId> new_group_id,
                       bool new_pinned_state);

  // Returns the tab at a particular index from the collection tree.
  // The index is a recursive index and if the index is invalid it returns
  // nullptr.
  tabs::TabModel* GetTabAtIndexRecursive(size_t index) const;

  // Removes the tab present at a recursive index in the collection and
  // returns the unique_ptr to the tab model. If there is no tab present
  // due to bad input then CHECK.
  std::unique_ptr<TabModel> RemoveTabAtIndexRecursive(size_t index);

  // TabCollection:
  // This will be false as this does not contain a tab as a direct child.
  bool ContainsTab(TabModel* tab_model) const override;
  bool ContainsTabRecursive(TabModel* tab_model) const override;
  // Returns true if the collection is the pinned collection or the
  // unpinned collection.
  bool ContainsCollection(TabCollection* collection) const override;
  std::optional<size_t> GetIndexOfTabRecursive(
      TabModel* tab_model) const override;
  std::optional<size_t> GetIndexOfCollection(
      TabCollection* collection) const override;
  // Tabs and Collections are not allowed to be removed from TabStripCollection.
  // `MaybeRemoveTab` and `MaybeRemoveCollection` will return nullptr.
  std::unique_ptr<TabModel> MaybeRemoveTab(TabModel* tab_model) override;
  std::unique_ptr<TabCollection> MaybeRemoveCollection(
      TabCollection* collection) override;
  size_t ChildCount() const override;
  size_t TabCountRecursive() const override;

  TabCollectionStorage* GetTabCollectionStorageForTesting() {
    return impl_.get();
  }

 private:
  // Underlying implementation for the storage of children.
  std::unique_ptr<TabCollectionStorage> impl_;

  // All of the pinned tabs for this tabstrip is present in this collection.
  // This should be below `impl_` to avoid being a dangling pointer during
  // destruction.
  raw_ptr<PinnedTabCollection> pinned_collection_;

  // All of the unpined tabs and groups for this tabstrip is present in this
  // collection. This should be below `impl_` to avoid being a dangling pointer
  // during destruction.
  raw_ptr<UnpinnedTabCollection> unpinned_collection_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_COLLECTION_H_
