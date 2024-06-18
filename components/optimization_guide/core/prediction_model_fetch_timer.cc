// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_model_fetch_timer.h"

#include "base/rand_util.h"
#include "base/time/default_clock.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide {

namespace {

// Provide a random time delta before fetching models.
base::TimeDelta RandomFetchDelay() {
  return base::RandTimeDelta(features::PredictionModelFetchRandomMinDelay(),
                             features::PredictionModelFetchRandomMaxDelay());
}

}  // namespace

PredictionModelFetchTimer::PredictionModelFetchTimer(
    PrefService* pref_service,
    base::RepeatingCallback<void(void)> fetch_callback)
    : clock_(base::DefaultClock::GetInstance()),
      pref_service_(pref_service),
      fetch_callback_(fetch_callback) {}

PredictionModelFetchTimer::~PredictionModelFetchTimer() = default;

void PredictionModelFetchTimer::NotifyModelFetchAttempt() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(
      prefs::kModelAndFeaturesLastFetchAttempt,
      clock_->Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
}

void PredictionModelFetchTimer::NotifyModelFetchSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(
      prefs::kModelLastFetchSuccess,
      clock_->Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
}

base::Time PredictionModelFetchTimer::GetLastFetchAttemptTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
      pref_service_->GetInt64(prefs::kModelAndFeaturesLastFetchAttempt)));
}

base::Time PredictionModelFetchTimer::GetLastFetchSuccessTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
      pref_service_->GetInt64(prefs::kModelLastFetchSuccess)));
}

bool PredictionModelFetchTimer::IsFirstModelFetch() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_ == PredictionModelFetchTimerState::kFirstFetch;
}

void PredictionModelFetchTimer::OnFetchTimerFired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(PredictionModelFetchTimerState::kNone, state_);
  fetch_callback_.Run();
}

void PredictionModelFetchTimer::MaybeScheduleFirstModelFetch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != PredictionModelFetchTimerState::kNone) {
    return;
  }

  // Add a slight delay to allow the rest of the browser startup process to
  // finish up.
  fetch_timer_.Start(FROM_HERE, features::PredictionModelFetchStartupDelay(),
                     this, &PredictionModelFetchTimer::OnFetchTimerFired);
  state_ = PredictionModelFetchTimerState::kFirstFetch;
}

void PredictionModelFetchTimer::ScheduleFetchOnModelRegistration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state_) {
    case PredictionModelFetchTimerState::kNone:
      // If no fetch is scheduled, maybe schedule one.
      MaybeScheduleFirstModelFetch();
      break;
    case PredictionModelFetchTimerState::kPeriodicFetch:
      state_ = PredictionModelFetchTimerState::kNewRegistrationFetch;
      fetch_timer_.Start(
          FROM_HERE, features::PredictionModelNewRegistrationFetchRandomDelay(),
          this, &PredictionModelFetchTimer::OnFetchTimerFired);
      break;
    case PredictionModelFetchTimerState::kFirstFetch:
    case PredictionModelFetchTimerState::kNewRegistrationFetch:
      break;
  }
}

void PredictionModelFetchTimer::SchedulePeriodicModelsFetch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!fetch_timer_.IsRunning());
  DCHECK_NE(PredictionModelFetchTimerState::kNone, state_);
  const base::TimeDelta time_until_update_time =
      GetLastFetchSuccessTime() + features::PredictionModelFetchInterval() -
      clock_->Now();
  const base::TimeDelta time_until_retry =
      GetLastFetchAttemptTime() + features::PredictionModelFetchRetryDelay() -
      clock_->Now();
  base::TimeDelta fetcher_delay =
      std::max(time_until_update_time, time_until_retry);
  state_ = PredictionModelFetchTimerState::kPeriodicFetch;
  if (fetcher_delay <= base::TimeDelta()) {
    fetch_timer_.Start(FROM_HERE, RandomFetchDelay(), this,
                       &PredictionModelFetchTimer::OnFetchTimerFired);
    return;
  }
  fetch_timer_.Start(FROM_HERE, fetcher_delay, this,
                     &PredictionModelFetchTimer::SchedulePeriodicModelsFetch);
}

void PredictionModelFetchTimer::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(fetch_timer_.IsRunning());
  fetch_timer_.Stop();
}

PredictionModelFetchTimer::PredictionModelFetchTimerState
PredictionModelFetchTimer::GetStateForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_;
}

const base::OneShotTimer* PredictionModelFetchTimer::GetFetchTimerForTesting()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return &fetch_timer_;
}

void PredictionModelFetchTimer::SetClockForTesting(const base::Clock* clock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  clock_ = clock;
}

void PredictionModelFetchTimer::ScheduleImmediateFetchForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fetch_timer_.Stop();
  fetch_timer_.Start(FROM_HERE, base::Milliseconds(1), this,
                     &PredictionModelFetchTimer::OnFetchTimerFired);
}

}  // namespace optimization_guide
