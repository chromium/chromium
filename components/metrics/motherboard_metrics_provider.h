// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_MOTHERBOARD_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_MOTHERBOARD_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"
#include "components/metrics/motherboard.h"

namespace metrics {

// MotherboardMetricsProvider adds Motherboard Info in the system profile. These
// include manufacturer and model.
class MotherboardMetricsProvider : public MetricsProvider {
 public:
  MotherboardMetricsProvider() = default;

  MotherboardMetricsProvider(const MotherboardMetricsProvider&) = delete;
  MotherboardMetricsProvider& operator=(const MotherboardMetricsProvider&) =
      delete;

  ~MotherboardMetricsProvider() override = default;

  void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto) override;

 private:
  // All the Motherboard information is generated in the constructor.
  const Motherboard motherboard_info_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_MOTHERBOARD_METRICS_PROVIDER_H_
