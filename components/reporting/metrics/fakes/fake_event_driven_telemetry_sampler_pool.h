// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_FAKES_FAKE_EVENT_DRIVEN_TELEMETRY_SAMPLER_POOL_H_
#define COMPONENTS_REPORTING_METRICS_FAKES_FAKE_EVENT_DRIVEN_TELEMETRY_SAMPLER_POOL_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "components/reporting/metrics/configured_sampler.h"
#include "components/reporting/metrics/event_driven_telemetry_sampler_pool.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting::test {

class FakeEventDrivenTelemetrySamplerPool
    : public EventDrivenTelemetrySamplerPool {
 public:
  FakeEventDrivenTelemetrySamplerPool();

  FakeEventDrivenTelemetrySamplerPool(
      const FakeEventDrivenTelemetrySamplerPool& other) = delete;
  FakeEventDrivenTelemetrySamplerPool& operator=(
      const FakeEventDrivenTelemetrySamplerPool& other) = delete;

  ~FakeEventDrivenTelemetrySamplerPool() override;

  // EventDrivenTelemetrySamplerPool:
  std::vector<ConfiguredSampler*> GetTelemetrySamplers(
      MetricEventType event_type) override;

  void AddEventSampler(MetricEventType event_type,
                       ConfiguredSampler* configured_sampler);

 private:
  base::flat_map<MetricEventType, std::vector<ConfiguredSampler*>>
      event_telemetry_map;
};

}  // namespace reporting::test

#endif  // COMPONENTS_REPORTING_METRICS_FAKES_FAKE_EVENT_DRIVEN_TELEMETRY_SAMPLER_POOL_H_
