// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_TEST_HELPER_BASE_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_TEST_HELPER_BASE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"

namespace ash {

// Base Network test helper class. This base class handles initialization and
// shutdown of Shill and Hermes DBus clients and provides utility methods to
// setup various network conditions. Initialization is done on construction and
// shutdown on destruction. This helper also provides utility methods for
// setting up various test network conditions.
//
// NOTE: For tests that use NetworkHandler::Get() directly, use
// NetworkHandlerTestHelper. For tests that only need NetworkStateHandler
// and or NetworkDeviceHandler to be initialized, use NetworkStateTestHelper.
class NetworkTestHelperBase {
 public:
  ~NetworkTestHelperBase();

  // Adds default shared profile and user profile in Shill profile client.
  void AddDefaultProfiles();

  // Resets the devices and services to the default (wifi device only).
  void ResetDevicesAndServices();

  // Clears any fake devices.
  void ClearDevices();

  // Clears any fake services.
  void ClearServices();

  // Clears the profile list.
  void ClearProfiles();

  // Configures a new service using Shill properties from |shill_json_string|
  // which must include a GUID and Type. Returns the service path, or "" if the
  // service could not be configured. Note: the 'GUID' key is also used as the
  // name of the service if no 'Name' key is provided.
  std::string ConfigureService(const std::string& shill_json_string);

  // Configures a new WiFi service with state |state|. Returns the service
  // path of the new service.
  std::string ConfigureWiFi(const std::string& state);

  // Returns a double value for property |key| associated with |service_path|.
  std::optional<double> GetServiceDoubleProperty(
      const std::string& service_path,
      const std::string& key);

  // Returns a string value for property |key| associated with |service_path|.
  // The result will be empty if the service or property do not exist.
  std::string GetServiceStringProperty(const std::string& service_path,
                                       const std::string& key);

  // Returns a base::Value::List for property |key| associated with
  // |service_path|.
  std::optional<base::Value::List> GetServiceListProperty(
      const std::string& service_path,
      const std::string& key);

  void SetServiceProperty(const std::string& service_path,
                          const std::string& key,
                          const base::Value& value);

  std::string GetProfileStringProperty(const std::string& profile_path,
                                       const std::string& key);

  void SetProfileProperty(const std::string& profile_path,
                          const std::string& key,
                          const base::Value& value);

  // Returns the path used for the shared and user profiles.
  const char* ProfilePathUser();

  // Returns the hash used for the user profile.
  const char* UserHash();

  ShillManagerClient::TestInterface* manager_test() { return manager_test_; }
  ShillProfileClient::TestInterface* profile_test() { return profile_test_; }
  ShillDeviceClient::TestInterface* device_test() { return device_test_; }
  ShillServiceClient::TestInterface* service_test() { return service_test_; }
  ShillIPConfigClient::TestInterface* ip_config_test() {
    return ip_config_test_;
  }

  HermesEuiccClient::TestInterface* hermes_euicc_test() {
    return hermes_euicc_test_;
  }
  HermesManagerClient::TestInterface* hermes_manager_test() {
    return hermes_manager_test_;
  }
  HermesProfileClient::TestInterface* hermes_profile_test() {
    return hermes_profile_test_;
  }

 protected:
  NetworkTestHelperBase();

 private:
  void ConfigureCallback(const dbus::ObjectPath& result);

  bool shill_clients_initialized_ = false;
  bool hermes_clients_initialized_ = false;

  std::string last_created_service_path_;
  int wifi_index_ = 0;

  raw_ptr<ShillManagerClient::TestInterface, DanglingUntriaged> manager_test_;
  raw_ptr<ShillProfileClient::TestInterface, DanglingUntriaged> profile_test_;
  raw_ptr<ShillDeviceClient::TestInterface, DanglingUntriaged> device_test_;
  raw_ptr<ShillServiceClient::TestInterface, DanglingUntriaged> service_test_;
  raw_ptr<ShillIPConfigClient::TestInterface, DanglingUntriaged>
      ip_config_test_;

  raw_ptr<HermesEuiccClient::TestInterface, DanglingUntriaged>
      hermes_euicc_test_;
  raw_ptr<HermesManagerClient::TestInterface, DanglingUntriaged>
      hermes_manager_test_;
  raw_ptr<HermesProfileClient::TestInterface, DanglingUntriaged>
      hermes_profile_test_;

  base::WeakPtrFactory<NetworkTestHelperBase> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_TEST_HELPER_BASE_H_
