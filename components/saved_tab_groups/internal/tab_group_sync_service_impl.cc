// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/tab_group_sync_service_impl.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/internal/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/internal/shared_tab_group_data_sync_bridge.h"
#include "components/saved_tab_groups/internal/stats.h"
#include "components/saved_tab_groups/internal/sync_data_type_configuration.h"
#include "components/saved_tab_groups/internal/tab_group_sync_bridge_mediator.h"
#include "components/saved_tab_groups/internal/tab_group_sync_coordinator_impl.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_metrics_logger.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/sync/base/account_pref_utils.h"
#include "components/sync/model/data_type_controller_delegate.h"

namespace tab_groups {
namespace {
constexpr base::TimeDelta kDelayBeforeMetricsLogged = base::Seconds(10);

void OnCanApplyOptimizationCompleted(
    TabGroupSyncService::UrlRestrictionCallback callback,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::optional<proto::UrlRestriction> url_restriction;
  if (metadata.any_metadata().has_value()) {
    url_restriction =
        optimization_guide::ParsedAnyMetadata<proto::UrlRestriction>(
            metadata.any_metadata().value());
  }

  std::move(callback).Run(std::move(url_restriction));
}

}  // namespace

TabGroupSyncServiceImpl::TabGroupSyncServiceImpl(
    std::unique_ptr<SavedTabGroupModel> model,
    std::unique_ptr<SyncDataTypeConfiguration> saved_tab_group_configuration,
    std::unique_ptr<SyncDataTypeConfiguration> shared_tab_group_configuration,
    PrefService* pref_service,
    std::unique_ptr<TabGroupSyncMetricsLogger> metrics_logger,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    signin::IdentityManager* identity_manager)
    : model_(std::move(model)),
      sync_bridge_mediator_(std::make_unique<TabGroupSyncBridgeMediator>(
          model_.get(),
          pref_service,
          std::move(saved_tab_group_configuration),
          std::move(shared_tab_group_configuration))),
      metrics_logger_(std::move(metrics_logger)),
      pref_service_(pref_service),
      opt_guide_(optimization_guide_decider) {
  model_->AddObserver(this);
  if (opt_guide_) {
    opt_guide_->RegisterOptimizationTypes(
        {optimization_guide::proto::SAVED_TAB_GROUP});
  }
  if (identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
  }
}

TabGroupSyncServiceImpl::~TabGroupSyncServiceImpl() {
  for (auto& observer : observers_) {
    observer.OnWillBeDestroyed();
  }
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

void TabGroupSyncServiceImpl::GetURLRestriction(
    const GURL& url,
    TabGroupSyncService::UrlRestrictionCallback callback) {
  if (!opt_guide_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  opt_guide_->CanApplyOptimization(
      url, optimization_guide::proto::SAVED_TAB_GROUP,
      base::BindOnce(&OnCanApplyOptimizationCompleted, std::move(callback)));
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

void TabGroupSyncServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  for (signin::ConsentLevel consent_level :
       {signin::ConsentLevel::kSignin, signin::ConsentLevel::kSync}) {
    // Only record metrics when setting the primary account.
    switch (event_details.GetEventTypeFor(consent_level)) {
      case signin::PrimaryAccountChangeEvent::Type::kNone:
      case signin::PrimaryAccountChangeEvent::Type::kCleared:
        break;
      case signin::PrimaryAccountChangeEvent::Type::kSet:
        if (metrics_logger_) {
          metrics_logger_->RecordMetricsOnSignin(model_->saved_tab_groups(),
                                                 consent_level);
        }
    }
  }
}

void TabGroupSyncServiceImpl::SetIsInitializedForTesting(bool initialized) {
  is_initialized_ = initialized;
}

void TabGroupSyncServiceImpl::Shutdown() {
  metrics_logger_.reset();
  identity_manager_observation_.Reset();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabGroupSyncServiceImpl::GetSavedTabGroupControllerDelegate() {
  return sync_bridge_mediator_->GetSavedTabGroupControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabGroupSyncServiceImpl::GetSharedTabGroupControllerDelegate() {
  return sync_bridge_mediator_->GetSharedTabGroupControllerDelegate();
}

void TabGroupSyncServiceImpl::SetTabGroupSyncDelegate(
    std::unique_ptr<TabGroupSyncDelegate> delegate) {
  auto coordinator =
      std::make_unique<TabGroupSyncCoordinatorImpl>(std::move(delegate), this);
  SetCoordinator(std::move(coordinator));
}

void TabGroupSyncServiceImpl::AddGroup(SavedTabGroup group) {
  if (!is_initialized_) {
    VLOG(2) << __func__ << " Invoked before init";
    pending_actions_.emplace_back(
        base::BindOnce(&TabGroupSyncServiceImpl::AddGroup,
                       weak_ptr_factory_.GetWeakPtr(), std::move(group)));
    return;
  }

  VLOG(2) << __func__;
  base::Uuid group_id = group.saved_guid();
  group.SetCreatedBeforeSyncingTabGroups(
      !sync_bridge_mediator_->IsSavedBridgeSyncing());
  group.SetCreatorCacheGuid(
      sync_bridge_mediator_->GetLocalCacheGuidForSavedBridge());

  std::optional<LocalTabGroupID> local_group_id = group.local_group_id();

  model_->Add(std::move(group));

  // Local group id can be null for tests.
  if (local_group_id.has_value()) {
    LogEvent(TabGroupEvent::kTabGroupCreated, local_group_id.value());
  }
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
      sync_bridge_mediator_->GetLocalCacheGuidForSavedBridge());

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

void TabGroupSyncServiceImpl::SaveGroup(SavedTabGroup group) {
  const base::Uuid sync_id = group.saved_guid();
  const LocalTabGroupID local_id = group.local_group_id().value();
  AddGroup(std::move(group));
  ConnectLocalTabGroup(sync_id, local_id, OpeningSource::kOpenedFromRevisitUi);
}

void TabGroupSyncServiceImpl::UnsaveGroup(const LocalTabGroupID& local_id) {
  std::optional<SavedTabGroup> group = GetGroup(local_id);
  CHECK(group);
  coordinator_->DisconnectLocalTabGroup(local_id);
  RemoveGroup(group->saved_guid());
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
    const LocalTabGroupID& local_id,
    OpeningSource opening_source) {
  if (!is_initialized_) {
    VLOG(2) << __func__ << " Invoked before init";
    pending_actions_.emplace_back(base::BindOnce(
        &TabGroupSyncServiceImpl::UpdateLocalTabGroupMapping,
        weak_ptr_factory_.GetWeakPtr(), sync_id, local_id, opening_source));
    return;
  }

  VLOG(2) << __func__;

  // If the group was marked as locally-closed in prefs, clear that entry - the
  // group has been reopened.
  RemoveLocallyClosedGroupIdFromPref(sync_id);

  model_->OnGroupOpenedInTabStrip(sync_id, local_id);

  // Record metrics.
  EventDetails event_details(TabGroupEvent::kTabGroupOpened);
  event_details.local_tab_group_id = local_id;
  event_details.opening_source = opening_source;

  const SavedTabGroup* group = model_->Get(local_id);
  metrics_logger_->LogEvent(event_details, group, nullptr);
}

void TabGroupSyncServiceImpl::RemoveLocalTabGroupMapping(
    const LocalTabGroupID& local_id,
    ClosingSource closing_source) {
  VLOG(2) << __func__;
  RemoveDeletedGroupIdFromPref(local_id);

  auto* group = model_->Get(local_id);
  if (!group) {
    return;
  }

  // Record the group's guid as locally-closed in prefs.
  AddLocallyClosedGroupIdToPref(group->saved_guid());

  model_->OnGroupClosedInTabStrip(local_id);

  // Record metrics.
  EventDetails event_details(TabGroupEvent::kTabGroupClosed);
  event_details.local_tab_group_id = local_id;
  event_details.closing_source = closing_source;
  metrics_logger_->LogEvent(event_details, group, nullptr);
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
    const LocalTabGroupID& local_id,
    OpeningSource opening_source) {
  if (!is_initialized_) {
    VLOG(2) << __func__ << " Invoked before init";
    pending_actions_.emplace_back(base::BindOnce(
        &TabGroupSyncServiceImpl::ConnectLocalTabGroup,
        weak_ptr_factory_.GetWeakPtr(), sync_id, local_id, opening_source));
    return;
  }

  VLOG(2) << __func__;
  coordinator_->ConnectLocalTabGroup(sync_id, local_id, opening_source);
}

bool TabGroupSyncServiceImpl::IsRemoteDevice(
    const std::optional<std::string>& cache_guid) const {
  std::optional<std::string> local_cache_guid =
      sync_bridge_mediator_->GetLocalCacheGuidForSavedBridge();
  if (!local_cache_guid || !cache_guid) {
    return false;
  }

  return local_cache_guid.value() != cache_guid.value();
}

bool TabGroupSyncServiceImpl::WasTabGroupClosedLocally(
    const base::Uuid& sync_tab_group_id) const {
  std::optional<std::string> account_id =
      sync_bridge_mediator_->GetAccountIdForSavedBridge();
  if (account_id) {
    return syncer::GetAccountKeyedPrefDictEntry(
        pref_service_, prefs::kLocallyClosedRemoteTabGroupIds,
        signin::GaiaIdHash::FromGaiaId(*account_id),
        sync_tab_group_id.AsLowercaseString().c_str());
  }
  return false;
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

TabGroupSyncMetricsLogger*
TabGroupSyncServiceImpl::GetTabGroupSyncMetricsLogger() {
  return metrics_logger_.get();
}

void TabGroupSyncServiceImpl::HandleTabGroupsReordered(TriggerSource source) {
  if (!is_initialized_) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupsReordered(source);
  }
}

void TabGroupSyncServiceImpl::SavedTabGroupReorderedLocally() {
  HandleTabGroupsReordered(TriggerSource::LOCAL);
}

void TabGroupSyncServiceImpl::SavedTabGroupReorderedFromSync() {
  HandleTabGroupsReordered(TriggerSource::REMOTE);
}

void TabGroupSyncServiceImpl::SavedTabGroupAddedFromSync(
    const base::Uuid& guid) {
  HandleTabGroupAdded(guid, TriggerSource::REMOTE);
}

void TabGroupSyncServiceImpl::SavedTabGroupAddedLocally(
    const base::Uuid& guid) {
  HandleTabGroupAdded(guid, TriggerSource::LOCAL);
}

void TabGroupSyncServiceImpl::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  HandleTabGroupUpdated(group_guid, tab_guid, TriggerSource::REMOTE);
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

  // When a group is deleted, there's no more need to keep any "was locally
  // closed" pref entry around.
  // TODO(crbug.com/363927991): This also gets called during signout, when all
  // groups that belong to the account get closed. In that case, the pref
  // entries should *not* get cleared. Currently this only works because the
  // account_id has already been cleared here, which is fragile. Ideally,
  // HandleTabGroupRemoved() would receive a "reason" param, where one of the
  // possible values would be "signout".
  RemoveLocallyClosedGroupIdFromPref(id_pair.first);

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

void TabGroupSyncServiceImpl::AddLocallyClosedGroupIdToPref(
    const base::Uuid& sync_id) {
  std::optional<std::string> account_id =
      sync_bridge_mediator_->GetAccountIdForSavedBridge();
  if (!account_id) {
    // If there's no signed-in account, nothing to do.
    return;
  }
  syncer::SetAccountKeyedPrefDictEntry(
      pref_service_, prefs::kLocallyClosedRemoteTabGroupIds,
      signin::GaiaIdHash::FromGaiaId(*account_id),
      sync_id.AsLowercaseString().c_str(), base::Value());
}

void TabGroupSyncServiceImpl::RemoveLocallyClosedGroupIdFromPref(
    const base::Uuid& sync_id) {
  std::optional<std::string> account_id =
      sync_bridge_mediator_->GetAccountIdForSavedBridge();
  if (!account_id) {
    // If there's no signed-in account, nothing to do. Most notably, this
    // happens right after sign-out, when all tab groups associated to the
    // account get closed.
    return;
  }
  syncer::RemoveAccountKeyedPrefDictEntry(
      pref_service_, prefs::kLocallyClosedRemoteTabGroupIds,
      signin::GaiaIdHash::FromGaiaId(*account_id),
      sync_id.AsLowercaseString().c_str());
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
    observer.OnTabGroupLocalIdChanged(group_guid,
                                      saved_tab_group->local_group_id());
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

  while (!pending_actions_.empty()) {
    auto callback = std::move(pending_actions_.front());
    pending_actions_.pop_front();
    std::move(callback).Run();
  }

  for (auto& observer : observers_) {
    observer.OnInitialized();
  }

  ForceRemoveClosedTabGroupsOnStartup();
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
      sync_bridge_mediator_->GetLocalCacheGuidForSavedBridge(), group_id,
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

void TabGroupSyncServiceImpl::ForceRemoveClosedTabGroupsOnStartup() {
  if (!ShouldForceRemoveClosedTabGroupsOnStartup()) {
    return;
  }

  std::vector<base::Uuid> group_ids;
  for (const auto& group : model_->saved_tab_groups()) {
    if (group.local_group_id()) {
      continue;
    }
    group_ids.push_back(group.saved_guid());
  }

  VLOG(0) << __func__
          << " Cleaning up groups on startup, groups# = " << group_ids.size();

  for (const auto& group_id : group_ids) {
    model_->Remove(group_id);
  }

  metrics_logger_->RecordTabGroupDeletionsOnStartup(group_ids.size());
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
