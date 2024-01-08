// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_PAIRING_HANDLER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_PAIRING_HANDLER_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/device_pairing_handler.h"
#include "chromeos/ash/services/bluetooth_config/fast_pair_delegate.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash::bluetooth_config {

// Concrete DevicePairingHandler implementation. Handles requests to pair to a
// Bluetooth device, serving as the device's PairingDelegate. This class relays
// the PairingDelegate method calls back to the client that initiated the
// pairing request via the request's DevicePairingDelegate.  When a pair request
// is received, it finds the device in BluetoothAdapter's list of devices.
class DevicePairingHandlerImpl
    : public DevicePairingHandler,
      public device::BluetoothDevice::PairingDelegate {
 public:
  class Factory {
   public:
    static std::unique_ptr<DevicePairingHandler> Create(
        mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
        AdapterStateController* adapter_state_controller,
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
        FastPairDelegate* fast_pair_delegate);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<DevicePairingHandler> CreateInstance(
        mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
        AdapterStateController* adapter_state_controller,
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
        FastPairDelegate* fast_pair_delegate) = 0;
  };

  DevicePairingHandlerImpl(
      mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
      AdapterStateController* adapter_state_controller,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      FastPairDelegate* fast_pair_delegate);
  ~DevicePairingHandlerImpl() override;

 private:
  friend class DevicePairingHandlerImplTest;

  // The delay between when a pairing has failed and the failure is processed.
  static const base::TimeDelta kPairingFailureDelay;

  // DevicePairingHandler:
  void FetchDevice(const std::string& device_address,
                   FetchDeviceCallback callback) override;
  void PerformPairDevice(const std::string& device_id) override;
  void PerformFinishCurrentPairingRequest(
      std::optional<device::ConnectionFailureReason> failure_reason,
      base::TimeDelta duration) override;
  void CancelPairing() override;
  void OnRequestPinCode(const std::string& pin_code) override;
  void OnRequestPasskey(const std::string& passkey) override;
  void OnConfirmPairing(bool confirmed) override;

  // device::BluetoothDevice::PairingDelegate:
  void RequestPinCode(device::BluetoothDevice* device) override;
  void RequestPasskey(device::BluetoothDevice* device) override;
  void DisplayPinCode(device::BluetoothDevice* device,
                      const std::string& pin_code) override;
  void DisplayPasskey(device::BluetoothDevice* device,
                      uint32_t passkey) override;
  void KeysEntered(device::BluetoothDevice* device, uint32_t entered) override;
  void ConfirmPasskey(device::BluetoothDevice* device,
                      uint32_t passkey) override;
  void AuthorizePairing(device::BluetoothDevice* device) override;

  // device::BluetoothDevice::Connect() callback.
  void OnDeviceConnect(
      std::optional<device::BluetoothDevice::ConnectErrorCode> error_code);
  void HandlePairingFailed(
      device::BluetoothDevice::ConnectErrorCode error_code);

  // Finds a BluetoothDevice* based on device_id. If no device is found, nullptr
  // is returned.
  device::BluetoothDevice* FindDevice(const std::string& device_id) const;

  // If true, indicates CancelPairing() was called.
  bool is_canceling_pairing_ = false;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  raw_ptr<FastPairDelegate> fast_pair_delegate_;

  base::WeakPtrFactory<DevicePairingHandlerImpl> weak_ptr_factory_{this};
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_PAIRING_HANDLER_IMPL_H_
