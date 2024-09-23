// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_BLUETOOTH_BLUETOOTH_POWER_STATE_OBSERVER_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_BLUETOOTH_BLUETOOTH_POWER_STATE_OBSERVER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "ui/base/interaction/state_observer.h"

namespace ash {

// This is a helper class that can be used in tests that use Kombucha to
// synchronously retrieve a Bluetooth adapter and observe its state.
class BluetoothPowerStateObserver : public ui::test::ObservationStateObserver<
                                        bool,
                                        device::BluetoothAdapter,
                                        device::BluetoothAdapter::Observer> {
 public:
  // Retrieving a Bluetooth adapter is an asynchronous operation. This function
  // will block the calling thread until it receives a Bluetooth adapter.
  static std::unique_ptr<BluetoothPowerStateObserver> Create();

  explicit BluetoothPowerStateObserver(
      scoped_refptr<device::BluetoothAdapter> adapter);
  ~BluetoothPowerStateObserver() override;

 private:
  // ui::test::ObservationStateObserver:
  bool GetStateObserverInitialState() const override;

  // device::BluetoothAdapter::Observer:
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  scoped_refptr<device::BluetoothAdapter> adapter_;
};

}  // namespace ash

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_BLUETOOTH_BLUETOOTH_POWER_STATE_OBSERVER_H_
