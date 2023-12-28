// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_FAKES_FAKE_EVENT_DRIVEN_TELEMETRY_COLLECTOR_POOL_H_
#define COMPONENTS_REPORTING_METRICS_FAKES_FAKE_EVENT_DRIVEN_TELEMETRY_COLLECTOR_POOL_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/event_driven_telemetry_collector_pool.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting::test {

class FakeEventDrivenTelemetryCollectorPool
    : public EventDrivenTelemetryCollectorPool {
 public:
  FakeEventDrivenTelemetryCollectorPool();

  FakeEventDrivenTelemetryCollectorPool(
      const FakeEventDrivenTelemetryCollectorPool& other) = delete;
  FakeEventDrivenTelemetryCollectorPool& operator=(
      const FakeEventDrivenTelemetryCollectorPool& other) = delete;

  ~FakeEventDrivenTelemetryCollectorPool() override;

  // EventDrivenTelemetryCollectorPool:
  std::vector<raw_ptr<CollectorBase, VectorExperimental>>
  GetTelemetryCollectors(MetricEventType event_type) override;

  void AddEventTelemetryCollector(MetricEventType event_type,
                                  CollectorBase* collector);

 private:
  base::flat_map<MetricEventType,
                 std::vector<raw_ptr<CollectorBase, VectorExperimental>>>
      event_telemetry_map_;
};

}  // namespace reporting::test

#endif  // COMPONENTS_REPORTING_METRICS_FAKES_FAKE_EVENT_DRIVEN_TELEMETRY_COLLECTOR_POOL_H_
