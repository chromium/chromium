// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/fakes/fake_event_driven_telemetry_collector_pool.h"

#include <vector>

#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting::test {

FakeEventDrivenTelemetryCollectorPool::FakeEventDrivenTelemetryCollectorPool() =
    default;

FakeEventDrivenTelemetryCollectorPool::
    ~FakeEventDrivenTelemetryCollectorPool() = default;

std::vector<raw_ptr<CollectorBase, VectorExperimental>>
FakeEventDrivenTelemetryCollectorPool::GetTelemetryCollectors(
    MetricEventType event_type) {
  return event_telemetry_map_.at(event_type);
}

void FakeEventDrivenTelemetryCollectorPool::AddEventTelemetryCollector(
    MetricEventType event_type,
    CollectorBase* collector) {
  event_telemetry_map_[event_type].push_back(collector);
}

}  // namespace reporting::test
