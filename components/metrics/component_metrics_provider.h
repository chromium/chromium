// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_COMPONENT_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_COMPONENT_METRICS_PROVIDER_H_

#include <vector>

#include "components/metrics/metrics_provider.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace component_updater {
struct ComponentInfo;
}

namespace metrics {

class SystemProfileProto;

// A delegate that returns a list of components that are loaded in the
// system.
class ComponentMetricsProviderDelegate {
 public:
  ComponentMetricsProviderDelegate() = default;
  virtual ~ComponentMetricsProviderDelegate() = default;

  virtual std::vector<component_updater::ComponentInfo> GetComponents() = 0;
};

// Stores and loads system information to prefs for stability logs.
class ComponentMetricsProvider : public MetricsProvider {
 public:
  explicit ComponentMetricsProvider(
      std::unique_ptr<ComponentMetricsProviderDelegate>
          components_info_delegate);

  ComponentMetricsProvider(const ComponentMetricsProvider&) = delete;
  ComponentMetricsProvider& operator=(const ComponentMetricsProvider&) = delete;

  ~ComponentMetricsProvider() override;

  // MetricsProvider:
  void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto) override;

  static SystemProfileProto_ComponentId CrxIdToComponentId(
      const std::string& app_id);

 private:
  std::unique_ptr<ComponentMetricsProviderDelegate> components_info_delegate_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_COMPONENT_METRICS_PROVIDER_H_
