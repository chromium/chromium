// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_RATE_LIMITER_SLIDE_WINDOW_H_
#define COMPONENTS_REPORTING_UTIL_RATE_LIMITER_SLIDE_WINDOW_H_

#include <cstddef>
#include <queue>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/reporting/util/rate_limiter_interface.h"

namespace reporting {

// Rate limiter implementation of the bucket slide window algorithm.
// It limits the total size of the events by `total_size` and tracks them
// using `bucket_count` buckets comprising `time_window` (thus each bucket
// tracks a period of `time_window / bucket_count`).
class RateLimiterSlideWindow : public RateLimiterInterface {
 public:
  RateLimiterSlideWindow(size_t total_size,
                         base::TimeDelta time_window,
                         size_t bucket_count);

  RateLimiterSlideWindow(const RateLimiterSlideWindow&) = delete;
  RateLimiterSlideWindow& operator=(const RateLimiterSlideWindow&) = delete;

  ~RateLimiterSlideWindow() override;

  // If the event is allowed, the method returns `true` and updates state to
  // prepare for the next call. Otherwise returns false.
  bool Acquire(size_t event_size) override;

 private:
  // Trims `bucket_events_size_` to make sure it fits
  // (now - `bucket_count_` * `time_bucket_`, now] sliding window.
  // Can be called whenever necessary (including `BucketsShift` and `Acquire`).
  void TrimBuckets(base::Time now);

  // Called every `time_bucket_` as long as `bucket_events_size_` is not empty.
  // Shifts buckets by one (drops the oldest bucket, adds a new empty one).
  void BucketsShift();

  SEQUENCE_CHECKER(sequence_checker_);

  // Total size of all events to be accepted in the window.
  const size_t total_size_;

  // Window bucket time period. Must be positive.
  const base::TimeDelta time_bucket_;

  // Window time span is determined as `bucket_count_` * `time_bucket_`.
  // Must be positive.
  const size_t bucket_count_;

  // The events size aggregated over `bucket_count_` * `time_bucket_`.
  size_t current_size_ GUARDED_BY_CONTEXT(sequence_checker_) = 0u;

  // Events sizes per bucket; undefined if empty, otherwise `front()` matches
  // `[start_timestamp_, start_timestamp_ + time_bucket_)` interval.
  std::queue<size_t> bucket_events_size_ GUARDED_BY_CONTEXT(sequence_checker_);

  // First bucket timestamp.
  base::Time start_timestamp_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Weak ptr factory.
  base::WeakPtrFactory<RateLimiterSlideWindow> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_RATE_LIMITER_SLIDE_WINDOW_H_
