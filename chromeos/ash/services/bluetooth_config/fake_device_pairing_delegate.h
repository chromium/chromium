// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_PAIRING_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_PAIRING_DELEGATE_H_

#include "chromeos/ash/services/bluetooth_config/fake_key_entered_handler.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::bluetooth_config {

class FakeDevicePairingDelegate : public mojom::DevicePairingDelegate {
 public:
  FakeDevicePairingDelegate();
  ~FakeDevicePairingDelegate() override;

  // Generates a PendingRemote associated with this object. To disconnect the
  // associated Mojo pipe, use DisconnectMojoPipe().
  mojo::PendingRemote<mojom::DevicePairingDelegate> GeneratePendingRemote();

  // Disconnects the Mojo pipe associated with a PendingRemote returned by
  // GeneratePendingRemote().
  void DisconnectMojoPipe();

  bool IsMojoPipeConnected() const;

  bool HasPendingRequestPinCodeCallback() const;
  void InvokePendingRequestPinCodeCallback(const std::string& pin_code);

  bool HasPendingRequestPasskeyCallback() const;
  void InvokePendingRequestPasskeyCallback(const std::string& passkey);

  bool HasPendingConfirmPasskeyCallback() const;
  void InvokePendingConfirmPasskeyCallback(bool confirmed);

  bool HasPendingAuthorizePairingCallback() const;
  void InvokePendingAuthorizePairingCallback(bool confirmed);

  const std::string& displayed_pin_code() const { return displayed_pin_code_; }
  const std::string& displayed_passkey() const { return displayed_passkey_; }
  const std::string& passkey_to_confirm() const { return passkey_to_confirm_; }
  FakeKeyEnteredHandler* key_entered_handler() const {
    return key_entered_handler_.get();
  }

 private:
  // mojom::DevicePairingDelegate:
  void RequestPinCode(RequestPinCodeCallback callback) override;
  void RequestPasskey(RequestPasskeyCallback callback) override;
  void DisplayPinCode(
      const std::string& pin_code,
      mojo::PendingReceiver<mojom::KeyEnteredHandler> handler) override;
  void DisplayPasskey(
      const std::string& passkey,
      mojo::PendingReceiver<mojom::KeyEnteredHandler> handler) override;
  void ConfirmPasskey(const std::string& passkey,
                      ConfirmPasskeyCallback callback) override;
  void AuthorizePairing(AuthorizePairingCallback callback) override;

  void OnDisconnected();

  RequestPinCodeCallback request_pin_code_callback_;
  RequestPasskeyCallback request_passkey_callback_;
  ConfirmPasskeyCallback confirm_passkey_callback_;
  AuthorizePairingCallback authorize_pairing_callback_;

  std::string displayed_pin_code_;
  std::string displayed_passkey_;
  std::string passkey_to_confirm_;
  std::unique_ptr<FakeKeyEnteredHandler> key_entered_handler_;

  mojo::Receiver<mojom::DevicePairingDelegate> receiver_{this};
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_PAIRING_DELEGATE_H_
