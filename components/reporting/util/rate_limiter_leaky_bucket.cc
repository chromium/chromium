// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/rate_limiter_leaky_bucket.h"

#include <cmath>
#include <cstddef>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace reporting {

RateLimiterLeakyBucket::RateLimiterLeakyBucket(size_t max_level,
                                               base::TimeDelta filling_time,
                                               base::TimeDelta filling_period)
    : max_level_(max_level),
      filling_time_(filling_time),
      filling_period_(filling_period) {
  CHECK_GT(max_level_, 0u);
  CHECK_GT(filling_time_, base::TimeDelta());
  // Make sure filling rate is reasonable - at least 1 token per period!
  CHECK_LT(filling_time_ / max_level_, filling_period_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // Empty initially, start filling in.
  ScheduleNextFill();
}

RateLimiterLeakyBucket::~RateLimiterLeakyBucket() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool RateLimiterLeakyBucket::Acquire(size_t event_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (event_size > max_level_) {
    return false;  // The event is too large, will never be accepted.
  }

  if (current_level_ < max_level_) {
    return false;  // The bucket has not filled in yet.
  }

  // Accept and account for the new event.
  current_level_ -= event_size;
  // Resume filling in.
  ScheduleNextFill();
  return true;
}

void RateLimiterLeakyBucket::NextFill() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_level_ += std::ceil(max_level_ * filling_period_ / filling_time_);
  if (current_level_ > max_level_) {
    current_level_ = max_level_;
  } else if (current_level_ < max_level_) {
    ScheduleNextFill();
  }
}

void RateLimiterLeakyBucket::ScheduleNextFill() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RateLimiterLeakyBucket::NextFill,
                     weak_ptr_factory_.GetWeakPtr()),
      filling_period_);
}
}  // namespace reporting
