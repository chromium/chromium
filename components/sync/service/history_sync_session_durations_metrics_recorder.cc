// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/history_sync_session_durations_metrics_recorder.h"

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_user_settings.h"

namespace syncer {

namespace {

base::TimeDelta SubtractInactiveTime(base::TimeDelta total_length,
                                     base::TimeDelta inactive_time) {
  // Subtract any time the user was inactive from our session length. If this
  // ends up giving the session negative length, which can happen if the feature
  // state changed after the user became inactive, log the length as 0.
  base::TimeDelta session_length = total_length - inactive_time;
  if (session_length.is_negative()) {
    session_length = base::TimeDelta();
  }
  return session_length;
}

void LogDuration(std::string_view histogram_suffix,
                 base::TimeDelta session_length) {
  base::UmaHistogramCustomTimes(
      base::StrCat({"Session.TotalDurationMax1Day.", histogram_suffix}),
      session_length, base::Milliseconds(1), base::Hours(24), 50);
}

}  // namespace

HistorySyncSessionDurationsMetricsRecorder::
    HistorySyncSessionDurationsMetricsRecorder(SyncService* sync_service)
    : sync_service_(sync_service) {
  // `sync_service` can be null if sync is disabled by a command line flag.
  if (sync_service_) {
    sync_observation_.Observe(sync_service_.get());
  }

  // Get the initial state.
  history_sync_status_ = DetermineHistorySyncStatus();
}

HistorySyncSessionDurationsMetricsRecorder::
    ~HistorySyncSessionDurationsMetricsRecorder() {
  DCHECK(!total_session_timer_) << "Missing a call to OnSessionEnded().";
  sync_observation_.Reset();
}

void HistorySyncSessionDurationsMetricsRecorder::OnSessionStarted() {
  total_session_timer_.emplace();
  history_sync_state_timer_.emplace();
}

void HistorySyncSessionDurationsMetricsRecorder::OnSessionEnded(
    base::TimeDelta session_length) {
  if (!total_session_timer_) {
    // If there was no active session, just ignore this call.
    return;
  }
  CHECK(history_sync_state_timer_);

  if (session_length.is_zero()) {
    // During Profile teardown, this method is called with a `session_length`
    // of zero.
    session_length = total_session_timer_->Elapsed();
  }

  base::TimeDelta total_session_time = total_session_timer_->Elapsed();
  base::TimeDelta history_sync_state_session_time =
      history_sync_state_timer_->Elapsed();
  total_session_timer_.reset();
  history_sync_state_timer_.reset();

  base::TimeDelta total_inactivity_time = total_session_time - session_length;

  LogHistorySyncDuration(history_sync_status_,
                         SubtractInactiveTime(history_sync_state_session_time,
                                              total_inactivity_time));
}

void HistorySyncSessionDurationsMetricsRecorder::OnStateChanged(
    SyncService* sync) {
  HistorySyncStatus new_history_sync_status = DetermineHistorySyncStatus();
  if (history_sync_status_ == new_history_sync_status) {
    return;
  }

  // If there's an ongoing session, record it and start a new one.
  if (history_sync_state_timer_) {
    LogHistorySyncDuration(history_sync_status_,
                           history_sync_state_timer_->Elapsed());
    history_sync_state_timer_.emplace();
  }

  history_sync_status_ = new_history_sync_status;
}

void HistorySyncSessionDurationsMetricsRecorder::OnSyncShutdown(
    SyncService* sync) {
  // Unreachable, since the service owning this instance is Shutdown() before
  // the SyncService.
  NOTREACHED();
}

HistorySyncSessionDurationsMetricsRecorder::HistorySyncStatus
HistorySyncSessionDurationsMetricsRecorder::DetermineHistorySyncStatus() const {
  if (!sync_service_ ||
      !sync_service_->GetUserSettings()->GetSelectedTypes().Has(
          UserSelectableType::kHistory)) {
    return HistorySyncStatus::kDisabled;
  }
  if (sync_service_->GetTransportState() ==
          SyncService::TransportState::PAUSED ||
      sync_service_->HasCachedPersistentAuthErrorForMetrics()) {
    return HistorySyncStatus::kEnabledWithError;
  }
  return HistorySyncStatus::kEnabledWithoutError;
}

// static
void HistorySyncSessionDurationsMetricsRecorder::LogHistorySyncDuration(
    HistorySyncStatus history_sync_status,
    base::TimeDelta session_length) {
  switch (history_sync_status) {
    case HistorySyncStatus::kDisabled:
      LogDuration("WithoutHistorySync", session_length);
      break;
    case HistorySyncStatus::kEnabledWithoutError:
      LogDuration("WithHistorySyncWithoutAuthError", session_length);
      // "WithHistorySync" gets logged regardless of whether there is an auth
      // error or not.
      LogDuration("WithHistorySync", session_length);
      break;
    case HistorySyncStatus::kEnabledWithError:
      LogDuration("WithHistorySyncAndAuthError", session_length);
      // "WithHistorySync" gets logged regardless of whether there is an auth
      // error or not.
      LogDuration("WithHistorySync", session_length);
      break;
  }
}

}  // namespace syncer
