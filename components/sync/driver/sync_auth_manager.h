// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_AUTH_MANAGER_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_AUTH_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_auth_util.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/engine/connection_status.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/backoff_entry.h"

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

namespace syncer {

struct SyncCredentials;

// SyncAuthManager tracks the account to be used for Sync and its authentication
// state. Note that this account may or may not be the primary account (as per
// IdentityManager::GetPrimaryAccountInfo() etc).
class SyncAuthManager : public signin::IdentityManager::Observer {
 public:
  // Called when the existence of an authenticated account changes. It's
  // guaranteed that this is only called for going from "no account" to "have
  // account" or vice versa, or if the existing account's |is_primary| bit
  // changed. I.e. SyncAuthManager will never directly switch from one account
  // to a different one. Call GetActiveAccountInfo to get the new state.
  using AccountStateChangedCallback = base::RepeatingClosure;
  // Called when the credential state changes, i.e. an access token was
  // added/changed/removed. Call GetCredentials to get the new state.
  using CredentialsChangedCallback = base::RepeatingClosure;

  // |identity_manager| may be null (this is the case if local Sync is enabled),
  // but if non-null, must outlive this object.
  SyncAuthManager(signin::IdentityManager* identity_manager,
                  const AccountStateChangedCallback& account_state_changed,
                  const CredentialsChangedCallback& credentials_changed);
  ~SyncAuthManager() override;

  // Tells the tracker to start listening for changes to the account/sign-in
  // status. This gets called during SyncService initialization, except in the
  // case of local Sync. Before this is called, GetActiveAccountInfo will always
  // return an empty AccountInfo. Note that this will *not* trigger any
  // callbacks, even if there is an active account afterwards.
  void RegisterForAuthNotifications();

  // Returns whether all relevant account information as returned by
  // GetActiveAccountInfo() has been fully loaded.
  bool IsActiveAccountInfoFullyLoaded() const;

  // Returns the account which should be used when communicating with the Sync
  // server. Note that this account may not be blessed for Sync-the-feature.
  SyncAccountInfo GetActiveAccountInfo() const;

  // Returns the last auth error that was encountered. The error could have come
  // from the Sync server or from the IdentityManager.
  GoogleServiceAuthError GetLastAuthError() const;

  // Returns the time at which the last auth error was set.
  base::Time GetLastAuthErrorTime() const;

  // Returns whether we are in the "Sync paused" state. That means there is a
  // primary account, but the user signed out in the content area, and so we
  // don't have credentials for it anymore.
  bool IsSyncPaused() const;

  // Returns the credentials to be passed to the SyncEngine.
  SyncCredentials GetCredentials() const;

  const std::string& access_token() const { return access_token_; }

  // Returns the state of the access token and token request, for display in
  // internals UI.
  SyncTokenStatus GetSyncTokenStatus() const;

  // Called by ProfileSyncService when Sync starts up and will try talking to
  // the server soon. This initiates fetching an access token.
  void ConnectionOpened();

  // Called by ProfileSyncService when the status of the connection to the Sync
  // server changed. Updates auth error state accordingly.
  void ConnectionStatusChanged(ConnectionStatus status);

  // Called by ProfileSyncService when the connection to the Sync server is
  // closed (due to Sync being shut down). Clears all related state (such as
  // cached access token, error from the server, etc).
  void ConnectionClosed();

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountSet(
      const CoreAccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnRefreshTokensLoaded() override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  // Test-only methods for inspecting/modifying internal state.
  bool IsRetryingAccessTokenFetchForTest() const;
  void ResetRequestAccessTokenBackoffForTest();

 private:
  SyncAccountInfo DetermineAccountToUse() const;

  // Updates |sync_account_| to the appropriate account (i.e.
  // DetermineAccountToUse) if necessary, and notifies observers of any changes
  // (sign-in/sign-out/"primary" bit change). Note that changing from one
  // account to another is exposed to observers as a sign-out + sign-in.
  // Returns whether the syncing account was updated.
  bool UpdateSyncAccountIfNecessary();

  // Invalidates any current access token, which means invalidating it with the
  // IdentityManager and also dropping our own cached copy. Meant to be called
  // when we know the current token is invalid (e.g. expired). Does not do
  // anything about any scheduled or ongoing request.
  void InvalidateAccessToken();

  // Clears any access token we have, and cancels any pending or scheduled
  // request for one.
  void ClearAccessTokenAndRequest();

  // Schedules a request for an access token according to the current
  // |request_access_token_backoff_|. Usually called after some transient error.
  void ScheduleAccessTokenRequest();

  // Immediately starts an access token request, unless one is already ongoing.
  // If another request is scheduled for later, it is canceled. Any access token
  // we currently have is invalidated.
  void RequestAccessToken();

  // Callback for |ongoing_access_token_fetch_|.
  void AccessTokenFetched(GoogleServiceAuthError error,
                          signin::AccessTokenInfo access_token_info);

  void SetLastAuthError(const GoogleServiceAuthError& error);

  signin::IdentityManager* const identity_manager_;

  const AccountStateChangedCallback account_state_changed_callback_;
  const CredentialsChangedCallback credentials_changed_callback_;

  bool registered_for_auth_notifications_ = false;

  // The account which we are using to sync. If this is non-empty, that does
  // *not* necessarily imply that Sync is actually running, e.g. because of
  // delayed startup.
  SyncAccountInfo sync_account_;

  // This is a cache of the last authentication response we received from
  // Chrome's identity/token management system.
  GoogleServiceAuthError last_auth_error_;
  base::Time last_auth_error_time_;

  // Whether Sync is currently connected to the server, i.e. ConnectionOpened()
  // has been called, but ConnectionClosed() hasn't. While this is false, we
  // don't try to get an access token. While it's true, we will *usually* have
  // either an access token or a pending/scheduled request for one, but this is
  // not guaranteed (e.g. in the case of a persistent auth error).
  bool connection_open_ = false;

  // The current access token. This is mutually exclusive with
  // |ongoing_access_token_fetch_| and |request_access_token_retry_timer_|:
  // We have at most one of a) an access token OR b) a pending request OR c) a
  // pending retry i.e. a scheduled request.
  std::string access_token_;

  // Pending request for an access token. Non-null iff there is a request
  // ongoing.
  std::unique_ptr<signin::AccessTokenFetcher> ongoing_access_token_fetch_;

  // If RequestAccessToken fails with transient error then retry requesting
  // access token with exponential backoff.
  base::OneShotTimer request_access_token_retry_timer_;
  net::BackoffEntry request_access_token_backoff_;

  // Info about the state of our access token, for display in the internals UI.
  // "Partial" because this instance is not fully populated - in particular,
  // |has_token| and |next_token_request_time| get computed on demand.
  SyncTokenStatus partial_token_status_;

  base::WeakPtrFactory<SyncAuthManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncAuthManager);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_AUTH_MANAGER_H_
