// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/rate_estimator.h"

#include "base/check.h"
#include "base/time/time.h"

using base::TimeTicks;

namespace download {

namespace {

static const int kDefaultBucketTimeSeconds = 1;
static const size_t kDefaultNumBuckets = 10;

}  // namespace

RateEstimator::RateEstimator()
    : history_(kDefaultNumBuckets),
      bucket_time_(base::Seconds(kDefaultBucketTimeSeconds)),
      oldest_index_(0),
      bucket_count_(1) {
  ResetBuckets(TimeTicks::Now());
}

RateEstimator::RateEstimator(base::TimeDelta bucket_time,
                             size_t num_buckets,
                             TimeTicks now)
    : history_(num_buckets),
      bucket_time_(bucket_time),
      oldest_index_(0),
      bucket_count_(1) {
  DCHECK(bucket_time_.InSeconds() > 0);
  ResetBuckets(now);
}

RateEstimator::~RateEstimator() = default;

void RateEstimator::Increment(uint32_t count) {
  Increment(count, TimeTicks::Now());
}

void RateEstimator::Increment(uint32_t count, TimeTicks now) {
  ClearOldBuckets(now);
  int64_t seconds_since_oldest = (now - oldest_time_).InSeconds();
  DCHECK(seconds_since_oldest >= 0);
  int64_t delta_buckets = seconds_since_oldest / bucket_time_.InSeconds();
  DCHECK(delta_buckets >= 0);
  size_t index_offset = static_cast<size_t>(delta_buckets);
  DCHECK(index_offset <= history_.size());
  size_t current_index = (oldest_index_ + delta_buckets) % history_.size();
  history_[current_index] += count;
}

uint64_t RateEstimator::GetCountPerSecond() const {
  return GetCountPerSecond(TimeTicks::Now());
}

uint64_t RateEstimator::GetCountPerSecond(TimeTicks now) const {
  const_cast<RateEstimator*>(this)->ClearOldBuckets(now);
  // TODO(cbentzel): Support fractional seconds for active bucket?
  // We explicitly don't check for overflow here. If it happens, unsigned
  // arithmetic at least guarantees behavior by wrapping around. The estimate
  // will be off, but the code will still be valid.
  uint64_t total_count = 0;
  for (size_t i = 0; i < bucket_count_; ++i) {
    size_t index = (oldest_index_ + i) % history_.size();
    total_count += history_[index];
  }
  return total_count / (bucket_count_ * bucket_time_.InSeconds());
}

void RateEstimator::ClearOldBuckets(TimeTicks now) {
  int64_t seconds_since_oldest = (now - oldest_time_).InSeconds();

  int64_t delta_buckets = seconds_since_oldest / bucket_time_.InSeconds();

  // It's possible (although unlikely) for there to be rollover with TimeTicks.
  // If that's the case, just reset the history.
  if (delta_buckets < 0) {
    ResetBuckets(now);
    return;
  }
  size_t delta_index = static_cast<size_t>(delta_buckets);

  // If we are within the current window, keep the existing data.
  if (delta_index < history_.size()) {
    bucket_count_ = delta_index + 1;
    return;
  }

  // If it's been long enough that all history data is too stale, just
  // clear all the buckets.
  size_t extra_buckets = delta_index - history_.size() + 1;
  if (extra_buckets > history_.size()) {
    ResetBuckets(now);
    return;
  }

  // Clear out stale buckets in the history.
  bucket_count_ = history_.size();
  for (size_t i = 0; i < extra_buckets; ++i) {
    history_[oldest_index_] = 0;
    oldest_index_ = (oldest_index_ + 1) % history_.size();
    oldest_time_ = oldest_time_ + bucket_time_;
  }
}

void RateEstimator::ResetBuckets(TimeTicks now) {
  for (size_t i = 0; i < history_.size(); ++i) {
    history_[i] = 0;
  }
  oldest_index_ = 0;
  bucket_count_ = 1;
  oldest_time_ = now;
}

}  // namespace download
