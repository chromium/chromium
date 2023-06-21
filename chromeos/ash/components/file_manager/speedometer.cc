// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/speedometer.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/time/time.h"

namespace file_manager {

void Speedometer::SetTotalBytes(const int64_t total_bytes) {
  if (total_bytes != total_bytes_) {
    DCHECK_GE(total_bytes, 0);
    // The goalposts are moving. Throw away the current samples.
    samples_.Clear();
    total_bytes_ = total_bytes;
  }
}

size_t Speedometer::GetSampleCount() const {
  // While the buffer isn't full, we want the CurrentIndex().
  return std::min(samples_.CurrentIndex(), samples_.BufferSize());
}

bool Speedometer::Update(const int64_t bytes) {
  DCHECK_GE(bytes, 0);
  DCHECK_LE(bytes, total_bytes_);

  const base::TimeTicks now = base::TimeTicks::Now();
  if (const auto it = samples_.End()) {
    const Sample& last = **it;
    DCHECK_GE(now, last.time);
    DCHECK_GE(bytes, last.bytes);
    // Drop this sample if we received the previous one less than 3 second ago.
    if (const base::TimeDelta d = now - last.time; d < base::Seconds(3)) {
      VLOG(1) << "Dropped sample {bytes: " << bytes
              << "} as the previous one was received " << d << " ago";
      return false;
    }
  }

  samples_.SaveToBuffer({now, bytes});
  return true;
}

double Speedometer::GetRemainingSeconds() const {
  auto it = samples_.Begin();
  if (!it) {
    return std::numeric_limits<double>::quiet_NaN();
  }

  const Sample& first = **it;
  int n = 1;

  double average_bytes = 0;
  double average_time = 0;
  while (++it) {
    const Sample& sample = **it;
    DCHECK_GE(sample.bytes, first.bytes);
    average_bytes += double(sample.bytes - first.bytes);
    DCHECK_GT(sample.time, first.time);
    average_time += (sample.time - first.time).InSecondsF();
    n++;
  }

  DCHECK_EQ(size_t(n), GetSampleCount());
  average_bytes /= double(n);
  average_time /= double(n);

  DCHECK_LE(average_bytes, total_bytes_ - first.bytes);

  double variance_time = 0;
  double covariance_time_bytes = 0;
  for (auto it2 = samples_.Begin(); it2; ++it2) {
    const Sample& sample = **it2;
    const double time_diff =
        (sample.time - first.time).InSecondsF() - average_time;
    variance_time += time_diff * time_diff;
    covariance_time_bytes +=
        time_diff * (double(sample.bytes - first.bytes) - average_bytes);
  }

  // Speed is the slope of the linear interpolation in bytes per second.
  const double speed = covariance_time_bytes / variance_time;
  DLOG_IF(FATAL, speed < 0)
      << " speed = " << speed << ", variance_time = " << variance_time
      << ", covariance_time_bytes = " << covariance_time_bytes;

  // The linear interpolation goes through (average_time, average_bytes).
  const double end_time =
      (double(total_bytes_ - first.bytes) - average_bytes) / speed +
      average_time;
  return end_time - (base::TimeTicks::Now() - first.time).InSecondsF();
}

}  // namespace file_manager
