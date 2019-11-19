// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_SCANNER_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_SCANNER_IMPL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/secure_channel/ble_scanner.h"
#include "chromeos/services/secure_channel/ble_service_data_helper.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {
class BluetoothDevice;
class BluetoothDiscoverySession;
}  // namespace device

namespace chromeos {

namespace secure_channel {

class BleServiceDataHelper;
class BleSynchronizerBase;

// Concrete BleScanner implementation.
class BleScannerImpl : public BleScanner,
                       public device::BluetoothAdapter::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual std::unique_ptr<BleScanner> BuildInstance(
        Delegate* delegate,
        BleServiceDataHelper* service_data_helper,
        BleSynchronizerBase* ble_synchronizer,
        scoped_refptr<device::BluetoothAdapter> adapter);

   private:
    static Factory* test_factory_;
  };

  ~BleScannerImpl() override;

 private:
  friend class SecureChannelBleScannerImplTest;

  // Extracts the service data corresponding to the ProximityAuth service UUID.
  // This is encapsulated within a class because device::BluetoothDevice does
  // not provide a way to override this functionality for tests.
  class ServiceDataProvider {
   public:
    virtual ~ServiceDataProvider();
    virtual const std::vector<uint8_t>* ExtractProximityAuthServiceData(
        device::BluetoothDevice* bluetooth_device);
  };

  BleScannerImpl(Delegate* delegate,
                 BleServiceDataHelper* service_data_helper,
                 BleSynchronizerBase* ble_synchronizer,
                 scoped_refptr<device::BluetoothAdapter> adapter);

  // BleScanner:
  void HandleScanFilterChange() override;

  // device::BluetoothAdapter::Observer:
  void DeviceAdvertisementReceived(device::BluetoothAdapter* adapter,
                                   device::BluetoothDevice* device,
                                   int16_t rssi,
                                   const std::vector<uint8_t>& eir) override;

  void UpdateDiscoveryStatus();
  bool IsDiscoverySessionActive();
  void ResetDiscoverySessionIfNotActive();

  void EnsureDiscoverySessionActive();
  void OnDiscoverySessionStarted(
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session);
  void OnStartDiscoverySessionError();

  void EnsureDiscoverySessionNotActive();
  void OnDiscoverySessionStopped();
  void OnStopDiscoverySessionError();

  void HandleDeviceUpdated(device::BluetoothDevice* bluetooth_device);
  void HandlePotentialScanResult(
      const std::string& service_data,
      const BleServiceDataHelper::DeviceWithBackgroundBool& potential_result,
      device::BluetoothDevice* bluetooth_device);

  void SetServiceDataProviderForTesting(
      std::unique_ptr<ServiceDataProvider> service_data_provider);

  BleServiceDataHelper* service_data_helper_;
  BleSynchronizerBase* ble_synchronizer_;
  scoped_refptr<device::BluetoothAdapter> adapter_;

  std::unique_ptr<ServiceDataProvider> service_data_provider_;

  bool is_initializing_discovery_session_ = false;
  bool is_stopping_discovery_session_ = false;

  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;
  std::unique_ptr<base::WeakPtrFactory<device::BluetoothDiscoverySession>>
      discovery_session_weak_ptr_factory_;

  base::WeakPtrFactory<BleScannerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BleScannerImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_SCANNER_IMPL_H_
