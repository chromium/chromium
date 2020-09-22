// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/primary_account_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/primary_account_policy_manager.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"

PrimaryAccountManager::PrimaryAccountManager(
    SigninClient* client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    signin::AccountConsistencyMethod account_consistency,
    std::unique_ptr<PrimaryAccountPolicyManager> policy_manager)
    : client_(client),
      token_service_(token_service),
      account_tracker_service_(account_tracker_service),
      initialized_(false),
#if !defined(OS_CHROMEOS)
      account_consistency_(account_consistency),
#endif
      policy_manager_(std::move(policy_manager)) {
  DCHECK(client_);
  DCHECK(account_tracker_service_);
}

PrimaryAccountManager::~PrimaryAccountManager() {
  token_service_->RemoveObserver(this);
}

// static
void PrimaryAccountManager::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGoogleServicesHostedDomain,
                               std::string());
  registry->RegisterStringPref(prefs::kGoogleServicesLastAccountId,
                               std::string());
  registry->RegisterStringPref(prefs::kGoogleServicesLastUsername,
                               std::string());
  registry->RegisterStringPref(prefs::kGoogleServicesAccountId, std::string());
  registry->RegisterBooleanPref(prefs::kGoogleServicesConsentedToSync, false);
  registry->RegisterBooleanPref(prefs::kAutologinEnabled, true);
  registry->RegisterListPref(prefs::kReverseAutologinRejectedEmailList);
  registry->RegisterBooleanPref(prefs::kSigninAllowed, true);
  registry->RegisterBooleanPref(prefs::kSignedInWithCredentialProvider, false);
}

// static
void PrimaryAccountManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGoogleServicesUsernamePattern,
                               std::string());
}

void PrimaryAccountManager::Initialize(PrefService* local_state) {
  // Should never call Initialize() twice.
  DCHECK(!IsInitialized());
  initialized_ = true;

  // If the user is clearing the token service from the command line, then
  // clear their login info also (not valid to be logged in without any
  // tokens).
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kClearTokenService))
    SetPrimaryAccountInternal(CoreAccountInfo(), false);

  std::string pref_account_id =
      client_->GetPrefs()->GetString(prefs::kGoogleServicesAccountId);

  // Initial value for the kGoogleServicesConsentedToSync preference if it is
  // missing.
  const PrefService::Preference* consented_pref =
      client_->GetPrefs()->FindPreference(
          prefs::kGoogleServicesConsentedToSync);
  if (consented_pref->IsDefaultValue()) {
    client_->GetPrefs()->SetBoolean(prefs::kGoogleServicesConsentedToSync,
                                    !pref_account_id.empty());
  }

  if (!pref_account_id.empty()) {
    if (account_tracker_service_->GetMigrationState() ==
        AccountTrackerService::MIGRATION_IN_PROGRESS) {
      CoreAccountInfo account_info =
          account_tracker_service_->FindAccountInfoByEmail(pref_account_id);
      // |account_info.gaia| could be empty if |account_id| is already gaia id.
      if (!account_info.gaia.empty()) {
        pref_account_id = account_info.gaia;
        client_->GetPrefs()->SetString(prefs::kGoogleServicesAccountId,
                                       account_info.gaia);
      }
    }
  }

  bool consented =
      client_->GetPrefs()->GetBoolean(prefs::kGoogleServicesConsentedToSync);
  CoreAccountId account_id = CoreAccountId::FromString(pref_account_id);
  CoreAccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  if (consented) {
    DCHECK(!account_info.account_id.empty());
    // First reset the state, because SetAuthenticatedAccountInfo can only be
    // called if the user is not already signed in.
    SetPrimaryAccountInternal(CoreAccountInfo(), /*consented=*/false);
    SetAuthenticatedAccountInfo(account_info);
  } else {
    SetPrimaryAccountInternal(account_info, consented);
  }

  if (policy_manager_) {
    policy_manager_->InitializePolicy(local_state, this);
  }
  // It is important to only load credentials after starting to observe the
  // token service.
  token_service_->AddObserver(this);
  token_service_->LoadCredentials(GetAuthenticatedAccountId());
}

bool PrimaryAccountManager::IsInitialized() const {
  return initialized_;
}

CoreAccountInfo PrimaryAccountManager::GetAuthenticatedAccountInfo() const {
  if (!IsAuthenticated())
    return CoreAccountInfo();
  return primary_account_info();
}

CoreAccountId PrimaryAccountManager::GetAuthenticatedAccountId() const {
  return GetAuthenticatedAccountInfo().account_id;
}

CoreAccountInfo PrimaryAccountManager::GetUnconsentedPrimaryAccountInfo()
    const {
  return primary_account_info();
}

bool PrimaryAccountManager::HasUnconsentedPrimaryAccount() const {
  return !primary_account_info().account_id.empty();
}

void PrimaryAccountManager::SetUnconsentedPrimaryAccountInfo(
    CoreAccountInfo account_info) {
  if (IsAuthenticated()) {
    DCHECK_EQ(account_info, GetAuthenticatedAccountInfo());
    return;
  }

  bool account_changed = account_info != primary_account_info();
  SetPrimaryAccountInternal(account_info, /*consented_to_sync=*/false);

  if (account_changed) {
    for (Observer& observer : observers_)
      observer.UnconsentedPrimaryAccountChanged(primary_account_info());
  }
}

void PrimaryAccountManager::SetAuthenticatedAccountInfo(
    const CoreAccountInfo& account_info) {
  DCHECK(!account_info.account_id.empty());
  DCHECK(!IsAuthenticated());

#if DCHECK_IS_ON()
  {
    std::string pref_account_id =
        client_->GetPrefs()->GetString(prefs::kGoogleServicesAccountId);
    bool consented_to_sync =
        client_->GetPrefs()->GetBoolean(prefs::kGoogleServicesConsentedToSync);

    DCHECK(pref_account_id.empty() || !consented_to_sync ||
           pref_account_id == account_info.account_id.ToString())
        << "account_id=" << account_info.account_id
        << " pref_account_id=" << pref_account_id;
  }
#endif  // DCHECK_IS_ON()

  SetPrimaryAccountInternal(account_info, /*consented_to_sync=*/true);

  // Go ahead and update the last signed in account info here as well. Once a
  // user is signed in the corresponding preferences should match. Doing it here
  // as opposed to on signin allows us to catch the upgrade scenario.
  client_->GetPrefs()->SetString(prefs::kGoogleServicesLastAccountId,
                                 account_info.account_id.ToString());
  client_->GetPrefs()->SetString(prefs::kGoogleServicesLastUsername,
                                 account_info.email);

  // Commit authenticated account info immediately so that it does not get lost
  // if Chrome crashes before the next commit interval.
  client_->GetPrefs()->CommitPendingWrite();
}

void PrimaryAccountManager::SetPrimaryAccountInternal(
    const CoreAccountInfo& account_info,
    bool consented_to_sync) {
  primary_account_info_ = account_info;

  PrefService* prefs = client_->GetPrefs();
  const std::string& account_id = primary_account_info_.account_id.ToString();
  if (account_id.empty()) {
    DCHECK(!consented_to_sync);
    prefs->ClearPref(prefs::kGoogleServicesAccountId);
    prefs->ClearPref(prefs::kGoogleServicesConsentedToSync);
  } else {
    prefs->SetString(prefs::kGoogleServicesAccountId, account_id);
    prefs->SetBoolean(prefs::kGoogleServicesConsentedToSync, consented_to_sync);
  }
}

bool PrimaryAccountManager::IsAuthenticated() const {
  bool consented_pref =
      client_->GetPrefs()->GetBoolean(prefs::kGoogleServicesConsentedToSync);
  DCHECK(!consented_pref || !primary_account_info().account_id.empty());
  return consented_pref;
}

void PrimaryAccountManager::SignIn(const std::string& username) {
  CoreAccountInfo info =
      account_tracker_service_->FindAccountInfoByEmail(username);
  DCHECK(!info.gaia.empty());
  DCHECK(!info.email.empty());
  DCHECK(!info.account_id.empty());
  if (IsAuthenticated()) {
    DCHECK_EQ(info.account_id, GetAuthenticatedAccountId())
        << "Changing the authenticated account while it is not allowed.";
    return;
  }

  bool account_changed = info != primary_account_info();
  SetAuthenticatedAccountInfo(info);

  for (Observer& observer : observers_) {
    if (account_changed)
      observer.UnconsentedPrimaryAccountChanged(info);
    observer.GoogleSigninSucceeded(info);
  }
}

void PrimaryAccountManager::UpdateAuthenticatedAccountInfo() {
  DCHECK(!primary_account_info().account_id.empty());
  DCHECK(IsAuthenticated());
  const CoreAccountInfo info = account_tracker_service_->GetAccountInfo(
      primary_account_info().account_id);
  DCHECK_EQ(info.account_id, primary_account_info().account_id);
  SetPrimaryAccountInternal(info, /*consented_to_sync=*/true);
}

void PrimaryAccountManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrimaryAccountManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

#if !defined(OS_CHROMEOS)
void PrimaryAccountManager::SignOut(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  RemoveAccountsOption remove_option =
      (account_consistency_ == signin::AccountConsistencyMethod::kDice)
          ? RemoveAccountsOption::kRemoveAuthenticatedAccountIfInError
          : RemoveAccountsOption::kRemoveAllAccounts;
  StartSignOut(signout_source_metric, signout_delete_metric, remove_option);
}

void PrimaryAccountManager::SignOutAndRemoveAllAccounts(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  StartSignOut(signout_source_metric, signout_delete_metric,
               RemoveAccountsOption::kRemoveAllAccounts);
}

void PrimaryAccountManager::SignOutAndKeepAllAccounts(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  StartSignOut(signout_source_metric, signout_delete_metric,
               RemoveAccountsOption::kKeepAllAccounts);
}
#endif  // !defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
void PrimaryAccountManager::RevokeSyncConsent() {
  DCHECK(IsAuthenticated());
  // TODO(https://crbug.com/1046746): Don't record metrics here.
  StartSignOut(signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS,
               signin_metrics::SignoutDelete::KEEPING,
               RemoveAccountsOption::kKeepAllAccounts,
               /*assert_signout_allowed=*/true);
}
#endif  // defined(OS_CHROMEOS)

void PrimaryAccountManager::StartSignOut(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric,
    RemoveAccountsOption remove_option,
    bool assert_signout_allowed) {
  VLOG(1) << "StartSignOut: " << static_cast<int>(signout_source_metric) << ", "
          << static_cast<int>(signout_delete_metric) << ", "
          << static_cast<int>(remove_option);
  if (IsAuthenticated()) {
    client_->PreSignOut(
        base::BindOnce(&PrimaryAccountManager::OnSignoutDecisionReached,
                       base::Unretained(this), signout_source_metric,
                       signout_delete_metric, remove_option,
                       assert_signout_allowed),
        signout_source_metric);
  } else {
    // Sign-out is always allowed if there's only unconsented primary account
    // without sync consent, so skip calling PreSignOut.
    OnSignoutDecisionReached(signout_source_metric, signout_delete_metric,
                             remove_option, assert_signout_allowed,
                             SigninClient::SignoutDecision::ALLOW_SIGNOUT);
  }
}

void PrimaryAccountManager::OnSignoutDecisionReached(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric,
    RemoveAccountsOption remove_option,
    bool assert_signout_allowed,
    SigninClient::SignoutDecision signout_decision) {
  DCHECK(IsInitialized());
  if (assert_signout_allowed)
    DCHECK_EQ(SigninClient::SignoutDecision::ALLOW_SIGNOUT, signout_decision);

  VLOG(1) << "OnSignoutDecisionReached: "
          << (signout_decision == SigninClient::SignoutDecision::ALLOW_SIGNOUT);
  signin_metrics::LogSignout(signout_source_metric, signout_delete_metric);
  if (primary_account_info().IsEmpty()) {
    return;
  }

  // TODO(crbug.com/887756): Consider moving this higher up, or document why
  // the above blocks are exempt from the |signout_decision| early return.
  if (signout_decision == SigninClient::SignoutDecision::DISALLOW_SIGNOUT) {
    VLOG(1) << "Ignoring attempt to sign out while signout disallowed";
    return;
  }

  const CoreAccountInfo account_info = primary_account_info();
  client_->GetPrefs()->ClearPref(prefs::kGoogleServicesHostedDomain);
  // Revoke the sync consent.
  if (IsAuthenticated())
    SetPrimaryAccountInternal(account_info, /*consented_to_sync=*/false);

  // Revoke all tokens before sending signed_out notification, because there
  // may be components that don't listen for token service events when the
  // profile is not connected to an account.
  switch (remove_option) {
    case RemoveAccountsOption::kRemoveAllAccounts:
      VLOG(0) << "Revoking all refresh tokens on server. Reason: sign out";
      SetUnconsentedPrimaryAccountInfo(CoreAccountInfo());
      token_service_->RevokeAllCredentials(
          signin_metrics::SourceForRefreshTokenOperation::
              kPrimaryAccountManager_ClearAccount);
      break;
    case RemoveAccountsOption::kRemoveAuthenticatedAccountIfInError:
      if (token_service_->RefreshTokenHasError(account_info.account_id)) {
        SetUnconsentedPrimaryAccountInfo(CoreAccountInfo());
        token_service_->RevokeCredentials(
            account_info.account_id,
            signin_metrics::SourceForRefreshTokenOperation::
                kPrimaryAccountManager_ClearAccount);
      }
      break;
    case RemoveAccountsOption::kKeepAllAccounts:
      // Do nothing.
      break;
  }

  for (Observer& observer : observers_)
    observer.GoogleSignedOut(account_info);
}

#if !defined(OS_CHROMEOS)
void PrimaryAccountManager::OnRefreshTokensLoaded() {
  token_service_->RemoveObserver(this);

  if (account_tracker_service_->GetMigrationState() ==
      AccountTrackerService::MIGRATION_IN_PROGRESS) {
    account_tracker_service_->SetMigrationDone();
  }

  // Remove account information from the account tracker service if needed.
  if (token_service_->HasLoadCredentialsFinishedWithNoErrors()) {
    std::vector<AccountInfo> accounts_in_tracker_service =
        account_tracker_service_->GetAccounts();
    const CoreAccountId authenticated_account_id = GetAuthenticatedAccountId();
    for (const auto& account : accounts_in_tracker_service) {
      if (authenticated_account_id != account.account_id &&
          !token_service_->RefreshTokenIsAvailable(account.account_id)) {
        VLOG(0) << "Removed account from account tracker service: "
                << account.account_id;
        account_tracker_service_->RemoveAccount(account.account_id);
      }
    }
  }
}
#endif  // !defined(OS_CHROMEOS)
