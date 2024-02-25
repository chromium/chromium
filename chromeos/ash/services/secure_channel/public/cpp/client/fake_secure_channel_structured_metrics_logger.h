// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_SECURE_CHANNEL_STRUCTURED_METRICS_LOGGER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_SECURE_CHANNEL_STRUCTURED_METRICS_LOGGER_H_

#include <optional>

#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_structured_metrics_logger.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"

namespace ash::secure_channel {
class FakeSecureChannelStructuredMetricsLogger
    : public SecureChannelStructuredMetricsLogger {
 public:
  FakeSecureChannelStructuredMetricsLogger();
  ~FakeSecureChannelStructuredMetricsLogger() override;

  void LogDiscoveryAttempt(
      mojom::DiscoveryResult result,
      std::optional<mojom::DiscoveryErrorCode> error_code) override;

  void LogNearbyConnectionState(
      mojom::NearbyConnectionStep step,
      secure_channel::mojom::NearbyConnectionStepResult result) override;

  void LogSecureChannelState(mojom::SecureChannelState state) override;
};
}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_SECURE_CHANNEL_STRUCTURED_METRICS_LOGGER_H_
