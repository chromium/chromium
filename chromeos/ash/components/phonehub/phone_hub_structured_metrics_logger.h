// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_STRUCTURED_METRICS_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_STRUCTURED_METRICS_LOGGER_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_structured_metrics_logger.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/metrics/structured/event.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {
base::TimeDelta kMaxStructuredMetricsPseudonymousIdDays = base::Days(90);
}
namespace ash::phonehub {

enum class DiscoveryEntryPoint {
  kUserSignIn = 0,
  kChromebookUnlock = 1,
  // TODO(b/324320785): Record attempt times with entrypoint instead of
  // recording retry as separate entrypoints.
  kAutomaticConnectionRetry = 2,
  kManualConnectionRetry = 3,
  kConnectionRetryAfterConnected = 4,
  kPhoneHubBubbleOpen = 5,
  kMultiDeviceFeatureSetup = 6,
  kBluetoothEnabled = 7,
  kUserEnabledFeature = 8,
  kUserOnboardedToFeature = 9
};

enum class PhoneHubMessageDirection {
  kPhoneToChromebook = 0,
  kChromebookToPhone = 1
};

enum class PhoneHubUiState {
  kDisconnected = 0,
  kConnecting = 1,
  kConnected = 2
};

enum class BluetoothStack { kBlueZ = 0, kFloss = 1 };

enum class NetworkState {
  kUnknown = 0,
  kSameNetwork = 1,
  kDifferentNetwork = 2,
  kPhoneOnCellular = 3
};

enum class Medium { kBluetooth = 0, kWebRTC = 1 };

class PhoneHubStructuredMetricsLogger
    : public ash::secure_channel::SecureChannelStructuredMetricsLogger {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  explicit PhoneHubStructuredMetricsLogger(PrefService* pref_service);
  ~PhoneHubStructuredMetricsLogger() override;

  PhoneHubStructuredMetricsLogger(const PhoneHubStructuredMetricsLogger&) =
      delete;
  PhoneHubStructuredMetricsLogger& operator=(
      const PhoneHubStructuredMetricsLogger&) = delete;

  void LogPhoneHubDiscoveryStarted(DiscoveryEntryPoint entry_point);

  // secure_channel::mojom::SecureChannelStructuredMetricsLogger
  void LogDiscoveryAttempt(
      secure_channel::mojom::DiscoveryResult result,
      std::optional<secure_channel::mojom::DiscoveryErrorCode> error_code)
      override;
  void LogNearbyConnectionState(
      secure_channel::mojom::NearbyConnectionStep step,
      secure_channel::mojom::NearbyConnectionStepResult result) override;
  void LogSecureChannelState(
      secure_channel::mojom::SecureChannelState state) override;

  void LogPhoneHubMessageEvent(proto::MessageType message_type,
                               PhoneHubMessageDirection message_direction);
  void LogPhoneHubUiStateUpdated(PhoneHubUiState ui_state);

  void ProcessPhoneInformation(const proto::PhoneProperties& phone_properties);

  void ResetCachedInformation();

  void ResetSessionId();

  void SetChromebookInfo(proto::CrosState& cros_state_message);

 private:
  FRIEND_TEST_ALL_PREFIXES(PhoneHubStructuredMetricsLoggerTest,
                           ProcessPhoneInformation_MissingFields);
  FRIEND_TEST_ALL_PREFIXES(PhoneHubStructuredMetricsLoggerTest,
                           ProcessPhoneInformation_AllFields);
  FRIEND_TEST_ALL_PREFIXES(PhoneHubStructuredMetricsLoggerTest, LogEvents);

  void UpdateIdentifiersIfNeeded();
  void OnNetworkStateListFetched(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);
  void UploadDeviceInfo();

  // Phone information
  std::optional<proto::NetworkStatus> phone_network_status_;
  std::optional<std::string> phone_network_ssid_;

  // Chromebook information
  BluetoothStack bluetooth_stack_;
  NetworkState network_state_ = NetworkState::kUnknown;
  std::string chromebook_locale_;

  std::string phone_hub_session_id_;
  Medium medium_ = Medium::kBluetooth;

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_;
  raw_ptr<PrefService> pref_service_;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_STRUCTURED_METRICS_LOGGER_H_
