// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_RATE_LIMITER_LEAKY_BUCKET_H_
#define COMPONENTS_REPORTING_UTIL_RATE_LIMITER_LEAKY_BUCKET_H_

#include <cstddef>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/reporting/util/rate_limiter_interface.h"

namespace reporting {

// Rate limiter implementation of the leaky bucket algorithm.

// A leaky bucket works as if filling up with water. The water represents the
// incoming events, and the bucket represents the system's capacity to accept
// them. The bucket has a hole in the bottom, which represents the rate at which
// the system can accept events. Water is not allowed to overflow the bucket.
//
// `max_level` is the maximum total size of events that can be in the bucket at
// any given time. `filling_time` represents the leakage rate at which events
// are allowed to go through the bucket. `filling_period` indicates how often do
// we want to update the level (the more frequently we do it, the fewer tokens
// we add each time).
//
// When a new event arrives, its size is released from the bucket, but only if
// the bucket is full, otherwise the event is rejected. After that the bucket
// resumes filling in at the prescribed rate - the time it takes is proportional
// to the event size.
//
// The outcome is that the events are accepted at no more than the leakage rate.
// It works well for averaging events rate over time.
//
class RateLimiterLeakyBucket : public RateLimiterInterface {
 public:
  RateLimiterLeakyBucket(size_t max_level,
                         base::TimeDelta filling_time,
                         base::TimeDelta filling_period = base::Seconds(1));

  RateLimiterLeakyBucket(const RateLimiterLeakyBucket&) = delete;
  RateLimiterLeakyBucket& operator=(const RateLimiterLeakyBucket&) = delete;

  ~RateLimiterLeakyBucket() override;

  // If the event is allowed, the method returns `true` and updates state to
  // prepare for the next call. Otherwise returns false.
  bool Acquire(size_t event_size) override;

 private:
  // Adds next portion of tokens.
  void NextFill();

  // Schedules adding next portion of tokens.
  void ScheduleNextFill();

  SEQUENCE_CHECKER(sequence_checker_);

  // Total size of the bucket.
  const size_t max_level_;

  // How long does it take to fill in the bucket up to maximum.
  const base::TimeDelta filling_time_;

  // How often to add the next portion to the bucket.
  const base::TimeDelta filling_period_;

  // Current level of the bucket. Starts with 0 and goes up at with such a rate
  // that fills it up to the `max_level_` in `filling_time_`.
  // New event is only accepted if the bucket is full.
  size_t current_level_ GUARDED_BY_CONTEXT(sequence_checker_) = 0u;

  // Weak ptr factory.
  base::WeakPtrFactory<RateLimiterLeakyBucket> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_RATE_LIMITER_LEAKY_BUCKET_H_
