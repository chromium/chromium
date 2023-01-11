// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fake_device_pairing_delegate.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"

namespace ash::bluetooth_config {

FakeDevicePairingDelegate::FakeDevicePairingDelegate() = default;

FakeDevicePairingDelegate::~FakeDevicePairingDelegate() = default;

mojo::PendingRemote<mojom::DevicePairingDelegate>
FakeDevicePairingDelegate::GeneratePendingRemote() {
  auto pending_remote = receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(base::BindOnce(
      &FakeDevicePairingDelegate::OnDisconnected, base::Unretained(this)));
  return pending_remote;
}

void FakeDevicePairingDelegate::DisconnectMojoPipe() {
  receiver_.reset();

  // Allow the disconnection to propagate.
  base::RunLoop().RunUntilIdle();
}

bool FakeDevicePairingDelegate::IsMojoPipeConnected() const {
  return receiver_.is_bound();
}

void FakeDevicePairingDelegate::RequestPinCode(
    RequestPinCodeCallback callback) {
  request_pin_code_callback_ = std::move(callback);
}

void FakeDevicePairingDelegate::RequestPasskey(
    RequestPasskeyCallback callback) {
  request_passkey_callback_ = std::move(callback);
}

void FakeDevicePairingDelegate::DisplayPinCode(
    const std::string& pin_code,
    mojo::PendingReceiver<mojom::KeyEnteredHandler> handler) {
  displayed_pin_code_ = pin_code;
  key_entered_handler_ =
      std::make_unique<FakeKeyEnteredHandler>(std::move(handler));
}

void FakeDevicePairingDelegate::DisplayPasskey(
    const std::string& passkey,
    mojo::PendingReceiver<mojom::KeyEnteredHandler> handler) {
  displayed_passkey_ = passkey;
  key_entered_handler_ =
      std::make_unique<FakeKeyEnteredHandler>(std::move(handler));
}

void FakeDevicePairingDelegate::ConfirmPasskey(
    const std::string& passkey,
    ConfirmPasskeyCallback callback) {
  passkey_to_confirm_ = passkey;
  confirm_passkey_callback_ = std::move(callback);
}

void FakeDevicePairingDelegate::AuthorizePairing(
    AuthorizePairingCallback callback) {
  authorize_pairing_callback_ = std::move(callback);
}

void FakeDevicePairingDelegate::OnDisconnected() {
  receiver_.reset();
}

bool FakeDevicePairingDelegate::HasPendingRequestPinCodeCallback() const {
  return !request_pin_code_callback_.is_null();
}

void FakeDevicePairingDelegate::InvokePendingRequestPinCodeCallback(
    const std::string& pin_code) {
  std::move(request_pin_code_callback_).Run(std::move(pin_code));
  base::RunLoop().RunUntilIdle();
}

bool FakeDevicePairingDelegate::HasPendingRequestPasskeyCallback() const {
  return !request_passkey_callback_.is_null();
}

void FakeDevicePairingDelegate::InvokePendingRequestPasskeyCallback(
    const std::string& passkey) {
  std::move(request_passkey_callback_).Run(std::move(passkey));
  base::RunLoop().RunUntilIdle();
}

bool FakeDevicePairingDelegate::HasPendingConfirmPasskeyCallback() const {
  return !confirm_passkey_callback_.is_null();
}

void FakeDevicePairingDelegate::InvokePendingConfirmPasskeyCallback(
    bool confirmed) {
  std::move(confirm_passkey_callback_).Run(std::move(confirmed));
  base::RunLoop().RunUntilIdle();
}

bool FakeDevicePairingDelegate::HasPendingAuthorizePairingCallback() const {
  return !authorize_pairing_callback_.is_null();
}

void FakeDevicePairingDelegate::InvokePendingAuthorizePairingCallback(
    bool confirmed) {
  std::move(authorize_pairing_callback_).Run(std::move(confirmed));
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash::bluetooth_config
