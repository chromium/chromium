// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_service_impl.h"

#include "base/functional/callback_forward.h"
#include "base/task/single_thread_task_runner.h"
#include "components/collaboration/internal/collaboration_controller.h"
#include "components/collaboration/internal/metrics.h"
#include "components/collaboration/public/collaboration_flow_type.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace collaboration {

using data_sharing::GroupData;
using data_sharing::GroupId;
using data_sharing::GroupMember;
using data_sharing::GroupToken;
using data_sharing::MemberRole;
using Flow = CollaborationController::Flow;
using metrics::CollaborationServiceJoinEvent;
using metrics::CollaborationServiceShareOrManageEvent;

CollaborationServiceImpl::CollaborationServiceImpl(
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    data_sharing::DataSharingService* data_sharing_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : tab_group_sync_service_(tab_group_sync_service),
      data_sharing_service_(data_sharing_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service) {
  // Initialize ServiceStatus.
  current_status_.collaboration_status = GetCollaborationStatus();

  current_status_.sync_status = GetSyncStatus();
  sync_observer_.Observe(sync_service_);

  current_status_.signin_status = GetSigninStatus();
  identity_manager_observer_.Observe(identity_manager_);
}

CollaborationServiceImpl::~CollaborationServiceImpl() {
  join_controllers_.clear();
}

bool CollaborationServiceImpl::IsEmptyService() {
  return false;
}

void CollaborationServiceImpl::AddObserver(
    CollaborationService::Observer* observer) {
  observers_.AddObserver(observer);
}

void CollaborationServiceImpl::RemoveObserver(
    CollaborationService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CollaborationServiceImpl::StartJoinFlow(
    std::unique_ptr<CollaborationControllerDelegate> delegate,
    const GURL& url) {
  const data_sharing::DataSharingService::ParseUrlResult parse_result =
      data_sharing_service_->ParseDataSharingUrl(url);

  GroupToken token;
  if (parse_result.has_value() && parse_result.value().IsValid()) {
    token = parse_result.value();
  }

  // TODO(crbug.com/393194653): Promote the active screen instead of closing and
  // starting a new flow if flow is ongoing.

  ExitConflictingFlows(base::BindOnce(
      &CollaborationServiceImpl::StartJoinFlowInternal,
      weak_ptr_factory_.GetWeakPtr(), std::move(delegate), token));

  RecordJoinEvent(CollaborationServiceJoinEvent::kStarted);
}

void CollaborationServiceImpl::StartShareOrManageFlow(
    std::unique_ptr<CollaborationControllerDelegate> delegate,
    const tab_groups::EitherGroupID& group_id) {
  auto it = share_controllers_.find(group_id);
  if (it != share_controllers_.end()) {
    it->second->delegate()->PromoteCurrentScreen();
    return;
  }

  ExitConflictingFlows(base::BindOnce(
      &CollaborationServiceImpl::StartShareOrManageFlowInternal,
      weak_ptr_factory_.GetWeakPtr(), std::move(delegate), group_id));

  RecordShareOrManageEvent(CollaborationServiceShareOrManageEvent::kStarted);
}

ServiceStatus CollaborationServiceImpl::GetServiceStatus() {
  return current_status_;
}

MemberRole CollaborationServiceImpl::GetCurrentUserRoleForGroup(
    const GroupId& group_id) {
  std::optional<GroupData> group_data =
      data_sharing_service_->ReadGroup(group_id);
  if (!group_data.has_value() || group_data.value().members.empty()) {
    // Group do not exist or is empty.
    return MemberRole::kUnknown;
  }

  CoreAccountInfo account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  if (account.IsEmpty()) {
    // No current logged in user.
    return MemberRole::kUnknown;
  }

  for (const GroupMember& member : group_data.value().members) {
    if (member.gaia_id == account.gaia) {
      return member.role;
    }
  }

  // Current user is not found in group.
  return MemberRole::kUnknown;
}

std::optional<data_sharing::GroupData> CollaborationServiceImpl::GetGroupData(
    const data_sharing::GroupId& group_id) {
  return data_sharing_service_->ReadGroup(group_id);
}

void CollaborationServiceImpl::OnStateChanged(syncer::SyncService* sync) {
  RefreshServiceStatus();
}

void CollaborationServiceImpl::OnSyncShutdown(syncer::SyncService* sync) {
  sync_observer_.Reset();
}

void CollaborationServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  RefreshServiceStatus();
}

void CollaborationServiceImpl::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  RefreshServiceStatus();
}

void CollaborationServiceImpl::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  RefreshServiceStatus();
}

void CollaborationServiceImpl::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  identity_manager_observer_.Reset();
}

void CollaborationServiceImpl::DeleteGroup(
    const data_sharing::GroupId& group_id,
    base::OnceCallback<void(bool)> callback) {
  data_sharing_service_->DeleteGroup(
      group_id,
      base::BindOnce(&CollaborationServiceImpl::OnCollaborationGroupRemoved,
                     weak_ptr_factory_.GetWeakPtr(), group_id,
                     std::move(callback)));
}

void CollaborationServiceImpl::LeaveGroup(
    const data_sharing::GroupId& group_id,
    base::OnceCallback<void(bool success)> callback) {
  data_sharing_service_->LeaveGroup(
      group_id,
      base::BindOnce(&CollaborationServiceImpl::OnCollaborationGroupRemoved,
                     weak_ptr_factory_.GetWeakPtr(), group_id,
                     std::move(callback)));
}

const std::map<data_sharing::GroupToken,
               std::unique_ptr<CollaborationController>>&
CollaborationServiceImpl::GetJoinControllersForTesting() {
  return join_controllers_;
}

void CollaborationServiceImpl::FinishJoinFlow(
    const data_sharing::GroupToken& token) {
  auto it = join_controllers_.find(token);
  if (it != join_controllers_.end()) {
    join_controllers_.erase(it);
  }
}

void CollaborationServiceImpl::FinishShareFlow(
    const tab_groups::EitherGroupID& group_id) {
  auto it = share_controllers_.find(group_id);
  if (it != share_controllers_.end()) {
    share_controllers_.erase(it);
  }
}

SyncStatus CollaborationServiceImpl::GetSyncStatus() {
  syncer::SyncUserSettings* user_settings = sync_service_->GetUserSettings();
  // The mapping between the selected type and what is actually sync'ed is done
  // in `GetUserSelectableTypeInfo()`.
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  if (user_settings->GetSelectedTypes().Has(
          syncer::UserSelectableType::kTabs)) {
    return SyncStatus::kSyncEnabled;
  }
#else
  if (user_settings->GetSelectedTypes().HasAll(
          {syncer::UserSelectableType::kSavedTabGroups})) {
    return SyncStatus::kSyncEnabled;
  }
#endif

  if (sync_service_->IsSyncFeatureEnabled()) {
    // Sync-the-feature is enabled, but the required data types are not.
    // The user needs to enable them in settings.
    return SyncStatus::kSyncWithoutTabGroup;
  } else {
    if (base::FeatureList::IsEnabled(
            syncer::kReplaceSyncPromosWithSignInPromos)) {
      // Sync-the-feature is not required, but the user needs to enable
      // the required data types in settings.
      return SyncStatus::kSyncWithoutTabGroup;
    } else {
      // The user needs to enable Sync-the-feature.
      return SyncStatus::kNotSyncing;
    }
  }
}

SigninStatus CollaborationServiceImpl::GetSigninStatus() {
  SigninStatus status = SigninStatus::kNotSignedIn;

  if (identity_manager_->HasPrimaryAccountWithRefreshToken(
          signin::ConsentLevel::kSignin)) {
    status = SigninStatus::kSignedIn;
  } else if (identity_manager_->HasPrimaryAccount(
                 signin::ConsentLevel::kSignin)) {
    status = SigninStatus::kSignedInPaused;
  }

  return status;
}

CollaborationStatus CollaborationServiceImpl::GetCollaborationStatus() {
  // TODO(haileywang): Support collaboration status updates.
  CollaborationStatus status = CollaborationStatus::kDisabled;
  if (base::FeatureList::IsEnabled(
          data_sharing::features::kDataSharingFeature)) {
    status = CollaborationStatus::kEnabledCreateAndJoin;
  } else if (base::FeatureList::IsEnabled(
                 data_sharing::features::kDataSharingJoinOnly)) {
    status = CollaborationStatus::kAllowedToJoin;
  }

  return status;
}

void CollaborationServiceImpl::RefreshServiceStatus() {
  ServiceStatus new_status;
  new_status.sync_status = GetSyncStatus();
  new_status.signin_status = GetSigninStatus();
  new_status.collaboration_status = GetCollaborationStatus();

  if (new_status != current_status_) {
    CollaborationService::Observer::ServiceStatusUpdate update;
    update.new_status = new_status;
    update.old_status = current_status_;
    current_status_ = new_status;
    observers_.Notify(&CollaborationService::Observer::OnServiceStatusChanged,
                      update);
  }
}

void CollaborationServiceImpl::ExitConflictingFlows(
    base::OnceCallback<void()> finish_callback) {
  if (join_controllers_.empty() && share_controllers_.empty()) {
    // Don't post task if we can already start the flow.
    std::move(finish_callback).Run();
    return;
  }

  for (const auto& [token, controller] : join_controllers_) {
    controller->Exit();
  }
  for (const auto& [id, controller] : share_controllers_) {
    controller->Exit();
  }

  // Post task to start new flow after all flows finishes.
  // Note: Invalid url parsing will start a new join flow with empty GroupToken.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(finish_callback));
}

void CollaborationServiceImpl::StartJoinFlowInternal(
    std::unique_ptr<CollaborationControllerDelegate> delegate,
    const GroupToken& token) {
  join_controllers_.insert(
      {token, std::make_unique<CollaborationController>(
                  Flow(FlowType::kJoin, token), this,
                  data_sharing_service_.get(), tab_group_sync_service_.get(),
                  sync_service_.get(), std::move(delegate),
                  base::BindOnce(&CollaborationServiceImpl::FinishJoinFlow,
                                 weak_ptr_factory_.GetWeakPtr(), token))});
}

void CollaborationServiceImpl::StartShareOrManageFlowInternal(
    std::unique_ptr<CollaborationControllerDelegate> delegate,
    const tab_groups::EitherGroupID& group_id) {
  share_controllers_.insert(
      {group_id,
       std::make_unique<CollaborationController>(
           Flow(FlowType::kShareOrManage, group_id), this,
           data_sharing_service_.get(), tab_group_sync_service_.get(),
           sync_service_.get(), std::move(delegate),
           base::BindOnce(&CollaborationServiceImpl::FinishShareFlow,
                          weak_ptr_factory_.GetWeakPtr(), group_id))});
}

void CollaborationServiceImpl::OnCollaborationGroupRemoved(
    const data_sharing::GroupId& group_id,
    base::OnceCallback<void(bool)> callback,
    data_sharing::DataSharingService::PeopleGroupActionOutcome result) {
  if (result ==
      data_sharing::DataSharingService::PeopleGroupActionOutcome::kSuccess) {
    tab_group_sync_service_->OnCollaborationRemoved(group_id.value());
    std::move(callback).Run(/*success=*/true);
    return;
  }

  std::move(callback).Run(/*success=*/false);
}

}  // namespace collaboration
