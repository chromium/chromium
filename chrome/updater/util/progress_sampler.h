// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_PROGRESS_SAMPLER_H_
#define CHROME_UPDATER_UTIL_PROGRESS_SAMPLER_H_

#include <optional>
#include <queue>

#include "base/time/time.h"

namespace updater {

// This class calculates the remaining time for an operation such as a download
// or an install.
class ProgressSampler {
 public:
  // Creates a sampler that keep samples within the last `sample_time_range` and
  // calculates the remaining time if the minimum time range of
  // `minimum_range_required` is reached.
  ProgressSampler(base::TimeDelta sample_time_range,
                  base::TimeDelta minimum_range_required);
  ~ProgressSampler();

  // Adds a sample for the current time.
  void AddSample(int64_t sample_value);

  // Gets how much time is remaining based on the samples provided previously
  // and `total`.
  std::optional<base::TimeDelta> GetRemainingTime(int64_t total);

 private:
  // Helpers.
  void AddSample(const base::Time& timestamp, int64_t sample_value);
  bool HasEnoughSamples() const;
  std::optional<double> GetAverageSpeedPerMs() const;
  void Reset();

  const base::TimeDelta sample_time_range_;
  const base::TimeDelta minimum_range_required_;

  struct Sample {
    Sample(const base::Time& local_timestamp, int64_t local_value)
        : timestamp(local_timestamp), value(local_value) {}

    const base::Time timestamp;
    const int64_t value;
  };

  std::queue<Sample> samples_;

  FRIEND_TEST_ALL_PREFIXES(ProgressSampler, Samples);
  FRIEND_TEST_ALL_PREFIXES(ProgressSampler, PercentageRange);
};

}  // namespace updater

#endif  // CHROME_UPDATER_UTIL_PROGRESS_SAMPLER_H_
