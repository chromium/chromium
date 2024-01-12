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

}  // namespace ash::phonehub
