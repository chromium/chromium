// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/progress_sampler.h"

#include <algorithm>
#include <optional>
#include <queue>

#include "base/check.h"
#include "base/check_op.h"
#include "base/time/time.h"

namespace updater {

ProgressSampler::ProgressSampler(base::TimeDelta sample_time_range,
                                 base::TimeDelta minimum_range_required)
    : sample_time_range_(sample_time_range),
      minimum_range_required_(minimum_range_required) {
  CHECK(minimum_range_required.is_positive());
}

ProgressSampler::~ProgressSampler() = default;

void ProgressSampler::AddSample(int64_t sample_value) {
  AddSample(base::Time::Now(), sample_value);
}

std::optional<base::TimeDelta> ProgressSampler::GetRemainingTime(
    int64_t total) {
  if (!total || !HasEnoughSamples()) {
    return {};
  }

  const int64_t current = samples_.back().value;
  if (total < current) {
    return {};
  }
  if (total == current) {
    return base::Milliseconds(0);
  }

  const std::optional<double> per_ms = GetAverageSpeedPerMs();
  if (!per_ms || *per_ms <= 0) {
    return {};
  }

  return base::Milliseconds((total - current + *per_ms - 1) / *per_ms);
}

void ProgressSampler::AddSample(const base::Time& timestamp,
                                int64_t sample_value) {
  // `Reset` if there is a value or clock regression.
  if (!samples_.empty() && (sample_value < samples_.back().value ||
                            timestamp < samples_.back().timestamp)) {
    Reset();
    return;
  }

  samples_.push(Sample(timestamp, sample_value));

  // Discard old data that is out of `sample_time_range`.
  while (samples_.back().timestamp - samples_.front().timestamp >
             sample_time_range_ &&
         samples_.size() > 2) {
    samples_.pop();
  }
}

bool ProgressSampler::HasEnoughSamples() const {
  if (samples_.size() < 2) {
    return false;
  }
  return samples_.back().timestamp - samples_.front().timestamp >
         minimum_range_required_;
}

std::optional<double> ProgressSampler::GetAverageSpeedPerMs() const {
  if (!HasEnoughSamples()) {
    return {};
  }
  const base::TimeDelta time_diff =
      samples_.back().timestamp - samples_.front().timestamp;
  return static_cast<double>(samples_.back().value - samples_.front().value) /
         time_diff.InMilliseconds();
}

void ProgressSampler::Reset() {
  std::queue<Sample> empty_queue;
  std::swap(samples_, empty_queue);
}

}  // namespace updater
