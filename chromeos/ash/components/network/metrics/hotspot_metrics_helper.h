// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_HOTSPOT_METRICS_HELPER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_HOTSPOT_METRICS_HELPER_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class HotspotController;

// This class is used to track the hotspot capabilities and status update and
// emits UMA metrics to the related histogram.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HotspotMetricsHelper
    : public LoginState::Observer,
      public HotspotCapabilitiesProvider::Observer,
      public HotspotStateHandler::Observer,
      public hotspot_config::mojom::HotspotEnabledStateObserver {
 public:
  // Emits enable/disable hotspot operation result to related UMA histogram.
  static void RecordSetTetheringEnabledResult(
      bool enabled,
      hotspot_config::mojom::HotspotControlResult result);

  // Emits check tethering readiness operation result to related UMA histogram.
  static void RecordCheckTetheringReadinessResult(
      HotspotCapabilitiesProvider::CheckTetheringReadinessResult result);

  // Emits set hotspot configuration operation result to related UMA histogram.
  static void RecordSetHotspotConfigResult(
      hotspot_config::mojom::SetHotspotConfigResult result);

  HotspotMetricsHelper();
  HotspotMetricsHelper(const HotspotMetricsHelper&) = delete;
  HotspotMetricsHelper& operator=(const HotspotMetricsHelper&) = delete;
  ~HotspotMetricsHelper() override;

  void Init(HotspotCapabilitiesProvider* hotspot_capabilities_provider,
            HotspotStateHandler* hotspot_state_handler,
            HotspotController* hotspot_controller);

  void set_is_enterprise_managed(bool is_enterprise_managed) {
    is_enterprise_managed_ = is_enterprise_managed;
  }

 private:
  friend class HotspotMetricsHelperTest;
  FRIEND_TEST_ALL_PREFIXES(HotspotMetricsHelperTest,
                           HotspotAllowStatusHistogram);
  FRIEND_TEST_ALL_PREFIXES(HotspotMetricsHelperTest,
                           HotspotUsageConfigHistogram);
  FRIEND_TEST_ALL_PREFIXES(HotspotMetricsHelperTest,
                           HotspotUsageDurationHistogram);
  FRIEND_TEST_ALL_PREFIXES(HotspotMetricsHelperTest,
                           HotspotMaxClientCountHistogram);
  FRIEND_TEST_ALL_PREFIXES(HotspotMetricsHelperTest,
                           HotspotIsDeviceManagedHistogram);
  FRIEND_TEST_ALL_PREFIXES(HotspotControllerTest, EnableTetheringSuccess);
  FRIEND_TEST_ALL_PREFIXES(HotspotControllerTest,
                           EnableTetheringReadinessCheckFailure);
  FRIEND_TEST_ALL_PREFIXES(HotspotControllerTest,
                           EnableTetheringNetworkSetupFailure);
  FRIEND_TEST_ALL_PREFIXES(HotspotControllerTest, DisableTetheringSuccess);
  FRIEND_TEST_ALL_PREFIXES(HotspotStateHandlerTest, SetAndGetHotspotConfig);
  FRIEND_TEST_ALL_PREFIXES(HotspotCapabilitiesProviderTest,
                           CheckTetheringReadiness);

  enum class HotspotMetricsSetEnabledResult;
  enum class HotspotMetricsSetConfigResult;
  enum class HotspotMetricsCheckReadinessResult;

  static const char kHotspotAllowStatusHistogram[];
  static const char kHotspotAllowStatusAtLoginHistogram[];
  static const char kHotspotEnableResultHistogram[];
  static const char kHotspotDisableResultHistogram[];
  static const char kHotspotSetConfigResultHistogram[];
  static const char kHotspotCheckReadinessResultHistogram[];
  static const char kHotspotUsageConfigAutoDisable[];
  static const char kHotspotUsageConfigMAR[];
  static const char kHotspotUsageConfigCompatibilityMode[];
  static const char kHotspotUsageDuration[];
  static const char kHotspotMaxClientCount[];
  static const char kHotspotIsDeviceManaged[];
  static const base::TimeDelta kLogAllowStatusAtLoginTimeout;

  static HotspotMetricsCheckReadinessResult GetCheckReadinessMetricsResult(
      const HotspotCapabilitiesProvider::CheckTetheringReadinessResult& result);
  static HotspotMetricsSetEnabledResult GetSetEnabledMetricsResult(
      const hotspot_config::mojom::HotspotControlResult& result);
  static HotspotMetricsSetConfigResult GetSetConfigMetricsResult(
      const hotspot_config::mojom::SetHotspotConfigResult& result);

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

  // Represents the operation result of set hotspot configuration used for
  // related UMA histogram. These values are persisted to logs. Entries should
  // not be renumbered and numeric values should never be reused.
  enum class HotspotMetricsSetConfigResult {
    kSuccess,
    kFailedNotLogin,
    kFailedInvalidConfiguration,
    kMaxValue = kFailedInvalidConfiguration,
  };

  // Represents the operation result of check tethering readiness used for
  // related UMA histogram. These values are persisted to logs. Entries should
  // not be renumbered and numeric values should never be reused.
  enum class HotspotMetricsCheckReadinessResult {
    kReady = 0,
    kNotAllowed = 1,
    kUpstreamNetworkNotAvailable = 2,
    kShillOperationFailed = 3,
    kUnknownResult = 4,
    kMaxValue = kUnknownResult,
  };

  // Represents the operation result of enable/disable hotspot used for related
  // UMA histograms. These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  enum class HotspotMetricsSetEnabledResult {
    kSuccess,
    kNotAllowed,
    kReadinessCheckFailure,
    kDisableWifiFailure,
    kInvalidConfiguration,
    kUpstreamNotAvailable,
    kNetworkSetupFailure,
    kWifiDriverFailure,
    kCellularAttachFailure,
    kShillOperationFailure,
    kUnknownFailure,
    kMaxValue = kUnknownFailure,
  };

  // HotspotCapabilitiesProvider::Observer:
  void OnHotspotCapabilitiesChanged() override;

  // HotspotStateHandler::Observer:
  void OnHotspotStatusChanged() override;

  // LoginState::Observer:
  void LoggedInStateChanged() override;

  // hotspot_config::mojom::HotspotEnabledStateObserver:
  void OnHotspotTurnedOn(bool wifi_turned_off) override;
  void OnHotspotTurnedOff(hotspot_config::mojom::DisableReason reason) override;

  void LogAllowStatus();
  void LogAllowStatusAtLogin();
  void LogUsageConfig();
  void LogUsageDuration();
  void LogMaxClientCount();
  void LogIsDeviceManaged();

  // Retrieves the latest hotspot allow status and converts to
  // HotspotMetricsAllowStatus enum. Return absl::nullopt if it is disallowed
  // due to device is not cellular capable.
  absl::optional<HotspotMetricsAllowStatus> GetMetricsAllowStatus();

  HotspotCapabilitiesProvider* hotspot_capabilities_provider_ = nullptr;
  HotspotStateHandler* hotspot_state_handler_ = nullptr;

  // A timer to wait for user connecting to their upstream cellular network
  // after login.
  base::OneShotTimer timer_;

  // Tracks the maximum connected client count per hotspot session.
  size_t max_client_count_ = 0;

  // Tracks whether the metrics are already logged for this session.
  bool is_metrics_logged_ = false;

  // Tracks whether the hotspot is active.
  bool is_hotspot_active_ = false;

  // Tracks the usage time for each hotspot session.
  absl::optional<base::ElapsedTimer> usage_timer_;

  // Tracks if the device is enterprise managed or not.
  bool is_enterprise_managed_ = false;

  mojo::Receiver<hotspot_config::mojom::HotspotEnabledStateObserver>
      hotspot_state_enabled_state_observer_receiver_{this};
  mojo::Receiver<hotspot_config::mojom::HotspotEnabledStateObserver>
      hotspot_controller_enabled_state_observer_receiver_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_HOTSPOT_METRICS_HELPER_H_
