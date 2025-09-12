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
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper_policy_fetch_tracker.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/account_state_fetcher.h"
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

SyncServiceStartupStateObserver::~SyncServiceStartupStateObserver() = default;

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

// static
std::unique_ptr<HistorySyncOptinHelper> HistorySyncOptinHelper::Create(
    signin::IdentityManager* identity_manager,
    Profile* profile,
    const AccountInfo& account_info,
    Delegate* delegate,
    LaunchContext launch_context) {
  switch (launch_context) {
    case LaunchContext::kInBrowser:
      return std::make_unique<HistorySyncOptinHelperInBrowser>(
          identity_manager, profile, account_info, delegate);
    case LaunchContext::kInProfilePicker:
      return std::make_unique<HistorySyncOptinHelperInProfilePicker>(
          identity_manager, profile, account_info, delegate);
  }
}

HistorySyncOptinHelper::HistorySyncOptinHelper(
    signin::IdentityManager* identity_manager,
    Profile* profile,
    const AccountInfo& account_info,
    Delegate* delegate)
    : profile_(profile),
      account_info_(account_info),
      delegate_(delegate),
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
}

HistorySyncOptinHelper::~HistorySyncOptinHelper() = default;

void HistorySyncOptinHelper::StartHistorySyncOptinFlow() {
  account_state_fetcher_->FetchAccountInfo();
}

void HistorySyncOptinHelper::ResumeShowHistorySyncOptinScreenFlow(
    signin::Tribool maybe_managed_account) {
  if (maybe_managed_account == signin::Tribool::kTrue) {
    maybe_managed_account_ = maybe_managed_account;
    if (DetermineManagementStatusAndShowManagementScreens()) {
      return;
    }
  }

  // For managed users the polices are fetched when the user accepts
  // management, which is done as part of
  // `DetermineManagementStatusAndShowManagementScreens`. We are ready to get
  // the sync service's startup state.
  syncer::SyncService* sync_service = GetSyncService(profile_);
  if (sync_service) {
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
    delegate_->ShowHistorySyncOptinScreen(profile());
    return;
  }
  // TODO(crbug.com/435191375): If the purpose of the flow is to enable
  // history sync we may want to show an error when sync is disabled. Otherwise,
  // if the goal is to sign in the user, we can skip the history
  // optin screen.
  delegate_->FinishFlowWithoutHistorySyncOptin();
}

signin::Tribool HistorySyncOptinHelper::AccountIsManaged(
    const AccountInfo& account_info) {
  if (!account_info.IsEmpty()) {
    return account_info.IsManaged();
  }
  return signin::Tribool::kUnknown;
}

// HistorySyncOptinHelperInBrowser
HistorySyncOptinHelperInBrowser::HistorySyncOptinHelperInBrowser(
    signin::IdentityManager* identity_manager,
    Profile* profile,
    const AccountInfo& account_info,
    Delegate* delegate)
    : HistorySyncOptinHelper(identity_manager,
                             profile,
                             account_info,
                             delegate) {}

HistorySyncOptinHelperInBrowser::~HistorySyncOptinHelperInBrowser() = default;

bool HistorySyncOptinHelperInBrowser::
    DetermineManagementStatusAndShowManagementScreens() {
  if (!profile_management_disclaimer_service_) {
    profile_management_disclaimer_service_ =
        ProfileManagementDisclaimerServiceFactory::GetForProfile(profile());
    CHECK(profile_management_disclaimer_service_);
    base::OnceCallback<void(Profile*, bool)>
        profile_management_accepted_callback = base::BindOnce(
            &HistorySyncOptinHelperInBrowser::OnManagementAccepted,
            weak_ptr_factory_.GetWeakPtr());
    // TODO(crbug.com/434964019): The caller must ensure that we are not already
    // creating a managed profile for another account using
    // `GetAccountBeingConsideredForManagementIfAny()`.
    profile_management_disclaimer_service_->EnsureManagedProfileForAccount(
        account_info().account_id,
        // TODO(crbug.com/434964019): Plump or retrieve the right access point.
        signin_metrics::AccessPoint::kEnterpriseManagementDisclaimerAfterSignin,
        std::move(profile_management_accepted_callback));
    return true;
  }
  return false;
}

void HistorySyncOptinHelperInBrowser::OnManagementAccepted(
    Profile* chosen_profile,
    bool) {
  // `chosen_profile` is null can mean:
  // 1) the user rejects management, or
  // 2) the flow was aborted, or
  // 3) user is not managed. This cases does not apply to the
  // HistorySyncOptinHelperInBrowser since we only reach this method
  // if the user is managed.
  CHECK_EQ(maybe_managed_account(), signin::Tribool::kTrue);

  if (chosen_profile) {
    SetProfile(chosen_profile);
    ResumeShowHistorySyncOptinScreenFlow(signin::Tribool::kTrue);
    return;
  }
  delegate()->FinishFlowWithoutHistorySyncOptin();
}

// HistorySyncOptinHelperInProfilePicker
HistorySyncOptinHelperInProfilePicker::HistorySyncOptinHelperInProfilePicker(
    signin::IdentityManager* identity_manager,
    Profile* profile,
    const AccountInfo& account_info,
    Delegate* delegate)
    : HistorySyncOptinHelper(identity_manager,
                             profile,
                             account_info,
                             delegate) {}

HistorySyncOptinHelperInProfilePicker::
    ~HistorySyncOptinHelperInProfilePicker() = default;

bool HistorySyncOptinHelperInProfilePicker::
    DetermineManagementStatusAndShowManagementScreens() {
  if (!policy_helper_) {
    // Register for policies to determine if the user is managed.
    // Show the management screen for managed user, before proceeding with the
    // flow.
    policy_helper_ = std::make_unique<HistorySyncOptinPolicyHelper>(
        profile(), account_info(),
        /*on_register_for_policies_callback=*/
        base::BindOnce(&HistorySyncOptinHelperInProfilePicker::
                           MaybeShowAccountManagementScreen,
                       weak_ptr_factory_.GetWeakPtr()),
        /*on_policies_fetched_callback=*/
        base::BindOnce(&HistorySyncOptinHelperInProfilePicker::
                           ResumeShowHistorySyncOptinScreenFlow,
                       weak_ptr_factory_.GetWeakPtr(), signin::Tribool::kTrue));
    policy_helper_->RegisterForPolicies();
    return true;
  }
  return false;
}

void HistorySyncOptinHelperInProfilePicker::MaybeShowAccountManagementScreen(
    bool is_managed_account) {
  if (!is_managed_account) {
    ResumeShowHistorySyncOptinScreenFlow(signin::Tribool::kFalse);
    return;
  }
  if (is_managed_account &&
      !enterprise_util::UserAcceptedAccountManagement(profile())) {
    // If the user has not yet have accepted management, we show the appropriate
    // screen.
    ShowAccountManagementScreen();
    return;
  }
  CHECK(policy_helper_);
  policy_helper_->FetchPolicies();
}

void HistorySyncOptinHelperInProfilePicker::ShowAccountManagementScreen() {
  CHECK(!enterprise_util::UserAcceptedAccountManagement(profile()));
  CHECK_EQ(maybe_managed_account(), signin::Tribool::kTrue);
  delegate()->ShowAccountManagementScreen(base::BindOnce(
      &HistorySyncOptinHelperInProfilePicker::OnAccountManagementScreenClosed,
      weak_ptr_factory_.GetWeakPtr()));
}

void HistorySyncOptinHelperInProfilePicker::OnAccountManagementScreenClosed(
    signin::SigninChoice result) {
  switch (result) {
    case signin::SIGNIN_CHOICE_CONTINUE:
    case signin::SIGNIN_CHOICE_SIZE:
      // These cases do not apply in the profile picker flow.
      NOTREACHED();
    case signin::SIGNIN_CHOICE_CANCEL:
      // TODO(crbug.com/404806750): Handle whether the account should be
      // kept or removed and proceed with the flow which should open the
      // browser.
      enterprise_util::SetUserAcceptedAccountManagement(profile(), false);
      delegate()->FinishFlowWithoutHistorySyncOptin();
      return;
    case signin::SIGNIN_CHOICE_NEW_PROFILE:
      // Mark the user having accepted the management.
      enterprise_util::SetUserAcceptedAccountManagement(profile(), true);
      CHECK(policy_helper_);
      policy_helper_->FetchPolicies();
      return;
  }
}
