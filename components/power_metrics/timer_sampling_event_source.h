// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_METRICS_TIMER_SAMPLING_EVENT_SOURCE_H_
#define COMPONENTS_POWER_METRICS_TIMER_SAMPLING_EVENT_SOURCE_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/power_metrics/sampling_event_source.h"

namespace power_metrics {

// Generates a sampling event at regular time intervals.
class TimerSamplingEventSource : public SamplingEventSource {
 public:
  // |interval| is the time interval between sampling events.
  explicit TimerSamplingEventSource(base::TimeDelta interval);

  ~TimerSamplingEventSource() override;

  // SamplingEventSource:
  bool Start(SamplingEventCallback callback) override;

 private:
  const base::TimeDelta interval_;
  base::RepeatingTimer timer_;
};

}  // namespace power_metrics

#endif  // COMPONENTS_POWER_METRICS_TIMER_SAMPLING_EVENT_SOURCE_H_
