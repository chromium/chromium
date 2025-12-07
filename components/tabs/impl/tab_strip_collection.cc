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
#include "components/tabs/public/tab_collection_observer.h"
#include "components/tabs/public/tab_collection_storage.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/unpinned_tab_collection.h"

namespace {
tabs::TabCollection* GetCommonAncestor(tabs::TabCollection* collection_a,
                                       tabs::TabCollection* collection_b) {
  if (collection_a == collection_b) {
    return collection_a;
  }

  // Use a set to store all ancestors of the first collection.
  std::set<tabs::TabCollection*> ancestors;

  // 1. Trace the path from collection_a to the root and store it.
  tabs::TabCollection* current_a = collection_a;
  while (current_a) {
    ancestors.insert(current_a);
    current_a = current_a->GetParentCollection();
  }

  // 2. Trace the path from collection_b up and check for a match.
  tabs::TabCollection* current_b = collection_b;
  while (current_b) {
    if (ancestors.contains(current_b)) {
      return current_b;
    }
    current_b = current_b->GetParentCollection();
  }

  return nullptr;
}

}  // namespace

namespace tabs {

TabStripCollection::TabStripCollection(bool send_notifications_immediately)
    : TabCollection(
          TabCollection::Type::TABSTRIP,
          {TabCollection::Type::PINNED, TabCollection::Type::UNPINNED},
          /*supports_tabs=*/false,
          send_notifications_immediately) {
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

  if (new_group_id.has_value()) {
    CHECK(GetTabGroupCollection(new_group_id.value()));
  }

  TabCollection::Position insertion_details =
      GetInsertionDetails(index, new_pinned_state, new_group_id);

  AddTabImpl(std::move(tab), insertion_details);
}

void TabStripCollection::MoveTabRecursive(
    size_t initial_index,
    size_t final_index,
    std::optional<tab_groups::TabGroupId> new_group_id,
    bool new_pinned_state) {
  CHECK(initial_index >= 0 && final_index >= 0);

  TabInterface* tab = GetTabAtIndexRecursive(initial_index);
  const std::optional<tab_groups::TabGroupId> old_group = tab->GetGroup();

  const std::vector<int> tab_indices = {static_cast<int>(initial_index)};

  std::set<tabs::TabCollection::Type> retain_collection_types;

  if (old_group.has_value() && old_group == new_group_id) {
    TabGroupTabCollection* old_group_collection =
        GetTabGroupCollection(old_group.value());

    // If we are moving the only tab in a group then treat this as a group move.
    if (old_group_collection->TabCountRecursive() == 1) {
      retain_collection_types.insert(
          {tabs::TabCollection::Type::SPLIT, tabs::TabCollection::Type::GROUP});
      new_group_id = std::nullopt;
    }
  }

  MoveTabsRecursive({tab_indices}, final_index, new_group_id, new_pinned_state,
                    retain_collection_types);
}

void TabStripCollection::MoveTabsRecursive(
    const std::vector<int>& tab_indices,
    size_t destination_index,
    std::optional<tab_groups::TabGroupId> new_group_id,
    bool new_pinned_state,
    const std::set<TabCollection::Type>& retain_collection_types) {
  CHECK(destination_index >= 0);
  ChildrenPtrs tab_or_collections =
      GetTabsAndCollectionsForMove(tab_indices, retain_collection_types);

  // If new group collection is created, we can find the first position of the
  // tab as though it was moving to the unpinned container and then update the
  // positions.
  TabCollection::Position move_position = GetMovePosition(
      tab_or_collections, destination_index, new_group_id, new_pinned_state);
  if (new_group_id.has_value() &&
      !GetTabGroupCollection(new_group_id.value())) {
    TabCollection::Position group_insertion_details = move_position;

    AddTabCollectionImpl(PopDetachedGroupCollection(new_group_id.value()),
                         group_insertion_details);
    move_position = {GetTabGroupCollection(new_group_id.value())->GetHandle(),
                     0};
  }

  for (auto& tab_or_collection : tab_or_collections) {
    TabCollection* src_parent_collection = nullptr;
    if (std::holds_alternative<TabInterface*>(tab_or_collection)) {
      TabInterface* tab_ptr = std::get<TabInterface*>(tab_or_collection);
      src_parent_collection = tab_ptr->GetParentCollection(GetPassKey());
      MoveTabImpl(tab_ptr, move_position);
    } else {
      TabCollection* collection_ptr =
          std::get<TabCollection*>(tab_or_collection);
      src_parent_collection = collection_ptr->GetParentCollection();
      MoveCollectionImpl(collection_ptr, move_position);
    }

    // Remove previous collection if needed. This also notifies the
    // collection is removed.
    if (src_parent_collection != unpinned_collection_ &&
        src_parent_collection != pinned_collection_ &&
        src_parent_collection->TabCountRecursive() == 0) {
      RemoveTabCollectionImpl(src_parent_collection);
    }

    // Update the move position to right after where the previous node is.
    move_position = GetNodePosition(tab_or_collection);
    move_position.index += 1;
  }
}

TabCollection::Position TabStripCollection::GetMovePosition(
    ChildrenPtrs tab_or_collections,
    size_t destination_index,
    std::optional<tab_groups::TabGroupId> new_group_id,
    bool new_pinned_state) {
  // Calculate destination collection.
  TabCollection* dst_collection = nullptr;
  if (new_pinned_state) {
    dst_collection = pinned_collection_;
  } else if (new_group_id.has_value() &&
             group_mapping_.contains(new_group_id.value())) {
    dst_collection = GetTabGroupCollection(new_group_id.value());
  } else {
    dst_collection = unpinned_collection_;
  }

  // Create a set for tab or collection.
  std::set<tabs::TabInterface*> tabs_moved;
  std::set<tabs::TabCollection*> collections_moved;
  for (const auto& tab_or_collection : tab_or_collections) {
    if (std::holds_alternative<TabInterface*>(tab_or_collection)) {
      tabs_moved.insert(std::get<TabInterface*>(tab_or_collection));
    } else {
      TabCollection* collection_ptr =
          std::get<TabCollection*>(tab_or_collection);
      collections_moved.insert(collection_ptr);
    }
  }

  // Find the move position. curr_index is updated by reference.
  size_t curr_insertion_index = 0;
  std::optional<TabCollection::Position> move_position =
      FindMovePositionRecursive(destination_index, dst_collection,
                                curr_insertion_index, tabs_moved,
                                collections_moved);

  return move_position.value();
}

ChildrenPtrs TabStripCollection::GetTabsAndCollectionsForMove(
    const std::vector<int>& tab_indices,
    const std::set<TabCollection::Type>& retain_collection_types) {
  std::set<const TabInterface*> selected_tabs;
  for (int index : tab_indices) {
    selected_tabs.insert(GetTabAtIndexRecursive(index));
  }

  // Contains set of all the collections fully covered by `tab_indices`. This
  // does not include `pinned_collection_` or `unpinned_collection_` as they
  // cannot be moved.
  std::set<const TabCollection*> selected_collections;

  if (retain_collection_types.contains(TabCollection::Type::GROUP)) {
    for (const auto& [group_id_, group_collection] : group_mapping_) {
      bool fully_selected = true;
      for (const TabInterface* tab : *group_collection) {
        if (!selected_tabs.contains(tab)) {
          fully_selected = false;
          break;
        }
      }

      if (fully_selected) {
        selected_collections.insert(group_collection);
      }
    }
  }

  if (retain_collection_types.contains(TabCollection::Type::SPLIT)) {
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

std::unique_ptr<TabInterface> TabStripCollection::RemoveTabAtIndexRecursive(
    size_t index) {
  TabInterface* tab_to_be_removed = GetTabAtIndexRecursive(index);
  TabCollection* parent_collection =
      tab_to_be_removed->GetParentCollection(GetPassKey());
  CHECK(parent_collection);

  // If it is the only tab in the group remove the collection.
  if (parent_collection != unpinned_collection_ &&
      parent_collection != pinned_collection_ &&
      parent_collection->TabCountRecursive() == 1) {
    RemoveTabCollectionImpl(parent_collection);
    return parent_collection->MaybeRemoveTab(tab_to_be_removed);
  } else {
    return RemoveTabImpl(tab_to_be_removed);
  }
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

void TabStripCollection::InsertTabCollectionAt(
    std::unique_ptr<TabCollection> collection,
    int index,
    int pinned,
    std::optional<tab_groups::TabGroupId> parent_group) {
  TabCollection::Position insertion_details =
      GetInsertionDetails(index, pinned, parent_group);

  CHECK(insertion_details.parent_handle.Get());
  AddTabCollectionImpl(std::move(collection), insertion_details);
}

std::unique_ptr<TabCollection> TabStripCollection::RemoveTabCollection(
    TabCollection* collection) {
  // Removed collection can be nullptr in the case of group. Return from
  // detached collection instead in that case.
  std::unique_ptr<TabCollection> removed_collection =
      RemoveTabCollectionImpl(collection);

  if (collection->type() == TabCollection::Type::GROUP) {
    return PopDetachedGroupCollection(
        static_cast<TabGroupTabCollection*>(collection)->GetTabGroupId());
  } else {
    CHECK(removed_collection);
    return removed_collection;
  }
}

void TabStripCollection::CreateTabGroup(
    std::unique_ptr<tabs::TabGroupTabCollection> tab_group_collection) {
  CHECK(tab_group_collection);
  detached_group_collections_.push_back(std::move(tab_group_collection));
}

TabGroupTabCollection* TabStripCollection::GetTabGroupCollection(
    tab_groups::TabGroupId group_id) {
  if (auto it = group_mapping_.find(group_id); it != group_mapping_.end()) {
    return it->second;
  }
  return nullptr;
}

std::vector<tab_groups::TabGroupId> TabStripCollection::GetAllTabGroupIds()
    const {
  std::vector<tab_groups::TabGroupId> group_ids;
  group_ids.reserve(group_mapping_.size());
  for (const auto& pair : group_mapping_) {
    group_ids.push_back(pair.first);
  }

  return group_ids;
}

void TabStripCollection::CloseDetachedTabGroup(
    const tab_groups::TabGroupId& group_id) {
  PopDetachedGroupCollection(group_id).reset();
}

TabGroupTabCollection* TabStripCollection::GetDetachedTabGroup(
    const tab_groups::TabGroupId& group_id) {
  auto it = std::find_if(
      detached_group_collections_.begin(), detached_group_collections_.end(),
      [group_id](const std::unique_ptr<TabGroupTabCollection>& collection) {
        return collection->GetTabGroupId() == group_id;
      });
  if (it == detached_group_collections_.end()) {
    return nullptr;
  }
  return it->get();
}

SplitTabCollection* TabStripCollection::GetSplitTabCollection(
    split_tabs::SplitTabId split_id) {
  if (auto it = split_mapping_.find(split_id); it != split_mapping_.end()) {
    return it->second;
  }
  return nullptr;
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

  // Create a new split.
  std::unique_ptr<SplitTabCollection> split =
      std::make_unique<SplitTabCollection>(split_id, visual_data);
  SplitTabCollection* split_ptr = split.get();
  TabCollection::Position insertion_details = {parent_collection->GetHandle(),
                                               dst_index};
  AddTabCollectionImpl(std::move(split), insertion_details);

  // Move tabs to the split.
  size_t insertion_index = 0;
  for (TabInterface* tab : tabs) {
    TabCollection::Position tab_move_details = {split_ptr->GetHandle(),
                                                insertion_index};
    MoveTabImpl(tab, tab_move_details);
    insertion_index += 1;
  }
}

void TabStripCollection::Unsplit(split_tabs::SplitTabId split_id) {
  SplitTabCollection* split = GetSplitTabCollection(split_id);
  if (!split) {
    return;
  }

  CHECK(split_mapping_.contains(split_id));
  TabCollection* parent_collection = split->GetParentCollection();
  size_t dst_index = parent_collection->GetIndexOfCollection(split).value();
  std::vector<TabInterface*> tabs = split->GetTabsRecursive();

  // Move tabs to the parent collection.
  size_t move_index = dst_index;
  for (TabInterface* tab : tabs) {
    TabCollection::Position tab_move_details = {parent_collection->GetHandle(),
                                                move_index};
    MoveTabImpl(tab, tab_move_details);
    move_index += 1;
  }

  RemoveTabCollectionImpl(split);
}

std::set<split_tabs::SplitTabId> TabStripCollection::ListSplits() const {
  std::set<split_tabs::SplitTabId> splits;
  for (const auto& [split_id, _] : split_mapping_) {
    splits.insert(split_id);
  }
  return splits;
}

void TabStripCollection::ValidateData() const {
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

std::optional<const tab_groups::TabGroupId> TabStripCollection::FindGroupIdFor(
    const tabs::TabCollection::Handle& collection_handle,
    base::PassKey<TabStripModel>) const {
  for (auto& pair : group_mapping_) {
    if (pair.second->GetHandle() == collection_handle) {
      return pair.first;
    }
  }

  return std::nullopt;
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
    CHECK(group_mapping_.erase(group_collection->GetTabGroupId()));

    for (const tabs::TabInterface* tab : *group_collection) {
      if (tab->IsSplit()) {
        split_mapping_.erase(tab->GetSplit().value());
      }
    }

  } else if (root_collection->type() == TabCollection::Type::SPLIT) {
    SplitTabCollection* split_collection =
        static_cast<SplitTabCollection*>(root_collection);
    CHECK(split_mapping_.erase(split_collection->GetSplitTabId()));
  }
}

void TabStripCollection::AddTabImpl(std::unique_ptr<TabInterface> tab,
                                    const TabCollection::Position& position) {
  auto [tab_collection_handle, insert_index] = position;
  TabCollection* tab_collection_ptr = tab_collection_handle.Get();

  TabInterface* tab_ptr =
      tab_collection_ptr->AddTab(std::move(tab), insert_index);

  TabCollectionNodes handles_added;
  handles_added.push_back(tab_ptr->GetHandle());

  tab_collection_ptr->NotifyOnChildrenAdded(GetPassKey(), handles_added,
                                            position, nullptr, false);
}

void TabStripCollection::AddTabCollectionImpl(
    std::unique_ptr<TabCollection> collection,
    const TabCollection::Position& position) {
  TabCollection* collection_ptr = collection.get();
  AddCollectionMapping(collection_ptr);

  auto [tab_collection_handle, insert_index] = position;
  TabCollection* parent_collection_ptr = tab_collection_handle.Get();

  parent_collection_ptr->AddCollection(std::move(collection), insert_index);

  TabCollectionNodes handles_added;
  handles_added.push_back(collection_ptr->GetHandle());

  // Signal whether the collection is an empty collection or a detached
  // collection.
  parent_collection_ptr->NotifyOnChildrenAdded(
      GetPassKey(), handles_added, position, nullptr,
      collection_ptr->TabCountRecursive() > 0);
}

std::unique_ptr<TabInterface> TabStripCollection::RemoveTabImpl(
    TabInterface* tab) {
  CHECK(tab);

  TabCollection* parent_collection = tab->GetParentCollection(GetPassKey());
  TabCollection::Position position = {
      parent_collection->GetHandle(),
      parent_collection->GetIndexOfTab(tab).value()};

  std::unique_ptr<TabInterface> removed_tab =
      parent_collection->MaybeRemoveTab(tab);

  CHECK(removed_tab);

  parent_collection->NotifyOnChildrenRemoved(
      GetPassKey(), position,
      std::vector{std::variant<tabs::TabCollectionHandle, tabs::TabHandle>{
          removed_tab->GetHandle()}},
      nullptr);

  return removed_tab;
}

std::unique_ptr<TabCollection> TabStripCollection::RemoveTabCollectionImpl(
    TabCollection* collection) {
  TabCollectionHandle collection_handle = collection->GetHandle();
  TabCollection* parent_collection = collection->GetParentCollection();
  TabCollection::Position position = {
      parent_collection->GetHandle(),
      parent_collection->GetIndexOfCollection(collection).value()};

  RemoveCollectionMapping(collection);
  std::unique_ptr<TabCollection> removed_collection =
      parent_collection->MaybeRemoveCollection(collection);

  // In the case of group return null and store it in
  // `detached_group_collections_` instead.
  if (removed_collection->type() == TabCollection::Type::GROUP) {
    detached_group_collections_.push_back(base::WrapUnique(
        static_cast<TabGroupTabCollection*>(removed_collection.release())));
  }

  parent_collection->NotifyOnChildrenRemoved(
      GetPassKey(), position, NodeHandles{collection_handle}, nullptr);
  return removed_collection;
}

void TabStripCollection::MoveTabImpl(TabInterface* tab_ptr,
                                     TabCollection::Position& position) {
  TabCollection* src_parent_collection =
      tab_ptr->GetParentCollection(GetPassKey());
  TabCollection::Position src_details{
      src_parent_collection->GetHandle(),
      src_parent_collection->GetIndexOfTab(tab_ptr).value()};

  TabCollectionNodes handles;
  handles.push_back(tab_ptr->GetHandle());

  TabCollection* dst_parent_collection = position.parent_handle.Get();

  // If the tab and the position are tied to the same direct parent then we need
  // might need to update the position index.
  if (src_parent_collection == dst_parent_collection &&
      src_parent_collection->GetIndexOfTab(tab_ptr).value() < position.index) {
    position.index -= 1;
  }

  std::unique_ptr<TabInterface> removed_tab =
      src_parent_collection->MaybeRemoveTab(tab_ptr);

  dst_parent_collection->AddTab(std::move(removed_tab), position.index);

  // Notify removes,add and moves based on the common ancestor.
  TabCollection* common_ancestor =
      GetCommonAncestor(src_parent_collection, dst_parent_collection);

  src_parent_collection->NotifyOnChildrenRemoved(GetPassKey(), src_details,
                                                 handles, common_ancestor);

  dst_parent_collection->NotifyOnChildrenAdded(GetPassKey(), handles, position,
                                               common_ancestor, false);

  common_ancestor->NotifyOnChildMoved(GetPassKey(), handles[0], src_details,
                                      position, nullptr);
}

void TabStripCollection::MoveCollectionImpl(TabCollection* collection_ptr,
                                            TabCollection::Position& position) {
  TabCollection* src_parent_collection = collection_ptr->GetParentCollection();
  TabCollection::Position src_details{
      src_parent_collection->GetHandle(),
      src_parent_collection->GetIndexOfCollection(collection_ptr).value()};

  TabCollectionNodes handles;
  handles.push_back(collection_ptr->GetHandle());

  TabCollection* dst_parent_collection = position.parent_handle.Get();

  // If the collection and the position are tied to the same direct parent then
  // we need might need to update the position index.
  if (src_parent_collection == dst_parent_collection &&
      src_parent_collection->GetIndexOfCollection(collection_ptr).value() <
          position.index) {
    position.index -= 1;
  }
  std::unique_ptr<TabCollection> removed_collection =
      src_parent_collection->MaybeRemoveCollection(collection_ptr);

  dst_parent_collection->AddCollection(std::move(removed_collection),
                                       position.index);

  // Notify removes,add and moves based on the common ancestor.
  TabCollection* common_ancestor =
      GetCommonAncestor(src_parent_collection, dst_parent_collection);

  src_parent_collection->NotifyOnChildrenRemoved(GetPassKey(), src_details,
                                                 handles, common_ancestor);

  dst_parent_collection->NotifyOnChildrenAdded(
      GetPassKey(), handles, position, common_ancestor,
      (collection_ptr->TabCountRecursive() > 0) ? true : false);

  common_ancestor->NotifyOnChildMoved(GetPassKey(), handles[0], src_details,
                                      position, nullptr);
}

TabCollection::Position TabStripCollection::GetInsertionDetails(
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

    if (group_collection->TabCountRecursive() == 0) {
      // Group has been created but not yet populated.
      direct_dst_index = 0;
    } else {
      size_t offset =
          GetIndexOfTabRecursive(group_collection->GetTabAtIndexRecursive(0))
              .value();

      direct_dst_index = group_collection->ToDirectIndex(index - offset);
    }
    insert_collection = group_collection;
  } else {
    size_t offset = pinned_collection_->TabCountRecursive();
    direct_dst_index = unpinned_collection_->ToDirectIndex(index - offset);
    insert_collection = unpinned_collection_;
  }

  return TabCollection::Position(insert_collection->GetHandle(),
                                 direct_dst_index);
}

TabCollection::Position TabStripCollection::GetNodePosition(
    ChildPtr tab_or_collection) {
  TabCollection::Position node_position;
  if (std::holds_alternative<TabInterface*>(tab_or_collection)) {
    TabInterface* tab_ptr = std::get<TabInterface*>(tab_or_collection);
    TabCollection* src_parent_collection =
        tab_ptr->GetParentCollection(GetPassKey());
    return TabCollection::Position{
        src_parent_collection->GetHandle(),
        src_parent_collection->GetIndexOfTab(tab_ptr).value()};
  } else {
    TabCollection* collection_ptr = std::get<TabCollection*>(tab_or_collection);
    TabCollection* src_parent_collection =
        collection_ptr->GetParentCollection();
    return TabCollection::Position{
        src_parent_collection->GetHandle(),
        src_parent_collection->GetIndexOfCollection(collection_ptr).value()};
  }
}

}  // namespace tabs
