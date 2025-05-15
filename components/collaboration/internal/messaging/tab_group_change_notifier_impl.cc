// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/tab_group_change_notifier_impl.h"

#include <unordered_map>

#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "components/collaboration/internal/messaging/tab_group_change_notifier.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/signin/public/identity_manager/identity_manager.h"

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
    const tab_groups::SavedTabGroup& after,
    tab_groups::TriggerSource source,
    const signin::IdentityManager* identity_manager) {
  std::vector<tab_groups::SavedTabGroupTab> removed_tabs;
  const std::map<base::Uuid, tab_groups::SavedTabGroup::RemovedTabMetadata>&
      removed_tabs_metadata = after.last_removed_tabs_metadata();

  // Find current signed-in user gaia.
  GaiaId account_gaia;
  if (source == tab_groups::TriggerSource::LOCAL) {
    CoreAccountInfo account =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    if (!account.IsEmpty()) {
      account_gaia = account.gaia;
    }
  }

  for (const tab_groups::SavedTabGroupTab& tab : before.saved_tabs()) {
    if (!after.ContainsTab(tab.saved_tab_guid())) {
      removed_tabs.emplace_back(tab);

      // Update user attributions for tab removal since they are still pointing
      // to the last update. Because ProcessTabGroupUpdates() are called on a
      // posted task, don't trust the TriggerSource here. Instead, we should
      // rely on the RemovedTabMetadata to figure out the right attribution.
      if (auto it = removed_tabs_metadata.find(tab.saved_tab_guid());
          it != removed_tabs_metadata.end()) {
        // Copy over metadata for the removed tabs from SavedTabGroup.
        const tab_groups::SavedTabGroup::RemovedTabMetadata& metadata =
            it->second;
        removed_tabs.back().SetUpdatedByAttribution(metadata.removed_by);
        removed_tabs.back().SetUpdateTime(metadata.removal_time);
      } else if (source == tab_groups::TriggerSource::LOCAL) {
        // If it's a local tab removal, it must by by the current signed-in
        // user.
        removed_tabs.back().SetUpdatedByAttribution(account_gaia);
        removed_tabs.back().SetUpdateTime(base::Time::Now());
      }
    }
  }
  return removed_tabs;
}

std::vector<
    std::pair<tab_groups::SavedTabGroupTab, tab_groups::SavedTabGroupTab>>
GetUpdatedTabs(const tab_groups::SavedTabGroup& before,
               const tab_groups::SavedTabGroup& after) {
  std::vector<
      std::pair<tab_groups::SavedTabGroupTab, tab_groups::SavedTabGroupTab>>
      updated_tabs;
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
      // Copy the tab title from the old tab to new tab as it will be used to
      // show instant message about which tab has updated.
      tab_groups::SavedTabGroupTab updated_tab(*new_tab);
      updated_tab.SetTitle(old_tab.title());
      updated_tabs.emplace_back(std::make_pair<>(old_tab, updated_tab));
    }
  }
  return updated_tabs;
}
}  // namespace

TabGroupChangeNotifierImpl::TabGroupChangeNotifierImpl(
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    signin::IdentityManager* identity_manager)
    : tab_group_sync_service_(tab_group_sync_service),
      identity_manager_(identity_manager) {
  CHECK(tab_group_sync_service_);
}

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

void TabGroupChangeNotifierImpl::BeforeTabGroupUpdateFromRemote(
    const base::Uuid& sync_group_id) {
  auto group_it = last_known_tab_groups_.find(sync_group_id);
  if (group_it == last_known_tab_groups_.end()) {
    return;
  }

  // This is an open group that we are aware of, and we have a copy of it in
  // `last_known_tab_groups_`. Let's update the title and focus info for the
  // respective tabs from the local tab model since SavedTabGroupTab doesn't
  // contain this info.
  tab_groups::SavedTabGroup last_known_group = group_it->second;
  for (auto& tab : last_known_group.saved_tabs()) {
    if (!tab.local_tab_id()) {
      continue;
    }

    auto tab_title =
        tab_group_sync_service_->GetTabTitle(tab.local_tab_id().value());
    tab.SetTitle(tab_title);
  }

  last_known_tab_groups_.insert_or_assign(sync_group_id, last_known_group);

  last_selected_tabs_ = tab_group_sync_service_->GetSelectedTabs();

  // Disable listening to tab selection change events because they interfere
  // with the computation of selected tab while sync update is being applied,
  // e.g. a selected tab removal will be perceived as a non-selected tab removal
  // since the tab removal and selection of adjacent tab both happen in the next
  // step. Moreover, this happens in different order on different platforms.
  ignore_tab_selection_events_ = true;
}

void TabGroupChangeNotifierImpl::OnTabGroupUpdated(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  if (!is_initialized_ || sync_bridge_update_type_ !=
                              tab_groups::SyncBridgeUpdateType::kDefaultState) {
    return;
  }

  // We only use this path for local updates. The remote updates are handled in
  // AfterTabGroupUpdateFromRemote.
  if (source != tab_groups::TriggerSource::LOCAL) {
    return;
  }

  last_selected_tabs_ = tab_group_sync_service_->GetSelectedTabs();
  OnTabGroupUpdatedInner(group.saved_guid(), source);
}

void TabGroupChangeNotifierImpl::AfterTabGroupUpdateFromRemote(
    const base::Uuid& sync_group_id) {
  if (!is_initialized_ || sync_bridge_update_type_ !=
                              tab_groups::SyncBridgeUpdateType::kDefaultState) {
    return;
  }

  OnTabGroupUpdatedInner(sync_group_id, tab_groups::TriggerSource::REMOTE);
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
    const std::set<tab_groups::LocalTabID>& selected_tabs) {
  if (!is_initialized_ || ignore_tab_selection_events_) {
    return;
  }

  std::set<tab_groups::LocalTabID> selection_removed;
  std::set<tab_groups::LocalTabID> selection_added;
  for (const auto& tab : last_selected_tabs_) {
    if (!base::Contains(selected_tabs, tab)) {
      selection_removed.insert(tab);
    }
  }
  for (const auto& tab : selected_tabs) {
    if (!base::Contains(last_selected_tabs_, tab)) {
      selection_added.insert(tab);
    }
  }

  last_selected_tabs_ = selected_tabs;

  for (const auto& tab : selection_removed) {
    for (auto& observer : observers_) {
      observer.OnTabSelectionChanged(tab, /*is_selected=*/false);
    }
  }

  for (const auto& tab : selection_added) {
    for (auto& observer : observers_) {
      observer.OnTabSelectionChanged(tab, /*is_selected=*/true);
    }
  }
}

void TabGroupChangeNotifierImpl::OnTabLastSeenTimeChanged(
    const base::Uuid& tab_id,
    tab_groups::TriggerSource source) {
  if (!is_initialized_) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabLastSeenTimeChanged(tab_id, source);
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

  if (sync_bridge_update_type_ ==
      tab_groups::SyncBridgeUpdateType::kDisableSync) {
    for (auto& observer : observers_) {
      observer.OnSyncDisabled();
    }
  }
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

void TabGroupChangeNotifierImpl::OnTabGroupUpdatedInner(
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

  // Reenable listening to tab selection change events. Also notify the
  // observers if the selection was changed in the meawhile.
  ignore_tab_selection_events_ = false;
  OnTabSelected(tab_group_sync_service_->GetSelectedTabs());
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
      GetRemovedTabs(before, after, source, identity_manager_);
  if (removed_tabs.size() > 0) {
    for (auto& observer : observers_) {
      for (auto& tab : removed_tabs) {
        bool is_selected = tab.local_tab_id()
                               ? base::Contains(last_selected_tabs_,
                                                tab.local_tab_id().value())
                               : false;

        observer.OnTabRemoved(tab, source, is_selected);
      }
    }
  }

  std::vector<
      std::pair<tab_groups::SavedTabGroupTab, tab_groups::SavedTabGroupTab>>
      updated_tab_pairs = GetUpdatedTabs(before, after);
  if (updated_tab_pairs.size() > 0) {
    for (auto& observer : observers_) {
      for (auto& [before_tab, after_tab] : updated_tab_pairs) {
        bool is_selected =
            after_tab.local_tab_id()
                ? base::Contains(last_selected_tabs_,
                                 after_tab.local_tab_id().value())
                : false;
        observer.OnTabUpdated(before_tab, after_tab, source, is_selected);
      }
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
