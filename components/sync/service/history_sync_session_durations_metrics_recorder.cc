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
  // |sync_service| can be null if sync is disabled by a command line flag.
  if (sync_service_) {
    sync_observation_.Observe(sync_service_.get());
  }

  // Get the initial state.
  history_sync_enabled_ = IsHistorySyncEnabled();
}

HistorySyncSessionDurationsMetricsRecorder::
    ~HistorySyncSessionDurationsMetricsRecorder() {
  DCHECK(!total_session_timer_) << "Missing a call to OnSessionEnded().";
  sync_observation_.Reset();
}

bool HistorySyncSessionDurationsMetricsRecorder::IsHistorySyncEnabled() const {
  if (!sync_service_) {
    return false;
  }
  if (!sync_service_->GetUserSettings()->GetSelectedTypes().Has(
          UserSelectableType::kHistory)) {
    return false;
  }
  return true;
}

void HistorySyncSessionDurationsMetricsRecorder::OnSessionStarted(
    base::TimeTicks session_start) {
  total_session_timer_ = std::make_unique<base::ElapsedTimer>();
  history_sync_state_timer_ = std::make_unique<base::ElapsedTimer>();
}

void HistorySyncSessionDurationsMetricsRecorder::OnSessionEnded(
    base::TimeDelta session_length) {
  if (!total_session_timer_) {
    // If there was no active session, just ignore this call.
    return;
  }
  CHECK(history_sync_state_timer_);

  if (session_length.is_zero()) {
    // During Profile teardown, this method is called with a |session_length|
    // of zero.
    session_length = total_session_timer_->Elapsed();
  }

  base::TimeDelta total_session_time = total_session_timer_->Elapsed();
  base::TimeDelta history_sync_state_session_time =
      history_sync_state_timer_->Elapsed();
  total_session_timer_.reset();
  history_sync_state_timer_.reset();

  base::TimeDelta total_inactivity_time = total_session_time - session_length;

  LogHistorySyncDuration(history_sync_enabled_,
                         SubtractInactiveTime(history_sync_state_session_time,
                                              total_inactivity_time));
}

void HistorySyncSessionDurationsMetricsRecorder::OnStateChanged(
    SyncService* sync) {
  bool new_history_sync_state = IsHistorySyncEnabled();
  if (history_sync_enabled_ == new_history_sync_state) {
    return;
  }

  // If there's an ongoing session, record it and start a new one.
  if (history_sync_state_timer_) {
    LogHistorySyncDuration(history_sync_enabled_,
                           history_sync_state_timer_->Elapsed());
    history_sync_state_timer_ = std::make_unique<base::ElapsedTimer>();
  }

  history_sync_enabled_ = new_history_sync_state;
}

// static
void HistorySyncSessionDurationsMetricsRecorder::LogHistorySyncDuration(
    bool history_sync_enabled,
    base::TimeDelta session_length) {
  LogDuration(history_sync_enabled ? "WithHistorySync" : "WithoutHistorySync",
              session_length);
}

}  // namespace syncer
