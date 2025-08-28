// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"

#include <memory>

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
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capability_fetcher.h"
#include "components/signin/public/identity_manager/account_info.h"
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

HistorySyncOptinHelper::HistorySyncOptinHelper(
    signin::IdentityManager* identity_manager,
    Profile* profile,
    const AccountInfo& account_info,
    Delegate* delegate)
    : profile_(profile),
      account_info_(account_info),
      delegate_(delegate),
      account_enterprise_policy_capability_fetcher_(std::make_unique<
                                                    AccountCapabilityFetcher>(
          identity_manager,
          account_info,
          /*get_capability_state_callback=*/
          base::BindRepeating(
              &HistorySyncOptinHelper::CanApplyAccountLevelEnterprisePolicies,
              base::Unretained(this)),
          /*on_capability_fetched_callback=*/
          base::BindOnce(
              &HistorySyncOptinHelper::StartShowHistorySyncOptinScreenFlow,
              base::Unretained(this)))) {
  CHECK(base::FeatureList::IsEnabled(switches::kEnableHistorySyncOptin));
  CHECK(delegate);
}

HistorySyncOptinHelper::~HistorySyncOptinHelper() {}

void HistorySyncOptinHelper::StartHistorySyncOptinFlow() {
  account_enterprise_policy_capability_fetcher_->FetchCapability();
}

void HistorySyncOptinHelper::StartShowHistorySyncOptinScreenFlow(
    signin::Tribool is_managed_account) {
  CHECK(is_managed_account_ == signin::Tribool::kUnknown);
  is_managed_account_ = is_managed_account;

  // TODO(crbug.com/434964019): Ensure the step of checking the state of the
  // sync service happens only after policy fetching and loading policies for
  // managed accounts.

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
            base::BindOnce(
                &HistorySyncOptinHelper::MaybeShowHistorySyncOptinScreen,
                weak_ptr_factory_.GetWeakPtr()));
    if (sync_startup_state_observer_) {
      return;
    }
  }
  MaybeShowHistorySyncOptinScreen();
}

void HistorySyncOptinHelper::MaybeShowHistorySyncOptinScreen() {
  if (is_managed_account_ != signin::Tribool::kTrue) {
    // TODO(crbug.com/434964019): Handle the managed account case by showing the
    // corresponding screen first. Deciding on the right screen might
    // require knowledge of the Sync Service's status.
    ShowHistorySyncOptinScreen();
    return;
  }
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
}

signin::Tribool HistorySyncOptinHelper::CanApplyAccountLevelEnterprisePolicies(
    const AccountInfo& account_info) {
  if (!account_info.IsEmpty()) {
    return account_info.CanApplyAccountLevelEnterprisePolicies();
  }
  return signin::Tribool::kUnknown;
}
