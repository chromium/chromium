// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_CHARACTERISTICS_FINDER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_CHARACTERISTICS_FINDER_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/secure_channel/remote_attribute.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace ash::secure_channel {

class BackgroundEidGenerator;

// Looks for given characteristics in a remote device, for which a GATT
// connection was already established. In the current BLE connection protocol
// (device::BluetoothDevice::CreateGattConnection), remote characteristic
// discovery starts immediatelly after a GATT connection was established. So,
// this class simply adds an observer for a characteristic discovery event and
// call |success_callback_| once all necessary characteristics were discovered.
class BluetoothLowEnergyCharacteristicsFinder
    : public device::BluetoothAdapter::Observer {
 public:
  // This callbacks takes as arguments (in this order): |remote_service_|,
  // |to_peripheral_char_| and |from_peripheral_char_|. Note that, since this is
  // called after the characteristics were discovered, their id field (e.g.
  // to_peripheral_char_.id) will be non-blank.
  typedef base::OnceCallback<void(const RemoteAttribute&,
                                  const RemoteAttribute&,
                                  const RemoteAttribute&)>
      SuccessCallback;

  // Constructs the object and registers itself as an observer for |adapter|,
  // waiting for |to_peripheral_char| and |from_peripheral_char| to be found.
  // When both characteristics were found |success_callback| is called. After
  // all characteristics of |service| were discovered, if |from_periphral_char|
  // or |to_peripheral| was not found, it calls |error_callback|. The object
  // will perform at most one call of the callbacks.
  //
  // Starts this operation by posting a task to |task_runner|.
  BluetoothLowEnergyCharacteristicsFinder(
      scoped_refptr<device::BluetoothAdapter> adapter,
      device::BluetoothDevice* device,
      const RemoteAttribute& remote_service,
      const RemoteAttribute& to_peripheral_char,
      const RemoteAttribute& from_peripheral_char,
      SuccessCallback success_callback,
      base::OnceClosure error_callback,
      const multidevice::RemoteDeviceRef& remote_device,
      std::unique_ptr<BackgroundEidGenerator> background_eid_generator,
      scoped_refptr<base::TaskRunner> task_runner =
          base::SingleThreadTaskRunner::GetCurrentDefault());

  BluetoothLowEnergyCharacteristicsFinder(
      const BluetoothLowEnergyCharacteristicsFinder&) = delete;
  BluetoothLowEnergyCharacteristicsFinder& operator=(
      const BluetoothLowEnergyCharacteristicsFinder&) = delete;

  ~BluetoothLowEnergyCharacteristicsFinder() override;

 protected:
  // device::BluetoothAdapter::Observer:
  void GattServicesDiscovered(device::BluetoothAdapter* adapter,
                              device::BluetoothDevice* device) override;

  // For testing. Used to mock this class.
  BluetoothLowEnergyCharacteristicsFinder(
      const multidevice::RemoteDeviceRef& remote_device);

 private:
  friend class SecureChannelBluetoothLowEnergyCharacteristicFinderTest;

  // Starts the process of finding GATT characteristics.
  void Start();

  // Scans the remote chracteristics of the service with |remote_service_.uuid|
  // in |device| and triggers the success or error callback.
  void ScanRemoteCharacteristics();

  // Sets proper identifiers on the service and characteristics and triggers the
  // |success_callback_|.
  void NotifySuccess(std::string service_id,
                     std::string tx_id,
                     std::string rx_id);

  // Triggers the |error_callback_| if there are no EID characteristic reads
  // pending.
  void NotifyFailureIfNoPendingEidCharReads();

  void TryToVerifyEid(device::BluetoothRemoteGattCharacteristic* eid_char);
  void OnRemoteCharacteristicRead(
      const std::string& service_id,
      std::optional<device::BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value);
  bool DoesEidMatchExpectedDevice(const std::vector<uint8_t>& eid_value_read);

  // The Bluetooth adapter where the connection was established.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // The Bluetooth device to which the connection was established.
  raw_ptr<device::BluetoothDevice, DanglingUntriaged> bluetooth_device_;

  // Remote service the |connection_| was established with.
  RemoteAttribute remote_service_;

  // Characteristic used to receive data from the remote device.
  RemoteAttribute to_peripheral_char_;

  // Characteristic used to receive data from the remote device.
  RemoteAttribute from_peripheral_char_;

  // Called when all characteristics were found.
  SuccessCallback success_callback_;

  // Keeps track of whether we have ever called either the success or error
  // callback.
  bool has_callback_been_invoked_ = false;

  // True once services have been discovered and parsed. Used to avoid
  // unnecessary work.
  bool have_services_been_parsed_ = false;

  // Called when there is an error.
  base::OnceClosure error_callback_;

  const multidevice::RemoteDeviceRef remote_device_;

  std::unique_ptr<BackgroundEidGenerator> background_eid_generator_;

  // A set of service IDs whose EID characteristics are being checked.
  base::flat_set<std::string> service_ids_pending_eid_read_;

  base::WeakPtrFactory<BluetoothLowEnergyCharacteristicsFinder>
      weak_ptr_factory_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_CHARACTERISTICS_FINDER_H_
