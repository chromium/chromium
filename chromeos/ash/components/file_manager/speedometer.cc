// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/speedometer.h"

#include <algorithm>

#include "base/time/time.h"

namespace file_manager {

size_t Speedometer::GetSampleCount() const {
  // While the buffer isn't full, we want the CurrentIndex().
  return std::min(samples_.CurrentIndex(), samples_.BufferSize());
}

double Speedometer::GetRemainingSeconds() const {
  // Not interpolated yet or not enough samples.
  if (end_time_ == 0) {
    return 0;
  }

  return end_time_ - (base::TimeTicks::Now() - start_time_).InSecondsF();
}

void Speedometer::Update(const int64_t total_processed_bytes) {
  // Drop this sample if we received the previous one less than 1 second ago.
  const double now = (base::TimeTicks::Now() - start_time_).InSecondsF();
  if (const auto it = samples_.End(); !it || now - it->time >= 1) {
    samples_.SaveToBuffer({now, total_processed_bytes});
    Interpolate();
  }
}

void Speedometer::Interpolate() {
  // Don't try to compute the linear interpolation unless we have enough
  // samples.
  if (GetSampleCount() < 2) {
    return;
  }

  double average_bytes = 0;
  double average_time = 0;
  for (auto it = samples_.Begin(); it; ++it) {
    const Sample& sample = **it;
    average_bytes += sample.bytes;
    average_time += sample.time;
  }

  average_bytes = average_bytes / GetSampleCount();
  average_time = average_time / GetSampleCount();

  double variance_time = 0;
  double covariance_time_bytes = 0;
  for (auto it = samples_.Begin(); it; ++it) {
    const Sample& sample = **it;
    const double time_diff = sample.time - average_time;
    variance_time += time_diff * time_diff;
    covariance_time_bytes += time_diff * (sample.bytes - average_bytes);
  }

  // The calculated speed is the slope of the linear interpolation in bytes per
  // second. The linear interpolation goes through the point (average_time,
  // average_bytes).
  const double speed = covariance_time_bytes / variance_time;
  end_time_ = (total_bytes_ - average_bytes) / speed + average_time;
}

}  // namespace file_manager
