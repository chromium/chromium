// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_HOTSPOT_METRICS_HELPER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_HOTSPOT_METRICS_HELPER_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// This class is used to track the hotspot capabilities and status update and
// emits UMA metrics to the related histogram.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HotspotMetricsHelper
    : public LoginState::Observer,
      public HotspotCapabilitiesProvider::Observer {
 public:
  HotspotMetricsHelper();
  HotspotMetricsHelper(const HotspotMetricsHelper&) = delete;
  HotspotMetricsHelper& operator=(const HotspotMetricsHelper&) = delete;
  ~HotspotMetricsHelper() override;

  void Init(HotspotCapabilitiesProvider* hotspot_capabilities_provider);

 private:
  friend class HotspotMetricsHelperTest;
  FRIEND_TEST_ALL_PREFIXES(HotspotMetricsHelperTest,
                           HotspotAllowStatusHistogram);

  static const char kHotspotAllowStatusHistogram[];
  static const char kHotspotAllowStatusAtLoginHistogram[];
  static const base::TimeDelta kLogAllowStatusAtLoginTimeout;

  // Represents the hotspot allow status on device. Note:
  // kDisallowNoCellularUpstream is not logged in the metric because it means
  // the device is not cellular capable, and it would drown out the metric by
  // adding the bucket. These values are persisted to logs. Entries should not
  // be renumbered and numeric values should never be reused.
  enum class HotspotMetricsAllowStatus {
    kAllowed,
    kDisallowedWiFiDownstreamNotSupported,
    kDisallowedNoWiFiSecurityModes,
    kDisallowedNoMobileData,
    kDisallowedReadinessCheckFail,
    kDisallowedByPolicy,
    kMaxValue = kDisallowedByPolicy,
  };

  // HotspotCapabilitiesProvider::Observer:
  void OnHotspotCapabilitiesChanged() override;

  // LoginState::Observer:
  void LoggedInStateChanged() override;

  void LogAllowStatus();
  void LogAllowStatusAtLogin();

  // Retrieves the latest hotspot allow status and converts to
  // HotspotMetricsAllowStatus enum. Return absl::nullopt if it is disallowed
  // due to device is not cellular capable.
  absl::optional<HotspotMetricsAllowStatus> GetMetricsAllowStatus();

  HotspotCapabilitiesProvider* hotspot_capabilities_provider_ = nullptr;

  // A timer to wait for user connecting to their upstream cellular network
  // after login.
  base::OneShotTimer timer_;

  // Tracks whether the metrics are already logged for this session.
  bool is_metrics_logged_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_HOTSPOT_METRICS_HELPER_H_
