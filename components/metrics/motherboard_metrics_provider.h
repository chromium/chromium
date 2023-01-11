// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_MOTHERBOARD_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_MOTHERBOARD_METRICS_PROVIDER_H_

#include <memory>
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/motherboard.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

// MotherboardMetricsProvider adds Motherboard Info in the system profile. These
// include manufacturer and model.
class MotherboardMetricsProvider : public MetricsProvider {
 public:
  MotherboardMetricsProvider();
  ~MotherboardMetricsProvider() override;

  MotherboardMetricsProvider(const MotherboardMetricsProvider&) = delete;
  MotherboardMetricsProvider& operator=(const MotherboardMetricsProvider&) =
      delete;

  // metrics::MetricsProvider:
  void AsyncInit(base::OnceClosure done_callback) override;
  void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto) override;

 private:
  // Initializes `motherboard_info_` on the UI thread.
  void InitializeMotherboard(base::OnceClosure cb,
                             std::unique_ptr<Motherboard> motherboard_info);

  std::unique_ptr<const Motherboard> motherboard_info_;
  base::WeakPtrFactory<MotherboardMetricsProvider> weak_ptr_factory_{this};
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_MOTHERBOARD_METRICS_PROVIDER_H_
