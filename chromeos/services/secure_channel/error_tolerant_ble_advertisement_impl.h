// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_ERROR_TOLERANT_BLE_ADVERTISEMENT_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_ERROR_TOLERANT_BLE_ADVERTISEMENT_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/secure_channel/device_id_pair.h"
#include "chromeos/services/secure_channel/error_tolerant_ble_advertisement.h"
#include "chromeos/services/secure_channel/foreground_eid_generator.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace chromeos {

namespace secure_channel {

class BleSynchronizerBase;

// Concrete ErrorTolerantBleAdvertisement implementation.
class ErrorTolerantBleAdvertisementImpl
    : public ErrorTolerantBleAdvertisement,
      public device::BluetoothAdvertisement::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<ErrorTolerantBleAdvertisement> BuildInstance(
        const DeviceIdPair& device_id_pair,
        std::unique_ptr<DataWithTimestamp> advertisement_data,
        BleSynchronizerBase* ble_synchronizer);

   private:
    static Factory* test_factory_;
  };

  ~ErrorTolerantBleAdvertisementImpl() override;

 private:
  friend class SecureChannelErrorTolerantBleAdvertisementImplTest;

  ErrorTolerantBleAdvertisementImpl(
      const DeviceIdPair& device_id_pair,
      std::unique_ptr<DataWithTimestamp> advertisement_data,
      BleSynchronizerBase* ble_synchronizer);

  // ErrorTolerantBleAdvertisement:
  void Stop(const base::Closure& callback) override;
  bool HasBeenStopped() override;

  // device::BluetoothAdvertisement::Observer
  void AdvertisementReleased(
      device::BluetoothAdvertisement* advertisement) override;

  void UpdateRegistrationStatus();
  void AttemptRegistration();
  void AttemptUnregistration();

  std::unique_ptr<device::BluetoothAdvertisement::UUIDList> CreateServiceUuids()
      const;

  std::unique_ptr<device::BluetoothAdvertisement::ServiceData>
  CreateServiceData() const;

  void OnAdvertisementRegistered(
      scoped_refptr<device::BluetoothAdvertisement> advertisement);
  void OnErrorRegisteringAdvertisement(
      device::BluetoothAdvertisement::ErrorCode error_code);
  void OnAdvertisementUnregistered();
  void OnErrorUnregisteringAdvertisement(
      device::BluetoothAdvertisement::ErrorCode error_code);

  const DataWithTimestamp& advertisement_data() const {
    return *advertisement_data_;
  }

  std::unique_ptr<DataWithTimestamp> advertisement_data_;
  BleSynchronizerBase* ble_synchronizer_;

  bool registration_in_progress_ = false;
  bool unregistration_in_progress_ = false;

  scoped_refptr<device::BluetoothAdvertisement> advertisement_;

  base::Closure stop_callback_;

  base::WeakPtrFactory<ErrorTolerantBleAdvertisementImpl> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(ErrorTolerantBleAdvertisementImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_ERROR_TOLERANT_BLE_ADVERTISEMENT_IMPL_H_
