// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/tab_strip_collection.h"

#include <memory>
#include <optional>
#include <set>

#include "base/containers/adapters.h"
#include "base/memory/ptr_util.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_id.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_collection_storage.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/unpinned_tab_collection.h"

namespace tabs {

TabStripCollection::TabStripCollection()
    : TabCollection(
          TabCollection::Type::TABSTRIP,
          {TabCollection::Type::PINNED, TabCollection::Type::UNPINNED},
          /*supports_tabs=*/false) {
  pinned_collection_ =
      AddCollection(std::make_unique<PinnedTabCollection>(), 0);
  unpinned_collection_ =
      AddCollection(std::make_unique<UnpinnedTabCollection>(), 1);
}

TabStripCollection::~TabStripCollection() = default;

size_t TabStripCollection::IndexOfFirstNonPinnedTab() const {
  return pinned_collection_->TabCountRecursive();
}

void TabStripCollection::AddTabRecursive(
    std::unique_ptr<TabInterface> tab,
    size_t index,
    std::optional<tab_groups::TabGroupId> new_group_id,
    bool new_pinned_state) {
  CHECK(tab);
  // `index` can be equal to the tab count as at this point the tab has not yet
  // been added.
  CHECK(index >= 0 && index <= TabCountRecursive());

  // First tab needs to be added to the group. In this case we need to create a
  // group collection.
  if (new_group_id.has_value() &&
      !GetTabGroupCollection(new_group_id.value())) {
    // Attempt to create a new group for the tab.
    TabGroupTabCollection* group_collection =
        MaybeAttachDetachedGroupCollection(index, new_group_id.value());

    // New empty group was attached, append tab to the group.
    CHECK(group_collection->ChildCount() == 0);
    TabInterface* inserted_tab = group_collection->AddTab(std::move(tab), 0);
    CHECK(GetIndexOfTabRecursive(inserted_tab) == index);
    return;
  }

  auto [tab_collection_ptr, insert_index] =
      GetInsertionDetails(index, new_pinned_state, new_group_id);
  tab_collection_ptr->AddTab(std::move(tab), insert_index);
}

void TabStripCollection::MoveTabRecursive(
    size_t initial_index,
    size_t final_index,
    std::optional<tab_groups::TabGroupId> new_group_id,
    bool new_pinned_state) {
  CHECK(initial_index >= 0 && final_index >= 0);

  TabInterface* tab = GetTabAtIndexRecursive(initial_index);
  const std::optional<tab_groups::TabGroupId> old_group = tab->GetGroup();
  TabGroupTabCollection* old_group_collection =
      old_group.has_value() ? GetTabGroupCollection(old_group.value())
                            : nullptr;
  const bool is_only_tab_in_group =
      old_group.has_value() && old_group_collection->TabCountRecursive() == 1;

  if (old_group == new_group_id && new_group_id.has_value() &&
      is_only_tab_in_group) {
    unpinned_collection_->MoveGroupToRecursive(
        final_index - pinned_collection_->TabCountRecursive(),
        old_group_collection);
  } else {
    std::unique_ptr<TabInterface> moved_data =
        RemoveTabRecursive(tab, old_group != new_group_id);
    AddTabRecursive(std::move(moved_data), final_index, new_group_id,
                    new_pinned_state);
  }
}

void TabStripCollection::MoveTabsRecursive(
    const std::vector<int>& tab_indices,
    size_t destination_index,
    std::optional<tab_groups::TabGroupId> new_group_id,
    bool new_pinned_state) {
  CHECK(destination_index >= 0);
  ChildrenVector moved_datas;

  // Removes the tabs and collections needed to be moved.
  for (auto& tab_or_collection : GetTabsAndCollectionsForMove(tab_indices)) {
    TabCollection* parent_collection;
    if (std::holds_alternative<TabInterface*>(tab_or_collection)) {
      TabInterface* tab_to_remove = std::get<TabInterface*>(tab_or_collection);
      parent_collection = tab_to_remove->GetParentCollection(GetPassKey());
      moved_datas.push_back(parent_collection->MaybeRemoveTab(tab_to_remove));
    } else {
      TabCollection* collection_to_remove =
          std::get<TabCollection*>(tab_or_collection);
      parent_collection = collection_to_remove->GetParentCollection();
      moved_datas.push_back(
          parent_collection->MaybeRemoveCollection(collection_to_remove));
    }

    // TODO(406529289): Remove this once groups collections can be moved by
    // dragging. This is needed as a group can be left in an empty state here if
    // all the tabs in the subcollection is removed.
    if (parent_collection->type() == TabCollection::Type::GROUP) {
      MaybeRemoveGroupCollection(
          static_cast<TabGroupTabCollection*>(parent_collection));
    }
  }

  // `tab_collection_ptr` is the final collection to insert `moved_datas`
  // starting at `insert_index`.
  auto [tab_collection_ptr, insert_index] =
      GetInsertionDetails(destination_index, new_pinned_state, new_group_id);
  CHECK(tab_collection_ptr);

  // Insert tabs and collections left to right so destination index can be used
  // in an incremental manner from the direct destination index computed from
  // `insertion_details`.
  for (size_t i = 0; i < moved_datas.size(); i++) {
    if (std::holds_alternative<std::unique_ptr<TabInterface>>(moved_datas[i])) {
      tab_collection_ptr->AddTab(
          std::move(std::get<std::unique_ptr<TabInterface>>(moved_datas[i])),
          insert_index + i);
    } else {
      tab_collection_ptr->AddCollection(
          std::move(std::get<std::unique_ptr<TabCollection>>(moved_datas[i])),
          insert_index + i);
    }
  }
}

ChildrenPtrs TabStripCollection::GetTabsAndCollectionsForMove(
    const std::vector<int>& tab_indices) {
  std::set<const TabInterface*> selected_tabs;
  for (int index : tab_indices) {
    selected_tabs.insert(GetTabAtIndexRecursive(index));
  }

  // Contains set of all the collections fully covered by `tab_indices`. This
  // does not include `pinned_collection_` or `unpinned_collection_` as they
  // cannot be moved.
  std::set<const TabCollection*> selected_collections;

  // TODO(406529289): Find selected group collections for dragging groups.
  for (const auto& [split_id, split_collection] : split_mapping_) {
    bool fully_selected = true;
    for (const TabInterface* tab : *split_collection) {
      if (!selected_tabs.contains(tab)) {
        fully_selected = false;
        break;
      }
    }

    if (fully_selected) {
      selected_collections.insert(split_collection);
    }
  }

  ChildrenPtrs move_datas;

  // Iterates through `tab_indices`. If the tab is a direct child of
  // `pinned_collection_` or `unpinned_collection_` return it directly as it
  // needs to be removed directly. Otherwise potentially return the biggest
  // subcollection that contains the tab to be removed.
  int array_index = 0;
  while (array_index < static_cast<int>(tab_indices.size())) {
    TabInterface* tab = GetTabAtIndexRecursive(tab_indices[array_index]);
    TabCollection* collection = tab->GetParentCollection(GetPassKey());
    if (collection == pinned_collection_ ||
        collection == unpinned_collection_) {
      TabInterface* move_data =
          GetTabAtIndexRecursive(tab_indices[array_index]);
      move_datas.push_back(move_data);
      array_index++;
      continue;
    } else {
      TabCollection* candidate_subtree_collection = collection;
      TabCollection* subtree_to_remove = nullptr;

      // Finds the largest subcollection containing `tab`. `subtree_to_remove`
      // is the current valid subcollection to remove while
      // `candidate_subtree_collection` is the next potential subcollection to
      // remove.
      while ((candidate_subtree_collection != pinned_collection_ &&
              candidate_subtree_collection != unpinned_collection_) &&
             selected_collections.contains(candidate_subtree_collection)) {
        subtree_to_remove = candidate_subtree_collection;
        candidate_subtree_collection =
            candidate_subtree_collection->GetParentCollection();
      }

      if (subtree_to_remove) {
        move_datas.push_back(subtree_to_remove);
        array_index += subtree_to_remove->TabCountRecursive();
      } else {
        TabInterface* move_data =
            GetTabAtIndexRecursive(tab_indices[array_index]);
        move_datas.push_back(move_data);
        array_index++;
      }
    }
  }

  return move_datas;
}

TabGroupTabCollection* TabStripCollection::AddTabGroup(
    std::unique_ptr<TabGroupTabCollection> group,
    int index) {
  AddCollectionMapping(group.get());
  const int dst_index =
      (index == static_cast<int>(TabCountRecursive()))
          ? unpinned_collection_->ChildCount()
          : unpinned_collection_
                ->GetDirectChildIndexOfCollectionContainingTab(
                    GetTabAtIndexRecursive(index))
                .value();
  return unpinned_collection_->AddCollection(std::move(group), dst_index);
}

std::unique_ptr<TabCollection> TabStripCollection::RemoveGroup(
    TabGroupTabCollection* group) {
  CHECK(group_mapping_.contains(group->GetTabGroupId()));
  RemoveCollectionMapping(group);
  return unpinned_collection_->MaybeRemoveCollection(group);
}

TabGroupTabCollection* TabStripCollection::GetTabGroupCollection(
    tab_groups::TabGroupId group_id) {
  if (!group_mapping_.contains(group_id)) {
    return nullptr;
  }
  return group_mapping_.at(group_id);
}

void TabStripCollection::MoveTabGroupTo(const tab_groups::TabGroupId& group,
                                        int to_index) {
  tabs::TabGroupTabCollection* group_collection = GetTabGroupCollection(group);

  CHECK(to_index >= static_cast<int>(pinned_collection_->TabCountRecursive()));

  unpinned_collection_->MoveGroupToRecursive(
      to_index - pinned_collection_->TabCountRecursive(), group_collection);
}

void TabStripCollection::InsertTabGroupAt(
    std::unique_ptr<TabGroupTabCollection> group_collection,
    int index) {
  CHECK(index >= static_cast<int>(pinned_collection_->TabCountRecursive()));
  AddTabGroup(std::move(group_collection), index);
}

std::unique_ptr<TabInterface> TabStripCollection::RemoveTabAtIndexRecursive(
    size_t index) {
  TabInterface* tab_to_be_removed = GetTabAtIndexRecursive(index);
  return RemoveTabRecursive(tab_to_be_removed);
}

std::unique_ptr<TabInterface> TabStripCollection::RemoveTabRecursive(
    TabInterface* tab,
    bool close_empty_group_collection) {
  CHECK(tab);

  TabCollection* parent_collection = tab->GetParentCollection(GetPassKey());
  const std::optional<tab_groups::TabGroupId> group = tab->GetGroup();

  std::unique_ptr<TabInterface> removed_tab =
      parent_collection->MaybeRemoveTab(tab);

  CHECK(removed_tab);

  if (group.has_value() && close_empty_group_collection) {
    MaybeRemoveGroupCollection(GetTabGroupCollection(group.value()));
  }

  return removed_tab;
}

std::unique_ptr<TabInterface> TabStripCollection::MaybeRemoveTab(
    TabInterface* tab) {
  CHECK(tab);
  return nullptr;
}

std::unique_ptr<TabCollection> TabStripCollection::MaybeRemoveCollection(
    TabCollection* collection) {
  CHECK(collection);
  return nullptr;
}

void TabStripCollection::CreateTabGroup(
    std::unique_ptr<tabs::TabGroupTabCollection> tab_group_collection) {
  CHECK(tab_group_collection);
  detached_group_collections_.push_back(std::move(tab_group_collection));
}

void TabStripCollection::CloseDetachedTabGroup(
    const tab_groups::TabGroupId& group_id) {
  PopDetachedGroupCollection(group_id).reset();
}

SplitTabCollection* TabStripCollection::GetSplitTabCollection(
    split_tabs::SplitTabId split_id) {
  if (!split_mapping_.contains(split_id)) {
    return nullptr;
  }
  return split_mapping_.at(split_id);
}

void TabStripCollection::CreateSplit(
    split_tabs::SplitTabId split_id,
    const std::vector<TabInterface*>& tabs,
    split_tabs::SplitTabVisualData visual_data) {
  CHECK(tabs.size() >= 2);
  TabCollection* parent_collection = tabs[0]->GetParentCollection(GetPassKey());
  CHECK(std::all_of(
      tabs.begin(), tabs.end(), [this, parent_collection](auto tab) {
        return parent_collection == tab->GetParentCollection(GetPassKey());
      }));

  size_t dst_index = parent_collection->GetIndexOfTab(tabs[0]).value();

  // Move tabs from parent to new split.
  std::unique_ptr<SplitTabCollection> split =
      std::make_unique<SplitTabCollection>(split_id, visual_data);
  for (TabInterface* tab : tabs) {
    split->AddTab(parent_collection->MaybeRemoveTab(tab), split->ChildCount());
  }

  // Insert split back into the parent.
  AddCollectionMapping(split.get());
  parent_collection->AddCollection(std::move(split), dst_index);
}

void TabStripCollection::Unsplit(split_tabs::SplitTabId split_id) {
  SplitTabCollection* split = GetSplitTabCollection(split_id);
  if (!split) {
    return;
  }

  CHECK(split_mapping_.contains(split_id));
  // Insert tabs removed from the split into the parent collection. Does so in
  // reverse to preserve the ordering of the tabs without having to increment
  // the index of the insertion point.
  TabCollection* parent_collection = split->GetParentCollection();
  size_t dst_index = parent_collection->GetIndexOfCollection(split).value();
  for (std::vector<TabInterface*> tabs = split->GetTabsRecursive();
       TabInterface* tab : base::Reversed(tabs)) {
    parent_collection->AddTab(split->MaybeRemoveTab(tab), dst_index);
  }

  RemoveCollectionMapping(split);
  parent_collection->MaybeRemoveCollection(split).reset();
}

void TabStripCollection::InsertSplitTabAt(
    std::unique_ptr<SplitTabCollection> split_collection,
    int index,
    int pinned,
    std::optional<tab_groups::TabGroupId> group) {
  AddCollectionMapping(split_collection.get());

  auto [tab_collection_ptr, insert_index] =
      GetInsertionDetails(index, pinned, group);
  CHECK(tab_collection_ptr);
  tab_collection_ptr->AddCollection(std::move(split_collection), insert_index);
}

std::unique_ptr<TabCollection> TabStripCollection::RemoveSplit(
    SplitTabCollection* split) {
  CHECK(split_mapping_.contains(split->GetSplitTabId()));
  RemoveCollectionMapping(split);

  return split->GetParentCollection()->MaybeRemoveCollection(split);
}

void TabStripCollection::ValidateData() const {
  CHECK(detached_group_collections_.empty());
  for (const auto& [_, group] : group_mapping_) {
    CHECK(group->ChildCount() > 0);
  }
  for (const auto& [_, split] : split_mapping_) {
    CHECK(split->ChildCount() >= 2);
    for (auto child : split->GetTabsRecursive()) {
      CHECK(split->GetSplitTabId() == child->GetSplit().value());
    }
  }
}

TabGroupTabCollection* TabStripCollection::MaybeAttachDetachedGroupCollection(
    int index,
    const tab_groups::TabGroupId& new_group) {
  CHECK(index >= 0);

  // Do not create a collection if the group is already present.
  if (GetTabGroupCollection(new_group)) {
    return nullptr;
  }

  return AddTabGroup(PopDetachedGroupCollection(new_group), index);
}

std::unique_ptr<tabs::TabGroupTabCollection>
TabStripCollection::PopDetachedGroupCollection(
    const tab_groups::TabGroupId& group_id) {
  auto it = std::find_if(
      detached_group_collections_.begin(), detached_group_collections_.end(),
      [group_id](
          const std::unique_ptr<tabs::TabGroupTabCollection>& collection) {
        return collection->GetTabGroupId() == group_id;
      });
  CHECK(it != detached_group_collections_.end());

  std::unique_ptr<tabs::TabGroupTabCollection> group_collection =
      std::move(*it);
  detached_group_collections_.erase(it);

  return group_collection;
}

void TabStripCollection::MaybeRemoveGroupCollection(
    TabGroupTabCollection* group_collection) {
  if (group_collection && group_collection->TabCountRecursive() == 0) {
    detached_group_collections_.push_back(
        base::WrapUnique(static_cast<TabGroupTabCollection*>(
            RemoveGroup(group_collection).release())));
  }
}

void TabStripCollection::AddCollectionMapping(TabCollection* root_collection) {
  if (root_collection->type() == TabCollection::Type::GROUP) {
    TabGroupTabCollection* group_collection =
        static_cast<TabGroupTabCollection*>(root_collection);
    group_mapping_.insert(
        {group_collection->GetTabGroupId(), group_collection});

    for (const tabs::TabInterface* tab : *group_collection) {
      if (tab->IsSplit()) {
        const split_tabs::SplitTabId split_id = tab->GetSplit().value();
        if (!split_mapping_.contains(split_id)) {
          split_mapping_.insert(
              {split_id, static_cast<SplitTabCollection*>(
                             tab->GetParentCollection(GetPassKey()))});
        }
      }
    }
  } else if (root_collection->type() == TabCollection::Type::SPLIT) {
    SplitTabCollection* split_collection =
        static_cast<SplitTabCollection*>(root_collection);
    split_mapping_.insert(
        {split_collection->GetSplitTabId(), split_collection});
  }
}

void TabStripCollection::RemoveCollectionMapping(
    TabCollection* root_collection) {
  if (root_collection->type() == TabCollection::Type::GROUP) {
    TabGroupTabCollection* group_collection =
        static_cast<TabGroupTabCollection*>(root_collection);
    group_mapping_.erase(group_collection->GetTabGroupId());

    for (const tabs::TabInterface* tab : *group_collection) {
      if (tab->IsSplit()) {
        split_mapping_.erase(tab->GetSplit().value());
      }
    }

  } else if (root_collection->type() == TabCollection::Type::SPLIT) {
    SplitTabCollection* split_collection =
        static_cast<SplitTabCollection*>(root_collection);
    split_mapping_.erase(split_collection->GetSplitTabId());
  }
}

std::pair<tabs::TabCollection*, int> TabStripCollection::GetInsertionDetails(
    int index,
    int pinned,
    std::optional<tab_groups::TabGroupId> group) {
  size_t direct_dst_index;
  TabCollection* insert_collection = nullptr;

  if (pinned) {
    direct_dst_index = pinned_collection_->ToDirectIndex(index);
    insert_collection = pinned_collection_;
  } else if (group.has_value()) {
    TabGroupTabCollection* group_collection =
        GetTabGroupCollection(group.value());
    CHECK(group_collection);

    size_t offset =
        GetIndexOfTabRecursive(group_collection->GetTabAtIndexRecursive(0))
            .value();
    direct_dst_index = group_collection->ToDirectIndex(index - offset);
    insert_collection = group_collection;
  } else {
    size_t offset = pinned_collection_->TabCountRecursive();
    direct_dst_index = unpinned_collection_->ToDirectIndex(index - offset);
    insert_collection = unpinned_collection_;
  }

  return std::pair<tabs::TabCollection*, int>(insert_collection,
                                              direct_dst_index);
}

}  // namespace tabs
