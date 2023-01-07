// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_TIMEOUT_H_
#define CHROME_TEST_CHROMEDRIVER_NET_TIMEOUT_H_

#include "base/time/time.h"

// A helper for tracking the time spent executing a task.  Creating a new
// instance marks the beginning of the task.  Task duration limit may be set
// in the contructor or at any time later, but only once.
class Timeout {
 public:
  Timeout();
  explicit Timeout(const base::TimeDelta& duration);

  // To be used when executing a sub-task with its own timeout, keeping track
  // of the parent task timeout at the same time. Creates a new Timeout whose
  // deadline is either Now() + |duration| or deadline of |outer|, whichever is
  // smaller. |outer| may be nullptr. Note: setting duration on |outer| won't
  // affect sub-timeouts that were created earlier!
  Timeout(const base::TimeDelta& duration, const Timeout* outer);

  // Sets the deadline by adding |duration| to the start time recored at
  // contruction.  Should not be called if the deadline is already set, unless
  // the duration is exactly the same.
  void SetDuration(const base::TimeDelta& duration);

  bool is_set() const { return !deadline_.is_null(); }

  // Whether the remaining time delta is less than or equal to zero.
  bool IsExpired() const;

  // Returns the duration if set, otherwise returns TimeDelta::Max().
  base::TimeDelta GetDuration() const;

  // Returns the remaining time if duration is set, otherwise returns
  // TimeDelta::Max().
  base::TimeDelta GetRemainingTime() const;

 private:
  base::TimeTicks start_;
  base::TimeTicks deadline_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_NET_TIMEOUT_H_
