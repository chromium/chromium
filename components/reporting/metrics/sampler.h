// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_SAMPLER_H_
#define COMPONENTS_REPORTING_METRICS_SAMPLER_H_

#include "base/callback.h"
#include "components/reporting/proto/metric_data.pb.h"

namespace reporting {

using InfoCallback = base::OnceCallback<void(InfoData)>;

using TelemetryCallback = base::OnceCallback<void(TelemetryData)>;

// A sampler is an object capable of collecting Information and Telemetry of a
// given type. So for example a NetworkSampler should collect the information
// and telemetry that can describe the state, usage, and health of the network
// interfaces and connections.
// Information is data that is not expected to be changed frequently or at all,
// for example, the serial number of a device. Information will only be
// transmitted once per session.
// Telemetry is data that may change over time, such as CPU usage, memory usage,
// apps opened by a user etc.. Telemetries are expected to be polled
// periodically.
class Sampler {
 public:
  virtual ~Sampler() = default;
  virtual void CollectInfo(InfoCallback callback) {}
  virtual void CollectTelemetry(TelemetryCallback callback) = 0;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_SAMPLER_H_
