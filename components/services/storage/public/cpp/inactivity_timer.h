// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_INACTIVITY_TIMER_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_INACTIVITY_TIMER_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace storage {

// A timer that fires a user action after a specified period of "inactivity"
// (not stopping or restarting the timer). Similar to a `RetainingOneShotTimer`,
// but is more robust to the fact that timers may or may not pause when the
// system is suspended (crbug.com/40296804).
//
// Sample usage:
//
//   class MyClass {
//    public:
//     void StartMonitoring() {
//       timer_.Start(FROM_HERE, base::Seconds(15),
//                    base::BindRepeating(&MyClass::OnTimeout,
//                                        base::Unretained(this)));
//     }
//     void OnActivity() {
//       timer_.Reset();
//     }
//    private:
//     void OnTimeout() {
//       // Called after ~15 seconds of inactivity.
//     }
//     InactivityTimer timer_;
//   };
class COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC) InactivityTimer {
 public:
  InactivityTimer();

  // Construct with action info. `Start()` or `Reset()` must be called to begin
  // the timer.
  InactivityTimer(const base::Location& posted_from,
                  base::TimeDelta delay,
                  base::RepeatingClosure action);

  ~InactivityTimer();

  InactivityTimer(const InactivityTimer&) = delete;
  InactivityTimer& operator=(const InactivityTimer&) = delete;

  // Starts the timer to run `action` after approximately `delay`. If the timer
  // is already running, it will be replaced with the new parameters.
  void Start(const base::Location& posted_from,
             base::TimeDelta delay,
             base::RepeatingClosure action);

  // Stops the timer. No-op if not running.
  void Stop();

  // (Re)starts the timer. The user action must be set.
  void Reset();

  bool IsRunning() const;

  // The time at which the timer is expected to fire. The timer must be running.
  base::TimeTicks ExpectedFiringTimeForTesting() const;

 private:
  void OnTimerFired();

  base::RepeatingClosure action_;

  // Count "strikes", each at a fractional duration of the total delay.
  int strikes_ = 0;
  base::RepeatingTimer timer_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_INACTIVITY_TIMER_H_
