// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/hotspot_metrics_helper.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_event_log.h"

namespace ash {

// static
const base::TimeDelta HotspotMetricsHelper::kLogAllowStatusAtLoginTimeout =
    base::Seconds(30);

// static
const char HotspotMetricsHelper::kHotspotAllowStatusHistogram[] =
    "Network.Ash.Hotspot.Upstream.Cellular.Capability.AllowStatus";

// static
const char HotspotMetricsHelper::kHotspotAllowStatusAtLoginHistogram[] =
    "Network.Ash.Hotspot.Upstream.Cellular.Capability.AllowStatusAtLogin";

HotspotMetricsHelper::HotspotMetricsHelper() = default;

HotspotMetricsHelper::~HotspotMetricsHelper() {
  if (hotspot_capabilities_provider_ &&
      hotspot_capabilities_provider_->HasObserver(this)) {
    hotspot_capabilities_provider_->RemoveObserver(this);
  }

  if (LoginState::IsInitialized()) {
    LoginState::Get()->RemoveObserver(this);
  }
}

void HotspotMetricsHelper::Init(
    HotspotCapabilitiesProvider* hotspot_capabilities_provider) {
  hotspot_capabilities_provider_ = hotspot_capabilities_provider;
  hotspot_capabilities_provider_->AddObserver(this);
  if (LoginState::IsInitialized()) {
    LoginState::Get()->AddObserver(this);
    LoggedInStateChanged();
  }
}

void HotspotMetricsHelper::OnHotspotCapabilitiesChanged() {
  LogAllowStatus();
}

void HotspotMetricsHelper::LoggedInStateChanged() {
  if (!LoginState::Get()->IsUserLoggedIn()) {
    timer_.Stop();
    is_metrics_logged_ = false;
    return;
  }

  timer_.Start(FROM_HERE, kLogAllowStatusAtLoginTimeout, this,
               &HotspotMetricsHelper::LogAllowStatusAtLogin);
}

void HotspotMetricsHelper::LogAllowStatus() {
  absl::optional<HotspotMetricsAllowStatus> metrics_allow_status =
      GetMetricsAllowStatus();
  if (!metrics_allow_status) {
    return;
  }

  base::UmaHistogramEnumeration(kHotspotAllowStatusHistogram,
                                *metrics_allow_status);
}

void HotspotMetricsHelper::LogAllowStatusAtLogin() {
  if (is_metrics_logged_) {
    return;
  }

  absl::optional<HotspotMetricsAllowStatus> metrics_allow_status =
      GetMetricsAllowStatus();
  if (!metrics_allow_status) {
    return;
  }

  base::UmaHistogramEnumeration(kHotspotAllowStatusAtLoginHistogram,
                                *metrics_allow_status);
  is_metrics_logged_ = true;
}

absl::optional<HotspotMetricsHelper::HotspotMetricsAllowStatus>
HotspotMetricsHelper::GetMetricsAllowStatus() {
  using hotspot_config::mojom::HotspotAllowStatus;

  HotspotAllowStatus allow_status =
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status;

  switch (allow_status) {
    case HotspotAllowStatus::kDisallowedNoWiFiDownstream:
      return HotspotMetricsAllowStatus::kDisallowedWiFiDownstreamNotSupported;
    case HotspotAllowStatus::kDisallowedNoWiFiSecurityModes:
      return HotspotMetricsAllowStatus::kDisallowedNoWiFiSecurityModes;
    case HotspotAllowStatus::kDisallowedNoMobileData:
      return HotspotMetricsAllowStatus::kDisallowedNoMobileData;
    case HotspotAllowStatus::kDisallowedReadinessCheckFail:
      return HotspotMetricsAllowStatus::kDisallowedReadinessCheckFail;
    case HotspotAllowStatus::kDisallowedByPolicy:
      return HotspotMetricsAllowStatus::kDisallowedByPolicy;
    case HotspotAllowStatus::kAllowed:
      return HotspotMetricsAllowStatus::kAllowed;
    case HotspotAllowStatus::kDisallowedNoCellularUpstream:
      // Do not emit kDisallowedNoCellularUpstream which means the device is
      // not cellular capable. Otherwise, it would drown out the metric.
      return absl::nullopt;
  }
}

}  // namespace ash
