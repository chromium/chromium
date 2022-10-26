// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_ADVERTISEMENT_GENERATOR_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_ADVERTISEMENT_GENERATOR_H_

#include <memory>

namespace ash {

namespace multidevice {
class RemoteDeviceRef;
}

namespace secure_channel {

class ForegroundEidGenerator;
struct DataWithTimestamp;

// Generates advertisements for the ProximityAuth BLE advertisement scheme.
class BleAdvertisementGenerator {
 public:
  // Generates an advertisement from the current device to |remote_device|. The
  // generated advertisement should be used immediately since it is based on the
  // current timestamp.
  static std::unique_ptr<DataWithTimestamp> GenerateBleAdvertisement(
      multidevice::RemoteDeviceRef remote_device,
      const std::string& local_device_public_key);

  BleAdvertisementGenerator(const BleAdvertisementGenerator&) = delete;
  BleAdvertisementGenerator& operator=(const BleAdvertisementGenerator&) =
      delete;

  virtual ~BleAdvertisementGenerator();

 protected:
  BleAdvertisementGenerator();

  virtual std::unique_ptr<DataWithTimestamp> GenerateBleAdvertisementInternal(
      multidevice::RemoteDeviceRef remote_device,
      const std::string& local_device_public_key);

 private:
  friend class SecureChannelBleAdvertisementGeneratorTest;
  friend class SecureChannelBluetoothHelperImplTest;

  static BleAdvertisementGenerator* instance_;

  // TODO(dcheng): Update this to follow the standard factory pattern.
  static void SetInstanceForTesting(BleAdvertisementGenerator* test_generator);

  void SetEidGeneratorForTesting(
      std::unique_ptr<ForegroundEidGenerator> test_eid_generator);

  std::unique_ptr<ForegroundEidGenerator> eid_generator_;
};

}  // namespace secure_channel
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_ADVERTISEMENT_GENERATOR_H_
