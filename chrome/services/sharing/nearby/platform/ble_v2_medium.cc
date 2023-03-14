// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_medium.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/services/sharing/nearby/platform/ble_v2_peripheral.h"
#include "third_party/nearby/src/internal/platform/implementation/ble_v2.h"

namespace nearby::chrome {

BleV2Medium::BleV2Medium() {
  LOG(WARNING) << "BleV2Medium default constructor not implemented yet.";
}

BleV2Medium::~BleV2Medium() {
  NOTIMPLEMENTED();
}

bool BleV2Medium::StartAdvertising(
    const api::ble_v2::BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertise_set_parameters) {
  NOTIMPLEMENTED();
  return false;
}

std::unique_ptr<BleV2Medium::AdvertisingSession> BleV2Medium::StartAdvertising(
    const api::ble_v2::BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertise_set_parameters,
    BleV2Medium::AdvertisingCallback callback) {
  NOTIMPLEMENTED();
  return nullptr;
}

bool BleV2Medium::StopAdvertising() {
  NOTIMPLEMENTED();
  return false;
}

bool BleV2Medium::StartScanning(const Uuid& service_uuid,
                                api::ble_v2::TxPowerLevel tx_power_level,
                                BleV2Medium::ScanCallback callback) {
  NOTIMPLEMENTED();
  return false;
}

bool BleV2Medium::StopScanning() {
  NOTIMPLEMENTED();
  return false;
}

// Fake impl to return hard coded advertisement.
std::unique_ptr<BleV2Medium::ScanningSession> BleV2Medium::StartScanning(
    const Uuid& service_uuid,
    api::ble_v2::TxPowerLevel tx_power_level,
    BleV2Medium::ScanningCallback callback) {
  LOG(WARNING) << "Ble StartScanning";

  base::ThreadPool::CreateSequencedTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BleV2Medium::SimulateAdvertisementFound,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  return std::make_unique<BleV2Medium::ScanningSession>(
      BleV2Medium::ScanningSession{
          .stop_scanning = []() { return absl::OkStatus(); },
      });
}

std::unique_ptr<api::ble_v2::GattServer> BleV2Medium::StartGattServer(
    api::ble_v2::ServerGattConnectionCallback callback) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<api::ble_v2::GattClient> BleV2Medium::ConnectToGattServer(
    api::ble_v2::BlePeripheral& peripheral,
    api::ble_v2::TxPowerLevel tx_power_level,
    api::ble_v2::ClientGattConnectionCallback callback) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<api::ble_v2::BleServerSocket> BleV2Medium::OpenServerSocket(
    const std::string& service_id) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<api::ble_v2::BleSocket> BleV2Medium::Connect(
    const std::string& service_id,
    api::ble_v2::TxPowerLevel tx_power_level,
    api::ble_v2::BlePeripheral& peripheral,
    CancellationFlag* cancellation_flag) {
  NOTIMPLEMENTED();
  return nullptr;
}

bool BleV2Medium::IsExtendedAdvertisementsAvailable() {
  NOTIMPLEMENTED();
  return false;
}

// Simulating start scan then return a peripheral.
void BleV2Medium::SimulateAdvertisementFound(
    BleV2Medium::ScanningCallback callback) {
  LOG(WARNING) << "Simulating Scanning";
  callback.start_scanning_result(absl::OkStatus());

  discovered_ble_peripherals_map_.emplace("invalid_address", BleV2Peripheral{});
  callback.advertisement_found_cb(
      discovered_ble_peripherals_map_.begin()->second,
      api::ble_v2::BleAdvertisementData{});
}

}  // namespace nearby::chrome
