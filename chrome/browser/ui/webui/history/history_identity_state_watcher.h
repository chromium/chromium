// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_IDENTITY_STATE_WATCHER_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_IDENTITY_STATE_WATCHER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service_observer.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

struct HistoryIdentityState {
  // LINT.IfChange(HistoryIdentityState.SignIn)
  enum class SignIn {
    kSignedOut = 0,
    // The user is signed in only in web.
    kWebOnlySignedIn = 1,
    // The user is signed in.
    kSignedIn = 2,
    // The user is pending sign-in.
    kSignInPending = 3,
  };
  // LINT.ThenChange(/chrome/browser/resources/history/constants.ts:HistorySignInState)

  // This enum is used to differentiate all the relevant history-sync states.
  // LINT.IfChange(HistoryIdentityState.SyncState)
  enum class SyncState {
    kTurnedOff = 0,
    kTurnedOn = 1,
    kDisabled = 2,
  };
  // LINT.ThenChange(/chrome/browser/resources/history/constants.ts:SyncState)

  SignIn sign_in = SignIn::kSignedOut;
  SyncState tab_sync = SyncState::kTurnedOff;
  SyncState history_sync = SyncState::kTurnedOff;

  bool operator==(const HistoryIdentityState& other) const = default;
};

// Watches for changes in the history sign-in state.
class HistoryIdentityStateWatcher : public syncer::SyncServiceObserver,
                                    public signin::IdentityManager::Observer {
 public:
  // `identity_manager` and `sync_service` may be null, but if non-null, must
  // outlive this instance.
  HistoryIdentityStateWatcher(signin::IdentityManager* identity_manager,
                              syncer::SyncService* sync_service,
                              base::RepeatingClosure callback);

  HistoryIdentityStateWatcher(const HistoryIdentityStateWatcher&) = delete;
  HistoryIdentityStateWatcher& operator=(const HistoryIdentityStateWatcher&) =
      delete;

  ~HistoryIdentityStateWatcher() override;

  HistoryIdentityState GetHistoryIdentityState() const;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  // Returns the current history-related sign-in state.
  HistoryIdentityState::SignIn GetHistorySignInState() const;

  // Returns the sync state for a given type.
  HistoryIdentityState::SyncState GetSyncStateForType(
      syncer::UserSelectableType type) const;

  // Checks if the sign-in and tabs sync state has changed and runs the callback
  // if it has.
  void UpdateIdentityState();
  // Weak references to the services this class observes.
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<syncer::SyncService> sync_service_;

  // Called when the history sync state changes.
  base::RepeatingClosure callback_;

  HistoryIdentityState cached_identity_state_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_IDENTITY_STATE_WATCHER_H_
