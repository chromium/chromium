// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_auth_manager.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/sync/base/stop_source.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/sync_credentials.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/identity/public/cpp/access_token_fetcher.h"

namespace browser_sync {

namespace {

constexpr char kSyncOAuthConsumerName[] = "sync";

constexpr net::BackoffEntry::Policy kRequestAccessTokenBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    2000,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    // TODO(crbug.com/246686): We should retry RequestAccessToken on connection
    // state change after backoff.
    1000 * 3600 * 4,  // 4 hours.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

}  // namespace

SyncAuthManager::SyncAccountInfo::SyncAccountInfo() = default;

SyncAuthManager::SyncAccountInfo::SyncAccountInfo(
    const AccountInfo& account_info,
    bool is_primary)
    : account_info(account_info), is_primary(is_primary) {}

SyncAuthManager::SyncAuthManager(
    syncer::SyncPrefs* sync_prefs,
    identity::IdentityManager* identity_manager,
    const AccountStateChangedCallback& account_state_changed,
    const CredentialsChangedCallback& credentials_changed)
    : sync_prefs_(sync_prefs),
      identity_manager_(identity_manager),
      account_state_changed_callback_(account_state_changed),
      credentials_changed_callback_(credentials_changed),
      registered_for_auth_notifications_(false),
      request_access_token_backoff_(&kRequestAccessTokenBackoffPolicy),
      weak_ptr_factory_(this) {
  DCHECK(sync_prefs_);
  // |identity_manager_| can be null if local Sync is enabled.
}

SyncAuthManager::~SyncAuthManager() {
  if (registered_for_auth_notifications_) {
    identity_manager_->RemoveObserver(this);
  }
}

void SyncAuthManager::RegisterForAuthNotifications() {
  DCHECK(!registered_for_auth_notifications_);
  DCHECK(sync_account_.account_info.account_id.empty());

  identity_manager_->AddObserver(this);
  registered_for_auth_notifications_ = true;

  // Also initialize the sync account here, but *without* notifying the
  // SyncService.
  sync_account_ = DetermineAccountToUse();
}

SyncAuthManager::SyncAccountInfo SyncAuthManager::GetActiveAccountInfo() const {
  if (!registered_for_auth_notifications_) {
    return SyncAccountInfo();
  }

#if defined(OS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(switches::kSyncSupportSecondaryAccount)) {
    // TODO(crbug.com/814787): Once the ChromeOS test setup is fixed, we can
    // just return |sync_account_| here instead of re-querying.
    return DetermineAccountToUse();
  }
#endif  // !defined(OS_CHROMEOS)
  // Note: At this point, |sync_account_| should generally be identical to the
  // result of a DetermineAccountToUse() call, but there are a few edge cases
  // when it isn't: E.g. when another identity observer gets notified before us
  // and calls in here, or when we're currently switching accounts in
  // UpdateSyncAccountIfNecessary(). So unfortunately we can't verify this.

  return sync_account_;
}

syncer::SyncTokenStatus SyncAuthManager::GetSyncTokenStatus() const {
  DCHECK(partial_token_status_.next_token_request_time.is_null());

  syncer::SyncTokenStatus token_status = partial_token_status_;
  token_status.has_token = !access_token_.empty();
  if (request_access_token_retry_timer_.IsRunning()) {
    base::TimeDelta delta =
        request_access_token_retry_timer_.desired_run_time() -
        base::TimeTicks::Now();
    token_status.next_token_request_time = base::Time::Now() + delta;
  }
  return token_status;
}

syncer::SyncCredentials SyncAuthManager::GetCredentials() const {
  // TODO(crbug.com/814787): Once the ChromeOS test setup is fixed, we can just
  // use |sync_account_| directly here.
  const AccountInfo account_info = GetActiveAccountInfo().account_info;

  syncer::SyncCredentials credentials;
  credentials.account_id = account_info.account_id;
  credentials.email = account_info.email;
  credentials.sync_token = access_token_;

  return credentials;
}

void SyncAuthManager::ConnectionStatusChanged(syncer::ConnectionStatus status) {
  partial_token_status_.connection_status_update_time = base::Time::Now();
  partial_token_status_.connection_status = status;

  switch (status) {
    case syncer::CONNECTION_AUTH_ERROR:
      // Sync server returned error indicating that access token is invalid. It
      // could be either expired or access is revoked. Let's request another
      // access token and if access is revoked then request for token will fail
      // with corresponding error. If access token is repeatedly reported
      // invalid, there may be some issues with server, e.g. authentication
      // state is inconsistent on sync and token server. In that case, we
      // backoff token requests exponentially to avoid hammering token server
      // too much and to avoid getting same token due to token server's caching
      // policy. |request_access_token_retry_timer_| is used to backoff request
      // triggered by both auth error and failure talking to GAIA server.
      // Therefore, we're likely to reach the backoff ceiling more quickly than
      // you would expect from looking at the BackoffPolicy if both types of
      // errors happen. We shouldn't receive two errors back-to-back without
      // attempting a token/sync request in between, thus crank up request delay
      // unnecessary. This is because we won't make a sync request if we hit an
      // error until GAIA succeeds at sending a new token, and we won't request
      // a new token unless sync reports a token failure. But to be safe, don't
      // schedule request if this happens.
      if (ongoing_access_token_fetch_) {
        // A request is already in flight; nothing further needs to be done at
        // this point.
        DCHECK(access_token_.empty());
        DCHECK(!request_access_token_retry_timer_.IsRunning());
      } else if (request_access_token_retry_timer_.IsRunning()) {
        // The timer to perform a request later is already running; nothing
        // further needs to be done at this point.
        DCHECK(access_token_.empty());
      } else if (request_access_token_backoff_.failure_count() == 0) {
        // First time request without delay. Currently invalid token is used
        // to initialize sync engine and we'll always end up here. We don't
        // want to delay initialization.
        request_access_token_backoff_.InformOfRequest(false);
        RequestAccessToken();
      } else {
        // Drop any access token here, to maintain the invariant that only one
        // of a token OR a pending request OR a pending retry can exist at any
        // time.
        InvalidateAccessToken();
        request_access_token_backoff_.InformOfRequest(false);
        ScheduleAccessTokenRequest();
      }
      break;
    case syncer::CONNECTION_OK:
      // Reset backoff time after successful connection.
      // Request shouldn't be scheduled at this time. But if it is, it's
      // possible that sync flips between OK and auth error states rapidly,
      // thus hammers token server. To be safe, only reset backoff delay when
      // no scheduled request.
      if (!request_access_token_retry_timer_.IsRunning()) {
        request_access_token_backoff_.Reset();
      }
      last_auth_error_ = GoogleServiceAuthError::AuthErrorNone();
      break;
    case syncer::CONNECTION_SERVER_ERROR:
      // TODO(crbug.com/839834): Verify whether CONNECTION_FAILED is really an
      // appropriate auth error here; maybe SERVICE_ERROR would be better?
      last_auth_error_ =
          GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED);
      break;
    case syncer::CONNECTION_NOT_ATTEMPTED:
      // The connection status should never change to "not attempted".
      NOTREACHED();
      break;
  }
}

void SyncAuthManager::InvalidateAccessToken() {
  if (access_token_.empty()) {
    return;
  }

  identity_manager_->RemoveAccessTokenFromCache(
      sync_account_.account_info.account_id,
      identity::ScopeSet{GaiaConstants::kChromeSyncOAuth2Scope}, access_token_);

  access_token_.clear();
  credentials_changed_callback_.Run();
}

void SyncAuthManager::ClearAccessTokenAndRequest() {
  access_token_.clear();
  request_access_token_retry_timer_.Stop();
  ongoing_access_token_fetch_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SyncAuthManager::ScheduleAccessTokenRequest() {
  DCHECK(access_token_.empty());
  DCHECK(!ongoing_access_token_fetch_);
  DCHECK(!request_access_token_retry_timer_.IsRunning());

  request_access_token_retry_timer_.Start(
      FROM_HERE, request_access_token_backoff_.GetTimeUntilRelease(),
      base::BindRepeating(&SyncAuthManager::RequestAccessToken,
                          weak_ptr_factory_.GetWeakPtr()));
}

void SyncAuthManager::Clear() {
  // TODO(crbug.com/839834): Clearing the auth error here isn't quite right.
  // It makes sense to clear any auth error we got from the Sync server, but we
  // should probably retain any errors from the identity manager.
  last_auth_error_ = GoogleServiceAuthError::AuthErrorNone();
  ClearAccessTokenAndRequest();
}

void SyncAuthManager::OnPrimaryAccountSet(
    const AccountInfo& primary_account_info) {
  UpdateSyncAccountIfNecessary();
}

void SyncAuthManager::OnPrimaryAccountCleared(
    const AccountInfo& previous_primary_account_info) {
  UMA_HISTOGRAM_ENUMERATION("Sync.StopSource", syncer::SIGN_OUT,
                            syncer::STOP_SOURCE_LIMIT);
  UpdateSyncAccountIfNecessary();
}

void SyncAuthManager::OnRefreshTokenUpdatedForAccount(
    const AccountInfo& account_info,
    bool is_valid) {
  if (UpdateSyncAccountIfNecessary()) {
    // If the syncing account was updated as a result of this, then all that's
    // necessary has been handled; nothing else to be done here.
    return;
  }

  if (account_info.account_id != sync_account_.account_info.account_id) {
    return;
  }

  if (!is_valid) {
    // When the refresh token is replaced by an invalid token, Sync must be
    // stopped immediately, even if the current access token is still valid.
    // This happens e.g. when the user signs out of the web with Dice enabled.
    ClearAccessTokenAndRequest();

    // Set the last auth error to the one that is specified in
    // google_service_auth_error.h to correspond to this case (token was
    // invalidated client-side).
    // TODO(blundell): Long-term, it would be nicer if Sync didn't have to
    // cache signin-level authentication errors.
    GoogleServiceAuthError invalid_token_error =
        GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_CLIENT);
    last_auth_error_ = invalid_token_error;

    credentials_changed_callback_.Run();
    return;
  }

  // If we already have an access token or previously failed to retrieve one
  // (and hence the retry timer is running), then request a fresh access token
  // now. This will also drop the current access token.
  if (!access_token_.empty() || request_access_token_retry_timer_.IsRunning()) {
    DCHECK(!ongoing_access_token_fetch_);
    RequestAccessToken();
  } else if (last_auth_error_ != GoogleServiceAuthError::AuthErrorNone()) {
    // If we were in an auth error state, then now's also a good time to try
    // again. In this case it's possible that there is already a pending
    // request, in which case RequestAccessToken will simply do nothing.
    RequestAccessToken();
  }
}

void SyncAuthManager::OnRefreshTokenRemovedForAccount(
    const std::string& account_id) {
  // If we're syncing to a different account, then this doesn't affect us.
  if (account_id != sync_account_.account_info.account_id) {
    return;
  }

  if (UpdateSyncAccountIfNecessary()) {
    // If the syncing account was updated as a result of this, then all that's
    // necessary has been handled; nothing else to be done here.
    return;
  }

  // If we're still here, then that means Chrome is still signed in to this
  // account. Keep Sync alive but set an auth error.
  DCHECK_EQ(sync_account_.account_info.account_id,
            identity_manager_->GetPrimaryAccountId());

  // TODO(crbug.com/839834): REQUEST_CANCELED doesn't seem like the right auth
  // error to use here. Maybe INVALID_GAIA_CREDENTIALS?
  last_auth_error_ =
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
  ClearAccessTokenAndRequest();

  credentials_changed_callback_.Run();
}

void SyncAuthManager::OnAccountsInCookieUpdated(
    const std::vector<AccountInfo>& accounts) {
  UpdateSyncAccountIfNecessary();
}

bool SyncAuthManager::IsRetryingAccessTokenFetchForTest() const {
  return request_access_token_retry_timer_.IsRunning();
}

void SyncAuthManager::ResetRequestAccessTokenBackoffForTest() {
  request_access_token_backoff_.Reset();
}

SyncAuthManager::SyncAccountInfo SyncAuthManager::DetermineAccountToUse()
    const {
  DCHECK(registered_for_auth_notifications_);

  // If there is a "primary account", i.e. the user explicitly chose to
  // sign-in to Chrome, then always use that account.
  if (identity_manager_->HasPrimaryAccount()) {
    return SyncAccountInfo(identity_manager_->GetPrimaryAccountInfo(),
                           /*is_primary=*/true);
  }

  // Otherwise, fall back to the default content area signed-in account.
  // TODO(crbug.com/871221): Add tests for this code path.
  if (base::FeatureList::IsEnabled(switches::kSyncStandaloneTransport) &&
      base::FeatureList::IsEnabled(switches::kSyncSupportSecondaryAccount)) {
    // Check if there is a content area signed-in account, and we have a refresh
    // token for it.
    std::vector<AccountInfo> cookie_accounts =
        identity_manager_->GetAccountsInCookieJar("SyncAuthManager");
    if (!cookie_accounts.empty() &&
        identity_manager_->HasAccountWithRefreshToken(
            cookie_accounts[0].account_id)) {
      return SyncAccountInfo(cookie_accounts[0], /*is_primary=*/false);
    }
  }
  return SyncAccountInfo();
}

bool SyncAuthManager::UpdateSyncAccountIfNecessary() {
  SyncAccountInfo new_account = DetermineAccountToUse();
  // If we're already using this account and its |is_primary| bit hasn't changed
  // (or there was and is no account to use), then there's nothing to do.
  if (new_account.account_info.account_id ==
          sync_account_.account_info.account_id &&
      new_account.is_primary == sync_account_.is_primary) {
    return false;
  }

  // Something has changed: Either this is a sign-in or sign-out, or the account
  // changed, or the account stayed the same but its |is_primary| bit changed.

  // Sign out of the old account (if any).
  if (!sync_account_.account_info.account_id.empty()) {
    sync_account_ = SyncAccountInfo();
    // Also clear any pending request or auth errors we might have, since they
    // aren't meaningful anymore.
    Clear();
    account_state_changed_callback_.Run();
  }

  // Sign in to the new account (if any).
  if (!new_account.account_info.account_id.empty()) {
    DCHECK_EQ(GoogleServiceAuthError::NONE, last_auth_error_.state());
    sync_account_ = new_account;
    account_state_changed_callback_.Run();
  }

  return true;
}

void SyncAuthManager::RequestAccessToken() {
  // Only one active request at a time.
  if (ongoing_access_token_fetch_) {
    DCHECK(access_token_.empty());
    DCHECK(!request_access_token_retry_timer_.IsRunning());
    return;
  }

  // If a request is scheduled for later, abandon that now since we'll send one
  // immediately.
  if (request_access_token_retry_timer_.IsRunning()) {
    request_access_token_retry_timer_.Stop();
  }

  const identity::ScopeSet kOAuth2ScopeSet{
      GaiaConstants::kChromeSyncOAuth2Scope};

  // Invalidate any previous token, otherwise the token service will return the
  // same token again.
  InvalidateAccessToken();

  // Finally, kick off a new access token fetch.
  partial_token_status_.token_request_time = base::Time::Now();
  partial_token_status_.token_receive_time = base::Time();
  ongoing_access_token_fetch_ =
      identity_manager_->CreateAccessTokenFetcherForAccount(
          sync_account_.account_info.account_id, kSyncOAuthConsumerName,
          kOAuth2ScopeSet,
          base::BindOnce(&SyncAuthManager::AccessTokenFetched,
                         base::Unretained(this)),
          identity::AccessTokenFetcher::Mode::kWaitUntilRefreshTokenAvailable);
}

void SyncAuthManager::AccessTokenFetched(
    GoogleServiceAuthError error,
    identity::AccessTokenInfo access_token_info) {
  DCHECK(ongoing_access_token_fetch_);
  ongoing_access_token_fetch_.reset();
  DCHECK(!request_access_token_retry_timer_.IsRunning());

  access_token_ = access_token_info.token;
  partial_token_status_.last_get_token_error = error;

  DCHECK_EQ(access_token_.empty(),
            error.state() != GoogleServiceAuthError::NONE);

  switch (error.state()) {
    case GoogleServiceAuthError::NONE:
      partial_token_status_.token_receive_time = base::Time::Now();
      sync_prefs_->SetSyncAuthError(false);
      last_auth_error_ = GoogleServiceAuthError::AuthErrorNone();
      break;
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::REQUEST_CANCELED:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      // Transient error. Retry after some time.
      // TODO(crbug.com/839834): SERVICE_ERROR is actually considered a
      // persistent error. Should we use .IsTransientError() instead of manually
      // listing cases here?
      request_access_token_backoff_.InformOfRequest(false);
      ScheduleAccessTokenRequest();
      break;
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
      sync_prefs_->SetSyncAuthError(true);
      last_auth_error_ = error;
      break;
    default:
      LOG(ERROR) << "Unexpected persistent error: " << error.ToString();
      last_auth_error_ = error;
  }

  credentials_changed_callback_.Run();
}

}  // namespace browser_sync
