// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_STOPWATCH_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_STOPWATCH_H_

#include <ostream>

#include "base/time/time.h"

namespace autofill_assistant {

// A simple stopwatch to measure cumulative times.
class Stopwatch {
 public:
  Stopwatch();
  // Start measuring the time when the method is called.
  // Returns true if the stopwatch was not already running.
  bool Start();
  // Starts measuring the time from `start_time`, if the stopwatch was already
  // running, it simply updates the recorded start time of the current run.
  void StartAt(base::TimeTicks start_time);
  // Stops the stopwatch and, if it was running, adds to the cumulative elapsed
  // time. If the stopwatch was not running, it has no effect and returns false.
  bool Stop();
  // Stops the stopwatch at `stop_time` and, if it was running, adds to the
  // cumulative elapsed time. If the stopwatch was not running, it has no effect
  // and returns false.
  bool StopAt(base::TimeTicks stop_time);
  // Adds `time` to the cumulative elapsed time held by this stopwatch.
  void AddTime(base::TimeDelta time);
  // Add `other`'s `TotalElapsed` to the cumulative elapsed time held by this
  // stopwatch.
  void AddTime(const Stopwatch& other);
  // Remove `time` from the cumulative elapsed time held by this stopwatch.
  void RemoveTime(base::TimeDelta time);
  // Remove up to `other`'s `TotalElapsed` from the cumulative elapsed time held
  // by this stopwatch.
  void RemoveTime(const Stopwatch& other);
  // Resets the stopwatch and doesn't start it again.
  void Reset();
  // Returns the total time accumulated by this stopwatch.
  base::TimeDelta TotalElapsed() const;

  // Whether the stopwatch is running or not.
  bool IsRunning() const;

  friend std::ostream& operator<<(std::ostream& out, const Stopwatch& action);

 private:
  base::TimeDelta LastElapsedAt(base::TimeTicks time) const;

  bool running_ = false;
  base::TimeTicks start_time_;
  base::TimeDelta elapsed_time_;
};

// This class holds two stopwatches: one for active and one for wait time (e.g.
// wait on preconditions or user action) spent while executing the action.
class ActionStopwatch {
 public:
  // Removes `time` from the total wait time and adds it to the active time.
  void TransferToActiveTime(base::TimeDelta time);
  // Removes `time` from the total active time and adds it to the wait time.
  void TransferToWaitTime(base::TimeDelta time);
  // Starts the active time stopwatch, stopping the wait time one if it was
  // running.
  void StartActiveTime();
  // Starts the active time and stops the wait time stopwatch at `start_time`,
  // which can be a time in the past or the future.
  void StartActiveTimeAt(base::TimeTicks start_time);
  // Starts the wait time stopwatch, stopping the active time one if it was
  // running.
  void StartWaitTime();
  // Starts the wait time and stops the active time stopwatch at `start_time`,
  // which can be a time in the past or the future.
  void StartWaitTimeAt(base::TimeTicks start_time);
  // Stops both stopwatches.
  void Stop();

  base::TimeDelta TotalActiveTime();
  base::TimeDelta TotalWaitTime();
  friend std::ostream& operator<<(std::ostream& out,
                                  const ActionStopwatch& action);

 private:
  Stopwatch active_time_stopwatch_;
  Stopwatch wait_time_stopwatch_;
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_STOPWATCH_H_
