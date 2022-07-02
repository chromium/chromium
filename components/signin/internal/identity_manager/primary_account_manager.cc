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
#include "base/observer_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"

using signin::PrimaryAccountChangeEvent;

PrimaryAccountManager::PrimaryAccountManager(
    SigninClient* client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service)
    : client_(client),
      token_service_(token_service),
      account_tracker_service_(account_tracker_service) {
  DCHECK(client_);
  DCHECK(account_tracker_service_);
}

PrimaryAccountManager::~PrimaryAccountManager() {
  token_service_->RemoveObserver(this);
}

// static
void PrimaryAccountManager::RegisterProfilePrefs(PrefRegistrySimple* registry) {
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif

  bool consented =
      client_->GetPrefs()->GetBoolean(prefs::kGoogleServicesConsentedToSync);
  CoreAccountId account_id = CoreAccountId::FromString(pref_account_id);
  CoreAccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  if (consented) {
    DCHECK(!account_info.account_id.empty());
    // First reset the state, because SetSyncPrimaryAccountInternal() can
    // only be called if there is no primary account.
    SetPrimaryAccountInternal(CoreAccountInfo(), /*consented_to_sync=*/false);
    SetSyncPrimaryAccountInternal(account_info);
  } else {
    SetPrimaryAccountInternal(account_info, consented);
  }

  // It is important to only load credentials after starting to observe the
  // token service.
  token_service_->AddObserver(this);
  token_service_->LoadCredentials(
      GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      HasPrimaryAccount(signin::ConsentLevel::kSync));
}

bool PrimaryAccountManager::IsInitialized() const {
  return initialized_;
}

CoreAccountInfo PrimaryAccountManager::GetPrimaryAccountInfo(
    signin::ConsentLevel consent_level) const {
  if (!HasPrimaryAccount(consent_level))
    return CoreAccountInfo();
  return primary_account_info();
}

CoreAccountId PrimaryAccountManager::GetPrimaryAccountId(
    signin::ConsentLevel consent_level) const {
  return GetPrimaryAccountInfo(consent_level).account_id;
}

void PrimaryAccountManager::SetPrimaryAccountInfo(
    const CoreAccountInfo& account_info,
    signin::ConsentLevel consent_level) {
  if (HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    DCHECK_EQ(account_info, GetPrimaryAccountInfo(signin::ConsentLevel::kSync))
        << "Changing the primary sync account is not allowed.";
    return;
  }
  DCHECK(!account_info.account_id.empty());
  DCHECK(!account_info.gaia.empty());
  DCHECK(!account_info.email.empty());
  DCHECK(!account_tracker_service_->GetAccountInfo(account_info.account_id)
              .IsEmpty())
      << "Account must be seeded before being set as primary account";

  PrimaryAccountChangeEvent::State previous_state = GetPrimaryAccountState();
  switch (consent_level) {
    case signin::ConsentLevel::kSync:
      SetSyncPrimaryAccountInternal(account_info);
      FirePrimaryAccountChanged(previous_state);
      return;
    case signin::ConsentLevel::kSignin:
      bool account_changed = account_info != primary_account_info();
      SetPrimaryAccountInternal(account_info, /*consented_to_sync=*/false);
      if (account_changed)
        FirePrimaryAccountChanged(previous_state);
      return;
  }
}

void PrimaryAccountManager::SetSyncPrimaryAccountInternal(
    const CoreAccountInfo& account_info) {
  DCHECK(!account_info.account_id.empty());
  DCHECK(!HasPrimaryAccount(signin::ConsentLevel::kSync));

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

  // Commit primary sync account info immediately so that it does not get lost
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

bool PrimaryAccountManager::HasPrimaryAccount(
    signin::ConsentLevel consent_level) const {
  bool consented_pref =
      client_->GetPrefs()->GetBoolean(prefs::kGoogleServicesConsentedToSync);
  if (primary_account_info().account_id.empty()) {
    DCHECK(!consented_pref);
    return false;
  }
  switch (consent_level) {
    case signin::ConsentLevel::kSignin:
      return true;
    case signin::ConsentLevel::kSync:
      return consented_pref;
  }
}

void PrimaryAccountManager::UpdatePrimaryAccountInfo() {
  const CoreAccountId primary_account_id = primary_account_info().account_id;
  DCHECK(!primary_account_id.empty());

  const CoreAccountInfo updated_account_info =
      account_tracker_service_->GetAccountInfo(primary_account_id);

  CHECK_EQ(primary_account_id, updated_account_info.account_id);
  // Calling SetPrimaryAccountInternal() is avoided in this case as the
  // primary account id did not change.
  primary_account_info_ = updated_account_info;
}

void PrimaryAccountManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrimaryAccountManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void PrimaryAccountManager::ClearPrimaryAccount(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  StartSignOut(signout_source_metric, signout_delete_metric,
               RemoveAccountsOption::kRemoveAllAccounts);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

void PrimaryAccountManager::RevokeSyncConsent(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  StartSignOut(signout_source_metric, signout_delete_metric,
               RemoveAccountsOption::kKeepAllAccounts);
}

void PrimaryAccountManager::StartSignOut(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric,
    RemoveAccountsOption remove_option) {
  VLOG(1) << "StartSignOut: " << static_cast<int>(signout_source_metric) << ", "
          << static_cast<int>(signout_delete_metric) << ", "
          << static_cast<int>(remove_option);
  client_->PreSignOut(
      base::BindOnce(&PrimaryAccountManager::OnSignoutDecisionReached,
                     base::Unretained(this), signout_source_metric,
                     signout_delete_metric, remove_option),
      signout_source_metric);
}

void PrimaryAccountManager::OnSignoutDecisionReached(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric,
    RemoveAccountsOption remove_option,
    SigninClient::SignoutDecision signout_decision) {
  DCHECK(IsInitialized());

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

  PrimaryAccountChangeEvent::State previous_state = GetPrimaryAccountState();

  // Revoke all tokens before sending signed_out notification, because there
  // may be components that don't listen for token service events when the
  // profile is not connected to an account.
  switch (remove_option) {
    case RemoveAccountsOption::kRemoveAllAccounts:
      VLOG(0) << "Revoking all refresh tokens on server. Reason: sign out";
      SetPrimaryAccountInternal(CoreAccountInfo(), /*consented_to_sync=*/false);
      token_service_->RevokeAllCredentials(
          signin_metrics::SourceForRefreshTokenOperation::
              kPrimaryAccountManager_ClearAccount);
      break;
    case RemoveAccountsOption::kKeepAllAccounts:
      if (previous_state.consent_level == signin::ConsentLevel::kSignin) {
        // Nothing to update as the primary account is already at kSignin
        // consent level. Prefer returning to avoid firing useless
        // OnPrimaryAccountChanged() notifications.
        return;
      }
      SetPrimaryAccountInternal(primary_account_info(),
                                /*consented_to_sync=*/false);
      break;
  }

  DCHECK(!HasPrimaryAccount(signin::ConsentLevel::kSync));
  FirePrimaryAccountChanged(previous_state);
}

PrimaryAccountChangeEvent::State PrimaryAccountManager::GetPrimaryAccountState()
    const {
  PrimaryAccountChangeEvent::State state(primary_account_info(),
                                         signin::ConsentLevel::kSignin);
  if (HasPrimaryAccount(signin::ConsentLevel::kSync))
    state.consent_level = signin::ConsentLevel::kSync;
  return state;
}

void PrimaryAccountManager::FirePrimaryAccountChanged(
    const PrimaryAccountChangeEvent::State& previous_state) {
  PrimaryAccountChangeEvent::State current_state = GetPrimaryAccountState();
  PrimaryAccountChangeEvent event_details(previous_state, current_state);

  DCHECK(event_details.GetEventTypeFor(signin::ConsentLevel::kSync) !=
             PrimaryAccountChangeEvent::Type::kNone ||
         event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
             PrimaryAccountChangeEvent::Type::kNone)
      << "PrimaryAccountChangeEvent with no change: " << event_details;

  for (Observer& observer : observers_)
    observer.OnPrimaryAccountChanged(event_details);
}

void PrimaryAccountManager::OnRefreshTokensLoaded() {
  token_service_->RemoveObserver(this);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (account_tracker_service_->GetMigrationState() ==
      AccountTrackerService::MIGRATION_IN_PROGRESS) {
    account_tracker_service_->SetMigrationDone();
  }
#endif

  // Remove account information from the account tracker service if needed.
  if (token_service_->HasLoadCredentialsFinishedWithNoErrors()) {
    std::vector<AccountInfo> accounts_in_tracker_service =
        account_tracker_service_->GetAccounts();
    const CoreAccountId primary_account_id_ =
        GetPrimaryAccountId(signin::ConsentLevel::kSignin);
    for (const auto& account : accounts_in_tracker_service) {
      if (primary_account_id_ != account.account_id &&
          !token_service_->RefreshTokenIsAvailable(account.account_id)) {
        VLOG(0) << "Removed account from account tracker service: "
                << account.account_id;
        account_tracker_service_->RemoveAccount(account.account_id);
      }
    }
  }
}
