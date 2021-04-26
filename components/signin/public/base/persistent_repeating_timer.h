// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_PERSISTENT_REPEATING_TIMER_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_PERSISTENT_REPEATING_TIMER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

class PrefService;

namespace signin {

// This class fires a task repeatedly, across application restarts. The timer
// stores the date of the last invocation in a preference, which is persisted
// to disk.
class PersistentRepeatingTimer {
 public:
  // The timer is not started at creation.
  PersistentRepeatingTimer(PrefService* prefs,
                           const char* timer_last_update_pref_name,
                           base::TimeDelta delay,
                           base::RepeatingClosure task);

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

  PrefService* prefs_;
  std::string last_fired_pref_name_;
  base::TimeDelta delay_;
  base::RepeatingClosure user_task_;

  base::RetainingOneShotTimer timer_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_PERSISTENT_REPEATING_TIMER_H_
