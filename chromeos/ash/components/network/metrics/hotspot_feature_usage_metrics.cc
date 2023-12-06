// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/hotspot_feature_usage_metrics.h"

#include "chromeos/ash/components/network/enterprise_managed_metadata_store.h"
#include "chromeos/ash/components/network/hotspot_controller.h"
#include "chromeos/ash/components/network/network_event_log.h"

namespace ash {

namespace {

const char kHotspotFeatureUsageName[] = "Hotspot";

}  // namespace

HotspotFeatureUsageMetrics::HotspotFeatureUsageMetrics() = default;

HotspotFeatureUsageMetrics::~HotspotFeatureUsageMetrics() = default;

void HotspotFeatureUsageMetrics::Init(
    EnterpriseManagedMetadataStore* enterprise_managed_metadata_store,
    HotspotCapabilitiesProvider* hotspot_capabilities_provider) {
  enterprise_managed_metadata_store_ = enterprise_managed_metadata_store;
  hotspot_capabilities_provider_ = hotspot_capabilities_provider;
  feature_usage_metrics_ = std::make_unique<feature_usage::FeatureUsageMetrics>(
      kHotspotFeatureUsageName, this);
}

bool HotspotFeatureUsageMetrics::IsEligible() const {
  using hotspot_config::mojom::HotspotAllowStatus;

  const HotspotAllowStatus allow_status =
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status;
  return allow_status != HotspotAllowStatus::kDisallowedNoCellularUpstream &&
         allow_status != HotspotAllowStatus::kDisallowedNoWiFiDownstream &&
         allow_status != HotspotAllowStatus::kDisallowedNoWiFiSecurityModes;
}

std::optional<bool> HotspotFeatureUsageMetrics::IsAccessible() const {
  if (!enterprise_managed_metadata_store_->is_enterprise_managed()) {
    return std::nullopt;
  }

  if (!IsEligible()) {
    return false;
  }
  using hotspot_config::mojom::HotspotAllowStatus;

  const HotspotAllowStatus allow_status =
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status;
  return allow_status != HotspotAllowStatus::kDisallowedByPolicy;
}

bool HotspotFeatureUsageMetrics::IsEnabled() const {
  using hotspot_config::mojom::HotspotAllowStatus;

  const HotspotAllowStatus allow_status =
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status;
  return allow_status == HotspotAllowStatus::kAllowed;
}

void HotspotFeatureUsageMetrics::RecordHotspotEnableAttempt(
    bool was_enabled_successfully) {
  if (!IsEnabled()) {
    // Record feature usage has to be called on an "enabled" device.
    NET_LOG(ERROR) << "Attempted to start hotspot on a disabled device.";
    return;
  }
  feature_usage_metrics_->RecordUsage(was_enabled_successfully);
}

}  // namespace ash
