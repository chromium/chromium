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

  if (new_group_id.has_value()) {
    CHECK(GetTabGroupCollection(new_group_id.value()));
  }

  std::pair<tabs::TabCollection*, int> insertion_details =
      GetInsertionDetails(index, new_pinned_state, new_group_id);
  auto [tab_collection_ptr, insert_index] = insertion_details;

  TabInterface* tab_ptr =
      tab_collection_ptr->AddTab(std::move(tab), insert_index);

  TabCollectionNodes handles_added;
  handles_added.push_back(tab_ptr->GetHandle());

  tab_collection_ptr->NotifyOnChildrenAdded(GetPassKey(), handles_added,
                                            insertion_details, this);
}

void TabStripCollection::AddTabRecursiveImpl(
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
        RemoveTabRecursiveImpl(tab, old_group != new_group_id);
    AddTabRecursiveImpl(std::move(moved_data), final_index, new_group_id,
                        new_pinned_state);
  }
}

void TabStripCollection::MoveTabsRecursive(
    const std::vector<int>& tab_indices,
    size_t destination_index,
    std::optional<tab_groups::TabGroupId> new_group_id,
    bool new_pinned_state,
    const std::set<TabCollection::Type>& retain_collection_types) {
  CHECK(destination_index >= 0);
  ChildrenVector moved_datas;

  // Removes the tabs and collections needed to be moved.
  for (auto& tab_or_collection :
       GetTabsAndCollectionsForMove(tab_indices, retain_collection_types)) {
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

    if (parent_collection->type() == TabCollection::Type::GROUP) {
      MaybeRemoveGroupCollection(
          static_cast<TabGroupTabCollection*>(parent_collection));
    }
  }

  if (new_group_id.has_value()) {
    MaybeAttachDetachedGroupCollection(destination_index, new_group_id.value());
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
  std::optional<tab_groups::TabGroupId> group_id =
      tab_to_be_removed->GetGroup();
  TabCollection* parent_collection =
      tab_to_be_removed->GetParentCollection(GetPassKey());
  CHECK(parent_collection);
  TabCollection* grandparent_collection =
      parent_collection->GetParentCollection();

  std::unique_ptr<TabInterface> removed_tab =
      RemoveTabRecursiveImpl(tab_to_be_removed);

  if (group_id.has_value()) {
    TabGroupTabCollection* detached_group =
        GetDetachedTabGroup(group_id.value());
    if (detached_group) {
      CHECK(grandparent_collection);
      grandparent_collection->NotifyOnChildrenRemoved(
          GetPassKey(),
          std::vector{std::variant<tabs::TabCollectionHandle, tabs::TabHandle>{
              detached_group->GetHandle()}},
          this);
      return removed_tab;
    }
  }
  parent_collection->NotifyOnChildrenRemoved(
      GetPassKey(),
      std::vector{std::variant<tabs::TabCollectionHandle, tabs::TabHandle>{
          removed_tab->GetHandle()}},
      this);
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

void TabStripCollection::InsertTabCollectionAt(
    std::unique_ptr<TabCollection> collection,
    int index,
    int pinned,
    std::optional<tab_groups::TabGroupId> parent_group) {
  TabCollection* collection_ptr = collection.get();
  AddCollectionMapping(collection_ptr);

  std::pair<tabs::TabCollection*, int> insertion_details =
      GetInsertionDetails(index, pinned, parent_group);
  auto [tab_collection_ptr, insert_index] = insertion_details;

  CHECK(insertion_details.first);
  tab_collection_ptr->AddCollection(std::move(collection), insert_index);

  TabCollectionNodes handles_added;
  handles_added.push_back(collection_ptr->GetHandle());

  tab_collection_ptr->NotifyOnChildrenAdded(GetPassKey(), handles_added,
                                            insertion_details, this);
}

std::unique_ptr<TabCollection> TabStripCollection::RemoveTabCollection(
    TabCollection* collection) {
  TabCollectionHandle collection_handle = collection->GetHandle();
  TabCollection* parent_collection = collection->GetParentCollection();
  std::unique_ptr<TabCollection> removed_collection =
      RemoveTabCollectionImpl(collection);
  parent_collection->NotifyOnChildrenRemoved(
      GetPassKey(), NodeHandles{collection_handle}, this);
  return removed_collection;
}

void TabStripCollection::CreateTabGroup(
    std::unique_ptr<tabs::TabGroupTabCollection> tab_group_collection) {
  CHECK(tab_group_collection);
  detached_group_collections_.push_back(std::move(tab_group_collection));
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
  return RemoveTabCollection(group);
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

void TabStripCollection::MoveTabGroupTo(const tab_groups::TabGroupId& group,
                                        int to_index) {
  tabs::TabGroupTabCollection* group_collection = GetTabGroupCollection(group);

  CHECK(to_index >= static_cast<int>(pinned_collection_->TabCountRecursive()));

  unpinned_collection_->MoveGroupToRecursive(
      to_index - pinned_collection_->TabCountRecursive(), group_collection);
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

  // Move tabs from parent to new split.
  std::unique_ptr<SplitTabCollection> split =
      std::make_unique<SplitTabCollection>(split_id, visual_data);
  for (TabInterface* tab : tabs) {
    split->AddTab(parent_collection->MaybeRemoveTab(tab), split->ChildCount());
  }

  // Insert split back into the parent.
  SplitTabCollection* split_collection_ptr = split.get();
  AddCollectionMapping(split_collection_ptr);
  parent_collection->AddCollection(std::move(split), dst_index);

  // First notify that the collection was added.
  TabCollectionNodes handles_added_split_collection;
  handles_added_split_collection.push_back(split_collection_ptr->GetHandle());
  std::pair<tabs::TabCollection*, int> insertion_details_split_collection =
      std::pair<tabs::TabCollection*, int>(parent_collection, dst_index);
  parent_collection->NotifyOnChildrenAdded(
      GetPassKey(), handles_added_split_collection,
      insertion_details_split_collection, this);

  // Second notify the tabs were added to split.
  TabCollectionNodes handles_added_tabs;
  std::pair<tabs::TabCollection*, int> insertion_details_tabs =
      std::pair<tabs::TabCollection*, int>(split_collection_ptr, 0);
  for (TabInterface* tab : tabs) {
    handles_added_tabs.push_back(tab->GetHandle());
  }

  split_collection_ptr->NotifyOnChildrenAdded(GetPassKey(), handles_added_tabs,
                                              insertion_details_tabs, this);
}

void TabStripCollection::Unsplit(split_tabs::SplitTabId split_id) {
  SplitTabCollection* split = GetSplitTabCollection(split_id);
  if (!split) {
    return;
  }

  CHECK(split_mapping_.contains(split_id));
  TabCollectionHandle split_handle = split->GetHandle();
  TabCollection* parent_collection = split->GetParentCollection();
  size_t dst_index = parent_collection->GetIndexOfCollection(split).value();
  std::vector<TabInterface*> tabs = split->GetTabsRecursive();
  TabCollectionNodes tab_handles;
  for (size_t curr_index = dst_index; TabInterface* tab : tabs) {
    parent_collection->AddTab(split->MaybeRemoveTab(tab), curr_index++);
    tab_handles.push_back(tab->GetHandle());
  }

  // TODO(crbug.com/444287055): Send move notifications for the tabs from split
  // to parent_collection.

  RemoveCollectionMapping(split);
  parent_collection->MaybeRemoveCollection(split).reset();
  parent_collection->NotifyOnChildrenRemoved(GetPassKey(),
                                             NodeHandles{split_handle}, this);
}

std::unique_ptr<TabCollection> TabStripCollection::RemoveSplit(
    SplitTabCollection* split) {
  return RemoveTabCollection(split);
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

std::unique_ptr<TabInterface> TabStripCollection::RemoveTabRecursiveImpl(
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

std::unique_ptr<TabCollection> TabStripCollection::RemoveTabCollectionImpl(
    TabCollection* collection) {
  RemoveCollectionMapping(collection);
  TabCollection* parent_collection = collection->GetParentCollection();
  return parent_collection->MaybeRemoveCollection(collection);
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
            RemoveTabCollectionImpl(group_collection).release())));
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

  return std::pair<tabs::TabCollection*, int>(insert_collection,
                                              direct_dst_index);
}

}  // namespace tabs
