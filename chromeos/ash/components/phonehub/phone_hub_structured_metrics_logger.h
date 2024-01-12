// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_STRUCTURED_METRICS_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_STRUCTURED_METRICS_LOGGER_H_

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

class PhoneHubStructuredMetricsLogger {
 public:
  PhoneHubStructuredMetricsLogger();
  ~PhoneHubStructuredMetricsLogger();

  PhoneHubStructuredMetricsLogger(const PhoneHubStructuredMetricsLogger&) =
      delete;
  PhoneHubStructuredMetricsLogger& operator=(
      const PhoneHubStructuredMetricsLogger&) = delete;

  void LogPhoneHubDiscoveryStarted(DiscoveryEntryPoint entry_point);
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_STRUCTURED_METRICS_LOGGER_H_
