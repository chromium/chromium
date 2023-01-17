// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_METRICS_METRICS_RECORDER_BASE_H_
#define CHROMECAST_METRICS_METRICS_RECORDER_BASE_H_

#include <string>

#include "base/time/tick_clock.h"
#include "chromecast/metrics/metrics_recorder.h"
#include "chromecast/metrics/timed_event_recorder.h"

namespace chromecast {

class MetricsRecorderBase : public MetricsRecorder {
 public:
  ~MetricsRecorderBase() override;

  // MetricsRecorder implementation (partial):
  void MeasureTimeUntilEvent(const std::string& event_name,
                             const std::string& measurement_name) override;
  void MeasureTimeUntilEvent(const std::string& event_name,
                             const std::string& measurement_name,
                             base::TimeTicks start_time) override;
  void RecordTimelineEvent(const std::string& event_name) override;

 protected:
  explicit MetricsRecorderBase(const base::TickClock* tick_clock = nullptr);

 private:
  const base::TickClock* const tick_clock_;
  TimedEventRecorder timed_event_recorder_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_METRICS_METRICS_RECORDER_BASE_H_
