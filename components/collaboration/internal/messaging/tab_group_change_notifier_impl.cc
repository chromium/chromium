// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/tab_group_change_notifier_impl.h"

#include <unordered_map>

#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "components/collaboration/internal/messaging/tab_group_change_notifier.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"

namespace collaboration::messaging {

namespace {
bool HasEqualTitle(const tab_groups::SavedTabGroup& a,
                   const tab_groups::SavedTabGroup& b) {
  return a.title() == b.title();
}

bool HasEqualColor(const tab_groups::SavedTabGroup& a,
                   const tab_groups::SavedTabGroup& b) {
  return a.color() == b.color();
}

bool IsTabConsideredUpdated(const tab_groups::SavedTabGroupTab& a,
                            const tab_groups::SavedTabGroupTab& b) {
  return a.url() != b.url();
}

std::vector<tab_groups::SavedTabGroupTab> GetAddedTabs(
    const tab_groups::SavedTabGroup& before,
    const tab_groups::SavedTabGroup& after) {
  std::vector<tab_groups::SavedTabGroupTab> added_tabs;
  for (const auto& tab : after.saved_tabs()) {
    if (!before.ContainsTab(tab.saved_tab_guid())) {
      added_tabs.emplace_back(tab);
    }
  }
  return added_tabs;
}

std::vector<tab_groups::SavedTabGroupTab> GetRemovedTabs(
    const tab_groups::SavedTabGroup& before,
    const tab_groups::SavedTabGroup& after) {
  std::vector<tab_groups::SavedTabGroupTab> removed_tabs;
  const std::map<base::Uuid, tab_groups::SavedTabGroup::RemovedTabMetadata>&
      removed_tabs_metadata = after.last_removed_tabs_metadata();
  for (const tab_groups::SavedTabGroupTab& tab : before.saved_tabs()) {
    if (!after.ContainsTab(tab.saved_tab_guid())) {
      removed_tabs.emplace_back(tab);

      if (auto it = removed_tabs_metadata.find(tab.saved_tab_guid());
          it != removed_tabs_metadata.end()) {
        // Copy over metadata for the removed tabs from SavedTabGroup.
        const tab_groups::SavedTabGroup::RemovedTabMetadata& metadata =
            it->second;
        removed_tabs.back().SetUpdatedByAttribution(metadata.removed_by);
        removed_tabs.back().SetUpdateTimeWindowsEpochMicros(
            metadata.removal_time);
      }
    }
  }
  return removed_tabs;
}

std::vector<tab_groups::SavedTabGroupTab> GetUpdatedTabs(
    const tab_groups::SavedTabGroup& before,
    const tab_groups::SavedTabGroup& after) {
  std::vector<tab_groups::SavedTabGroupTab> updated_tabs;
  for (const auto& old_tab : before.saved_tabs()) {
    if (!after.ContainsTab(old_tab.saved_tab_guid())) {
      // Skip if the tab has been removed.
      continue;
    }

    // The tab was contained in the after-version of the group, so we should
    // always be able to retrieve it.
    const tab_groups::SavedTabGroupTab* new_tab =
        after.GetTab(old_tab.saved_tab_guid());
    CHECK(new_tab);

    // This tab has potentially been updated.
    if (IsTabConsideredUpdated(old_tab, *new_tab)) {
      updated_tabs.emplace_back(*new_tab);
    }
  }
  return updated_tabs;
}
}  // namespace

TabGroupChangeNotifierImpl::TabGroupChangeNotifierImpl(
    tab_groups::TabGroupSyncService* tab_group_sync_service)
    : tab_group_sync_service_(tab_group_sync_service) {}

TabGroupChangeNotifierImpl::~TabGroupChangeNotifierImpl() = default;

void TabGroupChangeNotifierImpl::Initialize() {
  tab_group_sync_observer_.Observe(tab_group_sync_service_);
}

void TabGroupChangeNotifierImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TabGroupChangeNotifierImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool TabGroupChangeNotifierImpl::IsInitialized() {
  return is_initialized_;
}

void TabGroupChangeNotifierImpl::OnInitialized() {
  std::unique_ptr<std::vector<tab_groups::SavedTabGroup>> original_tab_groups =
      tab_group_sync_service_
          ->TakeSharedTabGroupsAvailableAtStartupForMessaging();
  if (original_tab_groups) {
    last_known_tab_groups_ = ConvertToMapOfSharedTabGroup(*original_tab_groups);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TabGroupChangeNotifierImpl::
              NotifyTabGroupChangeNotifierInitializedAndProcessChanges,
          weak_ptr_factory_.GetWeakPtr()));
}

void TabGroupChangeNotifierImpl::
    NotifyTabGroupChangeNotifierInitializedAndProcessChanges() {
  is_initialized_ = true;
  for (auto& observer : observers_) {
    observer.OnTabGroupChangeNotifierInitialized();
  }
  ProcessChangesSinceStartup();
}

void TabGroupChangeNotifierImpl::OnTabGroupAdded(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  if (!is_initialized_ || sync_bridge_update_type_ !=
                              tab_groups::SyncBridgeUpdateType::kDefaultState) {
    return;
  }
  if (!group.is_shared_tab_group()) {
    return;
  }
  if (last_known_tab_groups_.find(group.saved_guid()) !=
      last_known_tab_groups_.end()) {
    // Group already found, which is not expected to happen, but it is safe to
    // continue.
  }
  // Always update the local copy, even if the trigger was local.
  last_known_tab_groups_.insert_or_assign(group.saved_guid(), group);

  for (auto& observer : observers_) {
    observer.OnTabGroupAdded(last_known_tab_groups_.at(group.saved_guid()),
                             source);
  }
}

void TabGroupChangeNotifierImpl::OnTabGroupUpdated(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  if (!is_initialized_ || sync_bridge_update_type_ !=
                              tab_groups::SyncBridgeUpdateType::kDefaultState) {
    return;
  }

  // When an incoming sync update is received, it hasn't been processed by tab
  // model UI yet. Delay to post task to make sure tabs are updated with local
  // ID before notifying observers.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TabGroupChangeNotifierImpl::OnTabGroupUpdatedAfterPosted,
                     weak_ptr_factory_.GetWeakPtr(), group.saved_guid(),
                     source));
}

void TabGroupChangeNotifierImpl::OnTabGroupRemoved(
    const base::Uuid& sync_id,
    tab_groups::TriggerSource source) {
  if (!is_initialized_ || sync_bridge_update_type_ !=
                              tab_groups::SyncBridgeUpdateType::kDefaultState) {
    return;
  }
  auto group_it = last_known_tab_groups_.find(sync_id);
  if (group_it == last_known_tab_groups_.end()) {
    // Group already removed or is a saved tab group.
    return;
  }

  // Always remove the local copy, even if the trigger was local.
  tab_groups::SavedTabGroup removed_group = group_it->second;
  last_known_tab_groups_.erase(group_it);

  for (auto& observer : observers_) {
    observer.OnTabGroupRemoved(removed_group, source);
  }
}

void TabGroupChangeNotifierImpl::OnTabSelected(
    const tab_groups::SelectedTabInfo& selected_tab_info) {
  if (!is_initialized_) {
    return;
  }

  std::optional<tab_groups::SavedTabGroupTab> selected_saved_tab =
      GetSelectedSharedTabForPublishing(selected_tab_info.tab_group_id,
                                        selected_tab_info.tab_id);
  if (selected_saved_tab) {
    selected_saved_tab->SetTitle(selected_tab_info.tab_title.value_or(u""));
  }

  for (auto& observer : observers_) {
    observer.OnTabSelected(selected_saved_tab);
  }
}

void TabGroupChangeNotifierImpl::OnTabGroupLocalIdChanged(
    const base::Uuid& sync_id,
    const std::optional<tab_groups::LocalTabGroupID>& local_id) {
  // When tab group local id is changed, tabs are not updating yet.
  // Delay to post task to make sure tabs are
  // updated before notifying observers.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TabGroupChangeNotifierImpl::OnTabGroupOpenedOrClosed,
                     weak_ptr_factory_.GetWeakPtr(), sync_id, local_id));
}

void TabGroupChangeNotifierImpl::OnSyncBridgeUpdateTypeChanged(
    tab_groups::SyncBridgeUpdateType sync_bridge_update_type) {
  sync_bridge_update_type_ = sync_bridge_update_type;
}

void TabGroupChangeNotifierImpl::OnTabGroupOpenedOrClosed(
    const base::Uuid& sync_id,
    const std::optional<tab_groups::LocalTabGroupID>& local_id) {
  std::optional<tab_groups::SavedTabGroup> tab_group =
      tab_group_sync_service_->GetGroup(sync_id);
  if (tab_group) {
    if (local_id.has_value()) {
      for (auto& observer : observers_) {
        observer.OnTabGroupOpened(*tab_group);
      }
    } else {
      for (auto& observer : observers_) {
        observer.OnTabGroupClosed(*tab_group);
      }
    }
  }
}

void TabGroupChangeNotifierImpl::ProcessChangesSinceStartup() {
  std::unordered_map<base::Uuid, tab_groups::SavedTabGroup, base::UuidHash>
      current_tab_groups =
          ConvertToMapOfSharedTabGroup(tab_group_sync_service_->GetAllGroups());

  // Find added tab groups.
  std::vector<base::Uuid> added_tab_groups;
  for (const auto& [guid, group] : current_tab_groups) {
    if (last_known_tab_groups_.find(guid) == last_known_tab_groups_.end()) {
      added_tab_groups.emplace_back(guid);
    }
  }

  // Find updated and removed tab groups.
  std::vector<tab_groups::SavedTabGroup> updated_groups;
  std::vector<tab_groups::SavedTabGroup> removed_tab_groups;
  for (const auto& [guid, group] : last_known_tab_groups_) {
    if (current_tab_groups.find(guid) == current_tab_groups.end()) {
      removed_tab_groups.emplace_back(group);
    } else {
      updated_groups.emplace_back(group);
    }
  }

  last_known_tab_groups_ = current_tab_groups;

  // Publish added tab groups.
  for (const auto& guid : added_tab_groups) {
    for (auto& observer : observers_) {
      observer.OnTabGroupAdded(current_tab_groups.at(guid),
                               tab_groups::TriggerSource::REMOTE);
    }
  }

  // Publish removed tab groups.
  for (const auto& group : removed_tab_groups) {
    for (auto& observer : observers_) {
      observer.OnTabGroupRemoved(group, tab_groups::TriggerSource::REMOTE);
    }
  }

  // Process all metadata and tab updates within updated groups.
  for (const tab_groups::SavedTabGroup& old_group : updated_groups) {
    ProcessTabGroupUpdates(old_group,
                           current_tab_groups.at(old_group.saved_guid()),
                           tab_groups::TriggerSource::REMOTE);
  }
}

void TabGroupChangeNotifierImpl::OnTabGroupUpdatedAfterPosted(
    const base::Uuid& sync_tab_group_id,
    tab_groups::TriggerSource source) {
  std::optional<tab_groups::SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(sync_tab_group_id);
  if (!group || !group->is_shared_tab_group()) {
    return;
  }
  auto group_it = last_known_tab_groups_.find(group->saved_guid());
  if (group_it == last_known_tab_groups_.end()) {
    // We do not know what changed in the case where we got an update for
    // something unknown, so store the new value and tell our observers it was
    // added if this was not local.
    last_known_tab_groups_.emplace(group->saved_guid(), *group);
    for (auto& observer : observers_) {
      observer.OnTabGroupAdded(last_known_tab_groups_.at(group->saved_guid()),
                               source);
    }
    return;
  }

  // Create a copy of the old group and store the new one.
  tab_groups::SavedTabGroup last_known_group = group_it->second;
  last_known_tab_groups_.insert_or_assign(group->saved_guid(), *group);

  ProcessTabGroupUpdates(last_known_group, *group, source);
}

void TabGroupChangeNotifierImpl::ProcessTabGroupUpdates(
    const tab_groups::SavedTabGroup& before,
    const tab_groups::SavedTabGroup& after,
    tab_groups::TriggerSource source) {
  if (!HasEqualTitle(before, after)) {
    for (auto& observer : observers_) {
      observer.OnTabGroupNameUpdated(after, source);
    }
  }
  if (!HasEqualColor(before, after)) {
    for (auto& observer : observers_) {
      observer.OnTabGroupColorUpdated(after, source);
    }
  }

  std::vector<tab_groups::SavedTabGroupTab> added_tabs =
      GetAddedTabs(before, after);
  if (added_tabs.size() > 0) {
    for (auto& observer : observers_) {
      for (auto& tab : added_tabs) {
        observer.OnTabAdded(tab, source);
      }
    }
  }

  std::vector<tab_groups::SavedTabGroupTab> removed_tabs =
      GetRemovedTabs(before, after);
  if (removed_tabs.size() > 0) {
    for (auto& observer : observers_) {
      for (auto& tab : removed_tabs) {
        observer.OnTabRemoved(tab, source);
      }
    }
  }

  std::vector<tab_groups::SavedTabGroupTab> updated_tabs =
      GetUpdatedTabs(before, after);
  if (updated_tabs.size() > 0) {
    for (auto& observer : observers_) {
      for (auto& tab : updated_tabs) {
        observer.OnTabUpdated(tab, source);
      }
    }
  }
}

std::optional<tab_groups::SavedTabGroupTab>
TabGroupChangeNotifierImpl::GetSelectedSharedTabForPublishing(
    const std::optional<base::Uuid>& sync_tab_group_id,
    const std::optional<base::Uuid>& sync_tab_id) {
  if (!sync_tab_group_id || !sync_tab_id) {
    // A tab outside saved / shared tab groups was selected.
    return std::nullopt;
  }
  auto group_it = last_known_tab_groups_.find(sync_tab_group_id.value());
  if (group_it == last_known_tab_groups_.end()) {
    // A tab in a saved (not shared) tab group was selected.
    return std::nullopt;
  }

  // Try to look up live data first, since we are not always updated, e.g. if a
  // local tab ID changes for a particular tab.
  std::optional<tab_groups::SavedTabGroup> tab_group;
  if (sync_tab_group_id) {
    tab_group = tab_group_sync_service_->GetGroup(*sync_tab_group_id);
  }
  if (tab_group) {
    const tab_groups::SavedTabGroupTab* tab = tab_group->GetTab(*sync_tab_id);
    if (tab) {
      return *tab;
    }
  }

  // The tab is in a shared tab group.
  const tab_groups::SavedTabGroup& group = group_it->second;
  const tab_groups::SavedTabGroupTab* tab = group.GetTab(*sync_tab_id);

  if (!tab) {
    // If we are unable to find the tab within our shared tab group, we are
    // unable to tell our observers about which tab was selected, so we would
    // publish std::nullopt in that case.
    return std::nullopt;
  }

  // A tab within a shared tab group was selected.
  return *tab;
}

std::unordered_map<base::Uuid, tab_groups::SavedTabGroup, base::UuidHash>
TabGroupChangeNotifierImpl::ConvertToMapOfSharedTabGroup(
    const std::vector<tab_groups::SavedTabGroup>& groups) {
  std::unordered_map<base::Uuid, tab_groups::SavedTabGroup, base::UuidHash> map;
  for (const auto& tab_group : groups) {
    if (tab_group.is_shared_tab_group()) {
      map.emplace(tab_group.saved_guid(), tab_group);
    }
  }
  return map;
}

}  // namespace collaboration::messaging
