// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_RATE_LIMITER_TOKEN_BUCKET_H_
#define COMPONENTS_REPORTING_UTIL_RATE_LIMITER_TOKEN_BUCKET_H_

#include <cstddef>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/reporting/util/rate_limiter_interface.h"

namespace reporting {

// Rate limiter implementation of the token bucket algorithm.

// A token bucket works by an imaginary bucket that is filled with tokens. The
// tokens represent the incoming events, and the bucket represents the system's
// capacity to accept them. The bucket has a fixed number of tokens, and new
// tokens are added to the bucket at a fixed rate, until the bucket is full.
//
// `max_level` is the maximum total size of events that can be in the bucket at
// any given time. `filling_time` represents the rate at which events are
// allowed to go through the bucket. `filling_period` indicates how often do
// we want to update the level (the more frequently we do it, the fewer tokens
// we add each time).
//
// When a new event arrives, its size is released from the bucket. If the bucket
// does not have enough tokens in it, the event is rejected. After that the
// bucket resumes filling in at the prescribed rate.
//
// The outcome is that the events are accepted at no more than the filling rate.
// It is simple and effective, but can be inefficient for bursty events.
//
class RateLimiterTokenBucket : public RateLimiterInterface {
 public:
  RateLimiterTokenBucket(size_t max_level,
                         base::TimeDelta filling_time,
                         base::TimeDelta filling_period = base::Seconds(1));

  RateLimiterTokenBucket(const RateLimiterTokenBucket&) = delete;
  RateLimiterTokenBucket& operator=(const RateLimiterTokenBucket&) = delete;

  ~RateLimiterTokenBucket() override;

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
  // New event is only accepted if the bucket has enough tokens to account for
  // the event's size.
  size_t current_level_ GUARDED_BY_CONTEXT(sequence_checker_) = 0u;

  // Weak ptr factory.
  base::WeakPtrFactory<RateLimiterTokenBucket> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_RATE_LIMITER_TOKEN_BUCKET_H_
