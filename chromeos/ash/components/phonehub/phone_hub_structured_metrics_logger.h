// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_STRUCTURED_METRICS_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_STRUCTURED_METRICS_LOGGER_H_

#include <optional>

#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_structured_metrics_logger.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::phonehub {

enum class DiscoveryEntryPoint {
  kUserSignIn = 0,
  kChromebookUnlock = 1,
  kAutomaticConnectionRetry = 2,
  kManualConnectionRetry = 3,
  kConnectionRetryAfterConnected = 4,
  kPhoneHubBubbleOpen = 5,
  kMultiDeviceFeatureSetup = 6,
  kBluetoothEnabled = 7,
  kUserEnabledFeature = 8,
  kUserOnboardedToFeature = 9
};

class PhoneHubStructuredMetricsLogger
    : public ash::secure_channel::SecureChannelStructuredMetricsLogger {
 public:
  PhoneHubStructuredMetricsLogger();
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
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_STRUCTURED_METRICS_LOGGER_H_
