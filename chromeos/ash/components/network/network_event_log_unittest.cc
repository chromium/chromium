// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_event_log.h"

#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

class NetworkEventLogTest : public testing::Test {
 public:
  void SetUp() override {
    SetupDefaultShillState();
    base::RunLoop().RunUntilIdle();
  }

  const NetworkState* GetNetworkState(const std::string& service_path) {
    return NetworkHandler::Get()->network_state_handler()->GetNetworkState(
        service_path);
  }

 private:
  void SetupDefaultShillState() {
    // Make sure any observer calls complete before clearing devices and
    // services.
    base::RunLoop().RunUntilIdle();
    auto* device_test = ShillDeviceClient::Get()->GetTestInterface();
    device_test->ClearDevices();
    device_test->AddDevice("/device/eth0", shill::kTypeEthernet,
                           "stub_eth_device");
    device_test->AddDevice("/device/wlan0", shill::kTypeWifi,
                           "stub_wifi_device");
    device_test->AddDevice("/device/cellular0", shill::kTypeCellular,
                           "stub_cellular_device");

    auto* service_test = ShillServiceClient::Get()->GetTestInterface();
    service_test->ClearServices();
    const bool add_to_visible = true;

    // Ethernet
    service_test->AddService("/service/0", "ethernet_guid", "ethernet",
                             shill::kTypeEthernet, shill::kStateOnline,
                             add_to_visible);

    // Cellular
    service_test->AddService("/service/1", "cellular_guid", "cellular1",
                             shill::kTypeCellular, shill::kStateIdle,
                             add_to_visible);
    service_test->SetServiceProperty("/service/1",
                                     shill::kNetworkTechnologyProperty,
                                     base::Value(shill::kNetworkTechnologyLte));

    // WiFi
    service_test->AddService("/service/2", "wifi2_guid", "wifi2",
                             shill::kTypeWifi, shill::kStateIdle,
                             add_to_visible);
    service_test->SetServiceProperty("/service/2",
                                     shill::kSecurityClassProperty,
                                     base::Value(shill::kSecurityClassNone));

    service_test->AddService("/service/3", "wifi3_guid", "wifi3",
                             shill::kTypeWifi, shill::kStateIdle,
                             add_to_visible);
    service_test->SetServiceProperty("/service/3",
                                     shill::kSecurityClassProperty,
                                     base::Value(shill::kSecurityClassWep));

    service_test->AddService("/service/4", "wifi4_guid", "wifi4",
                             shill::kTypeWifi, shill::kStateIdle,
                             add_to_visible);
    service_test->SetServiceProperty("/service/4",
                                     shill::kSecurityClassProperty,
                                     base::Value(shill::kSecurityClass8021x));

    // VPN
    service_test->AddService("/service/5", "vpn5_guid", "vpn5", shill::kTypeVPN,
                             shill::kStateIdle, add_to_visible);
    auto provider_properties = base::Value::Dict().Set(
        shill::kTypeProperty, shill::kProviderL2tpIpsec);
    service_test->SetServiceProperty(
        "/service/5", shill::kProviderProperty,
        base::Value(std::move(provider_properties)));

    service_test->AddService("/service/6", "vpn6_guid", "vpn6", shill::kTypeVPN,
                             shill::kStateIdle, add_to_visible);
    auto provider_properties2 =
        base::Value::Dict().Set(shill::kTypeProperty, shill::kProviderOpenVpn);
    service_test->SetServiceProperty(
        "/service/6", shill::kProviderProperty,
        base::Value(std::move(provider_properties2)));

    base::RunLoop().RunUntilIdle();
  }

  base::test::TaskEnvironment environment_;
  NetworkHandlerTestHelper network_handler_test_helper_;
};

// See FakeShillManagerClient for default fake networks.
TEST_F(NetworkEventLogTest, NetworkId) {
  const NetworkState* service0 = GetNetworkState("/service/0");
  ASSERT_TRUE(service0);
  EXPECT_EQ("ethernet_0", NetworkId(service0));

  const NetworkState* service1 = GetNetworkState("/service/1");
  ASSERT_TRUE(service1);
  EXPECT_EQ("cellular_LTE_1", NetworkId(service1));

  const NetworkState* service2 = GetNetworkState("/service/2");
  ASSERT_TRUE(service2);
  EXPECT_EQ("wifi_none_2", NetworkId(service2));
  const NetworkState* service3 = GetNetworkState("/service/3");
  ASSERT_TRUE(service3);
  EXPECT_EQ("wifi_wep_3", NetworkId(service3));
  const NetworkState* service4 = GetNetworkState("/service/4");
  ASSERT_TRUE(service4);
  EXPECT_EQ("wifi_802_1x_4", NetworkId(service4));

  const NetworkState* service5 = GetNetworkState("/service/5");
  ASSERT_TRUE(service5);
  EXPECT_EQ("vpn_l2tpipsec_5", NetworkId(service5));
  const NetworkState* service6 = GetNetworkState("/service/6");
  ASSERT_TRUE(service6);
  EXPECT_EQ("vpn_openvpn_6", NetworkId(service6));
}

TEST_F(NetworkEventLogTest, NetworkPathId) {
  EXPECT_EQ("ethernet_0", NetworkPathId("/service/0"));
  EXPECT_EQ("cellular_LTE_1", NetworkPathId("/service/1"));
  EXPECT_EQ("wifi_none_2", NetworkPathId("/service/2"));
  EXPECT_EQ("wifi_wep_3", NetworkPathId("/service/3"));
  EXPECT_EQ("wifi_802_1x_4", NetworkPathId("/service/4"));
  EXPECT_EQ("vpn_l2tpipsec_5", NetworkPathId("/service/5"));
  EXPECT_EQ("vpn_openvpn_6", NetworkPathId("/service/6"));
  EXPECT_EQ("service_7", NetworkPathId("/service/7"));
  EXPECT_EQ("service_invalid_path", NetworkPathId("service_invalid_path"));
}

TEST_F(NetworkEventLogTest, NetworkGuidId) {
  EXPECT_EQ("ethernet_0", NetworkGuidId("ethernet_guid"));
  EXPECT_EQ("cellular_LTE_1", NetworkGuidId("cellular_guid"));
  EXPECT_EQ("wifi_none_2", NetworkGuidId("wifi2_guid"));
  EXPECT_EQ("wifi_wep_3", NetworkGuidId("wifi3_guid"));
  EXPECT_EQ("wifi_802_1x_4", NetworkGuidId("wifi4_guid"));
  EXPECT_EQ("vpn_l2tpipsec_5", NetworkGuidId("vpn5_guid"));
  EXPECT_EQ("vpn_openvpn_6", NetworkGuidId("vpn6_guid"));
  EXPECT_EQ("wifi99_guid", NetworkGuidId("wifi99_guid"));
}

}  // namespace ash
