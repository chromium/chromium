// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"

#include "chromeos/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/fake_device_cache.h"
#include "chromeos/services/bluetooth_config/in_process_instance.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace chromeos {
namespace bluetooth_config {

ScopedBluetoothConfigTestHelper::ScopedBluetoothConfigTestHelper() {
  OverrideInProcessInstanceForTesting(/*initializer=*/this);
}

ScopedBluetoothConfigTestHelper::~ScopedBluetoothConfigTestHelper() {
  OverrideInProcessInstanceForTesting(/*initializer=*/nullptr);
}

std::unique_ptr<AdapterStateController>
ScopedBluetoothConfigTestHelper::CreateAdapterStateController(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  auto fake_adapter_state_controller =
      std::make_unique<FakeAdapterStateController>();
  fake_adapter_state_controller_ = fake_adapter_state_controller.get();
  return fake_adapter_state_controller;
}

std::unique_ptr<DeviceCache> ScopedBluetoothConfigTestHelper::CreateDeviceCache(
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  auto fake_device_cache =
      std::make_unique<FakeDeviceCache>(adapter_state_controller);
  fake_device_cache_ = fake_device_cache.get();
  return fake_device_cache;
}

}  // namespace bluetooth_config
}  // namespace chromeos
