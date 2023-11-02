// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_STATE_TEST_HELPER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_STATE_TEST_HELPER_H_

#include "chromeos/ash/components/network/network_test_helper_base.h"

namespace ash {

class NetworkDeviceHandler;
class NetworkStateHandler;

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

 private:
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos {
using ::ash::NetworkStateTestHelper;
}

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_STATE_TEST_HELPER_H_
