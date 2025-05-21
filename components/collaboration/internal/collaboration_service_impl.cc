// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_service_impl.h"

#include "base/functional/callback_forward.h"
#include "base/task/single_thread_task_runner.h"
#include "components/collaboration/internal/collaboration_controller.h"
#include "components/collaboration/internal/metrics.h"
#include "components/collaboration/public/collaboration_flow_type.h"
#include "components/collaboration/public/collaboration_utils.h"
#include "components/collaboration/public/pref_names.h"
#include "components/collaboration/public/service_status.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/data_sharing_utils.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/public/group_data.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/collaboration_id.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "ui/base/device_form_factor.h"

namespace collaboration {

using data_sharing::GroupData;
using data_sharing::GroupId;
using data_sharing::GroupMember;
using data_sharing::GroupToken;
using data_sharing::MemberRole;
using Flow = CollaborationController::Flow;
using metrics::CollaborationServiceJoinEvent;
using metrics::CollaborationServiceShareOrManageEvent;
using Outcome = signin::AccountManagedStatusFinder::Outcome;
using ParseUrlResult = data_sharing::ParseUrlResult;
using ParseUrlStatus = data_sharing::ParseUrlStatus;

CollaborationServiceImpl::CollaborationServiceImpl(
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    data_sharing::DataSharingService* data_sharing_service,
    signin::IdentityManager* identity_manager,
    PrefService* profile_prefs)
    : tab_group_sync_service_(tab_group_sync_service),
      data_sharing_service_(data_sharing_service),
      identity_manager_(identity_manager),
      profile_prefs_(profile_prefs) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Initialize ServiceStatus.
  current_status_.sync_status = SyncStatus::kNotSyncing;
  current_status_.signin_status = GetSigninStatus();
  identity_manager_observer_.Observe(identity_manager_);

  current_status_.collaboration_status = GetCollaborationStatus();

  registrar_.Init(profile_prefs_);
  registrar_.Add(
      prefs::kSharedTabGroupsManagedAccountSetting,
      base::BindRepeating(&CollaborationServiceImpl::RefreshServiceStatus,
                          base::Unretained(this)));
  registrar_.Add(
      ::prefs::kSigninAllowed,
      base::BindRepeating(&CollaborationServiceImpl::RefreshServiceStatus,
                          base::Unretained(this)));
}

CollaborationServiceImpl::~CollaborationServiceImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  join_controllers_.clear();
  registrar_.RemoveAll();
}

bool CollaborationServiceImpl::IsEmptyService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return false;
}

void CollaborationServiceImpl::AddObserver(
    CollaborationService::Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.AddObserver(observer);
}

void CollaborationServiceImpl::RemoveObserver(
    CollaborationService::Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.RemoveObserver(observer);
}

void CollaborationServiceImpl::StartJoinFlow(
    std::unique_ptr<CollaborationControllerDelegate> delegate,
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const ParseUrlResult parse_result =
      data_sharing::DataSharingUtils::ParseDataSharingUrl(url);

  // Note: Invalid url parsing will start a new join flow with empty GroupToken.
  GroupToken token;
  if (parse_result.has_value() && parse_result.value().IsValid()) {
    token = parse_result.value();
  }

  CancelAllFlows(base::BindOnce(
      &CollaborationServiceImpl::StartJoinFlowInternal,
      weak_ptr_factory_.GetWeakPtr(), std::move(delegate), token));

  RecordJoinEvent(data_sharing_service_->GetLogger(),
                  CollaborationServiceJoinEvent::kStarted);
}

void CollaborationServiceImpl::StartShareOrManageFlow(
    std::unique_ptr<CollaborationControllerDelegate> delegate,
    const tab_groups::EitherGroupID& either_id,
    CollaborationServiceShareOrManageEntryPoint entry) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  metrics::RecordShareOrManageEntryPoint(data_sharing_service_->GetLogger(),
                                         entry);

  CancelAllFlows(
      base::BindOnce(&CollaborationServiceImpl::StartCollaborationFlowInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(delegate),
                     either_id, FlowType::kShareOrManage));

  RecordShareOrManageEvent(data_sharing_service_->GetLogger(),
                           CollaborationServiceShareOrManageEvent::kStarted);
}

void CollaborationServiceImpl::StartLeaveOrDeleteFlow(
    std::unique_ptr<CollaborationControllerDelegate> delegate,
    const tab_groups::EitherGroupID& either_id,
    CollaborationServiceLeaveOrDeleteEntryPoint entry) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  metrics::RecordLeaveOrDeleteEntryPoint(data_sharing_service_->GetLogger(),
                                         entry);

  CancelAllFlows(
      base::BindOnce(&CollaborationServiceImpl::StartCollaborationFlowInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(delegate),
                     either_id, FlowType::kLeaveOrDelete));
}

void CollaborationServiceImpl::CancelAllFlows(
    base::OnceCallback<void()> finish_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (join_controllers_.empty() && collaboration_controllers_.empty()) {
    // Don't post task if we can already execute `finish_callback`.
    std::move(finish_callback).Run();
    return;
  }

  for (const auto& [token, controller] : join_controllers_) {
    controller->Cancel();
  }
  for (const auto& [id, controller] : collaboration_controllers_) {
    controller->Cancel();
  }

  // Post task to execute `finish_callback` after all flows have been cancelled.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(finish_callback));
}

void CollaborationServiceImpl::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // This is invoked right after the sync service is created.
  // Update the internal status.
  sync_service_ = sync_service;
  sync_observer_.Observe(sync_service_);
  current_status_.sync_status = GetSyncStatus();
}

ServiceStatus CollaborationServiceImpl::GetServiceStatus() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return current_status_;
}

MemberRole CollaborationServiceImpl::GetCurrentUserRoleForGroup(
    const GroupId& group_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::optional<GroupData> group_data =
      data_sharing_service_->ReadGroup(group_id);
  if (!group_data.has_value() || group_data.value().members.empty()) {
    // Group do not exist or is empty.
    return MemberRole::kUnknown;
  }

  return ::collaboration::GetCurrentUserRoleForGroup(identity_manager_.get(),
                                                     group_data.value());
}

std::optional<data_sharing::GroupData> CollaborationServiceImpl::GetGroupData(
    const data_sharing::GroupId& group_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return data_sharing_service_->ReadGroup(group_id);
}

void CollaborationServiceImpl::OnStateChanged(syncer::SyncService* sync) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RefreshServiceStatus();
}

void CollaborationServiceImpl::OnSyncShutdown(syncer::SyncService* sync) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  sync_observer_.Reset();
  sync_service_ = nullptr;
}

void CollaborationServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  account_managed_status_finder_.reset();
  RefreshServiceStatus();
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      // Cancel only if the previous account was not empty.
      if (!event_details.GetPreviousState().primary_account.IsEmpty()) {
        CancelAllFlows(base::DoNothing());
      }
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      CancelAllFlows(base::DoNothing());
      break;
  }
}

void CollaborationServiceImpl::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RefreshServiceStatus();
}

void CollaborationServiceImpl::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RefreshServiceStatus();
}

void CollaborationServiceImpl::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  identity_manager_observer_.Reset();
}

void CollaborationServiceImpl::DeleteGroup(
    const data_sharing::GroupId& group_id,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  data_sharing_service_->DeleteGroup(
      group_id,
      base::BindOnce(&CollaborationServiceImpl::OnCollaborationGroupRemoved,
                     weak_ptr_factory_.GetWeakPtr(), group_id,
                     std::move(callback)));
}

void CollaborationServiceImpl::LeaveGroup(
    const data_sharing::GroupId& group_id,
    base::OnceCallback<void(bool success)> callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  data_sharing_service_->LeaveGroup(
      group_id,
      base::BindOnce(&CollaborationServiceImpl::OnCollaborationGroupRemoved,
                     weak_ptr_factory_.GetWeakPtr(), group_id,
                     std::move(callback)));
}

bool CollaborationServiceImpl::ShouldInterceptNavigationForShareURL(
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ParseUrlResult result =
      data_sharing::DataSharingUtils::ParseDataSharingUrl(url);
  if (result.has_value()) {
    return true;
  }
  switch (result.error()) {
    case ParseUrlStatus::kUnknown:
    case ParseUrlStatus::kHostOrPathMismatchFailure:
      return false;
    case ParseUrlStatus::kQueryMissingFailure:
    case ParseUrlStatus::kSuccess:
      return true;
  }
}

void CollaborationServiceImpl::HandleShareURLNavigationIntercepted(
    const GURL& url,
    std::unique_ptr<data_sharing::ShareURLInterceptionContext> context,
    CollaborationServiceJoinEntryPoint entry) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  metrics::RecordJoinEntryPoint(data_sharing_service_->GetLogger(), entry);
  data_sharing_service_->HandleShareURLNavigationIntercepted(
      url, std::move(context));
}

const std::map<data_sharing::GroupToken,
               std::unique_ptr<CollaborationController>>&
CollaborationServiceImpl::GetJoinControllersForTesting() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return join_controllers_;
}

void CollaborationServiceImpl::FinishJoinFlow(
    const data_sharing::GroupToken& token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = join_controllers_.find(token);
  if (it != join_controllers_.end()) {
    join_controllers_.erase(it);
  }
}

void CollaborationServiceImpl::FinishCollaborationFlow(
    const tab_groups::EitherGroupID& group_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = collaboration_controllers_.find(group_id);
  if (it != collaboration_controllers_.end()) {
    collaboration_controllers_.erase(it);
  }
}

SyncStatus CollaborationServiceImpl::GetSyncStatus() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!sync_service_) {
    return SyncStatus::kNotSyncing;
  }

  if (sync_service_->IsSetupInProgress()) {
    // Do not update sync status when setup is in progress.
    return current_status_.sync_status;
  }

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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SigninStatus status = SigninStatus::kNotSignedIn;

  bool has_valid_primary_account =
      identity_manager_->HasPrimaryAccountWithRefreshToken(
          signin::ConsentLevel::kSignin) &&
      !identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          identity_manager_->GetPrimaryAccountId(
              signin::ConsentLevel::kSignin));
  if (has_valid_primary_account) {
    status = SigninStatus::kSignedIn;
  } else if (identity_manager_->HasPrimaryAccount(
                 signin::ConsentLevel::kSignin)) {
    status = SigninStatus::kSignedInPaused;
  } else if (!profile_prefs_->GetBoolean(::prefs::kSigninAllowed)) {
    status = SigninStatus::kSigninDisabled;
  }

  return status;
}

CollaborationStatus CollaborationServiceImpl::GetCollaborationStatus() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Check if device policy allow signin.
  if (!profile_prefs_->GetBoolean(::prefs::kSigninAllowed) &&
      profile_prefs_->IsManagedPreference(::prefs::kSigninAllowed)) {
    return CollaborationStatus::kDisabledForPolicy;
  }

  // Disable for automotive users.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_AUTOMOTIVE &&
      !base::FeatureList::IsEnabled(
          data_sharing::features::kCollaborationAutomotive)) {
    return CollaborationStatus::kDisabled;
  }

  // TODO(haileywang): Support collaboration status updates.
  CollaborationStatus status = CollaborationStatus::kDisabled;
  if (base::FeatureList::IsEnabled(
          data_sharing::features::kDataSharingFeature)) {
    status = CollaborationStatus::kEnabledCreateAndJoin;
  } else if (base::FeatureList::IsEnabled(
                 data_sharing::features::kDataSharingJoinOnly)) {
    status = CollaborationStatus::kAllowedToJoin;
  }

  if (current_status_.signin_status == SigninStatus::kNotSignedIn) {
    return status;
  }

  CoreAccountInfo account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (!signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
          account.email)) {
    return status;
  }

  // Enterprise account handling.
  if (!account_managed_status_finder_) {
    account_managed_status_finder_ =
        std::make_unique<signin::AccountManagedStatusFinder>(
            identity_manager_, account,
            base::BindOnce(&CollaborationServiceImpl::RefreshServiceStatus,
                           weak_ptr_factory_.GetWeakPtr()),
            base::Seconds(5));
  }

  // Enterprise V2: Check enterprise policy to allow/disallow collaboration
  // feature.
  if (base::FeatureList::IsEnabled(
          data_sharing::features::kCollaborationEntrepriseV2)) {
    switch (account_managed_status_finder_->GetOutcome()) {
      case Outcome::kConsumerGmail:
      case Outcome::kConsumerWellKnown:
      case Outcome::kConsumerNotWellKnown:
        break;
      default:
        if (profile_prefs_->GetInteger(
                collaboration::prefs::kSharedTabGroupsManagedAccountSetting) ==
            static_cast<int>(
                prefs::SharedTabGroupsManagedAccountSetting::kDisabled)) {
          return CollaborationStatus::kDisabledForPolicy;
        }
    }

    return status;
  }

  // Enterprise V1: Figure out if collaboration feature is disabled by account
  // policy. This early check allows to not disable collaboration feature when
  // the user need to refresh their account (refresh tokens unavailable).
  switch (account_managed_status_finder_->GetOutcome()) {
    case Outcome::kPending:
      status = CollaborationStatus::kDisabledPending;
      break;
    case Outcome::kError:
    case Outcome::kTimeout:
      status = CollaborationStatus::kDisabled;
      break;
    case Outcome::kEnterpriseGoogleDotCom:
    case Outcome::kEnterprise:
      status = CollaborationStatus::kDisabledForPolicy;
      break;
    case Outcome::kConsumerGmail:
    case Outcome::kConsumerWellKnown:
    case Outcome::kConsumerNotWellKnown:
      break;
  }

  return status;
}

void CollaborationServiceImpl::RefreshServiceStatus() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

void CollaborationServiceImpl::StartJoinFlowInternal(
    std::unique_ptr<CollaborationControllerDelegate> delegate,
    const GroupToken& token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  join_controllers_.insert(
      {token,
       std::make_unique<CollaborationController>(
           Flow(FlowType::kJoin, token), this, data_sharing_service_.get(),
           tab_group_sync_service_.get(), sync_service_.get(),
           identity_manager_.get(), std::move(delegate),
           base::BindOnce(&CollaborationServiceImpl::FinishJoinFlow,
                          weak_ptr_factory_.GetWeakPtr(), token))});
}

void CollaborationServiceImpl::StartCollaborationFlowInternal(
    std::unique_ptr<CollaborationControllerDelegate> delegate,
    const tab_groups::EitherGroupID& either_id,
    FlowType type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  collaboration_controllers_.insert(
      {either_id,
       std::make_unique<CollaborationController>(
           Flow(type, either_id), this, data_sharing_service_.get(),
           tab_group_sync_service_.get(), sync_service_.get(),
           identity_manager_.get(), std::move(delegate),
           base::BindOnce(&CollaborationServiceImpl::FinishCollaborationFlow,
                          weak_ptr_factory_.GetWeakPtr(), either_id))});
}

void CollaborationServiceImpl::OnCollaborationGroupRemoved(
    const data_sharing::GroupId& group_id,
    base::OnceCallback<void(bool)> callback,
    data_sharing::DataSharingService::PeopleGroupActionOutcome result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (result ==
      data_sharing::DataSharingService::PeopleGroupActionOutcome::kSuccess) {
    tab_group_sync_service_->OnCollaborationRemoved(
        syncer::CollaborationId(group_id.value()));
    data_sharing_service_->OnCollaborationGroupRemoved(group_id);
    std::move(callback).Run(/*success=*/true);
    return;
  }

  std::move(callback).Run(/*success=*/false);
}

}  // namespace collaboration
