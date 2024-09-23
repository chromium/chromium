// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/rate_limiter_slide_window.h"

#include <cstddef>
#include <utility>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace reporting {

RateLimiterSlideWindow::RateLimiterSlideWindow(size_t total_size,
                                               base::TimeDelta time_window,
                                               size_t bucket_count)
    : total_size_(total_size),
      time_bucket_(time_window / bucket_count),
      bucket_count_(bucket_count),
      start_timestamp_(base::Time::Min()) {
  CHECK_GT(total_size, 0u);
  CHECK_GT(time_window, base::TimeDelta());
  CHECK_GT(bucket_count, 0u);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RateLimiterSlideWindow::~RateLimiterSlideWindow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RateLimiterSlideWindow::TrimBuckets(base::Time now) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_LE(bucket_events_size_.size(), bucket_count_);
  const auto earliest_time = now - bucket_count_ * time_bucket_;
  // Drop all obsolete buckets.
  while (!bucket_events_size_.empty() && start_timestamp_ <= earliest_time) {
    CHECK_GE(current_size_, bucket_events_size_.front());
    current_size_ -= bucket_events_size_.front();
    bucket_events_size_.pop();
    start_timestamp_ += time_bucket_;
  }
}

void RateLimiterSlideWindow::BucketsShift() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!bucket_events_size_.empty());
  // Drop all obsolete buckets.
  TrimBuckets(/*now=*/base::Time::Now());
  // If more buckets are there, repeat once the earliest bucket becomes
  // obsolete.
  if (!bucket_events_size_.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RateLimiterSlideWindow::BucketsShift,
                       weak_ptr_factory_.GetWeakPtr()),
        time_bucket_);
  }
}

bool RateLimiterSlideWindow::Acquire(size_t event_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto now = base::Time::Now();
  // Drop all obsolete buckets.
  TrimBuckets(now);
  // Check whether the new event size fits.
  if (current_size_ + event_size > total_size_) {
    return false;  // Too large, cannot add.
  }
  // Event size fits in the window.
  if (bucket_events_size_.empty()) {
    // Window is empty, populate it with the new event only.
    bucket_events_size_.push(event_size);
    current_size_ = event_size;
    start_timestamp_ = now;
    // Initiate bucket purging, there are non-empty buckets now.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RateLimiterSlideWindow::BucketsShift,
                       weak_ptr_factory_.GetWeakPtr()),
        time_bucket_);
  } else {
    // Window is not empty. `TrimBuckets` guarantees its start is recent enough.
    CHECK_GT(start_timestamp_ + bucket_count_ * time_bucket_, now);
    // Pad window with 0 buckets if necessary.
    for (auto next_timestamp =
             start_timestamp_ + bucket_events_size_.size() * time_bucket_;
         next_timestamp <= now; next_timestamp += time_bucket_) {
      CHECK_LT(bucket_events_size_.size(), bucket_count_);
      bucket_events_size_.push(0u);
    }
    // Add the new event to the last bucket (which could be empty).
    CHECK_LE(bucket_events_size_.size(), bucket_count_);
    bucket_events_size_.back() += event_size;
    current_size_ += event_size;
    // No need to initiate bucket purging - it is already scheduled.
  }
  return true;
}
}  // namespace reporting
