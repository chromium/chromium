// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_MOTHERBOARD_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_MOTHERBOARD_METRICS_PROVIDER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/metrics/cached_metrics_provider.h"
#include "components/metrics/motherboard.h"

namespace metrics {

class SystemProfileProto;

// MotherboardMetricsProvider adds Motherboard Info in the system profile. These
// include manufacturer and model.
class MotherboardMetricsProvider : public CachedMetricsProvider {
 public:
  MotherboardMetricsProvider();
  ~MotherboardMetricsProvider() override;

  MotherboardMetricsProvider(const MotherboardMetricsProvider&) = delete;
  MotherboardMetricsProvider& operator=(const MotherboardMetricsProvider&) =
      delete;

  // metrics::MetricsProvider:
  void AsyncInit(base::OnceClosure done_callback) override;

 protected:
  // CachedMetricsProvider:
  void ProvideSystemProfileMetricsFromCache(
      const SystemProfileProto& cached_system_profile,
      SystemProfileProto* system_profile) override;
  void ProvideSystemProfileMetricsNormal(
      SystemProfileProto* system_profile) override;

 private:
  // Initializes `motherboard_info_` on the UI thread.
  void InitializeMotherboard(base::OnceClosure cb,
                             std::unique_ptr<Motherboard> motherboard_info);

  std::unique_ptr<const Motherboard> motherboard_info_;
  base::WeakPtrFactory<MotherboardMetricsProvider> weak_ptr_factory_{this};
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_MOTHERBOARD_METRICS_PROVIDER_H_
