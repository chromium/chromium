// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_PAIRING_HANDLER_IMPL_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_PAIRING_HANDLER_IMPL_H_

#include "chromeos/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/device_pairing_handler.h"
#include "chromeos/services/bluetooth_config/fast_pair_delegate.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace chromeos {
namespace bluetooth_config {

// Concrete DevicePairingHandler implementation. When a pair request is
// received, it finds the device in BluetoothAdapter's list of devices.
class DevicePairingHandlerImpl : public DevicePairingHandler {
 public:
  class Factory {
   public:
    static std::unique_ptr<DevicePairingHandler> Create(
        mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
        AdapterStateController* adapter_state_controller,
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
        FastPairDelegate* fast_pair_delegate,
        base::OnceClosure finished_pairing_callback);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<DevicePairingHandler> CreateInstance(
        mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
        AdapterStateController* adapter_state_controller,
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
        FastPairDelegate* fast_pair_delegate,
        base::OnceClosure finished_pairing_callback) = 0;
  };

  DevicePairingHandlerImpl(
      mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
      AdapterStateController* adapter_state_controller,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      FastPairDelegate* fast_pair_delegate,
      base::OnceClosure finished_pairing_callback);
  ~DevicePairingHandlerImpl() override;

 private:
  // DevicePairingHandler:
  void FetchDevice(const std::string& device_address,
                   FetchDeviceCallback callback) override;
  device::BluetoothDevice* FindDevice(
      const std::string& device_id) const override;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  FastPairDelegate* fast_pair_delegate_;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_PAIRING_HANDLER_IMPL_H_
