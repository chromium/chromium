// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_TAB_STRIP_COLLECTION_H_
#define COMPONENTS_TABS_PUBLIC_TAB_STRIP_COLLECTION_H_

#include <memory>
#include <optional>
#include <unordered_map>

#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/split_tab_id.h"
#include "components/tabs/public/tab_collection.h"

namespace tabs {

class TabInterface;
class UnpinnedTabCollection;
class PinnedTabCollection;
class TabGroupTabCollection;
class SplitTabCollection;

// TabStripCollection is the storage representation of a tabstrip
// in a browser. This contains a pinned collection and an unpinned
// collection which then contain different tabs and group.
class TabStripCollection : public TabCollection {
 public:
  TabStripCollection();
  ~TabStripCollection() override;
  TabStripCollection(const TabStripCollection&) = delete;
  TabStripCollection& operator=(const TabStripCollection&) = delete;

  PinnedTabCollection* pinned_collection() { return pinned_collection_; }
  UnpinnedTabCollection* unpinned_collection() { return unpinned_collection_; }

  size_t IndexOfFirstNonPinnedTab() const;

  // Adds a tab to a particular recursive index in the collection. This forwards
  // calls to the appropriate parent collection (currently supports pinned,
  // unpinned, and group collections). If the inputs are incorrect this method
  // will fail and hit a CHECK.
  void AddTabRecursive(std::unique_ptr<TabInterface> tab,
                       size_t index,
                       std::optional<tab_groups::TabGroupId> new_group_id,
                       bool new_pinned_state);

  void MoveTabRecursive(size_t initial_index,
                        size_t final_index,
                        std::optional<tab_groups::TabGroupId> new_group_id,
                        bool new_pinned_state);
  void MoveTabsRecursive(const std::vector<int>& tab_indices,
                         size_t destination_index,
                         std::optional<tab_groups::TabGroupId> new_group_id,
                         bool new_pinned_state);

  // Removes the tab present at a recursive index in the collection and
  // returns the unique_ptr to the tab model. If there is no tab present
  // due to bad input then CHECK.
  std::unique_ptr<TabInterface> RemoveTabAtIndexRecursive(size_t index);

  // Removes the tab from the collection. If `close_empty_group_collection` is
  // true then group collection is closed when the last tab is removed from
  // the group collection.
  std::unique_ptr<TabInterface> RemoveTabRecursive(
      TabInterface* tab,
      bool close_empty_group_collection = true);

  // TabCollection:
  // Tabs and Collections are not allowed to be removed from TabStripCollection.
  // `MaybeRemoveTab` and `MaybeRemoveCollection` will return nullptr.
  std::unique_ptr<TabInterface> MaybeRemoveTab(TabInterface* tab) override;
  std::unique_ptr<TabCollection> MaybeRemoveCollection(
      TabCollection* collection) override;

  // Adds the `tab_group_collection` to `detached_group_collections_`
  // so that it can be used when inserting a tab to a group.
  void CreateTabGroup(
      std::unique_ptr<tabs::TabGroupTabCollection> tab_group_collection);

  // Group operations.
  // Use AddTabGroup and RemoveGroup to add/remove groups to the collection
  // structure while keeping track of the group ids in group_mapping_ so that
  // they can be looked up with GetTabGroupCollection.
  TabGroupTabCollection* AddTabGroup(
      std::unique_ptr<TabGroupTabCollection> group,
      int index);
  std::unique_ptr<TabCollection> RemoveGroup(TabGroupTabCollection* group);
  TabGroupTabCollection* GetTabGroupCollection(tab_groups::TabGroupId group_id);

  void MoveTabGroupTo(const tab_groups::TabGroupId& group, int to_index);

  // Adds the `tab_group_collection` to the collection hierarchy
  // with the first tab of the group starting at the recursive `index`.
  void InsertTabGroupAt(std::unique_ptr<TabGroupTabCollection> group_collection,
                        int index);

  // Clears all detached groups present in `detached_group_collections_`.
  void CloseDetachedTabGroup(const tab_groups::TabGroupId& group_id);

  // Split tab operations.
  SplitTabCollection* GetSplitTabCollection(split_tabs::SplitTabId split_id);
  void CreateSplit(split_tabs::SplitTabId split_id,
                   const std::vector<TabInterface*>& tabs,
                   split_tabs::SplitTabVisualData visual_data);
  void Unsplit(split_tabs::SplitTabId split_id);
  void InsertSplitTabAt(std::unique_ptr<SplitTabCollection> split_collection,
                        int index,
                        int pinned,
                        std::optional<tab_groups::TabGroupId> group);
  std::unique_ptr<TabCollection> RemoveSplit(SplitTabCollection* split);
  void ValidateData() const;

 private:
  // If the group specified by new_group is detached, pop it from the detached
  // groups vector and add it to the collections structure at the specified
  // `index`.
  TabGroupTabCollection* MaybeAttachDetachedGroupCollection(
      int index,
      const tab_groups::TabGroupId& new_group);

  void MaybeRemoveGroupCollection(TabGroupTabCollection* group_collection);

  // Removes the group collection with `group_id` from
  // `detached_group_collections_`.
  std::unique_ptr<tabs::TabGroupTabCollection> PopDetachedGroupCollection(
      const tab_groups::TabGroupId& group_id);

  // Returns the list of tabs and collection to remove for `MoveTabsRecursive`.
  // Collections might be present instead of tabs to retain certain collections
  // during drag.
  ChildrenPtrs GetTabsAndCollectionsForMove(
      const std::vector<int>& tab_indices);

  // Helper to centralize updates to `group_mapping_` and `split_mapping_`. If
  // `root_collection` is a group, the appropriate splits need to group need to
  // be updated in the `split_mapping_`.
  void AddCollectionMapping(TabCollection* root_collection);
  void RemoveCollectionMapping(TabCollection* root_collection);

  // Helper to compute the parent collection and direct index in the collection
  // to insert a tab or collection based on insertion properties like the
  // recursive index, pinned state and group to insert.
  std::pair<tabs::TabCollection*, int> GetInsertionDetails(
      int index,
      int pinned,
      std::optional<tab_groups::TabGroupId> group);

  // All of the pinned tabs for this tabstrip is present in this collection.
  // This should be below `impl_` to avoid being a dangling pointer during
  // destruction.
  raw_ptr<PinnedTabCollection> pinned_collection_ = nullptr;

  // All of the unpined tabs and groups for this tabstrip is present in this
  // collection. This should be below `impl_` to avoid being a dangling pointer
  // during destruction.
  raw_ptr<UnpinnedTabCollection> unpinned_collection_ = nullptr;

  // Lookup table to find group collections by their group ID.
  std::unordered_map<tab_groups::TabGroupId,
                     raw_ptr<TabGroupTabCollection>,
                     tab_groups::TabGroupIdHash>
      group_mapping_;

  // Lookup table to find split collections by their split ID.
  std::unordered_map<split_tabs::SplitTabId,
                     raw_ptr<SplitTabCollection>,
                     split_tabs::SplitTabIdHash>
      split_mapping_;

  // `tab_strip_model` creates this to allow extension of lifetime for groups to
  // allow for group_model_ updates and observation methods.
  std::vector<std::unique_ptr<tabs::TabGroupTabCollection>>
      detached_group_collections_;
};

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_TAB_STRIP_COLLECTION_H_
