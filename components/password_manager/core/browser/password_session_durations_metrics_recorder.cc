// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_session_durations_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"

namespace password_manager {

namespace {

void LogStateDuration(features_util::PasswordAccountStorageUserState user_state,
                      base::TimeDelta session_length) {
  std::string suffix =
      metrics_util::GetPasswordAccountStorageUserStateHistogramSuffix(
          user_state);
  base::UmaHistogramLongTimes(
      "PasswordManager.AccountStorageUserStateDuration." + suffix,
      session_length);
}

}  // namespace

PasswordSessionDurationsMetricsRecorder::
    PasswordSessionDurationsMetricsRecorder(PrefService* pref_service,
                                            syncer::SyncService* sync_service)
    : pref_service_(pref_service),
      sync_service_(sync_service),
      user_state_(features_util::ComputePasswordAccountStorageUserState(
          pref_service_,
          sync_service_)) {
  DCHECK(pref_service_);
  // |sync_service| can be null if sync is disabled by a command line flag.
  if (sync_service_) {
    sync_observation_.Observe(sync_service_.get());
  }
}

PasswordSessionDurationsMetricsRecorder::
    ~PasswordSessionDurationsMetricsRecorder() {
  DCHECK(!total_session_timer_) << "Missing a call to OnSessionEnded().";
}

void PasswordSessionDurationsMetricsRecorder::OnSessionStarted(
    base::TimeTicks session_start) {
  total_session_timer_ = std::make_unique<base::ElapsedTimer>();
  user_state_session_timer_ = std::make_unique<base::ElapsedTimer>();
}

void PasswordSessionDurationsMetricsRecorder::OnSessionEnded(
    base::TimeDelta session_length) {
  // If there was no active session, just ignore this call.
  if (!total_session_timer_) {
    return;
  }

  if (session_length.is_zero()) {
    // During Profile teardown, this method is called with a |session_length|
    // of zero.
    session_length = total_session_timer_->Elapsed();
  }

  DCHECK(user_state_session_timer_);

  // Record metrics for the just-ended session.
  base::TimeDelta total_session_time = total_session_timer_->Elapsed();
  base::TimeDelta user_state_session_time =
      user_state_session_timer_->Elapsed();
  total_session_timer_.reset();
  user_state_session_timer_.reset();

  // Subtract any time the user was inactive from our session length. If this
  // ends up giving the session negative length, which can happen if the
  // state changed after the user became inactive, log the length as 0.
  base::TimeDelta inactive_time = total_session_time - session_length;
  base::TimeDelta effective_session_time =
      std::max(user_state_session_time - inactive_time, base::TimeDelta());
  LogStateDuration(user_state_, effective_session_time);
}

void PasswordSessionDurationsMetricsRecorder::OnStateChanged(
    syncer::SyncService* sync) {
  CheckForUserStateChange();
}

void PasswordSessionDurationsMetricsRecorder::CheckForUserStateChange() {
  features_util::PasswordAccountStorageUserState new_user_state =
      features_util::ComputePasswordAccountStorageUserState(pref_service_,
                                                            sync_service_);
  // If the state is unchanged, nothing to do.
  if (new_user_state == user_state_) {
    return;
  }

  // The state has changed, so record metrics for the just-ended part of the
  // browsing session, and start recording time against the new state. However,
  // if there is no active session yet, then there's nothing to record.
  if (total_session_timer_) {
    DCHECK(user_state_session_timer_);
    LogStateDuration(user_state_, user_state_session_timer_->Elapsed());
    user_state_session_timer_ = std::make_unique<base::ElapsedTimer>();
  }
  user_state_ = new_user_state;
}

}  // namespace password_manager
