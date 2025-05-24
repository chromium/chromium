// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_HOTSPOT_METRICS_HELPER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_HOTSPOT_METRICS_HELPER_H_

#include <optional>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"
#include "chromeos/ash/components/network/hotspot_enabled_state_notifier.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class EnterpriseManagedMetadataStore;
class HotspotConfigurationHandler;
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
      hotspot_config::mojom::SetHotspotConfigResult result,
      const std::string& shill_error = "");

  // Emits hotspot enable operation latency to related UMA histogram.
  static void RecordEnableHotspotLatency(const base::TimeDelta& latency);

  HotspotMetricsHelper();
  HotspotMetricsHelper(const HotspotMetricsHelper&) = delete;
  HotspotMetricsHelper& operator=(const HotspotMetricsHelper&) = delete;
  ~HotspotMetricsHelper() override;

  void Init(EnterpriseManagedMetadataStore* enterprise_managed_metadata_store,
            HotspotCapabilitiesProvider* hotspot_capabilities_provider,
            HotspotStateHandler* hotspot_state_handler,
            HotspotController* hotspot_controller,
            HotspotConfigurationHandler* hotspot_configuration_handler,
            HotspotEnabledStateNotifier* hotspot_enabled_state_notifier,
            NetworkStateHandler* network_state_handler);

 private:
  friend class HotspotMetricsHelperTest;
  friend class HotspotControllerTest;
  friend class HotspotControllerConcurrencyApiTest;

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
  FRIEND_TEST_ALL_PREFIXES(HotspotMetricsHelperTest,
                           HotspotEnabledUpstreamStatusHistogram);
  FRIEND_TEST_ALL_PREFIXES(HotspotMetricsHelperTest,
                           HotspotDisableReasonHistogram);
  FRIEND_TEST_ALL_PREFIXES(HotspotMetricsHelperTest, HotspotSetConfigHistogram);
  FRIEND_TEST_ALL_PREFIXES(HotspotControllerTest, EnableTetheringSuccess);
  FRIEND_TEST_ALL_PREFIXES(HotspotControllerTest, AbortEnableTethering);
  FRIEND_TEST_ALL_PREFIXES(HotspotControllerTest,
                           ShillOperationFailureWhileAborting);
  FRIEND_TEST_ALL_PREFIXES(HotspotControllerTest,
                           EnableTetheringReadinessCheckFailure);
  FRIEND_TEST_ALL_PREFIXES(HotspotControllerTest,
                           EnableTetheringNetworkSetupFailure);
  FRIEND_TEST_ALL_PREFIXES(HotspotControllerTest, DisableTetheringSuccess);
  FRIEND_TEST_ALL_PREFIXES(HotspotControllerConcurrencyApiTest,
                           EnableTetheringSuccess);
  FRIEND_TEST_ALL_PREFIXES(HotspotControllerConcurrencyApiTest,
                           AbortEnableTethering);
  FRIEND_TEST_ALL_PREFIXES(HotspotControllerConcurrencyApiTest,
                           ShillOperationFailureWhileAborting);
  FRIEND_TEST_ALL_PREFIXES(HotspotControllerConcurrencyApiTest,
                           EnableTetheringReadinessCheckFailure);
  FRIEND_TEST_ALL_PREFIXES(HotspotControllerConcurrencyApiTest,
                           EnableTetheringNetworkSetupFailure);
  FRIEND_TEST_ALL_PREFIXES(HotspotControllerConcurrencyApiTest,
                           DisableTetheringSuccess);
  FRIEND_TEST_ALL_PREFIXES(HotspotConfigurationHandlerTest,
                           SetAndGetHotspotConfig);
  FRIEND_TEST_ALL_PREFIXES(HotspotCapabilitiesProviderTest,
                           CheckTetheringReadiness_Ready);
  FRIEND_TEST_ALL_PREFIXES(HotspotCapabilitiesProviderTest,
                           CheckTetheringReadiness_NotAllowed);
  FRIEND_TEST_ALL_PREFIXES(HotspotCapabilitiesProviderTest,
                           CheckTetheringReadiness_NotAllowedByCarrier);
  FRIEND_TEST_ALL_PREFIXES(HotspotCapabilitiesProviderTest,
                           CheckTetheringReadiness_UpstreamNotAvailable);
  FRIEND_TEST_ALL_PREFIXES(HotspotCapabilitiesProviderTest,
                           CheckTetheringReadiness_EmptyResult);
  FRIEND_TEST_ALL_PREFIXES(HotspotCapabilitiesProviderTest,
                           CheckTetheringReadiness_Failure);

  enum class HotspotMetricsSetEnabledResult;
  enum class HotspotMetricsSetConfigResult;
  enum class HotspotMetricsCheckReadinessResult;
  enum class HotspotMetricsDisableReason;

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
  static const char kHotspotEnableLatency[];
  static const char kHotspotUpstreamStatusWhenEnabled[];
  static const char kHotspotDisableReasonHistogram[];
  static const base::TimeDelta kLogAllowStatusAtLoginTimeout;

  static HotspotMetricsCheckReadinessResult GetCheckReadinessMetricsResult(
      const HotspotCapabilitiesProvider::CheckTetheringReadinessResult& result);
  static HotspotMetricsSetEnabledResult GetSetEnabledMetricsResult(
      const hotspot_config::mojom::HotspotControlResult& result);
  static HotspotMetricsSetConfigResult GetSetConfigMetricsResult(
      const hotspot_config::mojom::SetHotspotConfigResult& result,
      const std::string& shill_error);
  static HotspotMetricsDisableReason GetMetricsDisableReason(
      const hotspot_config::mojom::DisableReason& reason);

  // Represents the hotspot allow status on device. Note:
  // kDisallowNoCellularUpstream is not logged in the metric because it means
  // the device is not cellular capable, and it would drown out the metric by
  // adding the bucket. These values are persisted to logs. Entries should not
  // be renumbered and numeric values should never be reused.
  enum class HotspotMetricsAllowStatus {
    kAllowed = 0,
    kDisallowedWiFiDownstreamNotSupported = 1,
    kDisallowedNoWiFiSecurityModes = 2,
    kDisallowedNoMobileData = 3,
    kDisallowedReadinessCheckFail = 4,
    kDisallowedByPolicy = 5,
    kMaxValue = kDisallowedByPolicy,
  };

  // Represents the operation result of set hotspot configuration used for
  // related UMA histogram. These values are persisted to logs. Entries should
  // not be renumbered and numeric values should never be reused.
  enum class HotspotMetricsSetConfigResult {
    kSuccess = 0,
    kFailedNotLogin = 1,
    kFailedInvalidConfiguration = 2,
    kFailedIllegalOperation = 3,
    kFailedPermissionDenied = 4,
    kFailedInvalidArgument = 5,
    kFailedShillOperation = 6,
    kFailedUnknownShillError = 7,
    kMaxValue = kFailedUnknownShillError,
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
    kNotAllowedByCarrier = 5,
    kNotAllowedOnFW = 6,
    kNotAllowedOnVariant = 7,
    kNotAllowedUserNotEntitled = 8,
    kMaxValue = kNotAllowedUserNotEntitled,
  };

  // Represents the operation result of enable/disable hotspot used for related
  // UMA histograms. These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  enum class HotspotMetricsSetEnabledResult {
    kSuccess = 0,
    kNotAllowed = 1,
    kReadinessCheckFailure = 2,
    kDisableWifiFailure = 3,
    kInvalidConfiguration = 4,
    kUpstreamNotAvailable = 5,
    kNetworkSetupFailure = 6,
    kDownstreamWifiFailure = 7,
    kUpstreamFailure = 8,
    kShillOperationFailure = 9,
    kUnknownFailure = 10,
    kAlreadyFulfilled = 11,
    kAborted = 12,
    kInvalid = 13,
    kBusy = 14,
    kConcurrencyNotSupported = 15,
    kOperationFailure = 16,
    kMaxValue = kOperationFailure,
  };

  // Represents the upstream status when hotspot is enabled. These values are
  // persisted to logs. Entries should not be renumbered and numeric values
  // should never be reused.
  enum class HotspotMetricsUpstreamStatus {
    kWifiWithCellularConnected = 0,
    kWifiWithCellularNotConnected = 1,
    kMaxValue = kWifiWithCellularNotConnected,
  };

  // Represents the hotspot disable reason. These values are persisted to logs.
  // Entries should not be renumbered and numeric values should never be used.
  enum class HotspotMetricsDisableReason {
    kAutoDisabled = 0,
    kInternalError = 1,
    kUserInitiated = 2,
    kWifiEnabled = 3,
    kProhibitedByPolicy = 4,
    kUpstreamNetworkNotAvailable = 5,
    kSuspended = 6,
    kRestart = 7,
    kUpstreamNoInternet = 8,
    kDownstreamLinkDisconnect = 9,
    kDownstreamNetworkDisconnect = 10,
    kStartTimeout = 11,
    kUpstreamNotAvailable = 12,
    kUnknownError = 13,
    kResourceBusy = 14,
    kMaxValue = kResourceBusy,
  };

  // HotspotCapabilitiesProvider::Observer:
  void OnHotspotCapabilitiesChanged() override;

  // HotspotStateHandler::Observer:
  void OnHotspotStatusChanged() override;

  // LoginState::Observer:
  void LoggedInStateChanged() override;

  // hotspot_config::mojom::HotspotEnabledStateObserver:
  void OnHotspotTurnedOn() override;
  void OnHotspotTurnedOff(hotspot_config::mojom::DisableReason reason) override;

  void LogAllowStatus();
  void LogAllowStatusAtLogin();
  void LogUsageConfig();
  void LogUsageDuration();
  void LogMaxClientCount();
  void LogIsDeviceManaged();
  void LogUpstreamStatus();
  void LogDisableReason(const hotspot_config::mojom::DisableReason& reason);

  // Retrieves the latest hotspot allow status and converts to
  // HotspotMetricsAllowStatus enum. Return std::nullopt if it is disallowed
  // due to device is not cellular capable.
  std::optional<HotspotMetricsAllowStatus> GetMetricsAllowStatus();

  raw_ptr<EnterpriseManagedMetadataStore> enterprise_managed_metadata_store_ =
      nullptr;
  raw_ptr<HotspotCapabilitiesProvider> hotspot_capabilities_provider_ = nullptr;
  raw_ptr<HotspotStateHandler> hotspot_state_handler_ = nullptr;
  raw_ptr<HotspotConfigurationHandler> hotspot_configuration_handler_ = nullptr;
  raw_ptr<HotspotEnabledStateNotifier, DanglingUntriaged>
      hotspot_enabled_state_notifier_ = nullptr;
  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;

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
  std::optional<base::ElapsedTimer> usage_timer_;

  // Tracks if the device is enterprise managed or not.
  bool is_enterprise_managed_ = false;

  mojo::Receiver<hotspot_config::mojom::HotspotEnabledStateObserver>
      hotspot_enabled_state_notifier_receiver_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_HOTSPOT_METRICS_HELPER_H_
