// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_HISTORY_SYNC_SESSION_DURATIONS_METRICS_RECORDER_H_
#define COMPONENTS_SYNC_SERVICE_HISTORY_SYNC_SESSION_DURATIONS_METRICS_RECORDER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace syncer {

// Tracks the active browsing time that the user spends with history sync
// enabled as fraction of their total browsing time.
class HistorySyncSessionDurationsMetricsRecorder
    : public syncer::SyncServiceObserver {
 public:
  // Callers must ensure that the parameters outlive this object.
  explicit HistorySyncSessionDurationsMetricsRecorder(
      SyncService* sync_service);

  HistorySyncSessionDurationsMetricsRecorder(
      const HistorySyncSessionDurationsMetricsRecorder&) = delete;
  HistorySyncSessionDurationsMetricsRecorder& operator=(
      const HistorySyncSessionDurationsMetricsRecorder&) = delete;

  ~HistorySyncSessionDurationsMetricsRecorder() override;

  // Informs this service that a session started.
  void OnSessionStarted();
  void OnSessionEnded(base::TimeDelta session_length);

  // syncer::SyncServiceObserver:
  void OnStateChanged(SyncService* sync) override;
  void OnSyncShutdown(SyncService* sync) override;

 private:
  enum class HistorySyncStatus {
    kDisabled,
    kEnabledWithoutError,
    kEnabledWithError,
  };

  HistorySyncStatus DetermineHistorySyncStatus() const;

  static void LogHistorySyncDuration(HistorySyncStatus history_sync_status,
                                     base::TimeDelta session_length);

  const raw_ptr<SyncService> sync_service_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};

  HistorySyncStatus history_sync_status_ = HistorySyncStatus::kDisabled;

  // Tracks the elapsed active session time while the browser is open. The timer
  // is absent if there's no active session.
  std::optional<base::ElapsedTimer> total_session_timer_;
  // Tracks the elapsed active session time in the current history-sync-enabled
  // state. Absent if there's no active session.
  std::optional<base::ElapsedTimer> history_sync_state_timer_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_HISTORY_SYNC_SESSION_DURATIONS_METRICS_RECORDER_H_
