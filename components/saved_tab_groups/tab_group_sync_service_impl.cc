// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/tab_group_sync_service_impl.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/pref_names.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/saved_tab_groups/shared_tab_group_data_sync_bridge.h"
#include "components/saved_tab_groups/stats.h"
#include "components/saved_tab_groups/tab_group_store.h"
#include "components/saved_tab_groups/tab_group_sync_metrics_logger.h"
#include "components/saved_tab_groups/types.h"
#include "components/saved_tab_groups/utils.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace tab_groups {
namespace {
constexpr base::TimeDelta kDelayBeforeMetricsLogged = base::Seconds(10);

}  // namespace

TabGroupSyncServiceImpl::SyncDataTypeConfiguration::SyncDataTypeConfiguration(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> processor,
    syncer::OnceModelTypeStoreFactory store_factory)
    : change_processor(std::move(processor)),
      model_type_store_factory(std::move(store_factory)) {}

TabGroupSyncServiceImpl::SyncDataTypeConfiguration::
    ~SyncDataTypeConfiguration() = default;

TabGroupSyncServiceImpl::TabGroupSyncServiceImpl(
    std::unique_ptr<SavedTabGroupModel> model,
    std::unique_ptr<SyncDataTypeConfiguration> saved_tab_group_configuration,
    std::unique_ptr<SyncDataTypeConfiguration> shared_tab_group_configuration,
    std::unique_ptr<TabGroupStore> tab_group_store,
    PrefService* pref_service,
    std::map<base::Uuid, LocalTabGroupID> migrated_android_local_ids,
    std::unique_ptr<TabGroupSyncMetricsLogger> metrics_logger)
    : model_(std::move(model)),
      saved_bridge_(
          model_.get(),
          std::move(saved_tab_group_configuration->model_type_store_factory),
          std::move(saved_tab_group_configuration->change_processor),
          pref_service,
          std::move(migrated_android_local_ids)),
      tab_group_store_(std::move(tab_group_store)),
      pref_service_(pref_service),
      metrics_logger_(std::move(metrics_logger)) {
  if (shared_tab_group_configuration) {
    shared_bridge_ = std::make_unique<SharedTabGroupDataSyncBridge>(
        model_.get(),
        std::move(shared_tab_group_configuration->model_type_store_factory),
        std::move(shared_tab_group_configuration->change_processor),
        pref_service);
  }
  model_->AddObserver(this);
}

TabGroupSyncServiceImpl::~TabGroupSyncServiceImpl() {
  model_->RemoveObserver(this);
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

base::WeakPtr<syncer::ModelTypeControllerDelegate>
TabGroupSyncServiceImpl::GetSavedTabGroupControllerDelegate() {
  return saved_bridge_.change_processor()->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
TabGroupSyncServiceImpl::GetSharedTabGroupControllerDelegate() {
  if (!shared_bridge_) {
    return base::WeakPtr<syncer::ModelTypeControllerDelegate>();
  }

  return shared_bridge_->change_processor()->GetControllerDelegate();
}

void TabGroupSyncServiceImpl::AddGroup(SavedTabGroup group) {
  VLOG(2) << __func__;
  base::Uuid group_id = group.saved_guid();
  LocalTabGroupID local_group_id = group.local_group_id().value();
  group.SetCreatedBeforeSyncingTabGroups(!saved_bridge_.IsSyncing());
  group.SetCreatorCacheGuid(saved_bridge_.GetLocalCacheGuid());
  model_->Add(std::move(group));

  LogEvent(TabGroupEvent::kTabGroupCreated, local_group_id);
  tab_group_store_->StoreTabGroupIDMetadata(group_id,
                                            TabGroupIDMetadata(local_group_id));
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
  tab_group_store_->DeleteTabGroupIDMetadata(sync_id);
}

void TabGroupSyncServiceImpl::RemoveGroup(const base::Uuid& sync_id) {
  VLOG(2) << __func__;
  // TODO(shaktisahu): Provide LogEvent API to work with sync ID.
  model_->Remove(sync_id);
  tab_group_store_->DeleteTabGroupIDMetadata(sync_id);
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
  new_tab.SetCreatorCacheGuid(saved_bridge_.GetLocalCacheGuid());

  UpdateAttributions(group_id);
  group->SetLastUserInteractionTime(base::Time::Now());
  model_->AddTabToGroupLocally(group->saved_guid(), std::move(new_tab));
  LogEvent(TabGroupEvent::kTabAdded, group_id, std::nullopt);
}

void TabGroupSyncServiceImpl::UpdateTab(const LocalTabGroupID& group_id,
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
  if (!tab) {
    LOG(WARNING) << __func__ << " Called for a tab that doesn't exist";
    return;
  }

  // Update attributions for the tab first.
  UpdateAttributions(group_id, tab_id);

  // Update URL and title for the tab.
  SavedTabGroupTab updated_tab(*tab);
  updated_tab.SetLocalTabID(tab_id);
  updated_tab.SetTitle(title);
  updated_tab.SetURL(url);
  if (position.has_value()) {
    updated_tab.SetPosition(position.value());
  }

  group->SetLastUserInteractionTime(base::Time::Now());
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
  group->SetLastUserInteractionTime(base::Time::Now());
  model_->RemoveTabFromGroupLocally(sync_id, tab->saved_tab_guid());

  // The group might have deleted if this was the last tab, hence we should
  // delete it from mapping store too.
  group = model_->Get(group_id);
  if (!group) {
    tab_group_store_->DeleteTabGroupIDMetadata(sync_id);
  }
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

  UpdateAttributions(group_id);
  model_->UpdateLastUserInteractionTimeLocally(group_id);
  LogEvent(TabGroupEvent::kTabSelected, group_id, tab_id);
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
    LocalTabGroupID& local_id) {
  const SavedTabGroup* tab_group = model_->Get(local_id);
  VLOG(2) << __func__;
  return tab_group ? std::make_optional<SavedTabGroup>(*tab_group)
                   : std::nullopt;
}

std::vector<LocalTabGroupID> TabGroupSyncServiceImpl::GetDeletedGroupIds() {
  if (IsMigrationFromJavaSharedPrefsEnabled()) {
    return GetDeletedGroupIdsFromPref();
  }

  LOG(ERROR) << __func__ << " TabGroupStore is already deprecated";
  std::vector<LocalTabGroupID> deleted_ids;

  // Deleted groups are groups that have been deleted from sync, but we haven't
  // deleted them from mapping, since the local tab group still exists.
  std::set<base::Uuid> ids_from_sync;
  for (const auto& group : model_->saved_tab_groups()) {
    ids_from_sync.insert(group.saved_guid());
  }

  for (const auto& pair : tab_group_store_->GetAllTabGroupIDMetadata()) {
    const base::Uuid& id = pair.first;
    if (base::Contains(ids_from_sync, id)) {
      continue;
    }

    // Model doesn't know about this entry. Hence this is a deleted entry.
    deleted_ids.emplace_back(pair.second.local_tab_group_id);
  }

  return deleted_ids;
}

void TabGroupSyncServiceImpl::UpdateLocalTabGroupMapping(
    const base::Uuid& sync_id,
    const LocalTabGroupID& local_id) {
  VLOG(2) << __func__;
  model_->OnGroupOpenedInTabStrip(sync_id, local_id);
  tab_group_store_->StoreTabGroupIDMetadata(sync_id,
                                            TabGroupIDMetadata(local_id));
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
  tab_group_store_->DeleteTabGroupIDMetadata(group->saved_guid());
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

bool TabGroupSyncServiceImpl::IsRemoteDevice(
    const std::optional<std::string>& cache_guid) const {
  std::optional<std::string> local_cache_guid =
      saved_bridge_.GetLocalCacheGuid();
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
    const SavedTabGroup* removed_group) {
  std::pair<base::Uuid, std::optional<LocalTabGroupID>> id_pair;
  id_pair.first = removed_group->saved_guid();
  id_pair.second = removed_group->local_group_id();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TabGroupSyncServiceImpl::HandleTabGroupRemoved,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(id_pair), TriggerSource::REMOTE));
}

void TabGroupSyncServiceImpl::SavedTabGroupRemovedLocally(
    const SavedTabGroup* removed_group) {
  std::pair<base::Uuid, std::optional<LocalTabGroupID>> id_pair;
  id_pair.first = removed_group->saved_guid();
  id_pair.second = removed_group->local_group_id();
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

  for (auto& observer : observers_) {
    observer.OnTabGroupUpdated(*saved_tab_group, source);
  }
}

void TabGroupSyncServiceImpl::HandleTabGroupRemoved(
    std::pair<base::Uuid, std::optional<LocalTabGroupID>> id_pair,
    TriggerSource source) {
  VLOG(2) << __func__;
  for (auto& observer : observers_) {
    observer.OnTabGroupRemoved(id_pair.first, source);
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
  for (auto& observer : observers_) {
    observer.OnTabGroupUpdated(*saved_tab_group, TriggerSource::LOCAL);
  }
}

void TabGroupSyncServiceImpl::SavedTabGroupModelLoaded() {
  VLOG(2) << __func__;

  tab_group_store_->Initialize(
      base::BindOnce(&TabGroupSyncServiceImpl::OnReadTabGroupStore,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabGroupSyncServiceImpl::OnReadTabGroupStore() {
  VLOG(2) << __func__;

  for (const auto& group : model_->saved_tab_groups()) {
    auto sync_id = group.saved_guid();
    auto id_metadata = tab_group_store_->GetTabGroupIDMetadata(sync_id);
    if (!id_metadata) {
      continue;
    }

    model_->OnGroupOpenedInTabStrip(sync_id, id_metadata->local_tab_group_id);
  }

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
  model_->UpdateLastUpdaterCacheGuidForGroup(saved_bridge_.GetLocalCacheGuid(),
                                             group_id, tab_id);
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
