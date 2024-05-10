// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/nearby_platform_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace nearby::chrome::metrics {

void RecordGattServerScatternetDualRoleSupported(bool is_dual_role_supported) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.BleV2.ScatternetDualRoleSupported",
      is_dual_role_supported);
}

void RecordStartAdvertisingFailureReason(StartAdvertisingFailureReason reason,
                                         bool is_extended_advertisement) {
  // Record the overall StartAdvertising failure reason.
  base::UmaHistogramEnumeration(
      "Nearby.Connections.BleV2.StartAdvertising.FailureReason", reason);

  // Record the failure reason for the corresponding advertisement type.
  std::string suffix = is_extended_advertisement ? ".ExtendedAdvertisement"
                                                 : ".RegularAdvertisement";
  base::UmaHistogramEnumeration(
      "Nearby.Connections.BleV2.StartAdvertising.FailureReason" + suffix,
      reason);
}

void RecordStartAdvertisingResult(bool success,
                                  bool is_extended_advertisement) {
  // Record the overall StartAdvertising success rate.
  base::UmaHistogramBoolean("Nearby.Connections.BleV2.StartAdvertising.Result",
                            success);

  // Record the success rate for the corresponding advertisement type.
  std::string suffix = is_extended_advertisement ? ".ExtendedAdvertisement"
                                                 : ".RegularAdvertisement";
  base::UmaHistogramBoolean(
      "Nearby.Connections.BleV2.StartAdvertising.Result" + suffix, success);
}

void RecordStartScanningFailureReason(StartScanningFailureReason reason) {
  // Record the StartScanning failure reason.
  base::UmaHistogramEnumeration(
      "Nearby.Connections.BleV2.StartScanning.FailureReason", reason);
}
void RecordStartScanningResult(bool success) {
  // Record the StartScanning success rate.
  base::UmaHistogramBoolean("Nearby.Connections.BleV2.StartScanning.Result",
                            success);
}

}  // namespace nearby::chrome::metrics
