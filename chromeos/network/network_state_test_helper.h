// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_STATE_TEST_HELPER_H_
#define CHROMEOS_NETWORK_NETWORK_STATE_TEST_HELPER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"

namespace chromeos {

class NetworkStateHandler;

// Helper class for tests that use NetworkStateHandler. Handles initialization,
// shutdown, and adds default profiles and a wifi device (but no services).
// NOTE: This is not intended to be used with NetworkHandler::Initialize()
// which constructs its own NetworkStateHandler instance. When testing code that
// accesses NetworkHandler::Get() directly, use
// NetworkHandler::Get()->network_state_handler() directly instead.
class NetworkStateTestHelper {
 public:
  // If |use_default_devices_and_services| is false, the default devices and
  // services setup by the fake Shill handlers will be removed.
  explicit NetworkStateTestHelper(bool use_default_devices_and_services);
  ~NetworkStateTestHelper();

  // Call this before TearDown() to shut down NetworkStateHandler.
  void ShutdownNetworkState();

  // Resets the devices and services to the default (wifi device only).
  void ResetDevicesAndServices();

  // Clears any fake devices.
  void ClearDevices();

  // Clears any fake services.
  void ClearServices();

  // Calls ShillDeviceClient::TestInterface::AddDevice and sets update_received
  // on the DeviceState.
  void AddDevice(const std::string& device_path,
                 const std::string& type,
                 const std::string& name);

  // Configures a new service using Shill properties from |shill_json_string|
  // which must include a GUID and Type. Returns the service path, or "" if the
  // service could not be configured. Note: the 'GUID' key is also used as the
  // name of the service if no 'Name' key is provided.
  std::string ConfigureService(const std::string& shill_json_string);

  // Returns a string value for property |key| associated with |service_path|.
  // The result will be empty if the service or property do not exist.
  std::string GetServiceStringProperty(const std::string& service_path,
                                       const std::string& key);

  void SetServiceProperty(const std::string& service_path,
                          const std::string& key,
                          const base::Value& value);

  network_config::mojom::NetworkStatePropertiesPtr
  CreateStandaloneNetworkProperties(
      const std::string& id,
      network_config::mojom::NetworkType type,
      network_config::mojom::ConnectionStateType connection_state,
      int signal_strength);

  // Returns the path used for the shared and user profiles.
  const char* ProfilePathUser();

  // Returns the hash used for the user profile.
  const char* UserHash();

  NetworkStateHandler* network_state_handler() {
    return network_state_handler_.get();
  }

  ShillManagerClient::TestInterface* manager_test() { return manager_test_; }
  ShillProfileClient::TestInterface* profile_test() { return profile_test_; }
  ShillDeviceClient::TestInterface* device_test() { return device_test_; }
  ShillServiceClient::TestInterface* service_test() { return service_test_; }

 private:
  void ConfigureCallback(const dbus::ObjectPath& result);

  bool shill_clients_initialized_ = false;
  std::string last_created_service_path_;

  ShillManagerClient::TestInterface* manager_test_;
  ShillProfileClient::TestInterface* profile_test_;
  ShillDeviceClient::TestInterface* device_test_;
  ShillServiceClient::TestInterface* service_test_;

  std::unique_ptr<NetworkStateHandler> network_state_handler_;

  base::WeakPtrFactory<NetworkStateTestHelper> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_STATE_TEST_HELPER_H_
