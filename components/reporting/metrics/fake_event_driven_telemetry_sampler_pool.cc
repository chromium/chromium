// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/fake_event_driven_telemetry_sampler_pool.h"

#include <vector>

#include "base/containers/flat_map.h"
#include "components/reporting/metrics/configured_sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting::test {

FakeEventDrivenTelemetrySamplerPool::FakeEventDrivenTelemetrySamplerPool() =
    default;

FakeEventDrivenTelemetrySamplerPool::~FakeEventDrivenTelemetrySamplerPool() =
    default;

std::vector<ConfiguredSampler*>
FakeEventDrivenTelemetrySamplerPool::GetTelemetrySamplers(
    MetricEventType event_type) {
  return event_telemetry_map.at(event_type);
}

void FakeEventDrivenTelemetrySamplerPool::AddEventSampler(
    MetricEventType event_type,
    ConfiguredSampler* configured_sampler) {
  event_telemetry_map[event_type].push_back(configured_sampler);
}

}  // namespace reporting::test
