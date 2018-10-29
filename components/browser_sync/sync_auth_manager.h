// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_SYNC_AUTH_MANAGER_H_
#define COMPONENTS_BROWSER_SYNC_SYNC_AUTH_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/signin/core/browser/account_info.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/engine/connection_status.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/backoff_entry.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace identity {
class AccessTokenFetcher;
}

namespace syncer {
struct SyncCredentials;
class SyncPrefs;
}  // namespace syncer

namespace browser_sync {

// SyncAuthManager tracks the account to be used for Sync and its authentication
// state. Note that this account may or may not be the primary account (as per
// IdentityManager::GetPrimaryAccountInfo() etc).
class SyncAuthManager : public identity::IdentityManager::Observer {
 public:
  struct SyncAccountInfo {
    SyncAccountInfo();
    SyncAccountInfo(const AccountInfo& account_info, bool is_primary);

    AccountInfo account_info;
    bool is_primary = false;
  };

  // Called when the existence of an authenticated account changes. It's
  // guaranteed that this is only called for going from "no account" to "have
  // account" or vice versa, i.e. SyncAuthManager will never directly switch
  // from one account to a different one. Call GetActiveAccountInfo to get the
  // new state.
  using AccountStateChangedCallback = base::RepeatingClosure;
  // Called when the credential state changes, i.e. an access token was
  // added/changed/removed. Call GetCredentials to get the new state.
  using CredentialsChangedCallback = base::RepeatingClosure;

  // |sync_prefs| must not be null and must outlive this.
  // |identity_manager| may be null (this is the case if local Sync is enabled),
  // but if non-null, must outlive this object.
  SyncAuthManager(syncer::SyncPrefs* sync_prefs,
                  identity::IdentityManager* identity_manager,
                  const AccountStateChangedCallback& account_state_changed,
                  const CredentialsChangedCallback& credentials_changed);
  ~SyncAuthManager() override;

  // Tells the tracker to start listening for changes to the account/sign-in
  // status. This gets called during SyncService initialization, except in the
  // case of local Sync. Before this is called, GetActiveAccountInfo will always
  // return an empty AccountInfo. Note that this will *not* trigger any
  // callbacks, even if there is an active account afterwards.
  void RegisterForAuthNotifications();

  // Returns the account which should be used when communicating with the Sync
  // server. Note that this account may not be blessed for Sync-the-feature.
  SyncAccountInfo GetActiveAccountInfo() const;

  const GoogleServiceAuthError& GetLastAuthError() const {
    return last_auth_error_;
  }

  // Returns the credentials to be passed to the SyncEngine.
  syncer::SyncCredentials GetCredentials() const;

  const std::string& access_token() const { return access_token_; }

  // Returns the state of the access token and token request, for display in
  // internals UI.
  syncer::SyncTokenStatus GetSyncTokenStatus() const;

  // Called by ProfileSyncService when the status of the connection to the Sync
  // server changed. Updates auth error state accordingly.
  void ConnectionStatusChanged(syncer::ConnectionStatus status);

  // Clears all auth-related state (error, cached access token etc). Called
  // when Sync is turned off.
  void Clear();

  // identity::IdentityManager::Observer implementation.
  void OnPrimaryAccountSet(const AccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const AccountInfo& previous_primary_account_info) override;
  void OnRefreshTokenUpdatedForAccount(const AccountInfo& account_info,
                                       bool is_valid) override;
  void OnRefreshTokenRemovedForAccount(const std::string& account_id) override;
  void OnAccountsInCookieUpdated(
      const std::vector<AccountInfo>& accounts) override;

  // Test-only methods for inspecting/modifying internal state.
  bool IsRetryingAccessTokenFetchForTest() const;
  void ResetRequestAccessTokenBackoffForTest();

 private:
  // Determines which account should be used for Sync and returns the
  // corresponding AccountInfo.
  SyncAccountInfo DetermineAccountToUse() const;

  // Updates |sync_account_| to the appropriate account (i.e.
  // DetermineAccountToUse) if necessary, and notifies observers of any changes
  // (sign-in/sign-out/"primary" bit change). Note that changing from one
  // account to another is exposed to observers as a sign-out + sign-in.
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

  void AccessTokenFetched(GoogleServiceAuthError error,
                          identity::AccessTokenInfo access_token_info);

  syncer::SyncPrefs* const sync_prefs_;
  identity::IdentityManager* const identity_manager_;

  const AccountStateChangedCallback account_state_changed_callback_;
  const CredentialsChangedCallback credentials_changed_callback_;

  bool registered_for_auth_notifications_ = false;

  // The account which we are using to sync. If this is non-empty, that does
  // *not* necessarily imply that Sync is actually running, e.g. because of
  // delayed startup.
  SyncAccountInfo sync_account_;

  // This is a cache of the last authentication response we received either
  // from the sync server or from Chrome's identity/token management system.
  // TODO(crbug.com/839834): Differentiate between these types of auth errors,
  // since their semantics and lifetimes are quite different: e.g. the former
  // can only exist while the Sync engine is initialized; the latter exists
  // independent of Sync state, and probably shouldn't get reset in Clear().
  GoogleServiceAuthError last_auth_error_;

  // The current access token. This is mutually exclusive with
  // |ongoing_access_token_fetch_| and |request_access_token_retry_timer_|:
  // We have at most one of a) an access token OR b) a pending request OR c) a
  // pending retry i.e. a scheduled request.
  std::string access_token_;

  // Pending request for an access token. Non-null iff there is a request
  // ongoing.
  std::unique_ptr<identity::AccessTokenFetcher> ongoing_access_token_fetch_;

  // If RequestAccessToken fails with transient error then retry requesting
  // access token with exponential backoff.
  base::OneShotTimer request_access_token_retry_timer_;
  net::BackoffEntry request_access_token_backoff_;

  // Info about the state of our access token, for display in the internals UI.
  // "Partial" because this instance is not fully populated - in particular,
  // |have_token| and |next_token_request_time| get computed on demand.
  syncer::SyncTokenStatus partial_token_status_;

  base::WeakPtrFactory<SyncAuthManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncAuthManager);
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_SYNC_AUTH_MANAGER_H_
