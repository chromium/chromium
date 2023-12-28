// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_EVENT_DRIVEN_TELEMETRY_COLLECTOR_POOL_H_
#define COMPONENTS_REPORTING_METRICS_EVENT_DRIVEN_TELEMETRY_COLLECTOR_POOL_H_

#include <vector>

#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

// Interface for fetching collectors to collect telemetry data on event.
class EventDrivenTelemetryCollectorPool {
 public:
  virtual ~EventDrivenTelemetryCollectorPool() = default;

  // Get the telemetry collectors associated with `event_type`.
  virtual std::vector<raw_ptr<CollectorBase, VectorExperimental>>
  GetTelemetryCollectors(MetricEventType event_type) = 0;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_EVENT_DRIVEN_TELEMETRY_COLLECTOR_POOL_H_
