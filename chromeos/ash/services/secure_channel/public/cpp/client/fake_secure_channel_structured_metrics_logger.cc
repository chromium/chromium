// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_structured_metrics_logger.h"

namespace ash::secure_channel {

FakeSecureChannelStructuredMetricsLogger::
    FakeSecureChannelStructuredMetricsLogger() = default;
FakeSecureChannelStructuredMetricsLogger::
    ~FakeSecureChannelStructuredMetricsLogger() = default;

void FakeSecureChannelStructuredMetricsLogger::LogDiscoveryAttempt(
    mojom::DiscoveryResult result,
    std::optional<mojom::DiscoveryErrorCode> error_code) {}

void FakeSecureChannelStructuredMetricsLogger::LogNearbyConnectionState(
    mojom::NearbyConnectionStep step,
    secure_channel::mojom::NearbyConnectionStepResult result) {}

void FakeSecureChannelStructuredMetricsLogger::LogSecureChannelState(
    mojom::SecureChannelState state) {}
}  // namespace ash::secure_channel
