// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SESSION_DURATIONS_METRICS_RECORDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SESSION_DURATIONS_METRICS_RECORDER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/elapsed_timer.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace syncer {
class SyncService;
}  // namespace syncer

class PrefService;

namespace password_manager {

// Tracks the active browsing time that the user spends in each state related to
// the account-scoped password storage, i.e. signed in or not, opted in or not.
class PasswordSessionDurationsMetricsRecorder
    : public syncer::SyncServiceObserver {
 public:
  // |pref_service| must not be null and must outlive this object.
  // |sync_service| may be null (in incognito profiles or due to a commandline
  // flag), but if non-null must outlive this object.
  PasswordSessionDurationsMetricsRecorder(PrefService* pref_service,
                                          syncer::SyncService* sync_service);
  ~PasswordSessionDurationsMetricsRecorder() override;

  PasswordSessionDurationsMetricsRecorder(
      const PasswordSessionDurationsMetricsRecorder&) = delete;

  // Informs this class that a browsing session started or ended.
  void OnSessionStarted(base::TimeTicks session_start);
  void OnSessionEnded(base::TimeDelta session_length);

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  void CheckForUserStateChange();

  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<syncer::SyncService> sync_service_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};

  // Tracks the elapsed active session time while the browser is open. The timer
  // is null if there's no active session.
  std::unique_ptr<base::ElapsedTimer> total_session_timer_;

  // The current state of the user. Whenever this changes, duration metrics are
  // recorded.
  features_util::PasswordAccountStorageUserState user_state_;

  // Tracks the elapsed active session time in the current state. The timer is
  // null if there's no active session.
  std::unique_ptr<base::ElapsedTimer> user_state_session_timer_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SESSION_DURATIONS_METRICS_RECORDER_H_
