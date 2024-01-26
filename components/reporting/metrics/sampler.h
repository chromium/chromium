// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_SAMPLER_H_
#define COMPONENTS_REPORTING_METRICS_SAMPLER_H_

#include <optional>

#include "base/functional/callback.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

using OptionalMetricCallback =
    base::OnceCallback<void(std::optional<MetricData>)>;

// A sampler is an object capable of collecting metrics data of a given type.
// Metrics data can be either Information or Telemetry.
// Information is data that is not expected to be changed frequently or at all,
// for example, the serial number of a device. Information will only be
// transmitted once per session.
// Telemetry is data that may change over time, such as CPU usage, memory usage,
// apps opened by a user etc.. Telemetries are expected to be polled
// periodically.
// So for example a NetworkTelemetrySampler should collect the
// telemetry that can describe the state, usage, and health of the network
// connections.
class Sampler {
 public:
  virtual ~Sampler() = default;
  virtual void MaybeCollect(OptionalMetricCallback callback) = 0;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_SAMPLER_H_
