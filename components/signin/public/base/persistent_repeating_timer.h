// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_PERSISTENT_REPEATING_TIMER_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_PERSISTENT_REPEATING_TIMER_H_

#include <string>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/timer/wall_clock_timer.h"

class PrefService;

namespace signin {

// This class fires a task repeatedly, across application restarts. The timer
// stores the date of the last invocation in a preference, which is persisted
// to disk.
class PersistentRepeatingTimer {
 public:
  // The timer is not started at creation.
  // `prefs` must not be null.
  // `clock` and `tick_clock` may be null, in which case the default clocks will
  // be used. (They're injectable for testing purposes.)
  PersistentRepeatingTimer(PrefService* prefs,
                           const char* timer_last_update_pref_name,
                           base::TimeDelta delay,
                           base::RepeatingClosure task,
                           const base::Clock* clock = nullptr,
                           const base::TickClock* tick_clock = nullptr);

  ~PersistentRepeatingTimer();

  // Starts the timer. Calling Start() when the timer is running has no effect.
  void Start();

 private:
  // Reads the date of the last event from the pref.
  base::Time GetLastFired();

  // Updates the pref with the current time.
  void SetLastFiredNow();

  // Called when |timer_| fires.
  void OnTimerFired();

  const raw_ref<PrefService> prefs_;
  const std::string last_fired_pref_name_;
  const base::TimeDelta delay_;
  base::RepeatingClosure user_task_;

  const raw_ref<const base::Clock> clock_;

  base::WallClockTimer wall_timer_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_PERSISTENT_REPEATING_TIMER_H_
