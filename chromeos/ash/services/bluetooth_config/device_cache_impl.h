// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_CACHE_IMPL_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_CACHE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/device_cache.h"
#include "chromeos/ash/services/bluetooth_config/device_name_manager.h"
#include "chromeos/ash/services/bluetooth_config/fast_pair_delegate.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash::bluetooth_config {

// Concrete DeviceCache implementation. When this class is created, it uses
// BluetoothAdapter to fetch an initial list of devices; then, it observes
// BluetoothAdapter so that it can update the cache when devices are added,
// removed, or changed.
//
// Additionally, it uses AdapterStateController to ensure that no devices are
// returned unless Bluetooth is enabled or enabling.
class DeviceCacheImpl : public DeviceCache,
                        public AdapterStateController::Observer,
                        public device::BluetoothAdapter::Observer,
                        public DeviceNameManager::Observer {
 public:
  DeviceCacheImpl(AdapterStateController* adapter_state_controller,
                  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
                  DeviceNameManager* device_name_manager,
                  FastPairDelegate* fast_pair_delegate);
  ~DeviceCacheImpl() override;

 private:
  // Wrapper for unpaired BluetoothDevicePropertiesPtrs that contains the
  // device's inquiry_rssi. This is used for sorting unpaired devices based on
  // their signal strength.
  struct UnpairedDevice {
    UnpairedDevice(const device::BluetoothDevice* device,
                   FastPairDelegate* fast_pair_delegate);
    ~UnpairedDevice();

    mojom::BluetoothDevicePropertiesPtr device_properties;
    std::optional<int8_t> inquiry_rssi;
  };

  friend class DeviceCacheImplTest;

  // DeviceCache:
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
  PerformGetPairedDevices() const override;

  std::vector<mojom::BluetoothDevicePropertiesPtr> PerformGetUnpairedDevices()
      const override;

  // AdapterStateController::Observer:
  void OnAdapterStateChanged() override;

  // device::BluetoothAdapter::Observer:
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DevicePairedChanged(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device,
                           bool new_paired_status) override;
  void DeviceConnectedStateChanged(device::BluetoothAdapter* adapter,
                                   device::BluetoothDevice* device,
                                   bool is_now_connected) override;
  void DeviceBlockedByPolicyChanged(device::BluetoothAdapter* adapter,
                                    device::BluetoothDevice* device,
                                    bool new_blocked_status) override;
  void DeviceBatteryChanged(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device,
                            device::BluetoothDevice::BatteryType type) override;

  // DeviceNameManager::Observer:
  void OnDeviceNicknameChanged(
      const std::string& device_id,
      const std::optional<std::string>& nickname) override;

  // Fetches all known devices from BluetoothAdapter and populates them into
  // |paired_devices_| and |unpaired_devices_|.
  void FetchInitialDeviceLists();

  // Adds |device| to |paired_devices_|, but only if |device| is paired. If the
  // device was already present in the list, this function updates its metadata
  // to reflect up-to-date values. This function sorts the list after a new
  // element is inserted. Returns true if the device was added or updated in the
  // list.
  bool AttemptSetDeviceInPairedDeviceList(device::BluetoothDevice* device);

  // Removes |device| from |paired_devices_| if it exists in the list. Returns
  // true if the device was removed from the list.
  bool RemoveFromPairedDeviceList(device::BluetoothDevice* device);

  // Attempts to add updated metadata about |device| to |paired_devices_|. If
  // |device| is not found in |paired_devices_|, no update is performed. Returns
  // true if the device was updated in the list.
  bool AttemptUpdatePairedDeviceMetadata(device::BluetoothDevice* device);

  // Sorts |paired_devices_| based on connection state. This function is called
  // each time a device is added to the list. This is not particularly
  // efficient, but the list is expected to be small and is only sorted when its
  // contents change.
  void SortPairedDeviceList();

  // Adds |device| to |unpaired_devices_|, but only if |device| is unpaired. If
  // the device was already present in the list, this function updates its
  // metadata to reflect up-to-date values. This function sorts the list after a
  // new element is inserted. Returns true if the device was added or updated in
  // the list.
  bool AttemptSetDeviceInUnpairedDeviceList(device::BluetoothDevice* device);

  // Removes |device| from |unpaired_devices_| if it exists in the list. Returns
  // true if the device was removed from the list.
  bool RemoveFromUnpairedDeviceList(device::BluetoothDevice* device);

  // Attempts to add updated metadata about |device| to |paired_devices_|. If
  // |device| is not found in |unpaired_devices_|, it is added. Returns true if
  // the device was updated in the list.
  bool AttemptUpdateUnpairedDeviceMetadata(device::BluetoothDevice* device);

  // Sorts |unpaired_devices_| based on signal strength. This function is called
  // each time a device is added to the list. This is not particularly
  // efficient, but the list is expected to be small and is only sorted when its
  // contents change.
  void SortUnpairedDeviceList();

  mojom::PairedBluetoothDevicePropertiesPtr
  GeneratePairedBluetoothDeviceProperties(
      const device::BluetoothDevice* device);

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  raw_ptr<DeviceNameManager> device_name_manager_;
  raw_ptr<FastPairDelegate> fast_pair_delegate_;

  // Sorted by connection status.
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> paired_devices_;

  // Sorted by signal strength.
  std::vector<std::unique_ptr<UnpairedDevice>> unpaired_devices_;

  base::ScopedObservation<AdapterStateController,
                          AdapterStateController::Observer>
      adapter_state_controller_observation_{this};
  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};
  base::ScopedObservation<DeviceNameManager, DeviceNameManager::Observer>
      device_name_manager_observation_{this};
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_CACHE_IMPL_H_
