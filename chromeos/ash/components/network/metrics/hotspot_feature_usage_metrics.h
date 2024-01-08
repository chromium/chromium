// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_HOTSPOT_FEATURE_USAGE_METRICS_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_HOTSPOT_FEATURE_USAGE_METRICS_H_

#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"

namespace ash {

class EnterpriseManagedMetadataStore;

// Reports daily Hotspot Standard Feature Usage Logging metrics.
// Eligible: devices that are both cellular and WiFi capable, and have
// the feature enabled.
// Accessible: devices out of Eligible device that are not blocked by
// enterprise policy.
// Enabled: devices that are allowed to enable hotspot, meaning they are
// successfully connecting to a cellular network and passed the tethering
// readiness check.
// Engaged: devices that have turned on hotspot successfully.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HotspotFeatureUsageMetrics
    : public feature_usage::FeatureUsageMetrics::Delegate {
 public:
  HotspotFeatureUsageMetrics();
  HotspotFeatureUsageMetrics(const HotspotFeatureUsageMetrics&) = delete;
  HotspotFeatureUsageMetrics& operator=(const HotspotFeatureUsageMetrics&) =
      delete;
  ~HotspotFeatureUsageMetrics() override;

  void Init(EnterpriseManagedMetadataStore* enterprise_managed_metadata_store,
            HotspotCapabilitiesProvider* hotspot_capabilities_provider);

  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const override;
  // Return std::nullopt if device is not managed, true if the feature is
  // allowed by the policy, false if the feature is prohibited by the policy.
  std::optional<bool> IsAccessible() const override;
  bool IsEnabled() const override;

  void RecordHotspotEnableAttempt(bool was_enabled_successfully);

 private:
  raw_ptr<EnterpriseManagedMetadataStore> enterprise_managed_metadata_store_ =
      nullptr;
  raw_ptr<HotspotCapabilitiesProvider> hotspot_capabilities_provider_ = nullptr;
  std::unique_ptr<feature_usage::FeatureUsageMetrics> feature_usage_metrics_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_HOTSPOT_FEATURE_USAGE_METRICS_H_
