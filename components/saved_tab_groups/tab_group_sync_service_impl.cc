// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/tab_group_sync_service_impl.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/pref_names.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/saved_tab_groups/shared_tab_group_data_sync_bridge.h"
#include "components/saved_tab_groups/stats.h"
#include "components/saved_tab_groups/sync_data_type_configuration.h"
#include "components/saved_tab_groups/tab_group_sync_metrics_logger.h"
#include "components/saved_tab_groups/types.h"
#include "components/saved_tab_groups/utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_controller_delegate.h"

namespace tab_groups {
namespace {
constexpr base::TimeDelta kDelayBeforeMetricsLogged = base::Seconds(10);

}  // namespace

TabGroupSyncServiceImpl::TabGroupSyncServiceImpl(
    std::unique_ptr<SavedTabGroupModel> model,
    std::unique_ptr<SyncDataTypeConfiguration> saved_tab_group_configuration,
    std::unique_ptr<SyncDataTypeConfiguration> shared_tab_group_configuration,
    PrefService* pref_service,
    std::unique_ptr<TabGroupSyncMetricsLogger> metrics_logger)
    : model_(std::move(model)),
      sync_bridge_mediator_(model_.get(),
                            pref_service,
                            std::move(saved_tab_group_configuration),
                            std::move(shared_tab_group_configuration)),
      pref_service_(pref_service),
      metrics_logger_(std::move(metrics_logger)) {
  model_->AddObserver(this);
}

TabGroupSyncServiceImpl::~TabGroupSyncServiceImpl() {
  model_->RemoveObserver(this);
}

void TabGroupSyncServiceImpl::SetCoordinator(
    std::unique_ptr<TabGroupSyncCoordinator> coordinator) {
  CHECK(!coordinator_);
  coordinator_ = std::move(coordinator);
  if (IsTabGroupSyncCoordinatorEnabled()) {
    AddObserver(coordinator_.get());
  }
}

std::unique_ptr<ScopedLocalObservationPauser>
TabGroupSyncServiceImpl::CreateScopedLocalObserverPauser() {
  return coordinator_->CreateScopedLocalObserverPauser();
}

void TabGroupSyncServiceImpl::AddObserver(
    TabGroupSyncService::Observer* observer) {
  observers_.AddObserver(observer);

  // If the observer is added late and missed the init signal, send the signal
  // now.
  if (is_initialized_) {
    observer->OnInitialized();
  }
}

void TabGroupSyncServiceImpl::RemoveObserver(
    TabGroupSyncService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void TabGroupSyncServiceImpl::Shutdown() {
  metrics_logger_.reset();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabGroupSyncServiceImpl::GetSavedTabGroupControllerDelegate() {
  return sync_bridge_mediator_.GetSavedTabGroupControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabGroupSyncServiceImpl::GetSharedTabGroupControllerDelegate() {
  return sync_bridge_mediator_.GetSharedTabGroupControllerDelegate();
}

void TabGroupSyncServiceImpl::AddGroup(SavedTabGroup group) {
  VLOG(2) << __func__;
  base::Uuid group_id = group.saved_guid();
  LocalTabGroupID local_group_id = group.local_group_id().value();
  group.SetCreatedBeforeSyncingTabGroups(
      !sync_bridge_mediator_.IsSavedBridgeSyncing());
  group.SetCreatorCacheGuid(
      sync_bridge_mediator_.GetLocalCacheGuidForSavedBridge());
  model_->Add(std::move(group));

  LogEvent(TabGroupEvent::kTabGroupCreated, local_group_id);
}

void TabGroupSyncServiceImpl::RemoveGroup(const LocalTabGroupID& local_id) {
  VLOG(2) << __func__;

  auto* group = model_->Get(local_id);
  if (!group) {
    return;
  }

  base::Uuid sync_id = group->saved_guid();
  LogEvent(TabGroupEvent::kTabGroupRemoved, local_id);
  model_->Remove(local_id);
}

void TabGroupSyncServiceImpl::RemoveGroup(const base::Uuid& sync_id) {
  VLOG(2) << __func__;
  // TODO(shaktisahu): Provide LogEvent API to work with sync ID.
  model_->Remove(sync_id);
}

void TabGroupSyncServiceImpl::UpdateVisualData(
    const LocalTabGroupID local_group_id,
    const tab_groups::TabGroupVisualData* visual_data) {
  VLOG(2) << __func__;
  model_->UpdateVisualData(local_group_id, visual_data);
  UpdateAttributions(local_group_id);
  LogEvent(TabGroupEvent::kTabGroupVisualsChanged, local_group_id,
           std::nullopt);
  stats::RecordTabGroupVisualsMetrics(visual_data);
}

void TabGroupSyncServiceImpl::UpdateGroupPosition(
    const base::Uuid& sync_id,
    std::optional<bool> is_pinned,
    std::optional<int> new_index) {
  VLOG(2) << __func__;

  std::optional<SavedTabGroup> group = GetGroup(sync_id);
  if (!group.has_value()) {
    return;
  }

  if (is_pinned.has_value() && group->is_pinned() != is_pinned) {
    model_->TogglePinState(sync_id);
  }

  if (new_index.has_value()) {
    model_->ReorderGroupLocally(sync_id, new_index.value());
  }
}

void TabGroupSyncServiceImpl::AddTab(const LocalTabGroupID& group_id,
                                     const LocalTabID& tab_id,
                                     const std::u16string& title,
                                     GURL url,
                                     std::optional<size_t> position) {
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    LOG(WARNING) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const auto* tab = group->GetTab(tab_id);
  if (tab) {
    LOG(WARNING) << __func__ << " Called for a tab that already exists";
    return;
  }

  SavedTabGroupTab new_tab(url, title, group->saved_guid(), position,
                           /*saved_tab_guid=*/std::nullopt, tab_id);
  new_tab.SetCreatorCacheGuid(
      sync_bridge_mediator_.GetLocalCacheGuidForSavedBridge());

  UpdateAttributions(group_id);
  model_->UpdateLastUserInteractionTimeLocally(group_id);
  model_->AddTabToGroupLocally(group->saved_guid(), std::move(new_tab));
  LogEvent(TabGroupEvent::kTabAdded, group_id, std::nullopt);
}

void TabGroupSyncServiceImpl::UpdateTab(
    const LocalTabGroupID& group_id,
    const LocalTabID& tab_id,
    const SavedTabGroupTabBuilder& tab_builder) {
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    LOG(WARNING) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const auto* tab = group->GetTab(tab_id);
  if (!tab) {
    LOG(WARNING) << __func__ << " Called for a tab that doesn't exist";
    return;
  }

  // Update attributions for the tab first.
  UpdateAttributions(group_id, tab_id);

  // Use the builder to create the updated tab.
  SavedTabGroupTab updated_tab = tab_builder.Build(*tab);

  model_->UpdateLastUserInteractionTimeLocally(group_id);
  model_->UpdateTabInGroup(group->saved_guid(), std::move(updated_tab));
  LogEvent(TabGroupEvent::kTabNavigated, group_id, tab_id);
}

void TabGroupSyncServiceImpl::RemoveTab(const LocalTabGroupID& group_id,
                                        const LocalTabID& tab_id) {
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    LOG(WARNING) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  auto* tab = group->GetTab(tab_id);
  if (!tab) {
    LOG(WARNING) << __func__ << " Called for a tab that doesn't exist";
    return;
  }

  base::Uuid sync_id = group->saved_guid();
  UpdateAttributions(group_id);
  LogEvent(TabGroupEvent::kTabRemoved, group_id, tab_id);
  model_->UpdateLastUserInteractionTimeLocally(group_id);
  model_->RemoveTabFromGroupLocally(sync_id, tab->saved_tab_guid());
}

void TabGroupSyncServiceImpl::MoveTab(const LocalTabGroupID& group_id,
                                      const LocalTabID& tab_id,
                                      int new_group_index) {
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    LOG(WARNING) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  auto* tab = group->GetTab(tab_id);
  if (!tab) {
    LOG(WARNING) << __func__ << " Called for a tab that doesn't exist";
    return;
  }

  UpdateAttributions(group_id);
  model_->MoveTabInGroupTo(group->saved_guid(), tab->saved_tab_guid(),
                           new_group_index);
  LogEvent(TabGroupEvent::kTabGroupTabsReordered, group_id, std::nullopt);
}

void TabGroupSyncServiceImpl::OnTabSelected(const LocalTabGroupID& group_id,
                                            const LocalTabID& tab_id) {
  VLOG(2) << __func__;
  const SavedTabGroup* group = model_->Get(group_id);
  if (!group) {
    LOG(WARNING) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const SavedTabGroupTab* tab = group->GetTab(tab_id);
  if (!tab) {
    LOG(WARNING) << __func__ << " Called for a tab that doesn't exist";
    return;
  }

  UpdateAttributions(group_id);
  model_->UpdateLastUserInteractionTimeLocally(group_id);
  LogEvent(TabGroupEvent::kTabSelected, group_id, tab_id);
}

void TabGroupSyncServiceImpl::MakeTabGroupShared(
    const LocalTabGroupID& local_group_id,
    std::string_view collaboration_id) {
  model_->MakeTabGroupShared(local_group_id, std::string(collaboration_id));
}

std::vector<SavedTabGroup> TabGroupSyncServiceImpl::GetAllGroups() {
  VLOG(2) << __func__;
  std::vector<SavedTabGroup> non_empty_groups;
  for (const auto& group : model_->saved_tab_groups()) {
    if (group.saved_tabs().empty()) {
      continue;
    }
    non_empty_groups.push_back(group);
  }

  return non_empty_groups;
}

std::optional<SavedTabGroup> TabGroupSyncServiceImpl::GetGroup(
    const base::Uuid& guid) {
  VLOG(2) << __func__;
  const SavedTabGroup* tab_group = model_->Get(guid);
  return tab_group ? std::make_optional<SavedTabGroup>(*tab_group)
                   : std::nullopt;
}

std::optional<SavedTabGroup> TabGroupSyncServiceImpl::GetGroup(
    const LocalTabGroupID& local_id) {
  const SavedTabGroup* tab_group = model_->Get(local_id);
  VLOG(2) << __func__;
  return tab_group ? std::make_optional<SavedTabGroup>(*tab_group)
                   : std::nullopt;
}

std::vector<LocalTabGroupID> TabGroupSyncServiceImpl::GetDeletedGroupIds() {
  return GetDeletedGroupIdsFromPref();
}

void TabGroupSyncServiceImpl::OpenTabGroup(
    const base::Uuid& sync_group_id,
    std::unique_ptr<TabGroupActionContext> context) {
  VLOG(2) << __func__;
  coordinator_->HandleOpenTabGroupRequest(sync_group_id, std::move(context));
}

void TabGroupSyncServiceImpl::UpdateLocalTabGroupMapping(
    const base::Uuid& sync_id,
    const LocalTabGroupID& local_id) {
  VLOG(2) << __func__;
  model_->OnGroupOpenedInTabStrip(sync_id, local_id);
}

void TabGroupSyncServiceImpl::RemoveLocalTabGroupMapping(
    const LocalTabGroupID& local_id) {
  VLOG(2) << __func__;
  RemoveDeletedGroupIdFromPref(local_id);

  auto* group = model_->Get(local_id);
  if (!group) {
    return;
  }

  model_->OnGroupClosedInTabStrip(local_id);
}

void TabGroupSyncServiceImpl::UpdateLocalTabId(
    const LocalTabGroupID& local_group_id,
    const base::Uuid& sync_tab_id,
    const LocalTabID& local_tab_id) {
  VLOG(2) << __func__;
  auto* group = model_->Get(local_group_id);
  CHECK(group);

  const auto* tab = group->GetTab(sync_tab_id);
  CHECK(tab);

  model_->UpdateLocalTabId(group->saved_guid(), *tab, local_tab_id);
}

void TabGroupSyncServiceImpl::ConnectLocalTabGroup(
    const base::Uuid& sync_id,
    const LocalTabGroupID& local_id) {
  VLOG(2) << __func__;
  CHECK(is_initialized_);
  coordinator_->ConnectLocalTabGroup(sync_id, local_id);
}

bool TabGroupSyncServiceImpl::IsRemoteDevice(
    const std::optional<std::string>& cache_guid) const {
  std::optional<std::string> local_cache_guid =
      sync_bridge_mediator_.GetLocalCacheGuidForSavedBridge();
  if (!local_cache_guid || !cache_guid) {
    return false;
  }

  return local_cache_guid.value() != cache_guid.value();
}

void TabGroupSyncServiceImpl::RecordTabGroupEvent(
    const EventDetails& event_details) {
  // Find the group from the passed sync or local ID.
  const SavedTabGroup* group = nullptr;
  if (event_details.local_tab_group_id) {
    group = model_->Get(event_details.local_tab_group_id.value());
  }

  if (!group) {
    LOG(WARNING) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const SavedTabGroupTab* tab = nullptr;
  if (event_details.local_tab_id) {
    tab = group->GetTab(event_details.local_tab_id.value());
  }

  metrics_logger_->LogEvent(event_details, group, tab);
}

void TabGroupSyncServiceImpl::SavedTabGroupAddedFromSync(
    const base::Uuid& guid) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TabGroupSyncServiceImpl::HandleTabGroupAdded,
                                weak_ptr_factory_.GetWeakPtr(), guid,
                                TriggerSource::REMOTE));
}

void TabGroupSyncServiceImpl::SavedTabGroupAddedLocally(
    const base::Uuid& guid) {
  HandleTabGroupAdded(guid, TriggerSource::LOCAL);
}

void TabGroupSyncServiceImpl::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TabGroupSyncServiceImpl::HandleTabGroupUpdated,
                                weak_ptr_factory_.GetWeakPtr(), group_guid,
                                tab_guid, TriggerSource::REMOTE));
}

void TabGroupSyncServiceImpl::SavedTabGroupUpdatedLocally(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  HandleTabGroupUpdated(group_guid, tab_guid, TriggerSource::LOCAL);
}

void TabGroupSyncServiceImpl::SavedTabGroupRemovedFromSync(
    const SavedTabGroup& removed_group) {
  std::pair<base::Uuid, std::optional<LocalTabGroupID>> id_pair;
  id_pair.first = removed_group.saved_guid();
  id_pair.second = removed_group.local_group_id();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TabGroupSyncServiceImpl::HandleTabGroupRemoved,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(id_pair), TriggerSource::REMOTE));
}

void TabGroupSyncServiceImpl::SavedTabGroupRemovedLocally(
    const SavedTabGroup& removed_group) {
  std::pair<base::Uuid, std::optional<LocalTabGroupID>> id_pair;
  id_pair.first = removed_group.saved_guid();
  id_pair.second = removed_group.local_group_id();
  HandleTabGroupRemoved(std::move(id_pair), TriggerSource::LOCAL);
}

void TabGroupSyncServiceImpl::HandleTabGroupAdded(const base::Uuid& guid,
                                                  TriggerSource source) {
  VLOG(2) << __func__;
  const SavedTabGroup* saved_tab_group = model_->Get(guid);

  if (!saved_tab_group) {
    return;
  }

  if (saved_tab_group->saved_tabs().empty()) {
    // Wait for another sync update with tabs before notifying the UI.
    empty_groups_.emplace(guid);
    return;
  }

  if (!is_initialized_) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupAdded(*saved_tab_group, source);
  }
}

void TabGroupSyncServiceImpl::HandleTabGroupUpdated(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid,
    TriggerSource source) {
  VLOG(2) << __func__;
  const SavedTabGroup* saved_tab_group = model_->Get(group_guid);

  if (!saved_tab_group) {
    return;
  }

  if (saved_tab_group->saved_tabs().empty()) {
    return;
  }

  if (base::Contains(empty_groups_, group_guid)) {
    empty_groups_.erase(group_guid);
    HandleTabGroupAdded(group_guid, source);
    return;
  }

  if (!is_initialized_) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupUpdated(*saved_tab_group, source);
  }
}

void TabGroupSyncServiceImpl::HandleTabGroupRemoved(
    std::pair<base::Uuid, std::optional<LocalTabGroupID>> id_pair,
    TriggerSource source) {
  VLOG(2) << __func__;

  if (is_initialized_) {
    for (auto& observer : observers_) {
      observer.OnTabGroupRemoved(id_pair.first, source);
    }
  }

  auto local_id = id_pair.second;
  if (!local_id.has_value()) {
    return;
  }

  // For sync initiated deletions, cache the local ID in prefs until the group
  // is closed in the UI.
  if (source == TriggerSource::REMOTE) {
    AddDeletedGroupIdToPref(local_id.value(), id_pair.first);
  }

  if (!is_initialized_) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupRemoved(local_id.value(), source);
  }
}

std::vector<LocalTabGroupID>
TabGroupSyncServiceImpl::GetDeletedGroupIdsFromPref() {
  std::vector<LocalTabGroupID> deleted_ids;

  ScopedDictPrefUpdate update(pref_service_, prefs::kDeletedTabGroupIds);
  base::Value::Dict& pref_data = update.Get();

  for (const auto [serialized_local_id, serialized_sync_id] : pref_data) {
    auto local_id = LocalTabGroupIDFromString(serialized_local_id);
    DCHECK(local_id.has_value());
    if (!local_id.has_value()) {
      continue;
    }

    deleted_ids.emplace_back(local_id.value());
  }

  return deleted_ids;
}

void TabGroupSyncServiceImpl::AddDeletedGroupIdToPref(
    const LocalTabGroupID& local_id,
    const base::Uuid& sync_id) {
  ScopedDictPrefUpdate update(pref_service_, prefs::kDeletedTabGroupIds);
  update->Set(LocalTabGroupIDToString(local_id), sync_id.AsLowercaseString());
}

void TabGroupSyncServiceImpl::RemoveDeletedGroupIdFromPref(
    const LocalTabGroupID& local_id) {
  ScopedDictPrefUpdate update(pref_service_, prefs::kDeletedTabGroupIds);
  update->Remove(LocalTabGroupIDToString(local_id));
}

void TabGroupSyncServiceImpl::SavedTabGroupLocalIdChanged(
    const base::Uuid& group_guid) {
  VLOG(2) << __func__;
  const SavedTabGroup* saved_tab_group = model_->Get(group_guid);
  CHECK(saved_tab_group);

  if (!is_initialized_) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupUpdated(*saved_tab_group, TriggerSource::LOCAL);
  }
}

void TabGroupSyncServiceImpl::SavedTabGroupModelLoaded() {
  VLOG(2) << __func__;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TabGroupSyncServiceImpl::NotifyServiceInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabGroupSyncServiceImpl::NotifyServiceInitialized() {
  VLOG(2) << __func__;

  is_initialized_ = true;
  for (auto& observer : observers_) {
    observer.OnInitialized();
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TabGroupSyncServiceImpl::RecordMetrics,
                     weak_ptr_factory_.GetWeakPtr()),
      kDelayBeforeMetricsLogged);
}

void TabGroupSyncServiceImpl::UpdateAttributions(
    const LocalTabGroupID& group_id,
    const std::optional<LocalTabID>& tab_id) {
  model_->UpdateLastUpdaterCacheGuidForGroup(
      sync_bridge_mediator_.GetLocalCacheGuidForSavedBridge(), group_id,
      tab_id);
}

void TabGroupSyncServiceImpl::RecordMetrics() {
  auto saved_tab_groups = model_->saved_tab_groups();
  std::vector<bool> is_remote(saved_tab_groups.size());

  for (size_t i = 0; i < saved_tab_groups.size(); ++i) {
    is_remote[i] = IsRemoteDevice(saved_tab_groups[i].creator_cache_guid());
  }

  metrics_logger_->RecordMetricsOnStartup(saved_tab_groups, is_remote);
}

void TabGroupSyncServiceImpl::LogEvent(
    TabGroupEvent event,
    LocalTabGroupID group_id,
    const std::optional<LocalTabID>& tab_id) {
  if (!metrics_logger_) {
    LOG(WARNING) << __func__ << " Metrics logger doesn't exist";
    return;
  }

  const auto* group = model_->Get(group_id);
  if (!group) {
    LOG(WARNING) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const auto* tab =
      tab_id.has_value() ? group->GetTab(tab_id.value()) : nullptr;

  EventDetails event_details(event);
  event_details.local_tab_group_id = group_id;
  event_details.local_tab_id = tab_id;
  metrics_logger_->LogEvent(event_details, group, tab);
}

}  // namespace tab_groups
