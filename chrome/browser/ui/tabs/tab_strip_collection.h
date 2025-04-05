// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_COLLECTION_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_COLLECTION_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_collection.h"
#include "components/tab_groups/tab_group_id.h"

namespace tabs {

class TabModel;
class UnpinnedTabCollection;
class PinnedTabCollection;
class TabGroupTabCollection;

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
  void AddTabRecursive(std::unique_ptr<TabModel> tab_model,
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
  size_t TotalTabCount() const;

  // Removes the tab present at a recursive index in the collection and
  // returns the unique_ptr to the tab model. If there is no tab present
  // due to bad input then CHECK.
  std::unique_ptr<TabModel> RemoveTabAtIndexRecursive(size_t index);

  // Removes the tab from the collection. If `close_empty_group_collection` is
  // true then group collection is closed when the last tab is removed from
  // the group collection.
  std::unique_ptr<TabModel> RemoveTabRecursive(
      TabModel* tab,
      bool close_empty_group_collection = true);

  // TabCollection:
  // Tabs and Collections are not allowed to be removed from TabStripCollection.
  // `MaybeRemoveTab` and `MaybeRemoveCollection` will return nullptr.
  std::unique_ptr<TabModel> MaybeRemoveTab(TabModel* tab_model) override;
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
  std::unique_ptr<TabGroupTabCollection> RemoveGroup(
      TabGroupTabCollection* group);
  TabGroupTabCollection* GetTabGroupCollection(
      tab_groups::TabGroupId group_id_);

  void MoveTabGroupTo(const tab_groups::TabGroupId& group, int to_index);

  // Adds the `tab_group_collection` to the collection hierarchy
  // with the first tab of the group starting at the recursive `index`.
  void InsertTabGroupAt(std::unique_ptr<TabGroupTabCollection> group_collection,
                        int index);

  // Clears all detached groups present in `detached_group_collections_`.
  void CloseDetachedTabGroup(const tab_groups::TabGroupId& group_id);

  void ValidateData() const;

 private:
  // If the group specified by new_group is detached, pop it from the detached
  // groups vector and add it to the collections structure at the specified
  // `index`.
  TabGroupTabCollection* MaybeAttachDetachedGroupCollection(
      int index,
      const tab_groups::TabGroupId& new_group);
  void MaybeRemoveGroupCollection(const tab_groups::TabGroupId& group);

  // Removes the group collection with `group_id` from
  // `detached_group_collections_`.
  std::unique_ptr<tabs::TabGroupTabCollection> PopDetachedGroupCollection(
      const tab_groups::TabGroupId& group_id);

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

  // `tab_strip_model` creates this to allow extension of lifetime for groups to
  // allow for group_model_ updates and observation methods.
  std::vector<std::unique_ptr<tabs::TabGroupTabCollection>>
      detached_group_collections_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_COLLECTION_H_
