// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/msbb_session_durations_metrics_recorder.h"

#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"

namespace unified_consent {

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

MsbbSessionDurationsMetricsRecorder::MsbbSessionDurationsMetricsRecorder(
    PrefService* pref_service)
    : consent_helper_(
          UrlKeyedDataCollectionConsentHelper::
              NewAnonymizedDataCollectionConsentHelper(pref_service)),
      last_msbb_enabled_(consent_helper_->IsEnabled()) {
  consent_helper_->AddObserver(this);
}

MsbbSessionDurationsMetricsRecorder::~MsbbSessionDurationsMetricsRecorder() {
  DCHECK(!total_session_timer_) << "Missing a call to OnSessionEnded().";
  consent_helper_->RemoveObserver(this);
}

void MsbbSessionDurationsMetricsRecorder::OnSessionStarted(
    base::TimeTicks session_start) {
  total_session_timer_ = std::make_unique<base::ElapsedTimer>();
  msbb_state_timer_ = std::make_unique<base::ElapsedTimer>();
}

void MsbbSessionDurationsMetricsRecorder::OnSessionEnded(
    base::TimeDelta session_length) {
  if (!total_session_timer_) {
    // If there was no active session, just ignore this call.
    return;
  }
  CHECK(msbb_state_timer_);

  if (session_length.is_zero()) {
    // During Profile teardown, this method is called with a |session_length|
    // of zero.
    session_length = total_session_timer_->Elapsed();
  }

  base::TimeDelta total_session_time = total_session_timer_->Elapsed();
  base::TimeDelta msbb_state_session_time = msbb_state_timer_->Elapsed();
  total_session_timer_.reset();
  msbb_state_timer_.reset();

  base::TimeDelta total_inactivity_time = total_session_time - session_length;

  LogMsbbDuration(
      last_msbb_enabled_,
      SubtractInactiveTime(msbb_state_session_time, total_inactivity_time));
}

void MsbbSessionDurationsMetricsRecorder::
    OnUrlKeyedDataCollectionConsentStateChanged(
        UrlKeyedDataCollectionConsentHelper* consent_helper) {
  CHECK_EQ(consent_helper_.get(), consent_helper);

  bool new_msbb_state = consent_helper_->IsEnabled();
  if (last_msbb_enabled_ == new_msbb_state) {
    return;
  }

  // If there's an ongoing session, record it and start a new one.
  if (msbb_state_timer_) {
    LogMsbbDuration(last_msbb_enabled_, msbb_state_timer_->Elapsed());
    msbb_state_timer_ = std::make_unique<base::ElapsedTimer>();
  }

  last_msbb_enabled_ = new_msbb_state;
}

// static
void MsbbSessionDurationsMetricsRecorder::LogMsbbDuration(
    bool msbb_enabled,
    base::TimeDelta session_length) {
  LogDuration(msbb_enabled ? "WithMsbb" : "WithoutMsbb", session_length);
}

}  // namespace unified_consent
