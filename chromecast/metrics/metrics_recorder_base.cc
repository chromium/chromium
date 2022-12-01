// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/metrics_recorder_base.h"

#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/metrics/cast_event_builder.h"
#include "net/base/ip_address.h"

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

void MetricsRecorderBase::RecordTimelineEvent(const std::string& event_name) {
  base::TimeTicks now =
      tick_clock_ ? tick_clock_->NowTicks() : base::TimeTicks::Now();
  timed_event_recorder_.RecordEvent(event_name, now);
}

}  // namespace chromecast
