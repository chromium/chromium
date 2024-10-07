// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_COLLECTION_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_COLLECTION_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_collection.h"
#include "chrome/browser/ui/tabs/tab_contents_data.h"

namespace tab_groups {
class TabGroupId;
}  // namespace tab_groups

namespace tabs {

class TabModel;
class TabCollectionStorage;
class UnpinnedTabCollection;
class PinnedTabCollection;
class TabGroupTabCollection;

// TabStripCollection is the storage representation of a tabstrip
// in a browser. This contains a pinned collection and an unpinned
// collection which then contain different tabs and group.
class TabStripCollection : public TabCollection, public TabContentsData {
 public:
  TabStripCollection();
  ~TabStripCollection() override;
  TabStripCollection(const TabStripCollection&) = delete;
  TabStripCollection& operator=(const TabStripCollection&) = delete;

  PinnedTabCollection* pinned_collection() { return pinned_collection_; }
  UnpinnedTabCollection* unpinned_collection() { return unpinned_collection_; }

  size_t IndexOfFirstNonPinnedTab() const override;

  // Returns the tab at a particular index from the collection tree.
  // The index is a recursive index and if the index is invalid it returns
  // nullptr.
  tabs::TabModel* GetTabAtIndexRecursive(size_t index) const override;

  // Adds a tab to a particular index in the collection in a
  // recursive method. This forwards calls to either the pinned
  // container or the unpinned container. If the inputs are incorrect
  // this method will fail and hit a CHECK.
  void AddTabRecursive(std::unique_ptr<TabModel> tab_model,
                       size_t index,
                       std::optional<tab_groups::TabGroupId> new_group_id,
                       bool new_pinned_state) override;

  void MoveTabRecursive(size_t initial_index,
                        size_t final_index,
                        std::optional<tab_groups::TabGroupId> new_group_id,
                        bool new_pinned_state) override;
  void MoveTabsRecursive(const std::vector<int>& tab_indices,
                         size_t destination_index,
                         std::optional<tab_groups::TabGroupId> new_group_id,
                         bool new_pinned_state) override;
  void MoveGroupTo(const TabGroupModel* group_model,
                   const tab_groups::TabGroupId& group,
                   int to_index) override;
  size_t TotalTabCount() const override;

  // Removes the tab present at a recursive index in the collection and
  // returns the unique_ptr to the tab model. If there is no tab present
  // due to bad input then CHECK.
  std::unique_ptr<TabModel> RemoveTabAtIndexRecursive(size_t index) override;

  // Removes the tab from the collection. If `close_empty_group_collection` is
  // true then group collection is closed when the last tab is removed from
  // the group collection.
  std::unique_ptr<TabModel> RemoveTabRecursive(
      TabModel* tab,
      bool close_empty_group_collection = true);

  // TabCollection:
  // This will be false as this does not contain a tab as a direct child.
  bool ContainsTab(TabModel* tab_model) const override;
  bool ContainsTabRecursive(TabModel* tab_model) const override;
  // Returns true if the collection is the pinned collection or the
  // unpinned collection.
  bool ContainsCollection(TabCollection* collection) const override;
  std::optional<size_t> GetIndexOfTabRecursive(
      const TabModel* tab_model) const override;
  std::optional<size_t> GetIndexOfCollection(
      TabCollection* collection) const override;
  // Tabs and Collections are not allowed to be removed from TabStripCollection.
  // `MaybeRemoveTab` and `MaybeRemoveCollection` will return nullptr.
  std::unique_ptr<TabModel> MaybeRemoveTab(TabModel* tab_model) override;
  std::unique_ptr<TabCollection> MaybeRemoveCollection(
      TabCollection* collection) override;
  size_t ChildCount() const override;

  TabCollectionStorage* GetTabCollectionStorageForTesting() {
    return impl_.get();
  }

  void ValidateData(const TabGroupModel* group_model) override;

 private:
  // Creates a new group collection with respect to a tab based on the
  // position of the tab in the collection.
  TabGroupTabCollection* MaybeCreateNewGroupCollectionForTab(
      int index,
      const tab_groups::TabGroupId& new_group);
  void MaybeRemoveGroupCollection(const tab_groups::TabGroupId& group);

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
