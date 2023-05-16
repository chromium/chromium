// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_SESSION_DURATIONS_METRICS_RECORDER_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_SESSION_DURATIONS_METRICS_RECORDER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/elapsed_timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace syncer {

// Tracks the active browsing time that the user spends signed in and/or syncing
// as fraction of their total browsing time.
class SyncSessionDurationsMetricsRecorder
    : public syncer::SyncServiceObserver,
      public signin::IdentityManager::Observer {
 public:
  // Callers must ensure that the parameters outlive this object.
  SyncSessionDurationsMetricsRecorder(
      SyncService* sync_service,
      signin::IdentityManager* identity_manager);

  SyncSessionDurationsMetricsRecorder(
      const SyncSessionDurationsMetricsRecorder&) = delete;
  SyncSessionDurationsMetricsRecorder& operator=(
      const SyncSessionDurationsMetricsRecorder&) = delete;

  ~SyncSessionDurationsMetricsRecorder() override;

  // Returns whether the user is signed in.
  // Note: this is not the same thing as |account_status_|.
  // |account_status_| says OFF (kind of like sayng "no, not signed-in") if the
  // account is in an error state.  IsSignedIn() does not; it will return
  // true for accounts that are signed-in in yet an error state.
  // The most common reason this happens is if a syncing user signs out
  // of the content area.  They will be put in an error state; this
  // function will return true.
  bool IsSignedIn() const;

  // Returns whether the user is syncing.
  // Note: this is not the same as |sync_status_|.
  // |sync_status_| says ON (kind of like saying "yes, syncing") even if
  // syncing is paused because the user signed out (i.e., the account is in an
  // error state).  IsSyncing() returns false in those cases.
  bool IsSyncing() const;

  // Informs this service that a session started at |session_start| time.
  void OnSessionStarted(base::TimeTicks session_start);
  void OnSessionEnded(base::TimeDelta session_length);

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

  // IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnRefreshTokensLoaded() override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error) override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

 private:
  // The state the feature is in. The state starts as UNKNOWN. After it moves
  // out of UNKNOWN, it can alternate between OFF and ON.
  enum class FeatureState { UNKNOWN, OFF, ON };

  static constexpr int GetFeatureStates(FeatureState feature1,
                                        FeatureState feature2);

  void LogSigninDuration(base::TimeDelta session_length);

  void LogSyncAndAccountDuration(base::TimeDelta session_length);

  bool ShouldLogUpdate(FeatureState new_sync_status,
                       FeatureState new_account_status);

  void UpdateSyncAndAccountStatus(FeatureState new_sync_status,
                                  FeatureState new_account_status);

  void HandleSyncAndAccountChange();

  // Returns |FeatureState::ON| iff there is a primary account with a valid
  // refresh token in the identity manager.
  FeatureState DeterminePrimaryAccountStatus() const;

  // Determines the sync status.
  FeatureState DetermineSyncStatus() const;

  const raw_ptr<SyncService> sync_service_;
  const raw_ptr<signin::IdentityManager> identity_manager_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // Tracks the elapsed active session time while the browser is open. The timer
  // is absent if there's no active session.
  std::unique_ptr<base::ElapsedTimer> total_session_timer_;

  FeatureState signin_status_ = FeatureState::UNKNOWN;
  // Tracks the elapsed active session time in the current signin status. The
  // timer is absent if there's no active session.
  std::unique_ptr<base::ElapsedTimer> signin_session_timer_;

  // Whether or not Chrome curently has a valid refresh token for an account.
  FeatureState account_status_ = FeatureState::UNKNOWN;
  // Whether or not sync is currently active.
  FeatureState sync_status_ = FeatureState::UNKNOWN;
  // Tracks the elapsed active session time in the current sync and account
  // status. The timer is absent if there's no active session.
  std::unique_ptr<base::ElapsedTimer> sync_account_session_timer_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_SESSION_DURATIONS_METRICS_RECORDER_H_
