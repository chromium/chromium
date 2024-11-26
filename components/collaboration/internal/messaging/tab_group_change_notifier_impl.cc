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

  if (!HasEqualTitle(last_known_group, group)) {
    for (auto& observer : observers_) {
      observer.OnTabGroupNameUpdated(group);
    }
  }
  if (!HasEqualColor(last_known_group, group)) {
    for (auto& observer : observers_) {
      observer.OnTabGroupColorUpdated(group);
    }
  }

  // TODO(crbug.com/378421557): Handle updated group tabs.
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

  // Find removed tab groups.
  std::vector<tab_groups::SavedTabGroup> removed_tab_groups;
  for (const auto& [guid, group] : last_known_tab_groups_) {
    if (current_tab_groups.find(guid) == current_tab_groups.end()) {
      removed_tab_groups.emplace_back(group);
    }
  }

  // Find groups with updated titles and colors.
  std::vector<tab_groups::SavedTabGroup> tab_groups_title_changed;
  std::vector<tab_groups::SavedTabGroup> tab_groups_color_changed;
  for (const auto& [guid, group] : last_known_tab_groups_) {
    auto current_group_it = current_tab_groups.find(guid);
    if (current_group_it == current_tab_groups.end()) {
      continue;
    }
    if (!HasEqualTitle(group, current_tab_groups.at(guid))) {
      tab_groups_title_changed.emplace_back(current_tab_groups.at(guid));
    }
    if (!HasEqualColor(group, current_tab_groups.at(guid))) {
      tab_groups_color_changed.emplace_back(current_tab_groups.at(guid));
    }
  }

  // TODO(crbug.com/378421557): Handle updated group tabs.

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

  // Publish groups with title changes.
  for (const auto& group : tab_groups_title_changed) {
    for (auto& observer : observers_) {
      observer.OnTabGroupNameUpdated(group);
    }
  }

  // Publish groups with color changes.
  for (const auto& group : tab_groups_color_changed) {
    for (auto& observer : observers_) {
      observer.OnTabGroupColorUpdated(group);
    }
  }
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
