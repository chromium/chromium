// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/historical_latencies_container.h"

#include <cmath>

#include "base/metrics/field_trial_params.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "components/network_time/network_time_tracker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network_time {

// Number of previous latencies to use for computing the standard deviation.
// Should be greater or equal 0 and less or equal kMaxNumHistoricalLatencies. If
// 0, the standard deviation will not be reported.
constexpr base::FeatureParam<int> kNumHistoricalLatencies{
    &kNetworkTimeServiceQuerying, "NumHistoricalLatencies", 3};

void HistoricalLatenciesContainer::Record(base::TimeDelta latency) {
  latencies_.SaveToBuffer(latency);
}

absl::optional<base::TimeDelta> HistoricalLatenciesContainer::StdDeviation()
    const {
  int num_historical_latencies = kNumHistoricalLatencies.Get();
  if (num_historical_latencies <= 0 ||
      num_historical_latencies > kMaxNumHistoricalLatencies) {
    return absl::nullopt;
  }

  base::CheckedNumeric<int64_t> mean;
  {
    auto it = latencies_.End();
    for (int i = 0; i < num_historical_latencies; ++i, --it) {
      if (!it)  // Less than `num_historical_latencies` recorded so far.
        return absl::nullopt;
      mean += it->InMicroseconds();
    }
    mean = mean / num_historical_latencies;
  }

  base::CheckedNumeric<int64_t> variance;
  {
    auto it = latencies_.End();
    for (int i = 0; i < num_historical_latencies; ++i, --it) {
      base::CheckedNumeric<int64_t> diff_from_mean =
          mean - it->InMicroseconds();
      variance += diff_from_mean * diff_from_mean;
    }
  }
  if (!variance.IsValid())
    return absl::nullopt;
  base::TimeDelta std_deviation = base::Microseconds(
      std::lround(std::sqrt(base::strict_cast<double>(variance.ValueOrDie()))));

  if (std_deviation.is_inf())
    return absl::nullopt;
  return std_deviation;
}

}  // namespace network_time
