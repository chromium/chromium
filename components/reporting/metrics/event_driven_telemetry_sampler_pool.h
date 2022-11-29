// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_EVENT_DRIVEN_TELEMETRY_SAMPLER_POOL_H_
#define COMPONENTS_REPORTING_METRICS_EVENT_DRIVEN_TELEMETRY_SAMPLER_POOL_H_

#include <vector>

#include "components/reporting/metrics/configured_sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

// Base class for fetching samplers to collect telemetry data on event.
class EventDrivenTelemetrySamplerPool {
 public:
  virtual ~EventDrivenTelemetrySamplerPool() = default;

  // Get the configured telemetry samplers associated with `event_type`.
  virtual std::vector<ConfiguredSampler*> GetTelemetrySamplers(
      MetricEventType event_type) = 0;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_EVENT_DRIVEN_TELEMETRY_SAMPLER_POOL_H_
