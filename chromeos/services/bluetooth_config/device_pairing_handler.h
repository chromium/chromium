// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_PAIRING_HANDLER_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_PAIRING_HANDLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace bluetooth_config {

// Handles requests to pair to a Bluetooth device, serving as the device's
// PairingDelegate. This class relays the PairingDelegate method calls back to
// the client that initiated the pairing request via the request's
// DevicePairingDelegate. This handler can be reused to pair to more than one
// device. Only one device should be attempted to be paired to at a time.
//
// This class uses AdapterStateController to ensure that pairing cannot occur
// when Bluetooth is not enabled.
class DevicePairingHandler : public mojom::DevicePairingHandler,
                             public device::BluetoothDevice::PairingDelegate,
                             public AdapterStateController::Observer {
 public:
  ~DevicePairingHandler() override;

 protected:
  DevicePairingHandler(
      mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
      AdapterStateController* adapter_state_controller,
      base::OnceClosure finished_pairing_callback);

  // Implementation-specific method that finds a BluetoothDevice* based on
  // device_id. If no device is found, nullptr is returned.
  virtual device::BluetoothDevice* FindDevice(
      const std::string& device_id) const = 0;

  // Cancels the pairing attempt occurring with the device with identifier
  // |current_pairing_device_id_| if it exists. Cancelling an active pairing
  // attempt will cause OnDeviceConnect() to fire with an error code.
  void CancelPairing();

  // Calls the finished_pairing_callback_ to indicate that this class should no
  // longer handle pairing requests. This is called at most once.
  void NotifyFinished();

  const std::string& current_pairing_device_id() const {
    return current_pairing_device_id_;
  }

 private:
  friend class DevicePairingHandlerImplTest;

  // mojom::DevicePairingHandler:
  void PairDevice(const std::string& device_id,
                  mojo::PendingRemote<mojom::DevicePairingDelegate> delegate,
                  PairDeviceCallback callback) override;

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

  // AdapterStateController::Observer:
  void OnAdapterStateChanged() override;

  // device::BluetoothDevice::Connect() callback.
  void OnDeviceConnect(
      absl::optional<device::BluetoothDevice::ConnectErrorCode> error_code);

  // mojom::DevicePairingHandler method callbacks.
  void OnRequestPinCode(const std::string& pin_code);
  void OnRequestPasskey(const std::string& passkey);
  void OnConfirmPairing(bool confirmed);

  // Invokes |pair_device_callback_| and resets this class' state to be ready
  // for another pairing request.
  void FinishCurrentPairingRequest(
      absl::optional<device::ConnectionFailureReason> failure_reason);

  void OnDelegateDisconnect();

  bool IsBluetoothEnabled() const;

  // Flushes queued Mojo messages in unit tests.
  void FlushForTesting();

  base::Time pairing_start_timestamp_;

  // The identifier of the device currently being paired with. This is null if
  // there is no in-progress pairing attempt.
  std::string current_pairing_device_id_;

  mojo::Remote<mojom::DevicePairingDelegate> delegate_;
  mojo::Remote<mojom::KeyEnteredHandler> key_entered_handler_;

  AdapterStateController* adapter_state_controller_;

  // Client callback set in PairDevice(), to be invoked once pairing has
  // finished.
  PairDeviceCallback pair_device_callback_;

  // Callback invoked that indicates this class should no longer handle any more
  // pairing attempts. This is the case if:
  // 1) The delegate disconnects
  // 2) Pairing is successful
  // 3) The handler is deleted
  // If pairing is unsuccessful, this callback won't be invoked because this
  // handler can still be reused for another pairing attempt.
  base::OnceClosure finished_pairing_callback_;

  mojo::Receiver<mojom::DevicePairingHandler> receiver_{this};

  base::ScopedObservation<AdapterStateController,
                          AdapterStateController::Observer>
      adapter_state_controller_observation_{this};

  base::WeakPtrFactory<DevicePairingHandler> weak_ptr_factory_{this};
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_PAIRING_HANDLER_H_
