// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_TIME_HISTORICAL_LATENCIES_CONTAINER_H_
#define COMPONENTS_NETWORK_TIME_HISTORICAL_LATENCIES_CONTAINER_H_

#include "base/containers/ring_buffer.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network_time {

// Number of historical latencies to record.
constexpr int kMaxNumHistoricalLatencies = 16;

// A class to record latencies of the previous `kNumHistoricalLatencies` time
// fetches.
class HistoricalLatenciesContainer {
 public:
  // Records a new latency in the container.
  void Record(base::TimeDelta latency);

  // Computes the standard deviation of the latest latencies. Returns nullopt
  // if not enough latencies have been recorded yet.
  absl::optional<base::TimeDelta> StdDeviation() const;

 private:
  base::RingBuffer<base::TimeDelta, size_t{kMaxNumHistoricalLatencies}>
      latencies_;
};

}  // namespace network_time

#endif  // COMPONENTS_NETWORK_TIME_HISTORICAL_LATENCIES_CONTAINER_H_
