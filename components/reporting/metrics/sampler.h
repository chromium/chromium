// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_SAMPLER_H_
#define COMPONENTS_REPORTING_METRICS_SAMPLER_H_

#include "base/callback.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

using OptionalMetricCallback =
    base::OnceCallback<void(absl::optional<MetricData>)>;
using MetricCallback = base::OnceCallback<void(MetricData)>;
using MetricRepeatingCallback = base::RepeatingCallback<void(MetricData)>;

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

// A `MetricEventObserver` object should observe events and report them using
// the `MetricRepeatingCallback` set using `SetOnEventObservedCallback`.
// Whether the object should observe/report events is controlled by is_enabled
// set using `SetReportingEnabled`.
class MetricEventObserver {
 public:
  virtual ~MetricEventObserver() = default;
  virtual void SetOnEventObservedCallback(MetricRepeatingCallback cb) = 0;
  virtual void SetReportingEnabled(bool is_enabled) = 0;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_SAMPLER_H_
