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
  for (const auto& tab : before.saved_tabs()) {
    if (!after.ContainsTab(tab.saved_tab_guid())) {
      removed_tabs.emplace_back(tab);
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
  if (!is_initialized_) {
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

  if (source == tab_groups::TriggerSource::LOCAL) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupAdded(last_known_tab_groups_.at(group.saved_guid()));
  }
}

void TabGroupChangeNotifierImpl::OnTabGroupUpdated(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  if (!is_initialized_) {
    return;
  }
  if (!group.is_shared_tab_group()) {
    return;
  }
  auto group_it = last_known_tab_groups_.find(group.saved_guid());
  if (group_it == last_known_tab_groups_.end()) {
    // We do not know what changed in the case where we got an update for
    // something unknown, so store the new value and tell our observers it was
    // added if this was not local.
    last_known_tab_groups_.emplace(group.saved_guid(), group);
    if (source == tab_groups::TriggerSource::LOCAL) {
      return;
    }
    for (auto& observer : observers_) {
      observer.OnTabGroupAdded(last_known_tab_groups_.at(group.saved_guid()));
    }
    return;
  }

  // Create a copy of the old group and store the new one.
  tab_groups::SavedTabGroup last_known_group = group_it->second;
  last_known_tab_groups_.insert_or_assign(group.saved_guid(), group);

  if (source == tab_groups::TriggerSource::LOCAL) {
    return;
  }

  ProcessTabGroupUpdates(last_known_group, group);
}

void TabGroupChangeNotifierImpl::OnTabGroupRemoved(
    const base::Uuid& sync_id,
    tab_groups::TriggerSource source) {
  if (!is_initialized_) {
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

  if (source == tab_groups::TriggerSource::LOCAL) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupRemoved(removed_group);
  }
}

void TabGroupChangeNotifierImpl::OnTabSelected(
    const std::optional<base::Uuid>& sync_tab_group_id,
    const std::optional<base::Uuid>& sync_tab_id) {
  if (!is_initialized_) {
    return;
  }

  std::optional<tab_groups::SavedTabGroupTab> selected_tab =
      GetSelectedSharedTabForPublishing(sync_tab_group_id, sync_tab_id);

  for (auto& observer : observers_) {
    observer.OnTabSelected(selected_tab);
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
      observer.OnTabGroupAdded(current_tab_groups.at(guid));
    }
  }

  // Publish removed tab groups.
  for (const auto& group : removed_tab_groups) {
    for (auto& observer : observers_) {
      observer.OnTabGroupRemoved(group);
    }
  }

  // Process all metadata and tab updates within updated groups.
  for (const tab_groups::SavedTabGroup& old_group : updated_groups) {
    ProcessTabGroupUpdates(old_group,
                           current_tab_groups.at(old_group.saved_guid()));
  }
}

void TabGroupChangeNotifierImpl::ProcessTabGroupUpdates(
    const tab_groups::SavedTabGroup& before,
    const tab_groups::SavedTabGroup& after) {
  if (!HasEqualTitle(before, after)) {
    for (auto& observer : observers_) {
      observer.OnTabGroupNameUpdated(after);
    }
  }
  if (!HasEqualColor(before, after)) {
    for (auto& observer : observers_) {
      observer.OnTabGroupColorUpdated(after);
    }
  }

  std::vector<tab_groups::SavedTabGroupTab> added_tabs =
      GetAddedTabs(before, after);
  if (added_tabs.size() > 0) {
    for (auto& observer : observers_) {
      for (auto& tab : added_tabs) {
        observer.OnTabAdded(tab);
      }
    }
  }

  std::vector<tab_groups::SavedTabGroupTab> removed_tabs =
      GetRemovedTabs(before, after);
  if (removed_tabs.size() > 0) {
    for (auto& observer : observers_) {
      for (auto& tab : removed_tabs) {
        observer.OnTabRemoved(tab);
      }
    }
  }

  std::vector<tab_groups::SavedTabGroupTab> updated_tabs =
      GetUpdatedTabs(before, after);
  if (updated_tabs.size() > 0) {
    for (auto& observer : observers_) {
      for (auto& tab : updated_tabs) {
        observer.OnTabUpdated(tab);
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
