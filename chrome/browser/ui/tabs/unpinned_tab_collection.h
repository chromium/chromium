// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_UNPINNED_TAB_COLLECTION_H_
#define CHROME_BROWSER_UI_TABS_UNPINNED_TAB_COLLECTION_H_

#include <memory>
#include <optional>

#include "chrome/browser/ui/tabs/tab_collection.h"
#include "chrome/browser/ui/tabs/tab_group_tab_collection.h"

namespace tab_groups {
class TabGroupId;
}  // namespace tab_groups

namespace tabs {

class TabModel;
class TabCollectionStorage;
class TabGroupTabCollection;

class UnpinnedTabCollection : public TabCollection {
 public:
  UnpinnedTabCollection();
  ~UnpinnedTabCollection() override;
  UnpinnedTabCollection(const UnpinnedTabCollection&) = delete;
  UnpinnedTabCollection& operator=(const UnpinnedTabCollection&) = delete;

  // Adds a `tab_model` to the collection at a particular index.
  void AddTab(std::unique_ptr<TabModel> tab_model, size_t direct_child_index);

  // Adds a tab to a particular index in the collection in a
  // recursive method. This method fails a check if the index is
  // invalid or the parameters passed in are incorrect.
  void AddTabRecursive(std::unique_ptr<TabModel> tab_model,
                       size_t index,
                       std::optional<tab_groups::TabGroupId> new_group_id);

  // Returns the tab at a particular index from the collection tree.
  // The index is a recursive index and if the index is invalid it returns
  // nullptr.
  tabs::TabModel* GetTabAtIndexRecursive(size_t index);

  // Appends a `tab_model` to the end of the collection.
  void AppendTab(std::unique_ptr<TabModel> tab_model);

  // Moves a `tab_model` to the `direct_child_dst_index` within the collection.
  // This operation is not recursive.
  void MoveTab(TabModel* tab_model, size_t direct_child_dst_index);

  // Removes and cleans the `tab_model`. The `tab_model` needs to be a direct
  // child of the collection.
  void CloseTab(TabModel* tab_model);

  // Returns the direct child index of the collection containing the tab or the
  // direct child index of the tab if it is a direct child of the unpinned
  // collection.
  std::optional<size_t> GetDirectChildIndexOfCollectionContainingTab(
      const TabModel* tab_model) const;

  // Explicit tab group related operations.
  // Adds a group to the collection at a particular index.
  TabGroupTabCollection* AddTabGroup(
      std::unique_ptr<TabGroupTabCollection> group,
      size_t index);

  // Moves a group to the `direct_child_dst_index` within the collection.
  // This operation is not recursive.
  void MoveTabGroup(TabGroupTabCollection* group,
                    size_t direct_child_dst_index);

  // Removes and cleans the group.
  void CloseTabGroup(TabGroupTabCollection* group);

  // Removes the group from the collection and returns the object to the caller.
  [[nodiscard]] std::unique_ptr<TabGroupTabCollection> RemoveGroup(
      TabGroupTabCollection* group);

  TabGroupTabCollection* GetTabGroupCollection(
      tab_groups::TabGroupId group_id_);

  TabCollectionStorage* GetTabCollectionStorageForTesting() {
    return impl_.get();
  }

  // TabCollection:
  bool ContainsTab(TabModel* tab_model) const override;

  bool ContainsTabRecursive(TabModel* tab_model) const override;

  bool ContainsCollection(TabCollection* collection) const override;

  std::optional<size_t> GetIndexOfTabRecursive(
      const TabModel* tab_model) const override;

  std::optional<size_t> GetIndexOfCollection(
      TabCollection* collection) const override;

  [[nodiscard]] std::unique_ptr<TabModel> MaybeRemoveTab(
      TabModel* tab_model) override;

  [[nodiscard]] std::unique_ptr<TabCollection> MaybeRemoveCollection(
      TabCollection* collection) override;

  size_t ChildCount() const override;

  void MoveGroupToRecursive(int index, TabGroupTabCollection* collection);

  void ValidateCollections() const;

 private:
  // Return the direct child index of a tab
  std::optional<size_t> GetIndexOfTab(const TabModel* tab_model) const;

  // Underlying implementation for the storage of children.
  std::unique_ptr<TabCollectionStorage> impl_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_UNPINNED_TAB_COLLECTION_H_
