// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/network_health_provider.h"

#include "base/containers/contains.h"
#include "base/test/task_environment.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {
namespace diagnostics {
namespace {

// Values for fake devices and services.
constexpr char kEthServicePath[] = "/service/eth/0";
constexpr char kEthServiceName[] = "eth_service_name";
constexpr char kEthGuid[] = "eth_guid";
constexpr char kEthDevicePath[] = "/device/eth1";
constexpr char kEthName[] = "eth_name";
constexpr char kWifiDevicePath[] = "/device/wifi1";
constexpr char kWifiGuid[] = "wifi_guid";
constexpr char kWifiName[] = "wifi_name";
constexpr char kVPNGuid[] = "vpn_guid";
constexpr char kVPNName[] = "vpn_name";

}  // namespace

class NetworkHealthProviderTest : public testing::Test {
 public:
  NetworkHealthProviderTest() {}

  ~NetworkHealthProviderTest() override = default;

  void SetUp() override {
    // Wait until CrosNetworkConfigTestHelper is fully setup.
    task_environment_.RunUntilIdle();
  }

 protected:
  // Adds a Service to the Manager and Service stubs.
  void AddService(const std::string& service_path,
                  const std::string& guid,
                  const std::string& name,
                  const std::string& type,
                  const std::string& state,
                  bool visible) {
    cros_network_config_test_helper_.network_state_helper()
        .service_test()
        ->AddService(service_path, guid, name, type, state, visible);

    task_environment_.RunUntilIdle();
  }

  // Adds a device for testing.
  void AddDevice(const std::string& device_path,
                 const std::string& type,
                 const std::string& name) {
    cros_network_config_test_helper_.network_state_helper()
        .device_test()
        ->AddDevice(device_path, type, name);

    task_environment_.RunUntilIdle();
  }

  void ResetCrosNetworkConfigDevicesAndServices() {
    // Clear test devices and services and setup the default wifi device.
    cros_network_config_test_helper_.network_state_helper()
        .ResetDevicesAndServices();
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_test_helper_;
  NetworkHealthProvider network_health_provider_;
};

TEST_F(NetworkHealthProviderTest, ConnectedNetworkStoredInActiveList) {
  ResetCrosNetworkConfigDevicesAndServices();
  AddService(kWifiDevicePath, kWifiGuid, kWifiName, shill::kTypeWifi,
             shill::kStateOnline, true);

  const std::vector<std::string>& network_guid_list =
      network_health_provider_.GetNetworkGuidListForTesting();
  ASSERT_EQ(1u, network_guid_list.size());
  ASSERT_EQ(kWifiGuid, network_guid_list[0]);
}

TEST_F(NetworkHealthProviderTest, MultipleConnectedNetworksStoredInActiveList) {
  ResetCrosNetworkConfigDevicesAndServices();
  AddService(kWifiDevicePath, kWifiGuid, kWifiName, shill::kTypeWifi,
             shill::kStateOnline, true);
  AddDevice(kEthDevicePath, shill::kTypeEthernet, kEthName);
  AddService(kEthServicePath, kEthGuid, kEthServiceName, shill::kTypeEthernet,
             shill::kStateReady, true);

  const std::vector<std::string>& network_guid_list =
      network_health_provider_.GetNetworkGuidListForTesting();
  ASSERT_EQ(2u, network_guid_list.size());
  ASSERT_TRUE(base::Contains(network_guid_list, kWifiGuid));
  ASSERT_TRUE(base::Contains(network_guid_list, kEthGuid));
}

TEST_F(NetworkHealthProviderTest, UnsupportedNetworkTypeIgnored) {
  ResetCrosNetworkConfigDevicesAndServices();
  AddService(kWifiDevicePath, kVPNGuid, kVPNName, shill::kTypeVPN,
             shill::kStateOffline, true);

  task_environment_.RunUntilIdle();

  const std::vector<std::string>& network_guid_list =
      network_health_provider_.GetNetworkGuidListForTesting();
  ASSERT_TRUE(network_guid_list.empty());
}

}  // namespace diagnostics
}  // namespace chromeos
