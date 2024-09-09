// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/primary_account_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
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
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/core_account_id.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

using signin::PrimaryAccountChangeEvent;

BASE_FEATURE(kRestorePrimaryAccountInfo,
             "RestorePrimaryAccountInfo",
             base::FEATURE_ENABLED_BY_DEFAULT);
namespace {

// Registers that the sign in occurred with an explicit user action.
// Affected by all signin sources except when signing in to Chrome caused by a
// web sign in or by an unknown source.
// Note: This value is potentially set before the
// `switches::kExplicitBrowserSigninUIOnDesktop` is enabled. It is currently not
// expected to be used and is logged for potential usages in the future.
const char kExplicitBrowserSigninWithoutFeatureEnabled[] =
    "signin.explicit_browser_signin";

enum class InitializePrefState {
  kWithPrimaryAccountId_NotConsentedForSync = 0,
  kWithPrimaryAccountId_ConsentedForSync = 1,
  kEmptyPrimaryAccountId_NotConsentedForSync = 2,
  kEmptyPrimaryAccountId_ConsentedForSync = 3,
  kMaxValue = kEmptyPrimaryAccountId_ConsentedForSync,
};

void LogPrimaryAccountChangeMetrics(PrimaryAccountChangeEvent event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case PrimaryAccountChangeEvent::Type::kNone:
      break;

    case PrimaryAccountChangeEvent::Type::kSet:
      if (!event_details.GetPreviousState().primary_account.IsEmpty()) {
        // TODO(crbug.com/40202341): Add dedicated logging for account change
        // events.
        DVLOG(1) << "Signin metrics: Not logging account change";
        break;
      }

      DCHECK(event_details.GetSetPrimaryAccountAccessPoint().has_value());
      base::UmaHistogramEnumeration(
          "Signin.SignIn.Completed",
          event_details.GetSetPrimaryAccountAccessPoint().value(),
          signin_metrics::AccessPoint::ACCESS_POINT_MAX);
      break;

    case PrimaryAccountChangeEvent::Type::kCleared:
      DCHECK(event_details.GetClearPrimaryAccountSource().has_value());
      base::UmaHistogramEnumeration(
          "Signin.SignOut.Completed",
          event_details.GetClearPrimaryAccountSource().value());
      break;
  }

  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSync)) {
    case PrimaryAccountChangeEvent::Type::kNone:
      break;

    case PrimaryAccountChangeEvent::Type::kSet:
      DCHECK(event_details.GetSetPrimaryAccountAccessPoint().has_value());
      base::UmaHistogramEnumeration(
          "Signin.SyncOptIn.Completed",
          event_details.GetSetPrimaryAccountAccessPoint().value(),
          signin_metrics::AccessPoint::ACCESS_POINT_MAX);
      break;

    case PrimaryAccountChangeEvent::Type::kCleared:
      DCHECK(event_details.GetClearPrimaryAccountSource().has_value());
      base::UmaHistogramEnumeration(
          "Signin.SyncTurnOff.Completed",
          event_details.GetClearPrimaryAccountSource().value());
      break;
  }
}

void LogPrimaryAccountPrefsOnInitialize(const std::string& pref_account_id,
                                        bool pref_consented_to_sync) {
  if (pref_account_id.empty()) {
    base::UmaHistogramEnumeration(
        "Signin.PAMInitialize.PrimaryAccountPrefs",
        pref_consented_to_sync
            ? InitializePrefState::kEmptyPrimaryAccountId_ConsentedForSync
            : InitializePrefState::kEmptyPrimaryAccountId_NotConsentedForSync);
  } else {
    base::UmaHistogramEnumeration(
        "Signin.PAMInitialize.PrimaryAccountPrefs",
        pref_consented_to_sync
            ? InitializePrefState::kWithPrimaryAccountId_ConsentedForSync
            : InitializePrefState::kWithPrimaryAccountId_NotConsentedForSync);
  }
}

}  // namespace

// A wrapper around PrefService that sets prefs only when updated. It can be
// configured to commit writes for the updated values on destruction.
//
// In this case: if pref values are updated, commit will happen on destruction
// and `commit_done_callback` is called when the prefs are written to the
// persistent storage; if no pref has been updated, the callback will simply be
// called during `ScopedPrefCommit`'s destruction.
//
// If `commit_done_callback` has a non-null value, `commit_on_destroy` must be
// true.
class PrimaryAccountManager::ScopedPrefCommit {
 public:
  ScopedPrefCommit(
      PrefService* pref_service,
      bool commit_on_destroy,
      base::OnceClosure commit_done_callback = base::NullCallback())
      : pref_service_(pref_service),
        commit_on_destroy_(commit_on_destroy),
        commit_done_callback_(std::move(commit_done_callback)) {
    if (commit_done_callback_) {
      // If `commit_on_destroy` is false, no commit will be done by
      // `ScopedPrefCommit` so the commit-related callback will not be called.
      // This CHECK ensures that the callback is not used (and expected to run)
      // in this case.
      CHECK(commit_on_destroy_);
    }
  }

  ~ScopedPrefCommit() {
    if (commit_on_destroy_ && need_commit_) {
      pref_service_->CommitPendingWrite(std::move(commit_done_callback_),
                                        base::NullCallback());
    } else if (!need_commit_ && commit_done_callback_) {
      std::move(commit_done_callback_).Run();
    }
  }

  void SetBoolean(const std::string& path, bool value) {
    if (pref_service_->GetBoolean(path) == value) {
      return;
    }

    need_commit_ = true;
    pref_service_->SetBoolean(path, value);
  }

  void SetString(const std::string& path, const std::string& value) {
    if (pref_service_->GetString(path) == value) {
      return;
    }

    need_commit_ = true;
    pref_service_->SetString(path, value);
  }

  void ClearPref(const std::string& path) {
    if (!pref_service_->HasPrefPath(path)) {
      return;
    }

    need_commit_ = true;
    pref_service_->ClearPref(path);
  }

 private:
  raw_ptr<PrefService> pref_service_ = nullptr;
  bool need_commit_ = false;
  bool commit_on_destroy_ = false;
  base::OnceClosure commit_done_callback_ = base::NullCallback();
};

PrimaryAccountManager::PrimaryAccount::PrimaryAccount(
    const CoreAccountInfo& account_info,
    bool consented_to_sync)
    : account_info(account_info), consented_to_sync(consented_to_sync) {}

PrimaryAccountManager::PrimaryAccountManager(
    SigninClient* client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service)
    : client_(client),
      token_service_(token_service),
      account_tracker_service_(account_tracker_service) {
  DCHECK(client_);
  DCHECK(account_tracker_service_);

  // Clear the pref it is was set and the feature is now off.
  if (!switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    ScopedPrefCommit scoped_pref_commit(client_->GetPrefs(),
                                        /*commit_on_destroy=*/false);
    scoped_pref_commit.ClearPref(prefs::kExplicitBrowserSignin);
    scoped_pref_commit.ClearPref(
        prefs::kCookieClearOnExitMigrationNoticeComplete);
  } else {
    signin_allowed_.Init(
        prefs::kSigninAllowed, client_->GetPrefs(),
        base::BindRepeating(&PrimaryAccountManager::OnSigninAllowedPrefChanged,
                            base::Unretained(this)));
  }

  // Prepare prefs before loading them.
  PrepareToLoadPrefs();

  PrefService* prefs = client_->GetPrefs();
  std::string pref_account_id =
      prefs->GetString(prefs::kGoogleServicesAccountId);
  bool pref_consented_to_sync =
      prefs->GetBoolean(prefs::kGoogleServicesConsentedToSync);
  LogPrimaryAccountPrefsOnInitialize(pref_account_id, pref_consented_to_sync);

  ScopedPrefCommit scoped_pref_commit(client_->GetPrefs(),
                                      /*commit_on_destroy=*/false);
  if (pref_account_id.empty()) {
    SetPrimaryAccountInternal(CoreAccountInfo(), /*consented_to_sync=*/false,
                              scoped_pref_commit);
  } else {
    auto [account_info, account_info_state] =
        GetOrRestorePrimaryAccountInfoOnInitialize(pref_account_id,
                                                   pref_consented_to_sync);
    base::UmaHistogramEnumeration(
        "Signin.PAMInitialize.PrimaryAccountInfoState", account_info_state);

    if (pref_consented_to_sync && !account_info.IsEmpty()) {
      SetPrimaryAccountInternal(account_info, /*consented_to_sync=*/true,
                                scoped_pref_commit);

      // Ensure that the last syncing account data is consistent with the
      // primary account. The last signed-in account data is written inside
      // SetPrimaryAccountInternal().
      scoped_pref_commit.SetString(prefs::kGoogleServicesLastSyncingGaiaId,
                                   account_info.gaia);
      scoped_pref_commit.SetString(prefs::kGoogleServicesLastSyncingUsername,
                                   account_info.email);
    } else if (ShouldSigninAllowedPrefAffectPrimaryAccount(
                   pref_consented_to_sync)) {
      SetPrimaryAccountInternal(CoreAccountInfo(), /*consented_to_sync=*/false,
                                scoped_pref_commit);
    } else {
      SetPrimaryAccountInternal(account_info, /*consented_to_sync=*/false,
                                scoped_pref_commit);
    }
  }

  // PrimaryAccountManager is initialized once the primary account and consent
  // level are loaded.
  CHECK(primary_account_.has_value());

  // Instrument metrics to know what fraction of users without a primary
  // account previously did have one, with sync enabled.
  RecordHadPreviousSyncAccount();

  // It is important to only load credentials after starting to observe the
  // token service.
  token_service_observation_.Observe(token_service_);
  token_service_->LoadCredentials(
      GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      HasPrimaryAccount(signin::ConsentLevel::kSync));
}

PrimaryAccountManager::~PrimaryAccountManager() = default;

// static
void PrimaryAccountManager::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGoogleServicesLastSyncingGaiaId,
                               std::string());
  registry->RegisterStringPref(prefs::kGoogleServicesLastSyncingUsername,
                               std::string());
  registry->RegisterStringPref(prefs::kGoogleServicesLastSignedInUsername,
                               std::string());
  registry->RegisterStringPref(prefs::kGoogleServicesAccountId, std::string());
  registry->RegisterBooleanPref(prefs::kGoogleServicesConsentedToSync, false);
  registry->RegisterStringPref(
      prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn, std::string());
  registry->RegisterStringPref(
      prefs::kGoogleServicesSyncingUsernameMigratedToSignedIn, std::string());
  registry->RegisterBooleanPref(prefs::kSigninAllowed, true);
  registry->RegisterBooleanPref(prefs::kSignedInWithCredentialProvider, false);
  registry->RegisterBooleanPref(kExplicitBrowserSigninWithoutFeatureEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kExplicitBrowserSignin, false);
}

// static
void PrimaryAccountManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGoogleServicesUsernamePattern,
                               std::string());
}

void PrimaryAccountManager::PrepareToLoadPrefs() {
  // Check this method is only called before loading the primary account.
  CHECK(!primary_account_.has_value());

  PrefService* prefs = client_->GetPrefs();

  // kGoogleServicesLastSignedInUsername was introduced much later than its
  // "Syncing" counterpart, so backfill. Note that having different values for
  // the 2 prefs is possible (user enabled sync, disabled, then signed-in with
  // a different account) and we should not overwrite the "SignedIn" pref in
  // that case.
  // TODO(crbug.com/337112658): Remove migration after 04/25.
  std::string last_syncing_username =
      prefs->GetString(prefs::kGoogleServicesLastSyncingUsername);
  std::string last_signed_in_username =
      prefs->GetString(prefs::kGoogleServicesLastSignedInUsername);
  if (!last_syncing_username.empty() && last_signed_in_username.empty()) {
    prefs->SetString(prefs::kGoogleServicesLastSignedInUsername,
                     last_syncing_username);
  }

  // If the user is clearing the token service from the command line, then
  // clear their login info also (not valid to be logged in without any
  // tokens).
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kClearTokenService)) {
    prefs->SetString(prefs::kGoogleServicesAccountId, "");
    prefs->SetBoolean(prefs::kGoogleServicesConsentedToSync, false);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Migrate primary account ID from email to Gaia ID if needed.
  std::string pref_account_id =
      prefs->GetString(prefs::kGoogleServicesAccountId);
  if (!pref_account_id.empty()) {
    if (account_tracker_service_->GetMigrationState() ==
        AccountTrackerService::MIGRATION_IN_PROGRESS) {
      CoreAccountInfo account_info =
          account_tracker_service_->FindAccountInfoByEmail(pref_account_id);
      // |account_info.gaia| could be empty if |account_id| is already gaia id.
      if (!account_info.gaia.empty()) {
        pref_account_id = account_info.gaia;
        prefs->SetString(prefs::kGoogleServicesAccountId, account_info.gaia);
      }
    }
  }
#endif
}

std::pair<CoreAccountInfo, PrimaryAccountManager::InitializeAccountInfoState>
PrimaryAccountManager::GetOrRestorePrimaryAccountInfoOnInitialize(
    const std::string& pref_account_id,
    bool pref_consented_to_sync) {
  // Check this method is only called before loading the primary account.
  CHECK(!primary_account_.has_value());

  // This method must only be called when the primary account pref is non-empty.
  CHECK(!pref_account_id.empty());
  CoreAccountId account_id = CoreAccountId::FromString(pref_account_id);
  CHECK(!account_id.empty());

  CoreAccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  if (!account_info.IsEmpty()) {
    return std::make_pair(account_info,
                          InitializeAccountInfoState::kAccountInfoAvailable);
  }

  if (!pref_consented_to_sync) {
    return std::make_pair(CoreAccountInfo(),
                          InitializeAccountInfoState::
                              kEmptyAccountInfo_RestoreFailedNotSyncConsented);
  }

  PrefService* prefs = client_->GetPrefs();
  std::string last_syncing_gaia_id =
      prefs->GetString(prefs::kGoogleServicesLastSyncingGaiaId);
  if (last_syncing_gaia_id.empty()) {
    return std::make_pair(CoreAccountInfo(),
                          InitializeAccountInfoState::
                              kEmptyAccountInfo_RestoreFailedNoLastSyncGaiaId);
  }
  std::string last_syncing_email =
      prefs->GetString(prefs::kGoogleServicesLastSyncingUsername);
  if (last_syncing_email.empty()) {
    return std::make_pair(CoreAccountInfo(),
                          InitializeAccountInfoState::
                              kEmptyAccountInfo_RestoreFailedNoLastSyncEmail);
  }

  if (account_id != account_tracker_service_->PickAccountIdForAccount(
                        last_syncing_gaia_id, last_syncing_email)) {
    return std::make_pair(
        CoreAccountInfo(),
        InitializeAccountInfoState::
            kEmptyAccountInfo_RestoreFailedAccountIdDontMatch);
  }

  if (base::FeatureList::IsEnabled(kRestorePrimaryAccountInfo)) {
    CHECK_EQ(account_id,
             account_tracker_service_->SeedAccountInfo(
                 last_syncing_gaia_id, last_syncing_email,
                 signin_metrics::AccessPoint::
                     ACCESS_POINT_RESTORE_PRIMARY_ACCOUNT_ON_PROFILE_LOAD));

    return std::make_pair(account_tracker_service_->GetAccountInfo(account_id),
                          InitializeAccountInfoState::
                              kEmptyAccountInfo_RestoreSuccessFromLastSyncInfo);
  } else {
    return std::make_pair(
        CoreAccountInfo(),
        InitializeAccountInfoState::
            kEmptyAccountInfo_RestoreFailedAsRestoreFeatureIsDisabled);
  }
}

const PrimaryAccountManager::PrimaryAccount&
PrimaryAccountManager::GetPrimaryAccount() const {
  CHECK(primary_account_.has_value());
  return primary_account_.value();
}

bool PrimaryAccountManager::HasPrimaryAccount(
    signin::ConsentLevel consent_level) const {
  const auto& primary_account = GetPrimaryAccount();
  switch (consent_level) {
    case signin::ConsentLevel::kSignin:
      return !primary_account.account_info.account_id.empty();
    case signin::ConsentLevel::kSync:
      return !primary_account.account_info.account_id.empty() &&
             primary_account.consented_to_sync;
  }
}

CoreAccountInfo PrimaryAccountManager::GetPrimaryAccountInfo(
    signin::ConsentLevel consent_level) const {
  if (!HasPrimaryAccount(consent_level))
    return CoreAccountInfo();
  return GetPrimaryAccount().account_info;
}

CoreAccountId PrimaryAccountManager::GetPrimaryAccountId(
    signin::ConsentLevel consent_level) const {
  return GetPrimaryAccountInfo(consent_level).account_id;
}

void PrimaryAccountManager::SetPrimaryAccountInfo(
    const CoreAccountInfo& account_info,
    signin::ConsentLevel consent_level,
    signin_metrics::AccessPoint access_point,
    base::OnceClosure prefs_committed_callback) {
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
    case signin::ConsentLevel::kSync: {
      // Commit primary sync account info immediately so that it does not get
      // lost if Chrome crashes before the next commit interval.
      ScopedPrefCommit sync_scoped_pref_commit(
          client_->GetPrefs(),
          /*commit_on_destroy*/ true, std::move(prefs_committed_callback));
      SetSyncPrimaryAccountInternal(account_info, sync_scoped_pref_commit);
      FirePrimaryAccountChanged(previous_state, access_point,
                                sync_scoped_pref_commit);
      return;
    }
    case signin::ConsentLevel::kSignin: {
      bool account_changed = account_info != GetPrimaryAccount().account_info;
      // Commit primary account info immediately so that it does not get lost
      // if Chrome exits before the next commit interval. (e.g. at the end of
      // Chrome restore.)
      ScopedPrefCommit signin_scoped_pref_commit(
          client_->GetPrefs(),
          /*commit_on_destroy*/ true, std::move(prefs_committed_callback));
      SetPrimaryAccountInternal(account_info, /*consented_to_sync=*/false,
                                signin_scoped_pref_commit);
      if (account_changed)
        FirePrimaryAccountChanged(previous_state, access_point,
                                  signin_scoped_pref_commit);
      return;
    }
  }
}

void PrimaryAccountManager::SetSyncPrimaryAccountInternal(
    const CoreAccountInfo& account_info,
    ScopedPrefCommit& scoped_pref_commit) {
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

  SetPrimaryAccountInternal(account_info, /*consented_to_sync=*/true,
                            scoped_pref_commit);

  // Go ahead and update the last signed in account info here as well. Once a
  // user is signed in the corresponding preferences should match. Doing it here
  // as opposed to on signin allows us to catch the upgrade scenario.
  scoped_pref_commit.SetString(prefs::kGoogleServicesLastSyncingGaiaId,
                               account_info.gaia);
  scoped_pref_commit.SetString(prefs::kGoogleServicesLastSyncingUsername,
                               account_info.email);
}

void PrimaryAccountManager::SetPrimaryAccountInternal(
    const CoreAccountInfo& account_info,
    bool consented_to_sync,
    ScopedPrefCommit& scoped_pref_commit) {
  CHECK(!account_info.account_id.empty() || !consented_to_sync);

  // 'account_info' might be a reference to the contents of `primary_account_`.
  // Create a PrimaryAccount object before calling emplace to avoid crashes.
  primary_account_.emplace(PrimaryAccount(account_info, consented_to_sync));
  std::string account_id =
      GetPrimaryAccount().account_info.account_id.ToString();
  scoped_pref_commit.SetString(prefs::kGoogleServicesAccountId, account_id);
  scoped_pref_commit.SetBoolean(prefs::kGoogleServicesConsentedToSync,
                                GetPrimaryAccount().consented_to_sync);
  // If this was a sign-out (account ID is empty), also clear the "account was
  // migrated" prefs.
  if (account_id.empty()) {
    scoped_pref_commit.ClearPref(
        prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn);
    scoped_pref_commit.ClearPref(
        prefs::kGoogleServicesSyncingUsernameMigratedToSignedIn);
  } else {
    scoped_pref_commit.SetString(prefs::kGoogleServicesLastSignedInUsername,
                                 account_info.email);
  }
}

void PrimaryAccountManager::RecordHadPreviousSyncAccount() const {
  if (HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    // If sync is on currently, do not record anything.
    return;
  }

  const std::string& last_gaia_id_with_sync_enabled =
      client_->GetPrefs()->GetString(prefs::kGoogleServicesLastSyncingGaiaId);
  const bool existed_primary_account_with_sync =
      !last_gaia_id_with_sync_enabled.empty();

  base::UmaHistogramBoolean(
      "Signin.HadPreviousSyncAccount.SyncOffOnProfileLoad",
      existed_primary_account_with_sync);

  if (!HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // The user is currently signed out (no primary account exists).
    base::UmaHistogramBoolean(
        "Signin.HadPreviousSyncAccount.SignedOutOnProfileLoad",
        existed_primary_account_with_sync);
  }
}

void PrimaryAccountManager::UpdatePrimaryAccountInfo() {
  CoreAccountId primary_account_id =
      GetPrimaryAccount().account_info.account_id;
  bool consented_to_sync = GetPrimaryAccount().consented_to_sync;
  CHECK(!primary_account_id.empty());

  const CoreAccountInfo updated_account_info =
      account_tracker_service_->GetAccountInfo(primary_account_id);
  CHECK_EQ(primary_account_id, updated_account_info.account_id);

  // Calling SetPrimaryAccountInternal() is avoided in this case as the
  // primary account id did not change.
  primary_account_.emplace(updated_account_info, consented_to_sync);
}

void PrimaryAccountManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrimaryAccountManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void PrimaryAccountManager::ClearPrimaryAccount(
    signin_metrics::ProfileSignout signout_source_metric) {
  StartSignOut(signout_source_metric, RemoveAccountsOption::kRemoveAllAccounts);
}

void PrimaryAccountManager::RemovePrimaryAccountButKeepTokens(
    signin_metrics::ProfileSignout signout_source_metric) {
  StartSignOut(signout_source_metric,
               RemoveAccountsOption::kKeepAllAccountsAndClearPrimary);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

void PrimaryAccountManager::RevokeSyncConsent(
    signin_metrics::ProfileSignout signout_source_metric) {
  StartSignOut(signout_source_metric, RemoveAccountsOption::kKeepAllAccounts);
}

void PrimaryAccountManager::StartSignOut(
    signin_metrics::ProfileSignout signout_source_metric,
    RemoveAccountsOption remove_option) {
  VLOG(1) << "StartSignOut: " << static_cast<int>(signout_source_metric) << ", "
          << static_cast<int>(remove_option);
  client_->PreSignOut(
      base::BindOnce(&PrimaryAccountManager::OnSignoutDecisionReached,
                     base::Unretained(this), signout_source_metric,
                     remove_option),
      signout_source_metric, HasPrimaryAccount(signin::ConsentLevel::kSync));
}

void PrimaryAccountManager::OnSignoutDecisionReached(
    signin_metrics::ProfileSignout signout_source_metric,
    RemoveAccountsOption remove_option,
    SigninClient::SignoutDecision signout_decision) {
  VLOG(1) << "OnSignoutDecisionReached: "
          << (signout_decision == SigninClient::SignoutDecision::ALLOW);

  // |REVOKE_SYNC_DISALLOWED| implies that removing the primary account is not
  // allowed as the sync consent is attached to the primary account. Therefore,
  // there is no need to check |remove_option| as regardless of its value, this
  // function will be no-op.
  bool abort_signout =
      GetPrimaryAccount().account_info.IsEmpty() ||
      signout_decision ==
          SigninClient::SignoutDecision::REVOKE_SYNC_DISALLOWED ||
      (remove_option == RemoveAccountsOption::kRemoveAllAccounts &&
       signout_decision ==
           SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);

  if (abort_signout) {
    // TODO(crbug.com/40240858): Add 'NOTREACHED()' after updating the
    // 'SigninManager', 'Dice Response Handler',
    // 'Lacros Profile Account Mapper'.
    VLOG(1) << "Ignoring attempt to sign out while signout disallowed";
    return;
  }

  signin_metrics::LogSignout(signout_source_metric);
  PrimaryAccountChangeEvent::State previous_state = GetPrimaryAccountState();

  // Revoke all tokens before sending signed_out notification, because there
  // may be components that don't listen for token service events when the
  // profile is not connected to an account.
  ScopedPrefCommit scoped_pref_commit(client_->GetPrefs(),
                                      /*commit_on_destroy*/ false);
  switch (remove_option) {
    case RemoveAccountsOption::kRemoveAllAccounts:
      VLOG(0)
          << "Revoking all refresh tokens on server. Reason: sign out; source: "
          << static_cast<int>(signout_source_metric);
      SetPrimaryAccountInternal(CoreAccountInfo(), /*consented_to_sync=*/false,
                                scoped_pref_commit);
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
      SetPrimaryAccountInternal(GetPrimaryAccount().account_info,
                                /*consented_to_sync=*/false,
                                scoped_pref_commit);
      break;
    case RemoveAccountsOption::kKeepAllAccountsAndClearPrimary:
      SetPrimaryAccountInternal(CoreAccountInfo(), /*consented_to_sync=*/false,
                                scoped_pref_commit);
      break;
  }

  DCHECK(!HasPrimaryAccount(signin::ConsentLevel::kSync));
  FirePrimaryAccountChanged(previous_state, signout_source_metric,
                            scoped_pref_commit);
}

PrimaryAccountChangeEvent::State PrimaryAccountManager::GetPrimaryAccountState()
    const {
  PrimaryAccountChangeEvent::State state(GetPrimaryAccount().account_info,
                                         signin::ConsentLevel::kSignin);
  if (HasPrimaryAccount(signin::ConsentLevel::kSync))
    state.consent_level = signin::ConsentLevel::kSync;
  return state;
}

void PrimaryAccountManager::ComputeExplicitBrowserSignin(
    const PrimaryAccountChangeEvent& event_details,
    ScopedPrefCommit& scoped_pref_commit) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case PrimaryAccountChangeEvent::Type::kNone:
      return;
    case PrimaryAccountChangeEvent::Type::kCleared:
      scoped_pref_commit.ClearPref(kExplicitBrowserSigninWithoutFeatureEnabled);
      if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
        scoped_pref_commit.ClearPref(prefs::kExplicitBrowserSignin);
      }
      return;
    case PrimaryAccountChangeEvent::Type::kSet:
      CHECK(event_details.GetSetPrimaryAccountAccessPoint().has_value());
      signin_metrics::AccessPoint access_point =
          event_details.GetSetPrimaryAccountAccessPoint().value();

      if (access_point == signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN ||
          access_point ==
              signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN) {
        scoped_pref_commit.ClearPref(
            kExplicitBrowserSigninWithoutFeatureEnabled);
        if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
          scoped_pref_commit.ClearPref(prefs::kExplicitBrowserSignin);
        }
      } else {
        // All others access points are explicit sign ins except the Web
        // Signin event.
        scoped_pref_commit.SetBoolean(
            kExplicitBrowserSigninWithoutFeatureEnabled, true);
        if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
          scoped_pref_commit.SetBoolean(prefs::kExplicitBrowserSignin, true);
        }
      }
  }
}

void PrimaryAccountManager::FirePrimaryAccountChanged(
    const PrimaryAccountChangeEvent::State& previous_state,
    absl::variant<signin_metrics::AccessPoint, signin_metrics::ProfileSignout>
        event_source,
    ScopedPrefCommit& scoped_pref_commit) {
  PrimaryAccountChangeEvent::State current_state = GetPrimaryAccountState();
  PrimaryAccountChangeEvent event_details(previous_state, current_state,
                                          event_source);

  DCHECK(event_details.GetEventTypeFor(signin::ConsentLevel::kSync) !=
             PrimaryAccountChangeEvent::Type::kNone ||
         event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
             PrimaryAccountChangeEvent::Type::kNone)
      << "PrimaryAccountChangeEvent with no change: " << event_details;

  LogPrimaryAccountChangeMetrics(event_details);

  ComputeExplicitBrowserSignin(event_details, scoped_pref_commit);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      PrimaryAccountChangeEvent::Type::kCleared) {
    SigninPrefs(*client_->GetPrefs())
        .SetChromeLastSignoutTime(previous_state.primary_account.gaia,
                                  base::Time::Now());
  }
#endif

  client_->OnPrimaryAccountChanged(event_details);

  for (Observer& observer : observers_) {
    observer.OnPrimaryAccountChanged(event_details);
  }
}

void PrimaryAccountManager::OnRefreshTokensLoaded() {
  token_service_observation_.Reset();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (account_tracker_service_->GetMigrationState() ==
      AccountTrackerService::MIGRATION_IN_PROGRESS) {
    account_tracker_service_->SetMigrationDone();
  }
#endif

// On Android, account seeding on Android is
// controlled by SigninManager, so don't remove any accounts here.
#if !BUILDFLAG(IS_ANDROID)
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
#endif
}

void PrimaryAccountManager::OnSigninAllowedPrefChanged() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (ShouldSigninAllowedPrefAffectPrimaryAccount(
          /*is_sync_consent=*/GetPrimaryAccountState().consent_level ==
          signin::ConsentLevel::kSync)) {
    ClearPrimaryAccount(signin_metrics::ProfileSignout::kPrefChanged);
  }
#endif
}

bool PrimaryAccountManager::ShouldSigninAllowedPrefAffectPrimaryAccount(
    bool is_sync_consent) {
  return switches::IsExplicitBrowserSigninUIOnDesktopEnabled() &&
         !signin_allowed_.GetValue() &&
         // If sync is enabled, we do not directly clear the primary account.
         // This is handled by `PrimaryAccountPolicyManager`. That flow is
         // extremely hard to follow especially for the case when the user is
         // syncing with a managed account as in that case the whole profile
         // needs to be deleted.
         !is_sync_consent;
}
