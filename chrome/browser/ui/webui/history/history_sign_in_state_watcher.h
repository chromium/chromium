// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_SIGN_IN_STATE_WATCHER_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_SIGN_IN_STATE_WATCHER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service_observer.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

// This enum is used to differentiate all the relevant sign-in/history-sync
// states.
// LINT.IfChange(HistorySignInState)
enum class HistorySignInState {
  kSignedOut = 0,
  // The user is signed in only in web.
  kWebOnlySignedIn = 1,
  // The user is signed in, but not syncing their tabs.
  kSignedInNotSyncingTabs = 2,
  // The user is signed in and syncing their tabs.
  kSignedInSyncingTabs = 3,
  // The user is pending sign-in, but not opted in to history/tabs sync before.
  kSignInPendingNotSyncingTabs = 4,
  // The user is pending sign-in and opted in to history/tabs sync before.
  kSignInPendingSyncingTabs = 5,
  // The sync is disabled.
  kSyncDisabled = 6,
};
// LINT.ThenChange(/chrome/browser/resources/history/constants.ts:HistorySignInState)

HistorySignInState GetHistorySignInState(
    const signin::IdentityManager* identity_manager,
    const syncer::SyncService* sync_service);

// Watches for changes in the history sign-in state.
class HistorySignInStateWatcher : public syncer::SyncServiceObserver,
                                  public signin::IdentityManager::Observer {
 public:
  // `identity_manager` and `sync_service` may be null, but if non-null, must
  // outlive this instance.
  HistorySignInStateWatcher(signin::IdentityManager* identity_manager,
                            syncer::SyncService* sync_service,
                            base::RepeatingClosure callback);

  HistorySignInStateWatcher(const HistorySignInStateWatcher&) = delete;
  HistorySignInStateWatcher& operator=(const HistorySignInStateWatcher&) =
      delete;

  ~HistorySignInStateWatcher() override;

  HistorySignInState GetSignInState() const;

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
  // Runs |callback_| when the sign-in state changes.
  void RunCallback();

  // Checks if the sign-in state has changed and runs the callback if it has.
  void UpdateSignInState();

  // Weak references to the services this class observes.
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<syncer::SyncService> sync_service_;

  // Called when the history sync state changes.
  base::RepeatingClosure callback_;

  HistorySignInState cached_signin_state_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_SIGN_IN_STATE_WATCHER_H_
