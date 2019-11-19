// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_SESSION_DURATIONS_METRICS_RECORDER_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_SESSION_DURATIONS_METRICS_RECORDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/scoped_observer.h"
#include "base/timer/elapsed_timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"

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
  ~SyncSessionDurationsMetricsRecorder() override;

  // Informs this service that a session started at |session_start| time.
  void OnSessionStarted(base::TimeTicks session_start);
  void OnSessionEnded(base::TimeDelta session_length);

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

  // IdentityManager::Observer:
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

  void LogSigninDuration(base::TimeDelta session_length);

  void LogSyncAndAccountDuration(base::TimeDelta session_length);

  bool ShouldLogUpdate(FeatureState new_sync_status,
                       FeatureState new_account_status);

  void UpdateSyncAndAccountStatus(FeatureState new_sync_status,
                                  FeatureState new_account_status);

  void HandleSyncAndAccountChange();

  // Returns |FeatureState::ON| iff there is at least one account in
  // |identity_manager| that has a valid refresh token.
  FeatureState DetermineAccountStatus() const;

  SyncService* const sync_service_;
  signin::IdentityManager* const identity_manager_;

  ScopedObserver<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observer_{this};
  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      identity_manager_observer_{this};

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

  DISALLOW_COPY_AND_ASSIGN(SyncSessionDurationsMetricsRecorder);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_SESSION_DURATIONS_METRICS_RECORDER_H_
