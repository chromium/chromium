// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_STATE_TEST_HELPER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_STATE_TEST_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/network/network_test_helper_base.h"
#include "chromeos/ash/components/network/technology_state_controller.h"

namespace ash {

class NetworkDeviceHandler;
class NetworkStateHandler;
class TechnologyStateController;

// Fake TechnologyStateController::HotspotOperationDelegate implementation for
// tests.
class FakeHotspotOperationDelegate
    : public TechnologyStateController::HotspotOperationDelegate {
 public:
  FakeHotspotOperationDelegate(
      TechnologyStateController* technology_state_controller);
  ~FakeHotspotOperationDelegate() override;

  // TechnologyStateController::HotspotOperationDelegate:
  void PrepareEnableWifi(
      base::OnceCallback<void(bool prepare_success)> callback) override;

 private:
  raw_ptr<TechnologyStateController> technology_state_controller_ = nullptr;
};

// Helper class for tests that use NetworkStateHandler and/or
// NetworkDeviceHandler. Handles initialization and shutdown of Shill and Hermes
// DBus clients and handler classes and instantiates NetworkStateHandler and
// NetworkDeviceHandler instances.
//
// NOTE: This is not intended to be used with NetworkHandler::Initialize()
// which constructs its own NetworkStateHandler instance. When testing code that
// accesses NetworkHandler::Get() use NetworkTestHelper directly.
//
// TODO(khorimoto): Rename this class since it now deals with more than just
// NetworkStates.
class NetworkStateTestHelper : public NetworkTestHelperBase {
 public:
  // If |use_default_devices_and_services| is false, the default devices and
  // services setup by the fake Shill handlers will be removed.
  explicit NetworkStateTestHelper(bool use_default_devices_and_services);
  ~NetworkStateTestHelper();

  // Calls ShillDeviceClient::TestInterface::AddDevice and sets update_received
  // on the DeviceState.
  void AddDevice(const std::string& device_path,
                 const std::string& type,
                 const std::string& name);

  NetworkStateHandler* network_state_handler() {
    return network_state_handler_.get();
  }

  NetworkDeviceHandler* network_device_handler() {
    return network_device_handler_.get();
  }

  TechnologyStateController* technology_state_controller() {
    return technology_state_controller_.get();
  }

 private:
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<TechnologyStateController> technology_state_controller_;
  std::unique_ptr<FakeHotspotOperationDelegate>
      fake_hotspot_operation_delegate_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_STATE_TEST_HELPER_H_
