// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_SCOPED_BLUETOOTH_CONFIG_TEST_HELPER_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_SCOPED_BLUETOOTH_CONFIG_TEST_HELPER_H_

#include "chromeos/services/bluetooth_config/initializer.h"

#include "components/session_manager/core/session_manager.h"

namespace chromeos {
namespace bluetooth_config {

class FakeAdapterStateController;
class FakeDeviceCache;
class FakeDiscoverySessionManager;
class FakeDeviceOperationHandler;

// Test helper which provides access to fake implementations. This class
// automatically overrides CrosBluetoothConfig when created and reverses the
// override when it is deleted.
class ScopedBluetoothConfigTestHelper : public Initializer {
 public:
  ScopedBluetoothConfigTestHelper();
  ScopedBluetoothConfigTestHelper(const ScopedBluetoothConfigTestHelper&) =
      delete;
  ScopedBluetoothConfigTestHelper& operator=(
      const ScopedBluetoothConfigTestHelper&) = delete;
  ~ScopedBluetoothConfigTestHelper() override;

  FakeAdapterStateController* fake_adapter_state_controller() {
    return fake_adapter_state_controller_;
  }

  FakeDeviceCache* fake_device_cache() { return fake_device_cache_; }

  FakeDiscoverySessionManager* fake_discovery_session_manager() {
    return fake_discovery_session_manager_;
  }

  session_manager::SessionManager* session_manager() {
    return &session_manager_;
  }

 private:
  // Initializer:
  std::unique_ptr<AdapterStateController> CreateAdapterStateController(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) override;
  std::unique_ptr<DeviceCache> CreateDeviceCache(
      AdapterStateController* adapter_state_controller,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) override;
  std::unique_ptr<DiscoverySessionManager> CreateDiscoverySessionManager(
      AdapterStateController* adapter_state_controller,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      DeviceCache* device_cache) override;
  std::unique_ptr<DeviceOperationHandler> CreateDeviceOperationHandler(
      AdapterStateController* adapter_state_controller,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) override;

  FakeAdapterStateController* fake_adapter_state_controller_;
  FakeDeviceCache* fake_device_cache_;
  FakeDiscoverySessionManager* fake_discovery_session_manager_;
  FakeDeviceOperationHandler* fake_device_operation_handler_;
  session_manager::SessionManager session_manager_;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_SCOPED_BLUETOOTH_CONFIG_TEST_HELPER_H_
