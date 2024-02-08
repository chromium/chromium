// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"

#include "base/notreached.h"

namespace ash::phonehub {

PhoneHubStructuredMetricsLogger::PhoneHubStructuredMetricsLogger() = default;
PhoneHubStructuredMetricsLogger::~PhoneHubStructuredMetricsLogger() = default;

void PhoneHubStructuredMetricsLogger::LogPhoneHubDiscoveryStarted(
    DiscoveryEntryPoint entry_point) {
  NOTIMPLEMENTED();
}

void PhoneHubStructuredMetricsLogger::LogDiscoveryAttempt(
    secure_channel::mojom::DiscoveryResult result,
    std::optional<secure_channel::mojom::DiscoveryErrorCode> error_code) {
  NOTIMPLEMENTED();
}

void PhoneHubStructuredMetricsLogger::LogNearbyConnectionState(
    secure_channel::mojom::NearbyConnectionStep step,
    secure_channel::mojom::NearbyConnectionStepResult result) {
  NOTIMPLEMENTED();
}

void PhoneHubStructuredMetricsLogger::LogSecureChannelState(
    secure_channel::mojom::SecureChannelState state) {
  NOTIMPLEMENTED();
}

void PhoneHubStructuredMetricsLogger::LogPhoneHubMessageEvent(
    proto::MessageType message_type,
    PhoneHubMessageDirection message_direction) {
  NOTIMPLEMENTED();
}

void PhoneHubStructuredMetricsLogger::LogPhoneHubUiStateUpdated(
    PhoneHubUiState ui_state) {
  std::string state;
  NOTIMPLEMENTED();
}
}  // namespace ash::phonehub
