// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_TIME_TIME_TRACKER_TIME_TRACKER_H_
#define COMPONENTS_NETWORK_TIME_TIME_TRACKER_TIME_TRACKER_H_

#include "base/time/time.h"

namespace network_time {

// A class that's created with a known good time, and provides an estimate of
// the current time by adding the system clock seconds that have elapsed since
// it was created.
class TimeTracker {
 public:
  struct TimeTrackerState {
    base::Time system_time;
    base::TimeTicks system_ticks;
    base::Time known_time;
    base::TimeDelta uncertainty;
  };
  TimeTracker(const base::Time& system_time,
              const base::TimeTicks& system_ticks,
              const base::Time& time,
              const base::TimeDelta& uncertainty);
  ~TimeTracker() = default;

  // Returns true if the time is available, false otherwise (e.g. if sync was
  // lost). Sets |estimated_ time| to an estimate of the true time. If
  // |uncertainty| is non-NULL, it will be set to an estimate of the error
  // range. |system_time| and |system_ticks| should come from the same clocks
  // used to retrieve the system time on creation.
  bool GetTime(const base::Time& system_time,
               const base::TimeTicks& system_ticks,
               base::Time* time,
               base::TimeDelta* uncertainty) const;

  TimeTrackerState GetStateAtCreation() const { return state_; }

 private:
  TimeTrackerState state_;
};

}  // namespace network_time

#endif  // COMPONENTS_NETWORK_TIME_TIME_TRACKER_TIME_TRACKER_H_
