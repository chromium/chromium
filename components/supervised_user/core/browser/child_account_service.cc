// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/child_account_service.h"

#include <functional>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/supervised_user/core/browser/list_family_members_service.h"
#include "components/supervised_user/core/browser/proto/families_common.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"
#include "components/supervised_user/core/browser/supervised_user_capabilities.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "google_apis/gaia/core_account_id.h"

namespace supervised_user {

namespace {
using ::base::BindRepeating;
}  // namespace

ChildAccountService::ChildAccountService(
    PrefService& user_prefs,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::OnceCallback<void(bool)> check_user_child_status_callback,
    ListFamilyMembersService& list_family_members_service)
    : identity_manager_(identity_manager),
      user_prefs_(user_prefs),
      url_loader_factory_(url_loader_factory),
      check_user_child_status_callback_(
          std::move(check_user_child_status_callback)) {
  set_custodian_prefs_subscription_ =
      list_family_members_service.SubscribeToSuccessfulFetches(BindRepeating(
          &RegisterFamilyPrefs,
          std::ref(user_prefs)));  // list_family_members_service is
                                   // an instance of a keyed service
                                   // and PrefService outlives it.
}

ChildAccountService::~ChildAccountService() = default;

void ChildAccountService::Init() {
  identity_manager_->AddObserver(this);

  std::move(check_user_child_status_callback_)
      .Run(supervised_user::IsSubjectToParentalControls(user_prefs_.get()));

  // If we're already signed in, check the account immediately just to be sure.
  // (We might have missed an update before registering as an observer.)
  // "Unconsented" because this class doesn't care about browser sync consent.
  AccountInfo primary_account_info = identity_manager_->FindExtendedAccountInfo(
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));

  if (!primary_account_info.IsEmpty()) {
    OnExtendedAccountInfoUpdated(primary_account_info);
    UpdateForceGoogleSafeSearch();
  }
}

void ChildAccountService::Shutdown() {
  identity_manager_->RemoveObserver(this);
}

#if BUILDFLAG(IS_CHROMEOS)
bool ChildAccountService::IsChildAccountStatusKnown() {
  return supervised_user::IsChildAccountStatusKnown(user_prefs_.get());
}

void ChildAccountService::AddChildStatusReceivedCallback(
    base::OnceClosure callback) {
  if (supervised_user::IsChildAccountStatusKnown(user_prefs_.get())) {
    std::move(callback).Run();
  } else {
    status_received_callback_list_.push_back(std::move(callback));
  }
}
#endif

ChildAccountService::AuthState ChildAccountService::GetGoogleAuthState() const {
  CoreAccountId primary_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (primary_account_id.empty()) {
    return AuthState::NOT_AUTHENTICATED;
  }

  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
      identity_manager_->GetAccountsInCookieJar();
  bool primary_account_has_cookie =
      accounts_in_cookie_jar_info.AreAccountsFresh() &&
      base::ranges::any_of(
          accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts(),
          [primary_account_id](const gaia::ListedAccount& account) {
            return account.id == primary_account_id && account.valid;
          });
  bool primary_account_has_token =
      !identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id);

  if (primary_account_has_cookie && primary_account_has_token) {
    return AuthState::AUTHENTICATED;
  }
  if (primary_account_has_token && !primary_account_has_cookie) {
    // The account reconcilor should automatically fix this state, by rebuilding
    // the account cookie.
    return AuthState::TRANSIENT_MOVING_TO_AUTHENTICATED;
  }
  // We either have no token or cookie (in which case we're in the stable
  // pending state), or we have a cookie and no token (in which case the
  // reconcilor will eventually get us to the stable pending state).
  return AuthState::PENDING;
}

base::CallbackListSubscription ChildAccountService::ObserveGoogleAuthState(
    const base::RepeatingCallback<void()>& callback) {
  return google_auth_state_observers_.Add(callback);
}

void ChildAccountService::SetSupervisionStatusAndNotifyObservers(
    bool supervision_status) {
  if (supervised_user::IsSubjectToParentalControls(user_prefs_.get()) !=
      supervision_status) {
    if (supervision_status) {
      EnableParentalControls(user_prefs_.get());
    } else {
      DisableParentalControls(user_prefs_.get());
    }
  }

  for (auto& callback : status_received_callback_list_) {
    std::move(callback).Run();
  }
  status_received_callback_list_.clear();
}

void ChildAccountService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  signin::PrimaryAccountChangeEvent::Type event_type =
      event_details.GetEventTypeFor(signin::ConsentLevel::kSignin);
  if (event_type == signin::PrimaryAccountChangeEvent::Type::kSet) {
    AccountInfo account_info = identity_manager_->FindExtendedAccountInfo(
        event_details.GetCurrentState().primary_account);
    if (!account_info.IsEmpty()) {
      OnExtendedAccountInfoUpdated(account_info);
    }
    // Otherwise OnExtendedAccountInfoUpdated will be notified once
    // the account info is available.
  } else if (event_type == signin::PrimaryAccountChangeEvent::Type::kCleared) {
    SetSupervisionStatusAndNotifyObservers(false);
  }
}

void ChildAccountService::UpdateForceGoogleSafeSearch() {
  if (!base::FeatureList::IsEnabled(
          supervised_user::kForceSafeSearchForUnauthenticatedSupervisedUsers)) {
    return;
  }
  bool is_subject_to_parental_controls =
      IsPrimaryAccountSubjectToParentalControls(identity_manager_) ==
      signin::Tribool::kTrue;

  // Supervised users who are signed in to Chrome and to the content area will
  // have account-level SafeSearch configuration applied based on their parent's
  // choices, and this setting should not be overridden.
  // Therefore, we only force SafeSearch on for an unauthenticated and
  // supervised primary account as a safe default.
  bool should_force_google_safe_search =
      (is_subject_to_parental_controls &&
       GetGoogleAuthState() != AuthState::AUTHENTICATED);
  user_prefs_->SetBoolean(policy::policy_prefs::kForceGoogleSafeSearch,
                          should_force_google_safe_search);
}

void ChildAccountService::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  // This method may get called when the account info isn't complete yet.
  // We deliberately don't check for that, as we are only interested in the
  // child account status.

  // This class doesn't care about browser sync consent.
  CoreAccountId auth_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (info.account_id != auth_account_id) {
    return;
  }

  SetSupervisionStatusAndNotifyObservers(info.is_child_account ==
                                         signin::Tribool::kTrue);
  OnAuthStateUpdated();
}

void ChildAccountService::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (account_info.account_id !=
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin)) {
    return;
  }

  OnAuthStateUpdated();
}

void ChildAccountService::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  if (account_info.account_id !=
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin)) {
    return;
  }

  OnAuthStateUpdated();
}

void ChildAccountService::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  OnAuthStateUpdated();
}

void ChildAccountService::OnAuthStateUpdated() {
  UpdateForceGoogleSafeSearch();
  google_auth_state_observers_.Notify();
}

}  // namespace supervised_user
