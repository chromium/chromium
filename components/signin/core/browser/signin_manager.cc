// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_manager.h"

#include <string>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/signin_internals_util.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

using namespace signin_internals_util;

SigninManager::SigninManager(
    SigninClient* client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    GaiaCookieManagerService* cookie_manager_service,
    SigninErrorController* signin_error_controller,
    signin::AccountConsistencyMethod account_consistency)
    : SigninManagerBase(client,
                        account_tracker_service,
                        signin_error_controller),
      type_(SIGNIN_TYPE_NONE),
      client_(client),
      token_service_(token_service),
      cookie_manager_service_(cookie_manager_service),
      account_consistency_(account_consistency),
      signin_manager_signed_in_(false),
      user_info_fetched_by_account_tracker_(false),
      weak_pointer_factory_(this) {}

SigninManager::~SigninManager() {}

std::string SigninManager::SigninTypeToString(SigninManager::SigninType type) {
  switch (type) {
    case SIGNIN_TYPE_NONE:
      return "No Signin";
    case SIGNIN_TYPE_WITH_REFRESH_TOKEN:
      return "With refresh token";
    case SIGNIN_TYPE_WITHOUT_REFRESH_TOKEN:
      return "Without refresh token";
  }

  NOTREACHED();
  return std::string();
}

bool SigninManager::PrepareForSignin(SigninType type,
                                     const std::string& gaia_id,
                                     const std::string& username,
                                     const std::string& password) {
  std::string account_id =
      account_tracker_service()->PickAccountIdForAccount(gaia_id, username);
  DCHECK(!account_id.empty());
  DCHECK(possibly_invalid_account_id_.empty() ||
         possibly_invalid_account_id_ == account_id);

  if (!IsAllowedUsername(username)) {
    // Account is not allowed by admin policy.
    HandleAuthError(
        GoogleServiceAuthError(GoogleServiceAuthError::ACCOUNT_DISABLED));
    return false;
  }

  // This attempt is either 1) the user trying to establish initial sync, or
  // 2) trying to refresh credentials for an existing username.  If it is 2, we
  // need to try again, but take care to leave state around tracking that the
  // user has successfully signed in once before with this username, so that on
  // restart we don't think sync setup has never completed.
  ClearTransientSigninData();
  type_ = type;
  possibly_invalid_account_id_.assign(account_id);
  possibly_invalid_gaia_id_.assign(gaia_id);
  possibly_invalid_email_.assign(username);
  password_.assign(password);
  signin_manager_signed_in_ = false;
  user_info_fetched_by_account_tracker_ = false;
  NotifyDiagnosticsObservers(SIGNIN_STARTED, SigninTypeToString(type));
  return true;
}

void SigninManager::StartSignInWithRefreshToken(
    const std::string& refresh_token,
    const std::string& gaia_id,
    const std::string& username,
    const std::string& password,
    const OAuthTokenFetchedCallback& callback) {
  DCHECK(!IsAuthenticated());
  SigninType signin_type = refresh_token.empty()
                               ? SIGNIN_TYPE_WITHOUT_REFRESH_TOKEN
                               : SIGNIN_TYPE_WITH_REFRESH_TOKEN;
  if (!PrepareForSignin(signin_type, gaia_id, username, password)) {
    return;
  }

  // Store the refresh token.
  temp_refresh_token_ = refresh_token;

  if (!callback.is_null()) {
    // Callback present, let the caller complete the pending sign-in.
    callback.Run(temp_refresh_token_);
  } else {
    // No callback, so just complete the pending signin.
    CompletePendingSignin();
  }
}

void SigninManager::CopyCredentialsFrom(const SigninManager& source) {
  DCHECK_NE(this, &source);
  possibly_invalid_account_id_ = source.possibly_invalid_account_id_;
  possibly_invalid_gaia_id_ = source.possibly_invalid_gaia_id_;
  possibly_invalid_email_ = source.possibly_invalid_email_;
  temp_refresh_token_ = source.temp_refresh_token_;
  password_ = source.password_;
  source.client_->AfterCredentialsCopied();
}

void SigninManager::ClearTransientSigninData() {
  DCHECK(IsInitialized());

  possibly_invalid_account_id_.clear();
  possibly_invalid_gaia_id_.clear();
  possibly_invalid_email_.clear();
  password_.clear();
  type_ = SIGNIN_TYPE_NONE;
  temp_refresh_token_.clear();
}

void SigninManager::HandleAuthError(const GoogleServiceAuthError& error) {
  ClearTransientSigninData();

  for (auto& observer : observer_list_)
    observer.GoogleSigninFailed(error);
}

void SigninManager::SignOut(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  RemoveAccountsOption remove_option =
      (account_consistency_ == signin::AccountConsistencyMethod::kDice)
          ? RemoveAccountsOption::kRemoveAuthenticatedAccountIfInError
          : RemoveAccountsOption::kRemoveAllAccounts;
  StartSignOut(signout_source_metric, signout_delete_metric, remove_option);
}

void SigninManager::SignOutAndRemoveAllAccounts(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  StartSignOut(signout_source_metric, signout_delete_metric,
               RemoveAccountsOption::kRemoveAllAccounts);
}

void SigninManager::SignOutAndKeepAllAccounts(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  StartSignOut(signout_source_metric, signout_delete_metric,
               RemoveAccountsOption::kKeepAllAccounts);
}

void SigninManager::StartSignOut(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric,
    RemoveAccountsOption remove_option) {
  client_->PreSignOut(
      base::BindOnce(&SigninManager::OnSignoutDecisionReached,
                     base::Unretained(this), signout_source_metric,
                     signout_delete_metric, remove_option),
      signout_source_metric);
}

void SigninManager::OnSignoutDecisionReached(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric,
    RemoveAccountsOption remove_option,
    SigninClient::SignoutDecision signout_decision) {
  DCHECK(IsInitialized());

  signin_metrics::LogSignout(signout_source_metric, signout_delete_metric);
  if (!IsAuthenticated()) {
    if (AuthInProgress()) {
      // If the user is in the process of signing in, then treat a call to
      // SignOut as a cancellation request.
      GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
      HandleAuthError(error);
    } else {
      // Clean up our transient data and exit if we aren't signed in.
      // This avoids a perf regression from clearing out the TokenDB if
      // SignOut() is invoked on startup to clean up any incomplete previous
      // signin attempts.
      ClearTransientSigninData();
    }
    return;
  }

  // TODO(crbug.com/887756): Consider moving this higher up, or document why
  // the above blocks are exempt from the |signout_decision| early return.
  if (signout_decision == SigninClient::SignoutDecision::DISALLOW_SIGNOUT) {
    DVLOG(1) << "Ignoring attempt to sign out while signout disallowed";
    return;
  }

  ClearTransientSigninData();

  AccountInfo account_info = GetAuthenticatedAccountInfo();
  const std::string account_id = GetAuthenticatedAccountId();
  const std::string username = account_info.email;
  const base::Time signin_time =
      base::Time::FromInternalValue(
          client_->GetPrefs()->GetInt64(prefs::kSignedInTime));
  ClearAuthenticatedAccountId();
  client_->GetPrefs()->ClearPref(prefs::kGoogleServicesHostedDomain);
  client_->GetPrefs()->ClearPref(prefs::kGoogleServicesAccountId);
  client_->GetPrefs()->ClearPref(prefs::kGoogleServicesUserAccountId);
  client_->GetPrefs()->ClearPref(prefs::kSignedInTime);
  client_->OnSignedOut();

  // Determine the duration the user was logged in and log that to UMA.
  if (!signin_time.is_null()) {
    base::TimeDelta signed_in_duration = base::Time::Now() - signin_time;
    UMA_HISTOGRAM_COUNTS_1M("Signin.SignedInDurationBeforeSignout",
                            signed_in_duration.InMinutes());
  }

  // Revoke all tokens before sending signed_out notification, because there
  // may be components that don't listen for token service events when the
  // profile is not connected to an account.
  switch (remove_option) {
    case RemoveAccountsOption::kRemoveAllAccounts:
      VLOG(0) << "Revoking all refresh tokens on server. Reason: sign out, "
              << "IsSigninAllowed: " << IsSigninAllowed();
      token_service_->RevokeAllCredentials();
      break;
    case RemoveAccountsOption::kRemoveAuthenticatedAccountIfInError:
      if (token_service_->RefreshTokenHasError(account_id))
        token_service_->RevokeCredentials(account_id);
      break;
    case RemoveAccountsOption::kKeepAllAccounts:
      // Do nothing.
      break;
  }

  FireGoogleSignedOut(account_id, account_info);
}

void SigninManager::Initialize(PrefService* local_state) {
  SigninManagerBase::Initialize(local_state);

  // local_state can be null during unit tests.
  if (local_state) {
    local_state_pref_registrar_.Init(local_state);
    local_state_pref_registrar_.Add(
        prefs::kGoogleServicesUsernamePattern,
        base::Bind(&SigninManager::OnGoogleServicesUsernamePatternChanged,
                   weak_pointer_factory_.GetWeakPtr()));
  }
  signin_allowed_.Init(prefs::kSigninAllowed,
                       client_->GetPrefs(),
                       base::Bind(&SigninManager::OnSigninAllowedPrefChanged,
                                  base::Unretained(this)));

  std::string account_id =
      client_->GetPrefs()->GetString(prefs::kGoogleServicesAccountId);
  std::string user = account_id.empty() ? std::string() :
      account_tracker_service()->GetAccountInfo(account_id).email;
  if ((!account_id.empty() && !IsAllowedUsername(user)) || !IsSigninAllowed()) {
    // User is signed in, but the username is invalid or signin is no longer
    // allowed, so the user must be sign out.
    //
    // This may happen in the following cases:
    //   a. The user has toggled off signin allowed in settings.
    //   b. The administrator changed the policy since the last signin.
    //
    // Note: The token service has not yet loaded its credentials, so accounts
    // cannot be revoked here.
    SignOutAndKeepAllAccounts(signin_metrics::SIGNIN_PREF_CHANGED_DURING_SIGNIN,
                              signin_metrics::SignoutDelete::IGNORE_METRIC);
  }

  account_tracker_service()->AddObserver(this);

  // It is important to only load credentials after starting to observe the
  // token service.
  token_service_->AddObserver(this);
  token_service_->LoadCredentials(GetAuthenticatedAccountId());
}

void SigninManager::Shutdown() {
  token_service_->RemoveObserver(this);
  account_tracker_service()->RemoveObserver(this);
  local_state_pref_registrar_.RemoveAll();
  SigninManagerBase::Shutdown();
}

void SigninManager::OnGoogleServicesUsernamePatternChanged() {
  if (IsAuthenticated() &&
      !IsAllowedUsername(GetAuthenticatedAccountInfo().email)) {
    // Signed in user is invalid according to the current policy so sign
    // the user out.
    SignOut(signin_metrics::GOOGLE_SERVICE_NAME_PATTERN_CHANGED,
            signin_metrics::SignoutDelete::IGNORE_METRIC);
  }
}

bool SigninManager::IsSigninAllowed() const {
  return signin_allowed_.GetValue();
}

void SigninManager::OnSigninAllowedPrefChanged() {
  if (!IsSigninAllowed() && (IsAuthenticated() || AuthInProgress()))
    SignOut(signin_metrics::SIGNOUT_PREF_CHANGED,
            signin_metrics::SignoutDelete::IGNORE_METRIC);
}

// static
bool SigninManager::IsUsernameAllowedByPolicy(const std::string& username,
                                              const std::string& policy) {
  if (policy.empty())
    return true;

  // Patterns like "*@foo.com" are not accepted by our regex engine (since they
  // are not valid regular expressions - they should instead be ".*@foo.com").
  // For convenience, detect these patterns and insert a "." character at the
  // front.
  base::string16 pattern = base::UTF8ToUTF16(policy);
  if (pattern[0] == L'*')
    pattern.insert(pattern.begin(), L'.');

  // See if the username matches the policy-provided pattern.
  UErrorCode status = U_ZERO_ERROR;
  const icu::UnicodeString icu_pattern(FALSE, pattern.data(), pattern.length());
  icu::RegexMatcher matcher(icu_pattern, UREGEX_CASE_INSENSITIVE, status);
  if (!U_SUCCESS(status)) {
    LOG(ERROR) << "Invalid login regex: " << pattern << ", status: " << status;
    // If an invalid pattern is provided, then prohibit *all* logins (better to
    // break signin than to quietly allow users to sign in).
    return false;
  }
  // The default encoding is UTF-8 in Chromium's ICU.
  icu::UnicodeString icu_input(username.data());
  matcher.reset(icu_input);
  status = U_ZERO_ERROR;
  UBool match = matcher.matches(status);
  DCHECK(U_SUCCESS(status));
  return !!match;  // !! == convert from UBool to bool.
}

// static
SigninManager* SigninManager::FromSigninManagerBase(
    SigninManagerBase* manager) {
  return static_cast<SigninManager*>(manager);
}

bool SigninManager::IsAllowedUsername(const std::string& username) const {
  const PrefService* local_state = local_state_pref_registrar_.prefs();
  if (!local_state)
    return true;  // In a unit test with no local state - all names are allowed.

  std::string pattern =
      local_state->GetString(prefs::kGoogleServicesUsernamePattern);
  return IsUsernameAllowedByPolicy(username, pattern);
}

bool SigninManager::AuthInProgress() const {
  return !possibly_invalid_account_id_.empty();
}

const std::string& SigninManager::GetAccountIdForAuthInProgress() const {
  return possibly_invalid_account_id_;
}

const std::string& SigninManager::GetGaiaIdForAuthInProgress() const {
  return possibly_invalid_gaia_id_;
}

const std::string& SigninManager::GetUsernameForAuthInProgress() const {
  return possibly_invalid_email_;
}

void SigninManager::MergeSigninCredentialIntoCookieJar() {
  if (account_consistency_ == signin::AccountConsistencyMethod::kMirror)
    return;

  if (!IsAuthenticated())
    return;

  cookie_manager_service_->AddAccountToCookie(GetAuthenticatedAccountId(),
                                              "ChromiumSigninManager");
}

void SigninManager::CompletePendingSignin() {
  NotifyDiagnosticsObservers(SIGNIN_COMPLETED, "Successful");
  DCHECK(!possibly_invalid_account_id_.empty());
  OnSignedIn();

  DCHECK(IsAuthenticated());

  if (!temp_refresh_token_.empty()) {
    std::string account_id = GetAuthenticatedAccountId();
    token_service_->UpdateCredentials(account_id, temp_refresh_token_);
    temp_refresh_token_.clear();
  }
  MergeSigninCredentialIntoCookieJar();
}

void SigninManager::OnExternalSigninCompleted(const std::string& username) {
  AccountInfo info =
      account_tracker_service()->FindAccountInfoByEmail(username);
  DCHECK(!info.gaia.empty());
  DCHECK(!info.email.empty());
  possibly_invalid_account_id_ = info.account_id;
  possibly_invalid_gaia_id_ = info.gaia;
  possibly_invalid_email_ = info.email;
  OnSignedIn();
}

void SigninManager::OnSignedIn() {
  bool reauth_in_progress = IsAuthenticated();

  client_->GetPrefs()->SetInt64(prefs::kSignedInTime,
                                base::Time::Now().ToInternalValue());

  SetAuthenticatedAccountInfo(possibly_invalid_gaia_id_,
                              possibly_invalid_email_);
  const std::string gaia_id = possibly_invalid_gaia_id_;

  possibly_invalid_account_id_.clear();
  possibly_invalid_gaia_id_.clear();
  possibly_invalid_email_.clear();
  signin_manager_signed_in_ = true;

  if (!reauth_in_progress)
    FireGoogleSigninSucceeded();

  client_->OnSignedIn(GetAuthenticatedAccountId(), gaia_id,
                      GetAuthenticatedAccountInfo().email, password_);

  signin_metrics::LogSigninProfile(client_->IsFirstRun(),
                                   client_->GetInstallDate());

  PostSignedIn();
}

void SigninManager::FireGoogleSigninSucceeded() {
  std::string account_id = GetAuthenticatedAccountId();
  std::string email = GetAuthenticatedAccountInfo().email;
  for (auto& observer : observer_list_) {
    observer.GoogleSigninSucceeded(account_id, email);
    observer.GoogleSigninSucceeded(GetAuthenticatedAccountInfo());
    observer.GoogleSigninSucceededWithPassword(account_id, email, password_);
  }
}

void SigninManager::FireGoogleSignedOut(const std::string& account_id,
                                        const AccountInfo& account_info) {
  for (auto& observer : observer_list_) {
    observer.GoogleSignedOut(account_id, account_info.email);
    observer.GoogleSignedOut(account_info);
  }
}

void SigninManager::PostSignedIn() {
  if (!signin_manager_signed_in_ || !user_info_fetched_by_account_tracker_)
    return;

  client_->PostSignedIn(GetAuthenticatedAccountId(),
                        GetAuthenticatedAccountInfo().email, password_);
  password_.clear();
}

void SigninManager::OnAccountUpdated(const AccountInfo& info) {
  if (!info.IsValid())
    return;

  user_info_fetched_by_account_tracker_ = true;
  PostSignedIn();
}

void SigninManager::OnAccountUpdateFailed(const std::string& account_id) {
  user_info_fetched_by_account_tracker_ = true;
  PostSignedIn();
}

void SigninManager::OnRefreshTokensLoaded() {
  token_service_->RemoveObserver(this);

  if (account_tracker_service()->GetMigrationState() ==
      AccountTrackerService::MIGRATION_IN_PROGRESS) {
    account_tracker_service()->SetMigrationDone();
  }

  // Remove account information from the account tracker service if needed.
  if (token_service_->HasLoadCredentialsFinishedWithNoErrors()) {
    std::vector<AccountInfo> accounts_in_tracker_service =
        account_tracker_service()->GetAccounts();
    for (const auto& account : accounts_in_tracker_service) {
      if (GetAuthenticatedAccountId() != account.account_id &&
          !token_service_->RefreshTokenIsAvailable(account.account_id)) {
        DVLOG(0) << "Removed account from account tracker service: "
                 << account.account_id;
        account_tracker_service()->RemoveAccount(account.account_id);
      }
    }
  }
}
