// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/tab_group_sync_service_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "components/data_sharing/public/logger.h"
#include "components/data_sharing/public/logger_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/page_entities_metadata.pb.h"
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
#include "components/saved_tab_groups/internal/versioning_message_controller_impl.h"
#include "components/saved_tab_groups/public/collaboration_finder.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_metrics_logger.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/sync/base/account_pref_utils.h"
#include "components/sync/base/collaboration_id.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "google_apis/gaia/gaia_id.h"

namespace tab_groups {
namespace {
constexpr base::TimeDelta kDelayBeforeMetricsLogged = base::Seconds(10);
constexpr base::TimeDelta kDelayBeforeTabGroupCleanUp = base::Seconds(10);

void LogTabGroupEvent(data_sharing::Logger* logger,
                      const std::string_view prefix,
                      const SavedTabGroup* group) {
  DATA_SHARING_LOG(logger_common::mojom::LogSource::TabGroupSyncService, logger,
                   TabGroupToShortLogString(prefix, group));
}

void LogTabGroupEvent(
    data_sharing::Logger* logger,
    const std::string_view prefix,
    base::Uuid group_id,
    const std::optional<syncer::CollaborationId> collaboration_id) {
  DATA_SHARING_LOG(
      logger_common::mojom::LogSource::TabGroupSyncService, logger,
      TabGroupIdsToShortLogString(prefix, group_id, collaboration_id));
}

bool IsSanitizationRequired(const SavedTabGroup& tab_group, const GURL url) {
  return tab_group.is_shared_tab_group() && url.SchemeIsHTTPOrHTTPS();
}

void UpdateTabTitleIfNeeded(
    const SavedTabGroup& group,
    SavedTabGroupTab& tab,
    optimization_guide::OptimizationGuideDecider* opt_guide,
    stats::TitleSanitizationType type) {
  if (!IsSanitizationRequired(group, tab.url())) {
    return;
  }

  std::u16string title = GetTitleFromUrlForDisplay(tab.url());
  // Optimization guide may be null in tests.
  if (!IsTabTitleSanitizationEnabled() || !opt_guide) {
    tab.SetTitle(title);
    return;
  }

  optimization_guide::OptimizationMetadata metadata;
  optimization_guide::OptimizationGuideDecision decision =
      opt_guide->CanApplyOptimization(
          tab.url(), optimization_guide::proto::PAGE_ENTITIES, &metadata);
  bool use_url_as_title = true;

  if (decision == optimization_guide::OptimizationGuideDecision::kTrue &&
      metadata.any_metadata().has_value()) {
    std::optional<optimization_guide::proto::PageEntitiesMetadata>
        page_entities_metadata = metadata.ParsedMetadata<
            optimization_guide::proto::PageEntitiesMetadata>();
    if (page_entities_metadata.has_value() &&
        !page_entities_metadata->alternative_title().empty()) {
      use_url_as_title = false;
      title = base::UnescapeForHTML(
          base::UTF8ToUTF16(page_entities_metadata->alternative_title()));
    }
  }
  stats::RecordSharedGroupTitleSanitization(use_url_as_title, type);
  tab.SetTitle(title);
}

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

bool IsUrlSyncable(
    const GURL& url,
    const GURL& previous_url,
    bool is_shared_tab_group,
    const std::optional<proto::UrlRestriction>& url_restriction) {
  if (!url_restriction.has_value()) {
    return true;
  }

  if (is_shared_tab_group && !url_restriction->block_for_share()) {
    return true;
  }

  if (!is_shared_tab_group && !url_restriction->block_for_sync()) {
    return true;
  }

  // Block the URL if only differs from the current one in fragment.
  if (url_restriction->block_if_only_fragment_differs() &&
      url.GetWithoutRef() == previous_url.GetWithoutRef()) {
    return false;
  }

  if (url_restriction->block_if_path_is_same() &&
      url.GetWithEmptyPath() == previous_url.GetWithEmptyPath() &&
      url.GetPath() == previous_url.GetPath()) {
    return false;
  }

  if (url_restriction->block_if_domain_is_same() &&
      url.GetWithEmptyPath() == previous_url.GetWithEmptyPath()) {
    return false;
  }

  return true;
}

}  // namespace

TabGroupSyncServiceImpl::TabGroupSyncServiceImpl(
    std::unique_ptr<SavedTabGroupModel> model,
    std::unique_ptr<SyncDataTypeConfiguration> saved_tab_group_configuration,
    std::unique_ptr<SyncDataTypeConfiguration> shared_tab_group_configuration,
    std::unique_ptr<SyncDataTypeConfiguration>
        shared_tab_group_account_configuration,
    PrefService* pref_service,
    std::unique_ptr<TabGroupSyncMetricsLogger> metrics_logger,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    signin::IdentityManager* identity_manager,
    data_sharing::personal_collaboration_data::PersonalCollaborationDataService*
        personal_collaboration_data_service,
    std::unique_ptr<CollaborationFinder> collaboration_finder,
    data_sharing::Logger* logger)
    : model_(std::move(model)),
      sync_bridge_mediator_(std::make_unique<TabGroupSyncBridgeMediator>(
          model_.get(),
          pref_service,
          logger,
          std::move(saved_tab_group_configuration),
          std::move(shared_tab_group_configuration))),
      metrics_logger_(std::move(metrics_logger)),
      collaboration_finder_(std::move(collaboration_finder)),
      logger_(logger),
      pref_service_(pref_service),
      opt_guide_(optimization_guide_decider),
      versioning_message_controller_(
          std::make_unique<VersioningMessageControllerImpl>(pref_service_,
                                                            this)) {
  if (personal_collaboration_data_service) {
    personal_collaboration_data_handler_ =
        std::make_unique<TabGroupSyncPersonalCollaborationDataHandler>(
            model_.get(), personal_collaboration_data_service);
  } else if (shared_tab_group_account_configuration) {
    shared_tab_group_account_data_bridge_ =
        std::make_unique<SharedTabGroupAccountDataSyncBridge>(
            std::move(shared_tab_group_account_configuration), *model_);
  }

  collaboration_finder_->SetClient(this);
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
  ClearAllUserData();
}

void TabGroupSyncServiceImpl::SetCoordinator(
    std::unique_ptr<TabGroupSyncCoordinator> coordinator) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!coordinator_);
  coordinator_ = std::move(coordinator);
  if (IsTabGroupSyncCoordinatorEnabled()) {
    AddObserver(coordinator_.get());
  }
}

std::unique_ptr<ScopedLocalObservationPauser>
TabGroupSyncServiceImpl::CreateScopedLocalObserverPauser() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(coordinator_);
  return coordinator_->CreateScopedLocalObserverPauser();
}

void TabGroupSyncServiceImpl::GetURLRestriction(
    const GURL& url,
    TabGroupSyncService::UrlRestrictionCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!opt_guide_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  opt_guide_->CanApplyOptimization(
      url, optimization_guide::proto::SAVED_TAB_GROUP,
      base::BindOnce(&OnCanApplyOptimizationCompleted, std::move(callback)));
}

std::unique_ptr<std::vector<SavedTabGroup>>
TabGroupSyncServiceImpl::TakeSharedTabGroupsAvailableAtStartupForMessaging() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return std::move(shared_tab_groups_available_at_startup_for_messaging_);
}

bool TabGroupSyncServiceImpl::HadSharedTabGroupsLastSession(
    bool open_shared_tab_groups) {
  return open_shared_tab_groups ? had_open_shared_tab_groups_on_startup_
                                : had_shared_tab_groups_on_startup_;
}

VersioningMessageController*
TabGroupSyncServiceImpl::GetVersioningMessageController() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return versioning_message_controller_.get();
}

void TabGroupSyncServiceImpl::AddObserver(
    TabGroupSyncService::Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.AddObserver(observer);

  // If the observer is added late and missed the init signal, send the signal
  // now.
  if (is_initialized_) {
    observer->OnInitialized();
  }
}

void TabGroupSyncServiceImpl::RemoveObserver(
    TabGroupSyncService::Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.RemoveObserver(observer);
}

void TabGroupSyncServiceImpl::OnLastTabClosed(
    const SavedTabGroup& saved_tab_group) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!saved_tab_group.is_shared_tab_group()) {
    return;
  }

  const std::vector<SavedTabGroupTab>& tabs = saved_tab_group.saved_tabs();
  if (tabs.size() != 1) {
    return;
  }

  // Create a new empty tab and remove the old tab.
  const SavedTabGroupTab& tab = tabs[0];
  std::pair<GURL, std::u16string> url_and_title = GetDefaultUrlAndTitle();
  SavedTabGroupTab new_tab(url_and_title.first, url_and_title.second,
                           saved_tab_group.saved_guid(), 0);
  new_tab.SetCreatorCacheGuid(
      sync_bridge_mediator_->GetLocalCacheGuidForSavedBridge());
  std::optional<GaiaId> gaia_id =
      sync_bridge_mediator_->GetTrackingGaiaIdForSharedBridge();
  if (gaia_id.has_value()) {
    new_tab.SetUpdatedByAttribution(std::move(gaia_id.value()));
  }
  model_->AddTabToGroupLocally(saved_tab_group.saved_guid(),
                               std::move(new_tab));
  RemoveTab(saved_tab_group.local_group_id().value(),
            tab.local_tab_id().value());

  // Inform UI about the tab group change.
  NotifyTabGroupUpdated(saved_tab_group.saved_guid(), TriggerSource::REMOTE);
}

void TabGroupSyncServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_initialized_ = initialized;

  if (initialized) {
    NotifyServiceInitialized();
  }
}

CollaborationFinder*
TabGroupSyncServiceImpl::GetCollaborationFinderForTesting() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return collaboration_finder_.get();
}

TabGroupSyncServiceImpl::TabGroupSharingTimeoutInfo::
    TabGroupSharingTimeoutInfo() = default;
TabGroupSyncServiceImpl::TabGroupSharingTimeoutInfo::
    ~TabGroupSharingTimeoutInfo() = default;

void TabGroupSyncServiceImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  metrics_logger_.reset();
  identity_manager_observation_.Reset();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabGroupSyncServiceImpl::GetSavedTabGroupControllerDelegate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return sync_bridge_mediator_->GetSavedTabGroupControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabGroupSyncServiceImpl::GetSharedTabGroupControllerDelegate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return sync_bridge_mediator_->GetSharedTabGroupControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabGroupSyncServiceImpl::GetSharedTabGroupAccountControllerDelegate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(shared_tab_group_account_data_bridge_);
  return shared_tab_group_account_data_bridge_->change_processor()
      ->GetControllerDelegate();
}

void TabGroupSyncServiceImpl::SetTabGroupSyncDelegate(
    std::unique_ptr<TabGroupSyncDelegate> delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto coordinator = std::make_unique<TabGroupSyncCoordinatorImpl>(
      std::move(delegate), this, pref_service_);
  SetCoordinator(std::move(coordinator));
}

void TabGroupSyncServiceImpl::AddGroup(SavedTabGroup group) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  if (group.is_shared_tab_group()) {
    std::optional<GaiaId> gaia_id =
        sync_bridge_mediator_->GetTrackingGaiaIdForSharedBridge();
    if (gaia_id.has_value()) {
      group.SetUpdatedByAttribution(gaia_id.value());
      for (SavedTabGroupTab& tab : group.saved_tabs()) {
        tab.SetUpdatedByAttribution(gaia_id.value());
      }
    }
  }

  std::optional<LocalTabGroupID> local_group_id = group.local_group_id();

  model_->AddedLocally(std::move(group));

  // Local group id can be null for tests.
  if (local_group_id.has_value()) {
    LogEvent(TabGroupEvent::kTabGroupCreated, local_group_id.value());
  }
}

void TabGroupSyncServiceImpl::RemoveGroup(const LocalTabGroupID& local_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << __func__;

  auto* group = model_->Get(local_id);
  if (!group) {
    return;
  }

  LogTabGroupEvent(logger_, "RemoveGroup", group);
  base::Uuid sync_id = group->saved_guid();
  LogEvent(TabGroupEvent::kTabGroupRemoved, local_id);
  model_->RemovedLocally(local_id);
}

void TabGroupSyncServiceImpl::RemoveGroup(const base::Uuid& sync_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << __func__;
  // TODO(shaktisahu): Provide LogEvent API to work with sync ID.
  LogTabGroupEvent(logger_, "RemoveGroup", sync_id,
                   std::optional<syncer::CollaborationId>());
  model_->RemovedLocally(sync_id);
}

void TabGroupSyncServiceImpl::UpdateVisualData(
    const LocalTabGroupID local_group_id,
    const tab_groups::TabGroupVisualData* visual_data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << __func__;
  model_->UpdateVisualDataLocally(local_group_id, visual_data);
  UpdateAttributions(local_group_id);
  UpdateSharedAttributions(local_group_id);
  LogEvent(TabGroupEvent::kTabGroupVisualsChanged, local_group_id,
           std::nullopt);
  stats::RecordTabGroupVisualsMetrics(visual_data);
}

void TabGroupSyncServiceImpl::UpdateGroupPosition(
    const base::Uuid& sync_id,
    std::optional<bool> is_pinned,
    std::optional<int> new_index) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

void TabGroupSyncServiceImpl::UpdateBookmarkNodeId(
    const base::Uuid& sync_id,
    std::optional<base::Uuid> bookmark_node_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << __func__;

  const SavedTabGroup* tab_group = model_->Get(sync_id);
  if (tab_group) {
    model_->UpdateBookmarkNodeId(sync_id, bookmark_node_id);
  }
}

void TabGroupSyncServiceImpl::AddTab(const LocalTabGroupID& group_id,
                                     const LocalTabID& tab_id,
                                     const std::u16string& title,
                                     const GURL& url,
                                     std::optional<size_t> position) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    DVLOG(1) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const auto* tab = group->GetTab(tab_id);
  if (tab) {
    DVLOG(1) << __func__ << " Called for a tab that already exists";
    return;
  }

  SavedTabGroupTab new_tab(url, title, group->saved_guid(), position,
                           /*saved_tab_guid=*/std::nullopt, tab_id);
  AddTabInternal(std::move(new_tab), group);
}

void TabGroupSyncServiceImpl::AddUrl(const base::Uuid& group_id,
                                     const std::u16string& title,
                                     const GURL& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << __func__;
  const SavedTabGroup* group = model_->Get(group_id);
  if (!group) {
    return;
  }
  SavedTabGroupTab new_tab(url, title, group->saved_guid(),
                           /*position*/ std::nullopt);

  AddTabInternal(std::move(new_tab), group);
}

void TabGroupSyncServiceImpl::NavigateTab(const LocalTabGroupID& group_id,
                                          const LocalTabID& tab_id,
                                          const GURL& url,
                                          const std::u16string& title) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    DVLOG(1) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const auto* tab = group->GetTab(tab_id);
  if (!tab) {
    DVLOG(1) << __func__ << " Called for a tab that doesn't exist";
    return;
  }

  if (IsUrlRestrictionEnabled()) {
    GetURLRestriction(
        url, base::BindOnce(&TabGroupSyncServiceImpl::NavigateTabInternal,
                            weak_ptr_factory_.GetWeakPtr(), group_id, tab_id,
                            url, title, tab->url()));
    return;
  }

  NavigateTabInternal(group_id, tab_id, url, title, tab->url(), std::nullopt);
}

void TabGroupSyncServiceImpl::UpdateTabProperties(
    const LocalTabGroupID& group_id,
    const LocalTabID& tab_id,
    const SavedTabGroupTabBuilder& tab_builder) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    DVLOG(1) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const auto* tab = group->GetTab(tab_id);
  if (!tab) {
    DVLOG(1) << __func__ << " Called for a tab that doesn't exist";
    return;
  }

  // Use the builder to create the updated tab.
  SavedTabGroupTabBuilder new_builder = tab_builder;
  SavedTabGroupTab updated_tab = new_builder.Build(*tab);
  model_->UpdateTabInGroup(group->saved_guid(), std::move(updated_tab),
                           /*notify_observers=*/false);
}

void TabGroupSyncServiceImpl::RemoveTab(const LocalTabGroupID& group_id,
                                        const LocalTabID& tab_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    DVLOG(1) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  auto* tab = group->GetTab(tab_id);
  if (!tab) {
    DVLOG(1) << __func__ << " Called for a tab that doesn't exist";
    return;
  }

  base::Uuid sync_id = group->saved_guid();
  UpdateAttributions(group_id);
  LogEvent(TabGroupEvent::kTabRemoved, group_id, tab_id);
  model_->UpdateLastUserInteractionTimeLocally(group_id);
  std::optional<GaiaId> local_gaia_id =
      group->is_shared_tab_group()
          ? sync_bridge_mediator_->GetTrackingGaiaIdForSharedBridge()
          : std::nullopt;
  model_->RemoveTabFromGroupLocally(sync_id, tab->saved_tab_guid(),
                                    std::move(local_gaia_id));
}

void TabGroupSyncServiceImpl::MoveTab(const LocalTabGroupID& group_id,
                                      const LocalTabID& tab_id,
                                      int new_group_index) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    DVLOG(1) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  auto* tab = group->GetTab(tab_id);
  if (!tab) {
    DVLOG(1) << __func__ << " Called for a tab that doesn't exist";
    return;
  }

  UpdateAttributions(group_id);
  model_->MoveTabInGroupTo(group->saved_guid(), tab->saved_tab_guid(),
                           new_group_index);
  LogEvent(TabGroupEvent::kTabGroupTabsReordered, group_id, std::nullopt);
}

void TabGroupSyncServiceImpl::OnTabSelected(
    const std::optional<LocalTabGroupID>& group_id,
    const LocalTabID& tab_id,
    const std::u16string& title) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Notify observers of tab selection event regardless of whether the tab is in
  // a tab group or not. This is important mainly for messaging backend service
  // which computes diff between previous and currently selected tabs and
  // accordingly turns on/off messages.
  NotifyTabSelected();

  // Update metrics and attributions.
  if (!group_id) {
    return;
  }

  const SavedTabGroup* group = model_->Get(*group_id);
  if (!group) {
    return;
  }

  const SavedTabGroupTab* tab = group->GetTab(tab_id);
  if (!tab) {
    return;
  }

  UpdateAttributions(*group_id);
  model_->UpdateLastUserInteractionTimeLocally(*group_id);
  if (group->is_shared_tab_group()) {
    model_->UpdateTabLastSeenTimeFromLocal(group->saved_guid(),
                                           tab->saved_tab_guid());
  }
  LogEvent(TabGroupEvent::kTabSelected, *group_id, tab_id);
}

void TabGroupSyncServiceImpl::NotifyTabSelected() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto selected_tabs = coordinator_->GetSelectedTabs();
  for (auto& observer : observers_) {
    observer.OnTabSelected(selected_tabs);
  }
}

std::u16string TabGroupSyncServiceImpl::GetTabTitle(
    const LocalTabID& local_tab_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return coordinator_->GetTabTitle(local_tab_id);
}

std::set<LocalTabID> TabGroupSyncServiceImpl::GetSelectedTabs() {
  return coordinator_->GetSelectedTabs();
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

void TabGroupSyncServiceImpl::MakeTabGroupShared(
    const LocalTabGroupID& local_group_id,
    const syncer::CollaborationId& collaboration_id,
    TabGroupSharingCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const SavedTabGroup* saved_group = model_->Get(local_group_id);
  if (!saved_group || saved_group->is_shared_tab_group()) {
    return;
  }

  RegisterPageEntityOptimizationTypeIfNeeded();

  LogTabGroupEvent(logger_, "MakeTabGroupShared", saved_group);

  std::optional<GaiaId> gaia_id =
      sync_bridge_mediator_->GetTrackingGaiaIdForSharedBridge();
  if (!gaia_id.has_value()) {
    // Do not share the group if the bridge is not syncing. This can happen if
    // the caller thinks we just signed in, but sync is still preparing for
    // the account and hasn't been through the initial merge.
    pending_actions_waiting_sign_in_.emplace_back(
        base::BindOnce(&TabGroupSyncServiceImpl::MakeTabGroupShared,
                       weak_ptr_factory_.GetWeakPtr(), local_group_id,
                       collaboration_id, std::move(callback)));
    return;
  }

  // Make a deep copy of the group without fields which are not used in shared
  // tab groups, and without migration of local IDs.
  SavedTabGroup shared_group =
      saved_group->CloneAsSharedTabGroup(collaboration_id);
  shared_group.SetUpdatedByAttribution(gaia_id.value());

  // The shared group must never be empty.
  CHECK(!shared_group.saved_tabs().empty());
  for (SavedTabGroupTab& tab : shared_group.saved_tabs()) {
    UpdateTabTitleIfNeeded(shared_group, tab, opt_guide_,
                           stats::TitleSanitizationType::kShareTabGroup);
    tab.SetUpdatedByAttribution(gaia_id.value());
  }

  LogTabGroupEvent(logger_, "MakeTabGroupShared - Starting Timer", saved_group);
  // The same group must never be shared twice at the same time.
  CHECK(shared_group.is_transitioning_to_shared());
  CHECK(!tab_group_sharing_timeout_info_.contains(shared_group.saved_guid()));
  tab_group_sharing_timeout_info_[shared_group.saved_guid()].callback =
      std::move(callback);
  tab_group_sharing_timeout_info_[shared_group.saved_guid()].timer.Start(
      FROM_HERE, base::Seconds(10),
      base::BindOnce(&TabGroupSyncServiceImpl::OnTabGroupSharingTimeout,
                     weak_ptr_factory_.GetWeakPtr(),
                     shared_group.saved_guid()));

  model_->AddedLocally(std::move(shared_group));
}

void TabGroupSyncServiceImpl::AboutToUnShareTabGroup(
    const LocalTabGroupID& local_group_id,
    base::OnceClosure on_complete_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const SavedTabGroup* saved_group = model_->Get(local_group_id);
  LogTabGroupEvent(logger_, "AboutToUnShareTabGroup", saved_group);
  model_->SetIsTransitioningToSaved(local_group_id, true);
  std::move(on_complete_callback).Run();
}

void TabGroupSyncServiceImpl::OnTabGroupUnShareComplete(
    const LocalTabGroupID& local_group_id,
    bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const SavedTabGroup* saved_group = model_->Get(local_group_id);
  if (!saved_group) {
    return;
  }

  {
    std::string prefix = "OnTabGroupUnShareComplete ";
    prefix += success ? "Success" : "Failure";
    LogTabGroupEvent(logger_, prefix, saved_group);
  }
  if (!success) {
    return;
  }

  // Make a deep copy of shared tab group.
  SavedTabGroup cloned_group = saved_group->CloneAsSavedTabGroup();

  // The originating saved group for this shared tab group might still be alive.
  // Remove it.
  std::optional<base::Uuid> originating_tab_group_guid =
      saved_group->GetOriginatingTabGroupGuid();
  if (originating_tab_group_guid.has_value()) {
    const SavedTabGroup* originating_group =
        model_->Get(originating_tab_group_guid.value());
    if (originating_group) {
      DCHECK(!originating_group->local_group_id());
      LogTabGroupEvent(logger_, "Cleanup Saved Group", originating_group);
      RemoveGroup(originating_tab_group_guid.value());
    }
  }

  cloned_group.SetCreatedBeforeSyncingTabGroups(
      !sync_bridge_mediator_->IsSavedBridgeSyncing());
  cloned_group.SetCreatorCacheGuid(
      sync_bridge_mediator_->GetLocalCacheGuidForSavedBridge());
  model_->AddedLocally(std::move(cloned_group));
}

void TabGroupSyncServiceImpl::OnCollaborationRemoved(
    const syncer::CollaborationId& collaboration_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::optional<SavedTabGroup> shared_group =
      FindGroupWithCollaborationId(collaboration_id);
  if (!shared_group) {
    return;
  }

  LogTabGroupEvent(logger_, "OnCollaborationRemoved",
                   std::addressof(shared_group.value()));
  // Call the sync bridge to stop tracking the group, so the
  // tombstone won't be uploaded to the sync server. This method is invoked
  // during Leave Group / Delete Group flow, and not for Unshare flow.
  sync_bridge_mediator_->UntrackEntitiesForCollaboration(collaboration_id);

  // Since we are deleting the shared group, delete the originating group if
  // it still exists.
  std::optional<base::Uuid> originating_tab_group_guid =
      shared_group->GetOriginatingTabGroupGuid();
  if (originating_tab_group_guid.has_value()) {
    const SavedTabGroup* originating_group =
        model_->Get(originating_tab_group_guid.value());
    CHECK(!originating_group || !originating_group->local_group_id());
    if (originating_group) {
      LogTabGroupEvent(logger_, "Removing Originating Group",
                       originating_group);
    }
    RemoveGroup(originating_tab_group_guid.value());
  }

  RemoveGroup(shared_group->saved_guid());

  // Inform the UI as if the tab group has been removed from sync.
  // TODO(crbug.com/399410173): Remove the group locally should also trigger UI
  // update.
  for (TabGroupSyncService::Observer& observer : observers_) {
    if (shared_group->local_group_id().has_value()) {
      observer.OnTabGroupRemoved(shared_group->local_group_id().value(),
                                 TriggerSource::REMOTE);
    }
    observer.OnTabGroupRemoved(shared_group->saved_guid(),
                               TriggerSource::REMOTE);
  }
}

void TabGroupSyncServiceImpl::MakeTabGroupSharedForTesting(
    const LocalTabGroupID& local_group_id,
    const syncer::CollaborationId& collaboration_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  model_->MakeTabGroupSharedForTesting(local_group_id, collaboration_id);
}

void TabGroupSyncServiceImpl::MakeTabGroupUnsharedForTesting(
    const LocalTabGroupID& local_group_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  model_->MakeTabGroupUnsharedForTesting(local_group_id);
}

bool TabGroupSyncServiceImpl::ShouldExposeSavedTabGroupInList(
    const SavedTabGroup& group) const {
  // TODO(crbug.com/395160538): Simplify the logic of filtering out groups
  // that are in transition between saved and shared.
  if (group.saved_tabs().empty() || group.is_hidden()) {
    return false;
  }

  // Skip group that are in the middle of migration between shared and saved.
  // For a migrating group, the originating group should not be hidden.
  const auto originating_group_id = group.GetOriginatingTabGroupGuid();
  if (originating_group_id) {
    const SavedTabGroup* originating_group =
        model_->Get(originating_group_id.value());
    if (originating_group && !originating_group->is_hidden()) {
      return false;
    }
  }

  if (base::Contains(shared_tab_groups_waiting_for_collaboration_,
                     group.saved_guid(),
                     [](const auto& entry) { return std::get<1>(entry); })) {
    // The shared tab group should not be returned while its collaboration is
    // not available.
    return false;
  }

  return true;
}

std::vector<const SavedTabGroup*> TabGroupSyncServiceImpl::ReadAllGroups()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<const SavedTabGroup*> tab_groups;
  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    if (ShouldExposeSavedTabGroupInList(group)) {
      tab_groups.push_back(&group);
    }
  }
  return tab_groups;
}

std::vector<SavedTabGroup> TabGroupSyncServiceImpl::GetAllGroups() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<SavedTabGroup> tab_groups;
  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    if (ShouldExposeSavedTabGroupInList(group)) {
      tab_groups.push_back(group);
    }
  }
  return tab_groups;
}

std::optional<SavedTabGroup> TabGroupSyncServiceImpl::GetGroup(
    const base::Uuid& guid) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Do not filter the group if it was requested directly using ID.

  const SavedTabGroup* tab_group = model_->Get(guid);
  return tab_group ? std::make_optional<SavedTabGroup>(*tab_group)
                   : std::nullopt;
}

std::optional<SavedTabGroup> TabGroupSyncServiceImpl::GetGroup(
    const LocalTabGroupID& local_id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Do not filter the group if it was requested directly using ID.

  const SavedTabGroup* tab_group = model_->Get(local_id);
  return tab_group ? std::make_optional<SavedTabGroup>(*tab_group)
                   : std::nullopt;
}

std::optional<SavedTabGroup> TabGroupSyncServiceImpl::GetGroup(
    const EitherGroupID& either_id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const SavedTabGroup* tab_group = nullptr;
  if (std::holds_alternative<LocalTabGroupID>(either_id)) {
    tab_group = model_->Get(std::get<LocalTabGroupID>(either_id));
  } else {
    tab_group = model_->Get(std::get<base::Uuid>(either_id));
  }

  return tab_group ? std::make_optional<SavedTabGroup>(*tab_group)
                   : std::nullopt;
}

std::vector<LocalTabGroupID> TabGroupSyncServiceImpl::GetDeletedGroupIds()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetDeletedGroupIdsFromPref();
}

std::optional<std::u16string>
TabGroupSyncServiceImpl::GetTitleForPreviouslyExistingSharedTabGroup(
    const syncer::CollaborationId& collaboration_id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (titles_for_previously_existing_shared_tab_groups_.find(
          collaboration_id) ==
      titles_for_previously_existing_shared_tab_groups_.end()) {
    return std::nullopt;
  }
  return titles_for_previously_existing_shared_tab_groups_.at(collaboration_id);
}

std::optional<LocalTabGroupID> TabGroupSyncServiceImpl::OpenTabGroup(
    const base::Uuid& sync_group_id,
    std::unique_ptr<TabGroupActionContext> context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(coordinator_);
  VLOG(2) << __func__;
  return coordinator_->HandleOpenTabGroupRequest(sync_group_id,
                                                 std::move(context));
}

void TabGroupSyncServiceImpl::UpdateLocalTabGroupMapping(
    const base::Uuid& sync_id,
    const LocalTabGroupID& local_id,
    OpeningSource opening_source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(coordinator_);
  if (!is_initialized_) {
    VLOG(2) << __func__ << " Invoked before init";
    pending_actions_.emplace_back(base::BindOnce(
        &TabGroupSyncServiceImpl::ConnectLocalTabGroup,
        weak_ptr_factory_.GetWeakPtr(), sync_id, local_id, opening_source));
    return;
  }

  VLOG(2) << __func__;

  if (GetGroup(sync_id)) {
    // Only connect the group if it exists in the model.
    UpdateLocalTabGroupMapping(sync_id, local_id, opening_source);
    coordinator_->ConnectLocalTabGroup(sync_id, local_id);
  } else {
    // This can happen in cases where a group was deleted remotely before
    // startup.
    // See crbug.com/392174867 for more details.
    coordinator_->OnTabGroupRemoved(local_id, TriggerSource::REMOTE);
  }
}

bool TabGroupSyncServiceImpl::IsRemoteDevice(
    const std::optional<std::string>& cache_guid) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::optional<std::string> local_cache_guid =
      sync_bridge_mediator_->GetLocalCacheGuidForSavedBridge();
  if (!local_cache_guid || !cache_guid) {
    return false;
  }

  return local_cache_guid.value() != cache_guid.value();
}

bool TabGroupSyncServiceImpl::WasTabGroupClosedLocally(
    const base::Uuid& sync_tab_group_id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::optional<GaiaId> gaia_id =
      sync_bridge_mediator_->GetGaiaIdForSavedBridge();
  if (gaia_id) {
    return syncer::GetAccountKeyedPrefDictEntry(
        pref_service_, prefs::kLocallyClosedRemoteTabGroupIds,
        signin::GaiaIdHash::FromGaiaId(*gaia_id),
        sync_tab_group_id.AsLowercaseString().c_str());
  }
  return false;
}

void TabGroupSyncServiceImpl::RecordTabGroupEvent(
    const EventDetails& event_details) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Find the group from the passed sync or local ID.
  const SavedTabGroup* group = nullptr;
  if (event_details.local_tab_group_id) {
    group = model_->Get(event_details.local_tab_group_id.value());
  }

  if (!group) {
    DVLOG(1) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const SavedTabGroupTab* tab = nullptr;
  if (event_details.local_tab_id) {
    tab = group->GetTab(event_details.local_tab_id.value());
  }

  metrics_logger_->LogEvent(event_details, group, tab);
}

void TabGroupSyncServiceImpl::UpdateArchivalStatus(const base::Uuid& sync_id,
                                                   bool archival_status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << __func__;

  std::optional<SavedTabGroup> group = GetGroup(sync_id);
  if (!group.has_value()) {
    return;
  }

  model_->UpdateArchivalStatus(sync_id, archival_status);
}

void TabGroupSyncServiceImpl::UpdateTabLastSeenTime(const base::Uuid& group_id,
                                                    const base::Uuid& tab_id,
                                                    TriggerSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Verify tab exists before updating. This method may be called from
  // sync services, such as the MessagingBackendService which doesn't
  // necessarily know if the tab still exists.
  std::optional<SavedTabGroup> group = GetGroup(group_id);
  if (!group.has_value()) {
    return;
  }

  const SavedTabGroupTab* tab = group->GetTab(tab_id);
  if (!tab) {
    return;
  }

  model_->UpdateTabLastSeenTimeFromLocal(group_id, tab_id);
}

TabGroupSyncMetricsLogger*
TabGroupSyncServiceImpl::GetTabGroupSyncMetricsLogger() {
  return metrics_logger_.get();
}

void TabGroupSyncServiceImpl::HandleTabGroupsReordered(TriggerSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // For shared tab groups, we need to be able to know that a group has been
  // removed and inform a user about it through a message. To facilitate this,
  // we store the last known tab group title before removal.
  if (removed_group.is_shared_tab_group()) {
    titles_for_previously_existing_shared_tab_groups_.emplace(
        removed_group.collaboration_id().value(), removed_group.title());
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TabGroupSyncServiceImpl::HandleTabGroupRemoved,
                                weak_ptr_factory_.GetWeakPtr(), removed_group,
                                TriggerSource::REMOTE));
}

void TabGroupSyncServiceImpl::SavedTabGroupRemovedLocally(
    const SavedTabGroup& removed_group) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  HandleTabGroupRemoved(removed_group, TriggerSource::LOCAL);
}

void TabGroupSyncServiceImpl::HandleTabGroupAdded(const base::Uuid& guid,
                                                  TriggerSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_initialized_) {
    return;
  }

  const SavedTabGroup* saved_tab_group = model_->Get(guid);
  if (!saved_tab_group) {
    return;
  }

  LogTabGroupEvent(logger_, "HandleTabGroupAdded", saved_tab_group);

  if (saved_tab_group->is_hidden()) {
    // Ignore any updates to the groups which were hidden.
    return;
  }

  if (saved_tab_group->is_shared_tab_group()) {
    RegisterPageEntityOptimizationTypeIfNeeded();
  }

  if (saved_tab_group->saved_tabs().empty()) {
    LogTabGroupEvent(logger_, "HandleTabGroupAdded - Empty", saved_tab_group);
    empty_groups_.emplace(guid);
    // Wait for another sync update with tabs before notifying the UI.
    return;
  }

  if (saved_tab_group->is_transitioning_to_shared()) {
    // Wait for the shared group to be committed to the server before notifying.
    return;
  }

  for (const SavedTabGroup* shared_group : model_->GetSharedTabGroupsOnly()) {
    if (shared_group->GetOriginatingTabGroupGuid() ==
        saved_tab_group->saved_guid()) {
      // This group is the originating saved tab group of a shared tab group.
      // Mark it as hidden and ignore it.
      model_->SetGroupHidden(saved_tab_group->saved_guid());
      return;
    }
  }

  if (saved_tab_group->collaboration_id()) {
    const syncer::CollaborationId& collaboration_id =
        saved_tab_group->collaboration_id().value();
    if (!collaboration_finder_->IsCollaborationAvailable(collaboration_id)) {
      LogTabGroupEvent(logger_, "Missing CollaborationId - Waiting",
                       saved_tab_group);
      shared_tab_groups_waiting_for_collaboration_.emplace_back(
          collaboration_id, guid, source);
      return;
    }
  }

  // Post task is used here to avoid reentrancy. See crbug.com/373500807 for
  // details.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TabGroupSyncServiceImpl::NotifyTabGroupAdded,
                                weak_ptr_factory_.GetWeakPtr(), guid, source));
}

void TabGroupSyncServiceImpl::HandleTabGroupUpdated(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid,
    TriggerSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_initialized_) {
    return;
  }

  const SavedTabGroup* saved_tab_group = model_->Get(group_guid);
  if (!saved_tab_group || saved_tab_group->saved_tabs().empty()) {
    return;
  }

  if (saved_tab_group->is_hidden()) {
    // Ignore any updates to the groups which were hidden.
    return;
  }

  auto iter = std::find_if(shared_tab_groups_waiting_for_collaboration_.begin(),
                           shared_tab_groups_waiting_for_collaboration_.end(),
                           [group_guid](const auto& entry) {
                             return std::get<1>(entry) == group_guid;
                           });
  if (iter != shared_tab_groups_waiting_for_collaboration_.end()) {
    // We are waiting for the corresponding people group to be available in
    // DataSharingService. Ignore this update and continue waiting.
    return;
  }

  if (saved_tab_group->is_transitioning_to_shared()) {
    // The group is in the process of sharing, ignore any updates to it.
    return;
  }

  if (base::Contains(empty_groups_, group_guid)) {
    empty_groups_.erase(group_guid);
    // This is the first time we are notifying the observers about the group as
    // it was empty before.
    HandleTabGroupAdded(group_guid, source);
    return;
  }

  if (tab_group_sharing_timeout_info_.contains(group_guid)) {
    // The group has been created in MakeTabGroupShared() and was waiting for
    // the server commit to complete. Now it has been committed, so the rest of
    // the flow can continue.
    HandleTabGroupAdded(group_guid, source);
    return;
  }

  UpdateLastSeenTimeForAnyFocusedTabForRemoteUpdates(saved_tab_group, source);

  // Post task is used here to avoid reentrancy. See crbug.com/373500807 for
  // details.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TabGroupSyncServiceImpl::NotifyTabGroupUpdated,
                     weak_ptr_factory_.GetWeakPtr(), group_guid, source));
}

void TabGroupSyncServiceImpl::
    UpdateLastSeenTimeForAnyFocusedTabForRemoteUpdates(
        const SavedTabGroup* group,
        TriggerSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (source != TriggerSource::REMOTE) {
    return;
  }

  if (!group->is_shared_tab_group()) {
    return;
  }

  for (const LocalTabID& local_tab_id : GetSelectedTabs()) {
    const SavedTabGroupTab* tab = group->GetTab(local_tab_id);
    if (tab) {
      model_->UpdateTabLastSeenTimeFromLocal(group->saved_guid(),
                                             tab->saved_tab_guid());
    }
  }
}

void TabGroupSyncServiceImpl::NotifyTabGroupAdded(const base::Uuid& guid,
                                                  TriggerSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const SavedTabGroup* saved_tab_group = model_->Get(guid);
  if (!saved_tab_group || saved_tab_group->saved_tabs().empty()) {
    return;
  }

  LogTabGroupEvent(logger_, "NotifyTabGroupAdded", saved_tab_group);

  // Saved tab group should be transitions to shared before notifying observers
  // because the new group may be opened automatically on some platforms.
  bool group_migrated_to_shared =
      TransitionSavedToSharedTabGroupIfNeeded(*saved_tab_group);

  bool group_migrated_to_saved =
      TransitionSharedToSavedTabGroupIfNeeded(*saved_tab_group);

  // Remove the group from the list of groups waiting for committing to the
  // server and notify that sharing succeeded.
  if (tab_group_sharing_timeout_info_.contains(guid)) {
    NotifyTabGroupSharingResult(guid, TabGroupSharingResult::kSuccess);
  }

  if (group_migrated_to_shared || group_migrated_to_saved) {
    NotifyTabGroupMigrated(saved_tab_group->saved_guid(), source);

    // Simulate tab group update after the transition to notify observers which
    // don't handle the migration case (e.g. because they don't store their
    // GUIDs).

    if (!WasTabGroupClosedLocally(guid) &&
        !saved_tab_group->local_group_id().has_value()) {
      for (TabGroupSyncService::Observer& observer : observers_) {
        observer.OnTabGroupAdded(*saved_tab_group, source);
      }
    } else {
      NotifyTabGroupUpdated(saved_tab_group->saved_guid(), source);
    }

    return;
  }

  // The group wasn't transition from any pre-existing SavedTabGroup, so it's
  // just a normal new group.
  for (TabGroupSyncService::Observer& observer : observers_) {
    observer.OnTabGroupAdded(*saved_tab_group, source);
  }
}

void TabGroupSyncServiceImpl::NotifyTabGroupUpdated(const base::Uuid& guid,
                                                    TriggerSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const SavedTabGroup* saved_tab_group = model_->Get(guid);
  if (!saved_tab_group || saved_tab_group->saved_tabs().empty()) {
    return;
  }

  if (source == TriggerSource::REMOTE) {
    for (auto& observer : observers_) {
      observer.BeforeTabGroupUpdateFromRemote(guid);
    }
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupUpdated(*saved_tab_group, source);
  }

  if (source == TriggerSource::REMOTE) {
    for (auto& observer : observers_) {
      observer.AfterTabGroupUpdateFromRemote(guid);
    }
  }
}

void TabGroupSyncServiceImpl::NotifyTabGroupMigrated(
    const base::Uuid& new_group_guid,
    TriggerSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const SavedTabGroup* new_group = model_->Get(new_group_guid);
  CHECK(new_group);
  // Originating saved tab group must exist if it was transitioned.
  CHECK(new_group->GetOriginatingTabGroupGuid().has_value());
  LogTabGroupEvent(logger_, "NotifyTabGroupMigrated", new_group);
  for (TabGroupSyncService::Observer& observer : observers_) {
    observer.OnTabGroupMigrated(
        *new_group, new_group->GetOriginatingTabGroupGuid().value(), source);
  }
}

void TabGroupSyncServiceImpl::HandleTabGroupRemoved(
    const SavedTabGroup& removed_group,
    TriggerSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LogTabGroupEvent(logger_, "HandleTabGroupRemoved", &removed_group);

  // When a group is deleted, there's no more need to keep any "was locally
  // closed" pref entry around.
  // TODO(crbug.com/363927991): This also gets called during signout, when all
  // groups that belong to the account get closed. In that case, the pref
  // entries should *not* get cleared. Currently this only works because the
  // account_id has already been cleared here, which is fragile. Ideally,
  // HandleTabGroupRemoved() would receive a "reason" param, where one of the
  // possible values would be "signout".
  RemoveLocallyClosedGroupIdFromPref(removed_group.saved_guid());

  // Clean up from the list of shared groups waiting for people group, if
  // applicable.
  std::erase_if(shared_tab_groups_waiting_for_collaboration_,
                [&](const auto& entry) {
                  return std::get<1>(entry) == removed_group.saved_guid();
                });

  if (removed_group.GetOriginatingTabGroupGuid().has_value()) {
    const SavedTabGroup* originating_group =
            model_->Get(removed_group.GetOriginatingTabGroupGuid().value());
    if (originating_group && originating_group->is_hidden()) {
      // It's possible that the originating saved tab group still exists when
      // the shared tab group is removed. This can happen if the sharing
      // operation failed on the remote client. In this case, restore the
      // originating saved tab group.
      // In case the shared tab group was unshared on the remote client, the
      // originating saved tab group will be removed by the client as well, so
      // it will be deleted from the model eventually.
      model_->RestoreHiddenGroupFromSync(originating_group->saved_guid());
    }
  }

  if (is_initialized_) {
    for (auto& observer : observers_) {
      observer.OnTabGroupRemoved(removed_group.saved_guid(), source);
    }
  }

  if (!removed_group.local_group_id().has_value()) {
    return;
  }

  // For sync initiated deletions, cache the local ID in prefs until the group
  // is closed in the UI.
  if (source == TriggerSource::REMOTE) {
    AddDeletedGroupIdToPref(removed_group.local_group_id().value(),
                            removed_group.saved_guid());
  }

  if (!is_initialized_) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupRemoved(removed_group.local_group_id().value(), source);
  }
}

std::vector<LocalTabGroupID>
TabGroupSyncServiceImpl::GetDeletedGroupIdsFromPref() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ScopedDictPrefUpdate update(pref_service_, prefs::kDeletedTabGroupIds);
  update->Set(LocalTabGroupIDToString(local_id), sync_id.AsLowercaseString());
}

void TabGroupSyncServiceImpl::RemoveDeletedGroupIdFromPref(
    const LocalTabGroupID& local_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ScopedDictPrefUpdate update(pref_service_, prefs::kDeletedTabGroupIds);
  update->Remove(LocalTabGroupIDToString(local_id));
}

void TabGroupSyncServiceImpl::AddLocallyClosedGroupIdToPref(
    const base::Uuid& sync_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::optional<GaiaId> gaia_id =
      sync_bridge_mediator_->GetGaiaIdForSavedBridge();
  if (!gaia_id) {
    // If there's no signed-in account, nothing to do.
    return;
  }
  syncer::SetAccountKeyedPrefDictEntry(
      pref_service_, prefs::kLocallyClosedRemoteTabGroupIds,
      signin::GaiaIdHash::FromGaiaId(*gaia_id),
      sync_id.AsLowercaseString().c_str(), base::Value());
}

void TabGroupSyncServiceImpl::RemoveLocallyClosedGroupIdFromPref(
    const base::Uuid& sync_id) {
  std::optional<GaiaId> gaia_id =
      sync_bridge_mediator_->GetGaiaIdForSavedBridge();
  if (!gaia_id) {
    // If there's no signed-in account, nothing to do. Most notably, this
    // happens right after sign-out, when all tab groups associated to the
    // account get closed.
    return;
  }
  syncer::RemoveAccountKeyedPrefDictEntry(
      pref_service_, prefs::kLocallyClosedRemoteTabGroupIds,
      signin::GaiaIdHash::FromGaiaId(*gaia_id),
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

void TabGroupSyncServiceImpl::SavedTabGroupTabLastSeenTimeUpdated(
    const base::Uuid& tab_id,
    TriggerSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : observers_) {
    observer.OnTabLastSeenTimeChanged(tab_id, source);
  }
}

void TabGroupSyncServiceImpl::SavedTabGroupModelLoaded() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Store a snapshot of shared tab groups before notifying anyone else that
  // the service is initialized.
  // It is not safe to use observers to listen for Observer::OnInitialized and
  // query for the model at that point for a few reasons:
  // (1) The observer might be added too late, which means some calls could
  //     already be lost.
  // (2) There is a PostTask between the model being finished and observers
  //     being informed. This means that the state could have changed before we
  //     can retrieve it.
  StoreSharedTabGroupsAvailableAtStartupForMessaging();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TabGroupSyncServiceImpl::NotifyServiceInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabGroupSyncServiceImpl::NotifyServiceInitialized() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

  if (!model_->GetSharedTabGroupsOnly().empty()) {
    RegisterPageEntityOptimizationTypeIfNeeded();
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &TabGroupSyncServiceImpl::CleanUpOriginatingSavedTabGroupsIfNeeded,
          weak_ptr_factory_.GetWeakPtr()),
      kDelayBeforeTabGroupCleanUp);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TabGroupSyncServiceImpl::RecordMetrics,
                     weak_ptr_factory_.GetWeakPtr()),
      kDelayBeforeMetricsLogged);
}

void TabGroupSyncServiceImpl::OnSyncBridgeUpdateTypeChanged(
    SyncBridgeUpdateType sync_bridge_update_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (sync_bridge_update_type ==
          SyncBridgeUpdateType::kCompletedInitialMergeThisSession &&
      sync_bridge_mediator_->GetTrackingGaiaIdForSharedBridge().has_value()) {
    while (!pending_actions_waiting_sign_in_.empty()) {
      // User just signed-in. Run any pending actions.
      auto callback = std::move(pending_actions_waiting_sign_in_.front());
      pending_actions_waiting_sign_in_.pop_front();
      std::move(callback).Run();
    }
  } else if (sync_bridge_update_type == SyncBridgeUpdateType::kDisableSync) {
    // Clear the pending actions if user signs-out instead.
    pending_actions_waiting_sign_in_.clear();
  }

  // Post this event as all other sync generated events (add/update/deletion
  // etc) are posted from this class. It's essential for the observer to receive
  // them in the same sequence as they are originated.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TabGroupSyncServiceImpl::NotifyOnSyncBridgeUpdateTypeChanged,
          weak_ptr_factory_.GetWeakPtr(), sync_bridge_update_type));
}

void TabGroupSyncServiceImpl::TabGroupTransitioningToSavedRemovedFromSync(
    const base::Uuid& saved_group_id) {
  const SavedTabGroup* saved_tab_group = model_->Get(saved_group_id);
  if (!saved_tab_group) {
    return;
  }
  CHECK(saved_tab_group->is_shared_tab_group());
  CHECK(saved_tab_group->is_transitioning_to_saved());

  if (saved_tab_group->local_group_id().has_value()) {
    OnTabGroupUnShareComplete(saved_tab_group->local_group_id().value(),
                              /*success=*/true);
  }
}

void TabGroupSyncServiceImpl::NotifyOnSyncBridgeUpdateTypeChanged(
    SyncBridgeUpdateType sync_bridge_update_type) {
  for (auto& observer : observers_) {
    observer.OnSyncBridgeUpdateTypeChanged(sync_bridge_update_type);
  }
}

void TabGroupSyncServiceImpl::OnCollaborationAvailable(
    const syncer::CollaborationId& collaboration_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If there was a shared tab group waiting for the corresponding people group,
  // proceed now to notify the UI.
  auto iter = std::find_if(shared_tab_groups_waiting_for_collaboration_.begin(),
                           shared_tab_groups_waiting_for_collaboration_.end(),
                           [&](const auto& entry) {
                             return std::get<0>(entry) == collaboration_id;
                           });
  if (iter != shared_tab_groups_waiting_for_collaboration_.end()) {
    auto [unused, group_id, source] = std::move(*iter);
    shared_tab_groups_waiting_for_collaboration_.erase(iter);
    LogTabGroupEvent(logger_, "OnCollaborationAvailable Found", group_id,
                     collaboration_id);
    HandleTabGroupAdded(group_id, source);
  } else {
    LogTabGroupEvent(logger_, "OnCollaborationAvailable Unknown", base::Uuid(),
                     collaboration_id);
  }
}

void TabGroupSyncServiceImpl::UpdateAttributions(
    const LocalTabGroupID& group_id,
    const std::optional<LocalTabID>& tab_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  model_->UpdateLastUpdaterCacheGuidForGroup(
      sync_bridge_mediator_->GetLocalCacheGuidForSavedBridge(), group_id,
      tab_id);
}

void TabGroupSyncServiceImpl::UpdateSharedAttributions(
    const LocalTabGroupID& group_id,
    const std::optional<LocalTabID>& tab_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const SavedTabGroup* group = model_->Get(group_id);
  if (!group || !group->is_shared_tab_group()) {
    return;
  }

  std::optional<GaiaId> gaia_id =
      sync_bridge_mediator_->GetTrackingGaiaIdForSharedBridge();
  if (!gaia_id) {
    return;
  }

  model_->UpdateSharedAttribution(group_id, tab_id, std::move(gaia_id.value()));
}

void TabGroupSyncServiceImpl::
    StoreSharedTabGroupsAvailableAtStartupForMessaging() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  shared_tab_groups_available_at_startup_for_messaging_ =
      std::make_unique<std::vector<SavedTabGroup>>();
  for (const SavedTabGroup* group : model_->GetSharedTabGroupsOnly()) {
    CHECK(group);

    // Dereference to create a safe copy.
    shared_tab_groups_available_at_startup_for_messaging_->push_back(*group);

    had_shared_tab_groups_on_startup_ = true;
    if (group->local_group_id().has_value()) {
      had_open_shared_tab_groups_on_startup_ = true;
    }
  }
}

void TabGroupSyncServiceImpl::RecordMetrics() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto saved_tab_groups = model_->saved_tab_groups();
  std::vector<bool> is_remote(saved_tab_groups.size());

  for (size_t i = 0; i < saved_tab_groups.size(); ++i) {
    is_remote[i] = IsRemoteDevice(saved_tab_groups[i].creator_cache_guid());
  }

  metrics_logger_->RecordMetricsOnStartup(saved_tab_groups, is_remote);
}

void TabGroupSyncServiceImpl::ForceRemoveClosedTabGroupsOnStartup() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
    model_->RemovedLocally(group_id);
  }

  metrics_logger_->RecordTabGroupDeletionsOnStartup(group_ids.size());
}

void TabGroupSyncServiceImpl::CleanUpOriginatingSavedTabGroupsIfNeeded() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsOriginatingSavedGroupCleanUpEnabled()) {
    return;
  }

  std::vector<base::Uuid> group_ids_to_delete;
  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    if (!group.is_hidden()) {
      continue;
    }

    if (group.is_shared_tab_group()) {
      continue;
    }

    if (base::Time::Now() - group.update_time() >=
        GetOriginatingSavedGroupCleanUpTimeInterval()) {
      group_ids_to_delete.push_back(group.saved_guid());
    }
  }

  // For originating tab group that are not hidden ahd shared, they need to be
  // cleaned up after some time..
  for (const auto& group_id : group_ids_to_delete) {
    LogTabGroupEvent(logger_, "CleanupOriginatingGroup", group_id,
                     std::optional<syncer::CollaborationId>());
    RemoveGroup(group_id);
  }

  FinishTransitionToSharedIfNotCompleted();
}

void TabGroupSyncServiceImpl::LogEvent(
    TabGroupEvent event,
    LocalTabGroupID group_id,
    const std::optional<LocalTabID>& tab_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!metrics_logger_) {
    DVLOG(1) << __func__ << " Metrics logger doesn't exist";
    return;
  }

  const auto* group = model_->Get(group_id);
  if (!group) {
    DVLOG(1) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const auto* tab =
      tab_id.has_value() ? group->GetTab(tab_id.value()) : nullptr;

  EventDetails event_details(event);
  event_details.local_tab_group_id = group_id;
  event_details.local_tab_id = tab_id;
  metrics_logger_->LogEvent(event_details, group, tab);
}

bool TabGroupSyncServiceImpl::TransitionSavedToSharedTabGroupIfNeeded(
    const SavedTabGroup& shared_group) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!shared_group.is_shared_tab_group()) {
    return false;
  }

  if (TransitionOriginatingTabGroupToNewGroupIfNeeded(
          shared_group, OpeningSource::kConnectOnGroupShare,
          ClosingSource::kDisconnectOnGroupShared)) {
    if (shared_group.GetOriginatingTabGroupGuid().has_value()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&TabGroupSyncServiceImpl::
                             CleanUpOriginatingSavedTabGroupsIfNeeded,
                         weak_ptr_factory_.GetWeakPtr()),
          GetOriginatingSavedGroupCleanUpTimeInterval());
    }
    return true;
  }

  return false;
}

bool TabGroupSyncServiceImpl::TransitionSharedToSavedTabGroupIfNeeded(
    const SavedTabGroup& saved_group) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(crbug.com/370746008): After replacing the originating group here,
  // it needs to be deleted.
  if (saved_group.is_shared_tab_group()) {
    return false;
  }
  return TransitionOriginatingTabGroupToNewGroupIfNeeded(
      saved_group, OpeningSource::kConnectOnGroupUnShare,
      ClosingSource::kDisconnectOnGroupUnShared);
}

bool TabGroupSyncServiceImpl::TransitionOriginatingTabGroupToNewGroupIfNeeded(
    const SavedTabGroup& tab_group,
    OpeningSource opening_source,
    ClosingSource closing_source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::optional<base::Uuid> originating_tab_group_guid =
      tab_group.GetOriginatingTabGroupGuid();
  if (!originating_tab_group_guid.has_value()) {
    return false;
  }

  const SavedTabGroup* originating_tab_group =
      model_->Get(originating_tab_group_guid.value());
  if (!originating_tab_group) {
    // Originating group doesn't exist in the model and hence it wasn't
    // transitioned. The group may not exist if it was deleted from the current
    // device before the remote shared tab group was downloaded.
    return false;
  }

  LogTabGroupEvent(logger_, "TransitionLocalIds", &tab_group);
  model_->SetGroupHidden(originating_tab_group_guid.value());

  if (originating_tab_group->local_group_id().has_value()) {
    // The group is open in the tab strip and needs to be transitioned with all
    // local IDs.

    // Make a copy because both groups will be updated.
    const LocalTabGroupID local_group_id =
        originating_tab_group->local_group_id().value();

    // Before disconnecting the local group, keep the local tab group IDs to
    // assign them later to the new group. Note that RemoveLocalTabGroupMapping
    // below will clear the local tab IDs.
    std::vector<std::optional<LocalTabID>> local_tab_ids;
    for (const SavedTabGroupTab& tab : originating_tab_group->saved_tabs()) {
      // All tabs should have local IDs but use optional to be safe.
      local_tab_ids.push_back(tab.local_tab_id());
    }

    // First, remove the local tab group mapping and then disconnect the local
    // tab group. Note that on some platforms the coordinator may call
    // RemoveLocalTabGroupMapping() but it should be a no-op.
    RemoveLocalTabGroupMapping(local_group_id, closing_source);
    coordinator_->DisconnectLocalTabGroup(local_group_id);

    // Connect the shared tab group to the local group: update the local tab
    // group mapping on all platforms, and update the mapping for session
    // restore.
    ConnectLocalTabGroup(tab_group.saved_guid(), local_group_id,
                         opening_source);

    // Assign local tab IDs on best effort basis if ConnectLocalTabGroup()
    // has not assigned them yet.
    const SavedTabGroup* transitioned_group = model_->Get(local_group_id);
    CHECK(transitioned_group);
    for (size_t i = 0; i < std::min(local_tab_ids.size(),
                                    transitioned_group->saved_tabs().size());
         ++i) {
      const SavedTabGroupTab& tab = transitioned_group->saved_tabs()[i];
      if (!tab.local_tab_id().has_value() &&
          tab.local_tab_id() != local_tab_ids[i]) {
        // Copy the tab as it's required for updating the local tab ID.
        model_->UpdateLocalTabId(transitioned_group->saved_guid(), tab,
                                 local_tab_ids[i]);
      }
    }
  }

  return true;
}

void TabGroupSyncServiceImpl::AddTabInternal(SavedTabGroupTab tab,
                                             const SavedTabGroup* group) {
  tab.SetCreatorCacheGuid(
      sync_bridge_mediator_->GetLocalCacheGuidForSavedBridge());
  UpdateTabTitleIfNeeded(*group, tab, opt_guide_,
                         stats::TitleSanitizationType::kAddTab);

  std::optional<LocalTabGroupID> local_group_id = group->local_group_id();

  if (local_group_id) {
    UpdateAttributions(*local_group_id);
  }

  if (group->is_shared_tab_group()) {
    std::optional<GaiaId> gaia_id =
        sync_bridge_mediator_->GetTrackingGaiaIdForSharedBridge();
    if (gaia_id.has_value()) {
      tab.SetUpdatedByAttribution(std::move(gaia_id.value()));
    }
  }

  // TODO(crbug.com/454431783): Add support for these API calls with the
  //  group sync ID. They currently only take local group IDs.
  if (local_group_id) {
    model_->UpdateLastUserInteractionTimeLocally(*local_group_id);
  }

  model_->AddTabToGroupLocally(group->saved_guid(), std::move(tab));

  if (local_group_id) {
    LogEvent(TabGroupEvent::kTabAdded, *local_group_id, std::nullopt);
  }
}

void TabGroupSyncServiceImpl::NavigateTabInternal(
    const LocalTabGroupID& group_id,
    const LocalTabID& tab_id,
    const GURL& url,
    const std::u16string& title,
    const GURL& previous_tab_url,
    const std::optional<proto::UrlRestriction>& url_restriction) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    DVLOG(1) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const auto* tab = group->GetTab(tab_id);
  if (!tab) {
    DVLOG(1) << __func__ << " Called for a tab that doesn't exist";
    return;
  }

  // The URL has changed after the URL restriction task is posted, early return.
  if (tab->url() != previous_tab_url) {
    return;
  }

  if (!IsUrlSyncable(url, previous_tab_url, group->is_shared_tab_group(),
                     url_restriction)) {
    return;
  }

  // Update attributions for the tab first.
  UpdateAttributions(group_id, tab_id);
  UpdateSharedAttributions(group_id, tab_id);

  // Use the builder to create the updated tab.
  bool will_update_url = url != tab->url();

  SavedTabGroupTab updated_tab(*tab);
  updated_tab.SetURL(url);
  updated_tab.SetTitle(title);
  UpdateTabTitleIfNeeded(*group, updated_tab, opt_guide_,
                         stats::TitleSanitizationType::kNavigateTab);

  model_->UpdateLastUserInteractionTimeLocally(group_id);
  model_->UpdateTabInGroup(group->saved_guid(), std::move(updated_tab),
                           /*notify_observers=*/will_update_url);
  LogEvent(TabGroupEvent::kTabNavigated, group_id, tab_id);
}

void TabGroupSyncServiceImpl::OnTabGroupSharingTimeout(
    const base::Uuid& group_guid) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!tab_group_sharing_timeout_info_.contains(group_guid)) {
    return;
  }

  NotifyTabGroupSharingResult(group_guid, TabGroupSharingResult::kTimedOut);

  const SavedTabGroup* group = model_->Get(group_guid);
  if (!group) {
    // This might happen in a corner case when the group was removed by a remote
    // device.
    return;
  }

  CHECK(group->is_shared_tab_group());

  LogTabGroupEvent(logger_, "OnTabGroupSharingTimeout", group);
  // Remove the shared group after timeout.
  model_->RemovedLocally(group->saved_guid());
}

void TabGroupSyncServiceImpl::NotifyTabGroupSharingResult(
    const base::Uuid& group_guid,
    TabGroupSharingResult result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(tab_group_sharing_timeout_info_.contains(group_guid));

  TabGroupSharingCallback callback =
      std::move(tab_group_sharing_timeout_info_[group_guid].callback);
  tab_group_sharing_timeout_info_.erase(group_guid);
  std::move(callback).Run(result);
}

std::optional<SavedTabGroup>
TabGroupSyncServiceImpl::FindGroupWithCollaborationId(
    const syncer::CollaborationId& collaboration_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    if (group.collaboration_id().has_value() &&
        group.collaboration_id().value() == collaboration_id) {
      return group;
    }
  }
  return std::nullopt;
}

void TabGroupSyncServiceImpl::FinishTransitionToSharedIfNotCompleted() {
  std::vector<base::Uuid> shared_group_with_visible_originating_group;
  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    if (group.is_hidden()) {
      continue;
    }

    if (group.saved_tabs().empty()) {
      continue;
    }

    if (!group.is_shared_tab_group() ||
        !group.GetOriginatingTabGroupGuid().has_value()) {
      continue;
    }

    base::Uuid shared_group_id = group.saved_guid();
    // If the group is waiting for sync response, let the timeout handler
    // process it.
    if (tab_group_sharing_timeout_info_.contains(shared_group_id)) {
      continue;
    }

    // If the shared group has a visible originating group, clean it up later.
    const SavedTabGroup* originating_tab_group =
        model_->Get(group.GetOriginatingTabGroupGuid().value());
    if (originating_tab_group && !originating_tab_group->is_hidden()) {
      shared_group_with_visible_originating_group.push_back(shared_group_id);
    }
  }

  // If some of the shared group is a result of unfinished migration, migrate
  // the originating tab group now.
  for (const auto& shared_group_id :
       shared_group_with_visible_originating_group) {
    const SavedTabGroup* shared_group = model_->Get(shared_group_id);
    LogTabGroupEvent(logger_, "MigrateUnfinishedSharedGroup", shared_group_id,
                     std::optional<syncer::CollaborationId>());
    if (TransitionSavedToSharedTabGroupIfNeeded(*shared_group)) {
      NotifyTabGroupMigrated(shared_group_id, TriggerSource::REMOTE);
    }
  }
}

void TabGroupSyncServiceImpl::RegisterPageEntityOptimizationTypeIfNeeded() {
  if (opt_guide_ && !page_entity_optimization_type_registered_) {
    opt_guide_->RegisterOptimizationTypes(
        {optimization_guide::proto::PAGE_ENTITIES});
    page_entity_optimization_type_registered_ = true;
  }
}

}  // namespace tab_groups
