// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/adapter_state_controller_impl.h"

namespace chromeos {
namespace bluetooth_config {

AdapterStateControllerImpl::AdapterStateControllerImpl(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : bluetooth_adapter_(std::move(bluetooth_adapter)) {
  adapter_observation_.Observe(bluetooth_adapter_.get());
}

AdapterStateControllerImpl::~AdapterStateControllerImpl() = default;

mojom::BluetoothSystemState AdapterStateControllerImpl::GetAdapterState()
    const {
  if (!bluetooth_adapter_->IsPresent())
    return mojom::BluetoothSystemState::kUnavailable;

  return bluetooth_adapter_->IsPowered()
             ? mojom::BluetoothSystemState::kEnabled
             : mojom::BluetoothSystemState::kDisabled;
}

void AdapterStateControllerImpl::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  NotifyAdapterStateChanged();
}

void AdapterStateControllerImpl::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  NotifyAdapterStateChanged();
}

}  // namespace bluetooth_config
}  // namespace chromeos
