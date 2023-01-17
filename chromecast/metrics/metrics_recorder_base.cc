// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/metrics_recorder_base.h"

namespace chromecast {

MetricsRecorderBase::MetricsRecorderBase(const base::TickClock* tick_clock)
    : tick_clock_(tick_clock) {}

MetricsRecorderBase::~MetricsRecorderBase() = default;

void MetricsRecorderBase::MeasureTimeUntilEvent(
    const std::string& end_event,
    const std::string& measurement_name) {
  base::TimeTicks now =
      tick_clock_ ? tick_clock_->NowTicks() : base::TimeTicks::Now();
  timed_event_recorder_.MeasureTimeUntilEvent(end_event, measurement_name, now);
}

void MetricsRecorderBase::MeasureTimeUntilEvent(
    const std::string& end_event,
    const std::string& measurement_name,
    base::TimeTicks start_time) {
  timed_event_recorder_.MeasureTimeUntilEvent(end_event, measurement_name,
                                              start_time);
}

void MetricsRecorderBase::RecordTimelineEvent(const std::string& event_name) {
  base::TimeTicks now =
      tick_clock_ ? tick_clock_->NowTicks() : base::TimeTicks::Now();
  timed_event_recorder_.RecordEvent(event_name, now);
}

}  // namespace chromecast
