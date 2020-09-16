// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLUETOOTH_HELPER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLUETOOTH_HELPER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/secure_channel/bluetooth_helper.h"
#include "chromeos/services/secure_channel/data_with_timestamp.h"

namespace chromeos {

namespace secure_channel {

// Test BluetoothHelper implementation.
class FakeBluetoothHelper : public BluetoothHelper {
 public:
  FakeBluetoothHelper();
  ~FakeBluetoothHelper() override;

  // Sets the data to be returned by a GenerateForegroundAdvertisement() call.
  void SetAdvertisement(const DeviceIdPair& device_id_pair,
                        const DataWithTimestamp& service_data);

  void RemoveAdvertisement(const DeviceIdPair& device_id_pair);

  // Sets the identified device to be returned by a IdentifyRemoteDevice() call.
  void SetIdentifiedDevice(const std::string& service_data,
                           multidevice::RemoteDeviceRef identified_device,
                           bool is_background_advertisement);

 private:
  // BluetoothHelper:
  std::unique_ptr<DataWithTimestamp> GenerateForegroundAdvertisement(
      const DeviceIdPair& device_id_pair) override;
  base::Optional<DeviceWithBackgroundBool> PerformIdentifyRemoteDevice(
      const std::string& service_data,
      const DeviceIdPairSet& device_id_pair_set) override;

  std::unordered_map<DeviceIdPair, DataWithTimestamp, DeviceIdPairHash>
      device_id_pair_to_service_data_map_;

  std::unordered_map<std::string, DeviceWithBackgroundBool>
      service_data_to_device_with_background_bool_map_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothHelper);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLUETOOTH_HELPER_H_
