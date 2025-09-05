// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker_impl.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper_policy_fetch_tracker.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/account_state_fetcher.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/service/sync_service.h"

namespace {
bool AccountMayHaveCloudPolicies(Profile* profile,
                                 const AccountInfo& account_info) {
  return signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
             account_info.email) ||
         policy::ManagementServiceFactory::GetForProfile(profile)
             ->HasManagementAuthority(
                 policy::EnterpriseManagementAuthority::CLOUD) ||
         policy::ManagementServiceFactory::GetForProfile(profile)
             ->HasManagementAuthority(
                 policy::EnterpriseManagementAuthority::CLOUD_DOMAIN) ||
         policy::ManagementServiceFactory::GetForPlatform()
             ->HasManagementAuthority(
                 policy::EnterpriseManagementAuthority::CLOUD) ||
         policy::ManagementServiceFactory::GetForPlatform()
             ->HasManagementAuthority(
                 policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
}

syncer::SyncService* GetSyncService(Profile* profile) {
  return SyncServiceFactory::IsSyncAllowed(profile)
             ? SyncServiceFactory::GetForProfile(profile)
             : nullptr;
}
}  // namespace

SyncServiceStartupStateObserver::SyncServiceStartupStateObserver(
    syncer::SyncService* sync_service,
    base::OnceClosure on_state_updated_callback)
    : on_state_updated_callback_(std::move(on_state_updated_callback)) {
  CHECK(sync_service);
  CHECK(on_state_updated_callback_);

  sync_startup_tracker_ = std::make_unique<SyncStartupTracker>(
      sync_service,
      base::BindOnce(
          &SyncServiceStartupStateObserver::OnSyncStartupStateChanged,
          weak_pointer_factory_.GetWeakPtr()));
  return;
}

SyncServiceStartupStateObserver::~SyncServiceStartupStateObserver() {}

// static
std::unique_ptr<SyncServiceStartupStateObserver>
SyncServiceStartupStateObserver::
    MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
        syncer::SyncService* sync_service,
        Profile* profile,
        const AccountInfo& account_info,
        base::OnceClosure callback) {
  if (AccountMayHaveCloudPolicies(profile, account_info) &&
      SyncStartupTracker::GetServiceStartupState(sync_service) ==
          SyncStartupTracker::ServiceStartupState::kPending) {
    return std::make_unique<SyncServiceStartupStateObserver>(
        sync_service, std::move(callback));
  }
  return nullptr;
}

void SyncServiceStartupStateObserver::OnSyncStartupStateChanged(
    SyncStartupTracker::ServiceStartupState state) {
  switch (state) {
    case SyncStartupTracker::ServiceStartupState::kPending:
      NOTREACHED();
    case SyncStartupTracker::ServiceStartupState::kTimeout:
      DVLOG(1) << "Waiting for Sync Service to start timed out.";
      [[fallthrough]];
    case SyncStartupTracker::ServiceStartupState::kError:
    case SyncStartupTracker::ServiceStartupState::kComplete:
      std::move(on_state_updated_callback_).Run();
  }
}

HistorySyncOptinPolicyHelper::HistorySyncOptinPolicyHelper(
    Profile* profile,
    const AccountInfo& account_info,
    base::OnceCallback<void(bool)> on_register_for_policies_callback,
    base::OnceClosure on_policies_fetched_callback)
    : profile_(profile),
      account_info_(account_info),
      on_register_for_policies_callback_(
          std::move(on_register_for_policies_callback)),
      on_policies_fetched_callback_(std::move(on_policies_fetched_callback)) {
  CHECK(!on_register_for_policies_callback_.is_null());
  CHECK(!on_policies_fetched_callback_.is_null());
}

HistorySyncOptinPolicyHelper::~HistorySyncOptinPolicyHelper() = default;

void HistorySyncOptinPolicyHelper::RegisterForPolicies() {
  CHECK(!policy_fetch_tracker_);
  policy_fetch_tracker_ = TurnSyncOnHelperPolicyFetchTracker::CreateInstance(
      profile_, account_info_);
  policy_fetch_tracker_->RegisterForPolicy(
      std::move(on_register_for_policies_callback_));
}

void HistorySyncOptinPolicyHelper::FetchPolicies() {
  CHECK(policy_fetch_tracker_);
  CHECK(!on_policies_fetched_callback_.is_null());
  bool fetch_started = policy_fetch_tracker_->FetchPolicy(
      std::move(on_policies_fetched_callback_));
  CHECK(fetch_started);
}

HistorySyncOptinHelper::HistorySyncOptinHelper(
    signin::IdentityManager* identity_manager,
    Profile* profile,
    const AccountInfo& account_info,
    Delegate* delegate,
    LaunchContext launch_context)
    : profile_(profile),
      account_info_(account_info),
      delegate_(delegate),
      launch_context_(launch_context),
      account_state_fetcher_(std::make_unique<AccountStateFetcher>(
          identity_manager,
          account_info,
          /*get_account_state_callback=*/
          base::BindRepeating(&HistorySyncOptinHelper::AccountIsManaged,
                              base::Unretained(this)),
          /*on_account_info_fetched_callback=*/
          base::BindOnce(
              &HistorySyncOptinHelper::ResumeShowHistorySyncOptinScreenFlow,
              base::Unretained(this)))) {
  CHECK(base::FeatureList::IsEnabled(switches::kEnableHistorySyncOptin));
  CHECK(delegate);
  // TODO(crbug.com/434964019): HistorySyncOptinHelper will have two different
  // implementations based on the launch_context: One for the picker and one for
  // the browser cases.
}

HistorySyncOptinHelper::~HistorySyncOptinHelper() = default;

void HistorySyncOptinHelper::StartHistorySyncOptinFlow() {
  account_state_fetcher_->FetchAccountInfo();
}

void HistorySyncOptinHelper::MaybeShowAccountManagementScreen(
    bool is_managed_account) {
  if (!is_managed_account) {
    ResumeShowHistorySyncOptinScreenFlow(signin::Tribool::kFalse);
    return;
  }
  if (is_managed_account &&
      !enterprise_util::UserAcceptedAccountManagement(profile_)) {
    // If the user has not yet have accepted management, we show the appropriate
    // screen.
    ShowAccountManagementScreen();
    return;
  }
  CHECK(policy_helper_);
  policy_helper_->FetchPolicies();
}

void HistorySyncOptinHelper::ResumeShowHistorySyncOptinScreenFlow(
    signin::Tribool maybe_managed_account) {
  if (maybe_managed_account == signin::Tribool::kTrue && !policy_helper_ &&
      launch_context_ == LaunchContext::kInProfilePicker) {
    // Register for policies to determine if the user is managed.
    // Show the management screen for managed user, before proceeding with the
    // flow.
    policy_helper_ = std::make_unique<HistorySyncOptinPolicyHelper>(
        profile_, account_info_,
        /*on_register_for_policies_callback=*/
        base::BindOnce(
            &HistorySyncOptinHelper::MaybeShowAccountManagementScreen,
            weak_ptr_factory_.GetWeakPtr()),
        /*on_policies_fetched_callback=*/
        base::BindOnce(
            &HistorySyncOptinHelper::ResumeShowHistorySyncOptinScreenFlow,
            weak_ptr_factory_.GetWeakPtr(), signin::Tribool::kTrue));
    policy_helper_->RegisterForPolicies();
    return;
  }

  syncer::SyncService* sync_service = GetSyncService(profile_);
  if (sync_service) {
    // For managed users and users on enterprise machines that might have cloud
    // policies, it is important to wait until sync is initialized so that the
    // confirmation UI can be aware of startup errors. Since all users can be
    // subjected to cloud policies through device or browser management (CBCM),
    // this is needed to make sure that all cloud policies are loaded before any
    // dialog is shown to check whether sync was disabled by admin. Only wait
    // for cloud policies because local policies are instantly available. See
    // http://crbug.com/812546
    sync_startup_state_observer_ = SyncServiceStartupStateObserver::
        MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
            sync_service, profile_, account_info_,
            base::BindOnce(&HistorySyncOptinHelper::ShowHistorySyncOptinScreen,
                           weak_ptr_factory_.GetWeakPtr()));
    if (sync_startup_state_observer_) {
      return;
    }
  }
  ShowHistorySyncOptinScreen();
}

void HistorySyncOptinHelper::ShowHistorySyncOptinScreen() {
  if (GetSyncService(profile_)) {
    delegate_->ShowHistorySyncOptinScreen();
    return;
  }
  // TODO(crbug.com/435191375): If the purpose of the flow is to enable
  // history sync we may want to show an error when sync is disabled. Otherwise,
  // if the goal is to sign in the user, we can skip the history
  // optin screen.
  delegate_->FinishFlowWithoutHistorySyncOptin();
}

void HistorySyncOptinHelper::ShowAccountManagementScreen() {
  CHECK_EQ(launch_context_, LaunchContext::kInProfilePicker);
  CHECK(!enterprise_util::UserAcceptedAccountManagement(profile_));
  delegate_->ShowAccountManagementScreen(
      base::BindOnce(&HistorySyncOptinHelper::OnAccountManagementScreenClosed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HistorySyncOptinHelper::OnAccountManagementScreenClosed(
    signin::SigninChoice result) {
  switch (result) {
    case signin::SIGNIN_CHOICE_CONTINUE:
    case signin::SIGNIN_CHOICE_SIZE:
      // These cases do not apply in the profile picker flow.
      // TODO(crbug.com/404806750): Enforce that this method is only invoked
      // for the profile picker case.
      NOTREACHED();
    case signin::SIGNIN_CHOICE_CANCEL:
      // TODO(crbug.com/404806750): Handle whether the account should be
      // kept or removed and proceed with the flow which should open the
      // browser.
      delegate_->FinishFlowWithoutHistorySyncOptin();
      return;
    case signin::SIGNIN_CHOICE_NEW_PROFILE:
      // Mark the user having accepted the management.
      enterprise_util::SetUserAcceptedAccountManagement(profile_, true);
      CHECK(policy_helper_);
      policy_helper_->FetchPolicies();
      return;
  }
}

signin::Tribool HistorySyncOptinHelper::AccountIsManaged(
    const AccountInfo& account_info) {
  if (!account_info.IsEmpty()) {
    return account_info.IsManaged();
  }
  return signin::Tribool::kUnknown;
}
