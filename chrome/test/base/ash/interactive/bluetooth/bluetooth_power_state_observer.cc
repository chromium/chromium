// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/bluetooth/bluetooth_power_state_observer.h"

#include "chrome/test/base/ash/interactive/bluetooth/bluetooth_util.h"

namespace ash {

std::unique_ptr<BluetoothPowerStateObserver>
BluetoothPowerStateObserver::Create() {
  return std::make_unique<BluetoothPowerStateObserver>(GetBluetoothAdapter());
}

BluetoothPowerStateObserver::BluetoothPowerStateObserver(
    scoped_refptr<device::BluetoothAdapter> adapter)
    : ObservationStateObserver(adapter.get()), adapter_(std::move(adapter)) {
  DCHECK(adapter_);
}

BluetoothPowerStateObserver::~BluetoothPowerStateObserver() = default;

bool BluetoothPowerStateObserver::GetStateObserverInitialState() const {
  return adapter_->IsPowered();
}

void BluetoothPowerStateObserver::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  OnStateObserverStateChanged(powered);
}

}  // namespace ash
