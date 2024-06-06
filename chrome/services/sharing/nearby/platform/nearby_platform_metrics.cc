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

void RecordGattServiceRegistrationResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.BleV2.GattServer.RegisterGattServices.Result",
      success);
}

void RecordGattServiceRegistrationErrorReason(
    device::BluetoothGattService::GattErrorCode error_code) {
  base::UmaHistogramEnumeration(
      "Nearby.Connections.BleV2.GattServer.RegisterGattService.FailureReason",
      error_code);
}

void RecordCreateLocalGattServiceResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.BleV2.GattServer.CreateLocalGattService.Result",
      success);
}

void RecordCreateLocalGattCharacteristicResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.BleV2.GattServer.CreateCharacteristic.Result",
      success);
}

void RecordUpdateCharacteristicResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.BleV2.GattServer.UpdateCharacteristic.Result",
      success);
}

void RecordOnLocalCharacteristicReadResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.BleV2.GattServer.OnLocalCharacteristicRead.Result",
      success);
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

void RecordConnectToRemoteGattServerResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.BleV2.ConnectToGattServer.Result", success);
}

void RecordConnectToRemoteGattServerFailureReason(
    bluetooth::mojom::ConnectResult failure_reason) {
  CHECK(failure_reason != bluetooth::mojom::ConnectResult::SUCCESS)
      << __func__ << ": Only failure reasons are expected in metric logging";
  base::UmaHistogramEnumeration(
      "Nearby.Connections.BleV2.ConnectToGattServer.FailureReason",
      failure_reason);
}

void RecordConnectToRemoteGattServerDuration(base::TimeDelta duration) {
  base::UmaHistogramTimes(
      "Nearby.Connections.BleV2.ConnectToGattServer.Duration", duration);
}

void RecordGattClientReadCharacteristicResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.BleV2.GattClient.ReadCharacteristic.Result", success);
}

void RecordGattClientReadCharacteristicFailureReason(
    bluetooth::mojom::GattResult failure_reason) {
  CHECK(failure_reason != bluetooth::mojom::GattResult::SUCCESS)
      << __func__ << ": Only failure reasons are expected in metric logging";
  base::UmaHistogramEnumeration(
      "Nearby.Connections.BleV2.GattClient.ReadCharacteristic.FailureReason",
      failure_reason);
}

void RecordGattClientReadCharacteristicDuration(base::TimeDelta duration) {
  base::UmaHistogramTimes(
      "Nearby.Connections.BleV2.GattClient.ReadCharacteristic.Duration",
      duration);
}

}  // namespace nearby::chrome::metrics
