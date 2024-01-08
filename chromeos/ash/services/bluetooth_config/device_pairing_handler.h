// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_PAIRING_HANDLER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_PAIRING_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chromeos/ash/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::bluetooth_config {

// Handles requests to pair to a Bluetooth device. This handler can be reused to
// pair to more than one device. Only one device should be attempted to be
// paired to at a time.
//
// This class uses AdapterStateController to ensure that pairing cannot occur
// when Bluetooth is not enabled.
class DevicePairingHandler : public mojom::DevicePairingHandler,
                             public AdapterStateController::Observer {
 public:
  ~DevicePairingHandler() override;

 protected:
  DevicePairingHandler(
      mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
      AdapterStateController* adapter_state_controller);

  // Implementation-specific method that attempts to pair with the device with
  // id |device_id|.
  virtual void PerformPairDevice(const std::string& device_id) = 0;

  // Implementation-specific method that handles the pairing request finishing.
  virtual void PerformFinishCurrentPairingRequest(
      std::optional<device::ConnectionFailureReason> failure_reason,
      base::TimeDelta duration) = 0;

  // Implementation-specific method that cancels the current pairing attempt.
  virtual void CancelPairing() = 0;

  // mojom::DevicePairingHandler method callbacks.
  virtual void OnRequestPinCode(const std::string& pin_code) = 0;
  virtual void OnRequestPasskey(const std::string& passkey) = 0;
  virtual void OnConfirmPairing(bool confirmed) = 0;

  // Methods that notify |delegate_| of pairing related authorizations.
  void SendRequestPinCode();
  void SendRequestPasskey();
  void SendDisplayPinCode(const std::string& pin_code);
  void SendDisplayPasskey(uint32_t passkey);
  void SendKeysEntered(uint32_t entered);
  void SendConfirmPasskey(uint32_t passkey);
  void SendAuthorizePairing();

  // Invokes |pair_device_callback_| and resets this class' state to be ready
  // for another pairing request.
  void FinishCurrentPairingRequest(
      std::optional<device::ConnectionFailureReason> failure_reason);

  const std::string& current_pairing_device_id() const {
    return current_pairing_device_id_;
  }

 private:
  friend class DevicePairingHandlerImplTest;

  // mojom::DevicePairingHandler:
  void PairDevice(const std::string& device_id,
                  mojo::PendingRemote<mojom::DevicePairingDelegate> delegate,
                  PairDeviceCallback callback) override;

  // AdapterStateController::Observer:
  void OnAdapterStateChanged() override;

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

  raw_ptr<AdapterStateController> adapter_state_controller_;

  // Client callback set in PairDevice(), to be invoked once pairing has
  // finished.
  PairDeviceCallback pair_device_callback_;

  mojo::Receiver<mojom::DevicePairingHandler> receiver_{this};

  base::ScopedObservation<AdapterStateController,
                          AdapterStateController::Observer>
      adapter_state_controller_observation_{this};

  base::WeakPtrFactory<DevicePairingHandler> weak_ptr_factory_{this};
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_PAIRING_HANDLER_H_
