// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERDRAW_TRACKER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERDRAW_TRACKER_H_

#include <cstdint>
#include <vector>

#include "base/time/time.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {
class AggregatedFrame;

// Estimates overdraw of aggregated frames. Used in tests to
// get the overdraw data over a period of time.
class VIZ_SERVICE_EXPORT OverdrawTracker {
 public:
  using OverdrawTimeSeries = std::vector<float>;

  struct Settings {
    // Interval length determines the duration over which the overdraw
    // averages are aggregated. For example, an interval length of 5 seconds
    // will aggregate the overdraw of all frames observed within that 5-second
    // time frame and store an average overdraw of all these frames. A smaller
    // interval will increase granularity of overdraw data at the cost larger
    // time-series.
    uint16_t interval_length_in_seconds = 1;
  };

  static void EstimateAndRecordOverdrawAsUMAMetric(
      const AggregatedFrame* frame);

  // Estimates the overdraw of the `frame`.
  static float EstimateOverdraw(const AggregatedFrame* frame);

  explicit OverdrawTracker(const Settings& settings);

  OverdrawTracker(const OverdrawTracker&) = delete;
  OverdrawTracker& operator=(const OverdrawTracker&) = delete;

  ~OverdrawTracker();

  // Estimates and records the overdraw of the `frame` seen at `timestamp`. The
  // `timestamp` must be in future w.r.t `start_time`.
  void EstimateAndRecordOverdraw(const AggregatedFrame* frame,
                                 base::TimeTicks timestamp);

  // Returns the average overdraw of each interval as a time-series that begins
  // at `start_time_` and finishes at `latest_time_`. If there are no frames
  // recorded in an in-between interval, the average overdraw for that interval
  // will be zero.
  OverdrawTimeSeries TakeDataAsTimeSeries() const;

  void Reset();

  base::TimeTicks start_time_for_testing() const { return start_time_; }

 private:
  void Record(float overdraw, base::TimeTicks timestamp);

  size_t GetIntervalIndex(base::TimeTicks timestamp) const;

  const Settings settings_;

  base::TimeTicks start_time_;  // When the tracker was created.

  struct DecomposedAverage {
    float GetAverage() const {
      return count > 0 ? static_cast<float>(aggregate) / count : 0;
    }

    void AddValue(float value) {
      aggregate += value;
      ++count;
    }

    int count = 0;
    float aggregate = 0;
  };

  // Tracks average overdraw of frames recorded for each interval.
  // index=i represents time interval:
  //   [start_time_ + i * interval_size, start_time_ + (i+1) * interval_size)
  // Note: averages are stored as `DecomposedAverage` to efficiently update
  // average of each interval.
  std::vector<DecomposedAverage> interval_overdraw_averages_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERDRAW_TRACKER_H_
