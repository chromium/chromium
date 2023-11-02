// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_SCOPED_TIMED_TASK_LOGGER_H_
#define CHROME_CHROME_CLEANER_LOGGING_SCOPED_TIMED_TASK_LOGGER_H_

#include "base/callback.h"
#include "base/time/time.h"

namespace chrome_cleaner {

// A class to compute the time it takes to execute a task within its lifespan
// and then call the given callback passing it the measured time.
class ScopedTimedTaskLogger {
 public:
  typedef base::OnceCallback<void(const base::TimeDelta&)> TimerCallback;

  // If |elapsed_time| exceeds |threshold|, then log
  // "|logging_text| took '|elapsed_time|' seconds."
  static void LogIfExceedThreshold(const char* logging_text,
                                   const base::TimeDelta& threshold,
                                   const base::TimeDelta& elapsed_time);

  // Start tracking the elapsed time between construction and destruction of
  // this object. |timer_callback| is called in the destructor with the measured
  // time.
  explicit ScopedTimedTaskLogger(TimerCallback timer_callback);
  // Convenience constructor that uses |LogIfExceedThreshold| with
  // |logging_text| and a threshold of one second as the callback. The ownership
  // of |logging_text| is retained by the caller, who must ensure that
  // |logging_text| outlives the |ScopedTimedTaskLogger| object.
  explicit ScopedTimedTaskLogger(const char* logging_text);
  ~ScopedTimedTaskLogger();

 private:
  base::Time start_time_;
  TimerCallback timer_callback_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_SCOPED_TIMED_TASK_LOGGER_H_
