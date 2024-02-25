// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_FETCH_TIMER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_FETCH_TIMER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"

class PrefService;

namespace optimization_guide {

// Handles the fetch timer and relevant state for scheduling the initial and
// periodic fetching fetching of prediction models.
class PredictionModelFetchTimer {
 public:
  // Encapsulates the different states of the fetch timer.
  enum PredictionModelFetchTimerState {
    // No model fetch happened.
    kNone = 0,
    // First model fetch is scheduled for.
    kFirstFetch = 1,
    // New registration fetch is scheduled for. This happens when a new
    // optimization target registration happens during the periodic fetch phase.
    kNewRegistrationFetch = 2,
    // Periodic fetch is scheduled for.
    kPeriodicFetch = 3,
  };

  PredictionModelFetchTimer(PrefService* pref_service,
                            base::RepeatingCallback<void(void)> fetch_callback);

  PredictionModelFetchTimer(const PredictionModelFetchTimer&) = delete;
  PredictionModelFetchTimer& operator=(const PredictionModelFetchTimer&) =
      delete;

  ~PredictionModelFetchTimer();

  // Notifies that the prediction model fetch was attempted now.
  void NotifyModelFetchAttempt();

  // Notifies that the fetch for prediction models succeeded now.
  void NotifyModelFetchSuccess();

  void MaybeScheduleFirstModelFetch();
  void ScheduleFetchOnModelRegistration();
  void Stop();
  void SchedulePeriodicModelsFetch();

  // Returns if the first model fetch is happening.
  bool IsFirstModelFetch() const;

  PredictionModelFetchTimerState GetStateForTesting() const;

  const base::OneShotTimer* GetFetchTimerForTesting() const;

  void SetClockForTesting(const base::Clock* clock);

  // Schedules an immediate model fetch.
  void ScheduleImmediateFetchForTesting();

 private:
  friend class PredictionModelFetchTimerTest;

  // Called when the `fetch_timer` is fired.
  void OnFetchTimerFired();

  // Return the time when a prediction model fetch was last attempted.
  base::Time GetLastFetchAttemptTime() const;

  // Return the time when a prediction model fetch was last successfully
  // completed.
  base::Time GetLastFetchSuccessTime() const;

  // The timer used to schedule fetching prediction models and host model
  // features from the remote Optimization Guide Service.
  base::OneShotTimer fetch_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The clock used to schedule fetching from the remote Optimization Guide
  // Service.
  raw_ptr<const base::Clock> clock_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Holds the current state of the fetch.
  PredictionModelFetchTimerState state_ GUARDED_BY_CONTEXT(sequence_checker_) =
      PredictionModelFetchTimerState::kNone;

  // A reference to the PrefService for this profile. Not owned.
  raw_ptr<PrefService> pref_service_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;

  // The callback to call to trigger the model fetches.
  base::RepeatingCallback<void(void)> fetch_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_FETCH_TIMER_H_
