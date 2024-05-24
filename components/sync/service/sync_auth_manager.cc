// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_auth_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/sync/base/stop_source.h"
#include "components/sync/engine/sync_credentials.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace syncer {

namespace {

constexpr char kSyncOAuthConsumerName[] = "sync";

constexpr net::BackoffEntry::Policy
    kIgnoreFirstErrorRequestAccessTokenBackoffPolicy = {
        // Number of initial errors (in sequence) to ignore before applying
        // exponential back-off rules.
        1,

        // Initial delay for exponential back-off in ms.
        2000,

        // Factor by which the waiting time will be multiplied.
        2,

        // Fuzzing percentage. ex: 10% will spread requests randomly
        // between 90%-100% of the calculated time.
        0.2,  // 20%

        // Maximum amount of time we are willing to delay our request in ms.
        // TODO(crbug.com/40320443): We should retry RequestAccessToken on
        // connection state change after backoff.
        1000 * 3600 * 4,  // 4 hours.

        // Time to keep an entry from being discarded even when it
        // has no significant state, -1 to never discard.
        -1,

        // Don't use initial delay unless the last request was an error.
        false,
};

SyncAccountInfo DetermineAccountToUse(
    signin::IdentityManager* identity_manager) {
  // TODO(crbug.com/40246339): During signout, it can happen that the primary
  // account temporarily doesn't have a refresh token (before the account
  // itself gets removed). As a workaround for crbug.com/1383912 /
  // crbug.com/897628, do *not* use the account for Sync in this case. This
  // ensures that Sync metadata gets properly cleared during signout.
  if (identity_manager->AreRefreshTokensLoaded() &&
      !identity_manager->HasPrimaryAccountWithRefreshToken(
          signin::ConsentLevel::kSignin)) {
    return SyncAccountInfo();
  }

  // TODO(crbug.com/40066949): Simplify once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  return {.account_info = identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin),
          .is_sync_consented =
              identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)};
}

}  // namespace

SyncAuthManager::SyncAuthManager(
    signin::IdentityManager* identity_manager,
    const AccountStateChangedCallback& account_state_changed,
    const CredentialsChangedCallback& credentials_changed)
    : identity_manager_(identity_manager),
      account_state_changed_callback_(account_state_changed),
      credentials_changed_callback_(credentials_changed),
      request_access_token_backoff_(
          &kIgnoreFirstErrorRequestAccessTokenBackoffPolicy) {
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
  // If there's already a persistent auth error, also propagate that into our
  // local state. Note that (as of 2021-01) this shouldn't happen in practice:
  // Auth errors are not persisted, so it's unlikely that at this point in time
  // (early during browser startup) an auth error has already been detected.
  GoogleServiceAuthError token_error =
      identity_manager_->GetErrorStateOfRefreshTokenForAccount(
          sync_account_.account_info.account_id);
  if (token_error.IsPersistentError()) {
    SetLastAuthError(token_error);
  }
}

bool SyncAuthManager::IsActiveAccountInfoFullyLoaded() const {
  // The result of DetermineAccountToUse() is influenced by refresh tokens being
  // loaded due to how IdentityManager::ComputeUnconsentedPrimaryAccountInfo()
  // is implemented, which requires a refresh token.
  return identity_manager_->AreRefreshTokensLoaded();
}

SyncAccountInfo SyncAuthManager::GetActiveAccountInfo() const {
  // Note: |sync_account_| should generally be identical to the result of a
  // DetermineAccountToUse() call, but there are a few edge cases when it isn't:
  // E.g. when another identity observer gets notified before us and calls in
  // here, or when we're currently switching accounts in
  // UpdateSyncAccountIfNecessary(). So unfortunately we can't verify this.
  return sync_account_;
}

GoogleServiceAuthError SyncAuthManager::GetLastAuthError() const {
  DCHECK(!last_auth_error_.IsTransientError());
  return last_auth_error_;
}

base::Time SyncAuthManager::GetLastAuthErrorTime() const {
  // See GetLastAuthError().
  if (partial_token_status_.connection_status == CONNECTION_SERVER_ERROR) {
    return partial_token_status_.connection_status_update_time;
  }
  return last_auth_error_time_;
}

bool SyncAuthManager::IsSyncPaused() const {
  DCHECK(!GetLastAuthError().IsTransientError());
  return GetLastAuthError() != GoogleServiceAuthError::AuthErrorNone();
}

SyncTokenStatus SyncAuthManager::GetSyncTokenStatus() const {
  DCHECK(partial_token_status_.next_token_request_time.is_null());

  SyncTokenStatus token_status = partial_token_status_;
  token_status.has_token = !access_token_.empty();
  if (request_access_token_retry_timer_.IsRunning()) {
    base::TimeDelta delta =
        request_access_token_retry_timer_.desired_run_time() -
        base::TimeTicks::Now();
    token_status.next_token_request_time = base::Time::Now() + delta;
  }
  return token_status;
}

SyncCredentials SyncAuthManager::GetCredentials() const {
  return {.email = sync_account_.account_info.email,
          .access_token = access_token_};
}

void SyncAuthManager::ConnectionOpened() {
  DCHECK(registered_for_auth_notifications_);
  DCHECK(!connection_open_);

  connection_open_ = true;

  // At this point, we must not already have an access token or an attempt to
  // get one.
  DCHECK(access_token_.empty());
  DCHECK(!ongoing_access_token_fetch_);
  DCHECK(!request_access_token_retry_timer_.IsRunning());

  RequestAccessToken();
}

void SyncAuthManager::ConnectionStatusChanged(ConnectionStatus status) {
  DCHECK(registered_for_auth_notifications_);
  DCHECK(connection_open_);

  partial_token_status_.connection_status_update_time = base::Time::Now();
  partial_token_status_.connection_status = status;

  switch (status) {
    case CONNECTION_AUTH_ERROR:
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
      } else {
        // Drop any access token here, to maintain the invariant that only one
        // of a token OR a pending request OR a pending retry can exist at any
        // time.
        InvalidateAccessToken();
        request_access_token_backoff_.InformOfRequest(false);
        ScheduleAccessTokenRequest();
      }
      break;
    case CONNECTION_OK:
      // Reset backoff time after successful connection.
      // Request shouldn't be scheduled at this time. But if it is, it's
      // possible that sync flips between OK and auth error states rapidly,
      // thus hammers token server. To be safe, only reset backoff delay when
      // no scheduled request.
      if (!request_access_token_retry_timer_.IsRunning()) {
        request_access_token_backoff_.Reset();
      }
      break;
    case CONNECTION_SERVER_ERROR:
      // Not an auth error, nothing to do.
      break;
    case CONNECTION_NOT_ATTEMPTED:
      // The connection status should never change to "not attempted".
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void SyncAuthManager::InvalidateAccessToken() {
  DCHECK(registered_for_auth_notifications_);

  if (access_token_.empty()) {
    return;
  }

  identity_manager_->RemoveAccessTokenFromCache(
      sync_account_.account_info.account_id,
      signin::ScopeSet{GaiaConstants::kChromeSyncOAuth2Scope}, access_token_);

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

void SyncAuthManager::ConnectionClosed() {
  DCHECK(registered_for_auth_notifications_);
  DCHECK(connection_open_);

  partial_token_status_ = SyncTokenStatus();
  ClearAccessTokenAndRequest();

  connection_open_ = false;
}

void SyncAuthManager::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  UpdateSyncAccountIfNecessary();
}

void SyncAuthManager::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (UpdateSyncAccountIfNecessary()) {
    // If the syncing account was updated as a result of this, then all that's
    // necessary has been handled; nothing else to be done here.
    return;
  }

  if (account_info.account_id != sync_account_.account_info.account_id) {
    return;
  }

  // Compute the validity of the new refresh token: The identity code sets an
  // account's refresh token to be invalid if the user signs out of that account
  // on the web.
  GoogleServiceAuthError token_error =
      identity_manager_->GetErrorStateOfRefreshTokenForAccount(
          account_info.account_id);

  // GetErrorStateOfRefreshTokenForAccount() only reports persistent errors.
  DCHECK(!token_error.IsTransientError());

  if (token_error != GoogleServiceAuthError::AuthErrorNone()) {
    DCHECK(token_error.IsPersistentError());

    // When the refresh token is replaced by an invalid token, Sync must be
    // stopped immediately, even if the current access token is still valid.
    // This happens e.g. when the user signs out of the web with Dice enabled.
    ClearAccessTokenAndRequest();

    // Set the last auth error. Usually this happens in AccessTokenFetched(...)
    // if the fetch failed, but since we just canceled any access token request,
    // that's not going to happen in this case.
    SetLastAuthError(token_error);

    credentials_changed_callback_.Run();
  } else if (last_auth_error_ != GoogleServiceAuthError::AuthErrorNone()) {
    DCHECK(last_auth_error_.IsPersistentError());
    // Conversely, if we just exited the paused state, we need to reset the last
    // auth error and tell our client (i.e. the SyncService) so that it'll know
    // to resume syncing (if appropriate).
    SetLastAuthError(GoogleServiceAuthError::AuthErrorNone());
    credentials_changed_callback_.Run();

    // If we have an open connection to the server, then also get a new access
    // token now.
    if (connection_open_) {
      RequestAccessToken();
    }
  } else if (!access_token_.empty() ||
             request_access_token_retry_timer_.IsRunning()) {
    // If we already have an access token or previously failed to retrieve one
    // (and hence the retry timer is running), then request a fresh access token
    // now. This will also drop the current access token.
    DCHECK(!ongoing_access_token_fetch_);
    RequestAccessToken();
  }
}

void SyncAuthManager::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  // If we're syncing to a different account, then this doesn't affect us.
  if (account_id != sync_account_.account_info.account_id) {
    return;
  }

  bool changed = UpdateSyncAccountIfNecessary();
  // This should have removed the syncing account.
  DCHECK(changed);
  DCHECK(sync_account_.account_info.IsEmpty());
}

void SyncAuthManager::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  OnRefreshTokenUpdatedForAccount(account_info);
}

void SyncAuthManager::OnRefreshTokensLoaded() {
  DCHECK(IsActiveAccountInfoFullyLoaded());

  if (UpdateSyncAccountIfNecessary()) {
    // |account_state_changed_callback_| has already been called, no need to
    // consider calling it again.
    return;
  }

  if (sync_account_.account_info.account_id.empty()) {
    // Nothing actually changed, so |account_state_changed_callback_| hasn't
    // been called yet. However, this is the first time we can reliably tell the
    // user is signed out, exposed via IsActiveAccountInfoFullyLoaded(), so
    // let's treat it as account state change.
    account_state_changed_callback_.Run();
  }
}

bool SyncAuthManager::IsRetryingAccessTokenFetchForTest() const {
  return request_access_token_retry_timer_.IsRunning();
}

void SyncAuthManager::ResetRequestAccessTokenBackoffForTest() {
  request_access_token_backoff_.Reset();
}

SyncAccountInfo SyncAuthManager::DetermineAccountToUse() const {
  DCHECK(registered_for_auth_notifications_);
  return syncer::DetermineAccountToUse(identity_manager_);
}

bool SyncAuthManager::UpdateSyncAccountIfNecessary() {
  DCHECK(registered_for_auth_notifications_);

  SyncAccountInfo new_account = DetermineAccountToUse();
  if (new_account.account_info.account_id ==
      sync_account_.account_info.account_id) {
    // We're already using this account (or there was and is no account to use).
    // If the |is_sync_consented| bit hasn't changed either, then there's
    // nothing to do.
    if (new_account.is_sync_consented == sync_account_.is_sync_consented) {
      return false;
    }
    // The |is_sync_consented| bit *has* changed, so update our state and
    // notify.
    sync_account_ = new_account;
    account_state_changed_callback_.Run();
    return true;
  }

  // Something has changed: Either this is a sign-in or sign-out, or the account
  // changed.

  // Sign out of the old account (if any).
  if (!sync_account_.account_info.account_id.empty()) {
    sync_account_ = SyncAccountInfo();
    // Let the client (SyncService) know of the removed account *before*
    // throwing away the access token, so it can do "unregister" tasks.
    account_state_changed_callback_.Run();
    // Also clear any pending request or auth errors we might have, since they
    // aren't meaningful anymore.
    partial_token_status_ = SyncTokenStatus();
    ClearAccessTokenAndRequest();
    SetLastAuthError(GoogleServiceAuthError::AuthErrorNone());
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
  DCHECK(registered_for_auth_notifications_);
  DCHECK(connection_open_);

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

  // Invalidate any previous token, otherwise the token service will return the
  // same token again.
  InvalidateAccessToken();

  // Finally, kick off a new access token fetch.
  partial_token_status_.token_request_time = base::Time::Now();
  partial_token_status_.token_response_time = base::Time();
  ongoing_access_token_fetch_ =
      identity_manager_->CreateAccessTokenFetcherForAccount(
          sync_account_.account_info.account_id, kSyncOAuthConsumerName,
          {GaiaConstants::kChromeSyncOAuth2Scope},
          base::BindOnce(&SyncAuthManager::AccessTokenFetched,
                         base::Unretained(this)),
          signin::AccessTokenFetcher::Mode::kWaitUntilRefreshTokenAvailable);
}

void SyncAuthManager::AccessTokenFetched(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK(registered_for_auth_notifications_);

  DCHECK(ongoing_access_token_fetch_);
  ongoing_access_token_fetch_.reset();
  DCHECK(!request_access_token_retry_timer_.IsRunning());

  // Retry without backoff when the request is canceled for the first time. For
  // more details, see inline comments of
  // PrimaryAccountAccessTokenFetcher::OnAccessTokenFetchComplete.
  if (error.state() == GoogleServiceAuthError::REQUEST_CANCELED &&
      !access_token_retried_) {
    access_token_retried_ = true;
    RequestAccessToken();
    return;
  }

  access_token_ = access_token_info.token;
  partial_token_status_.token_response_time = base::Time::Now();
  partial_token_status_.last_get_token_error = error;

  DCHECK_EQ(access_token_.empty(),
            error.state() != GoogleServiceAuthError::NONE);

  if (error.IsTransientError()) {
    // Transient error. Retry after some time.
    request_access_token_backoff_.InformOfRequest(false);
    ScheduleAccessTokenRequest();
  } else {
    SetLastAuthError(error);
  }

  credentials_changed_callback_.Run();
}

void SyncAuthManager::SetLastAuthError(const GoogleServiceAuthError& error) {
  DCHECK(!error.IsTransientError());
  if (last_auth_error_ == error) {
    return;
  }
  last_auth_error_ = error;
  last_auth_error_time_ = base::Time::Now();
}

}  // namespace syncer
