// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_NEARBY_PLATFORM_METRICS_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_NEARBY_PLATFORM_METRICS_H_

#include "device/bluetooth/bluetooth_gatt_service.h"

namespace nearby::chrome::metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated. This enum should be kept in sync with the
// NearbyConnectionsStartAdvertisingFailureReason enum in
// //tools/metrics/histograms/metadata/nearby/enums.xml.
enum class StartAdvertisingFailureReason {
  kUnknown = 0,
  kNoExtendedAdvertisementSupport = 1,
  kAdapterRegisterAdvertisementFailed = 2,
  kFailedToRegisterGattServices = 3,
  kMaxValue = kFailedToRegisterGattServices,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated. This enum should be kept in sync with the
// NearbyConnectionsStartScanningFailureReason enum in
// //tools/metrics/histograms/metadata/nearby/enums.xml.
enum class StartScanningFailureReason {
  kUnknown = 0,
  kAdapterObserverationFailed = 1,
  kStartDiscoverySessionFailed = 2,
  kMaxValue = kStartDiscoverySessionFailed,
};

void RecordGattServerScatternetDualRoleSupported(bool is_dual_role_supported);
void RecordGattServiceRegistrationResult(bool success);
void RecordGattServiceRegistrationErrorReason(
    device::BluetoothGattService::GattErrorCode error_code);
void RecordCreateLocalGattServiceResult(bool success);
void RecordStartAdvertisingFailureReason(StartAdvertisingFailureReason reason,
                                         bool is_extended_advertisement);
void RecordStartAdvertisingResult(bool success, bool is_extended_advertisement);
void RecordStartScanningFailureReason(StartScanningFailureReason reason);
void RecordStartScanningResult(bool success);

}  // namespace nearby::chrome::metrics

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_NEARBY_PLATFORM_METRICS_H_
