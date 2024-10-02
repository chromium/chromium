// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_state.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/i18n/streaming_utf8_validator.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/tether_constants.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const char kTestCellularDevicePath[] = "cellular_path";
const char kTestCellularDeviceName[] = "cellular_name";

}  // namespace

class NetworkStateTest : public testing::Test {
 public:
  NetworkStateTest() = default;

  NetworkStateTest(const NetworkStateTest&) = delete;
  NetworkStateTest& operator=(const NetworkStateTest&) = delete;

  // testing::Test:
  void SetUp() override {
    network_state_ = std::make_unique<NetworkState>("test_path");
    AddCellularDevice();
  }

  void TearDown() override { network_state_.reset(); }

 protected:
  const DeviceState* GetCellularDevice() {
    return helper_.network_state_handler()->GetDeviceState(
        kTestCellularDevicePath);
  }

  bool SetProperty(const std::string& key, base::Value value) {
    const bool result = network_state_->PropertyChanged(key, value);
    properties_.Set(key, std::move(value));
    return result;
  }

  bool SetStringProperty(const std::string& key, const std::string& value) {
    return SetProperty(key, base::Value(value));
  }

  bool SignalInitialPropertiesReceived() {
    return network_state_->InitialPropertiesReceived(properties_);
  }

  void SetConnectionState(const std::string& connection_state) {
    network_state_->SetConnectionState(connection_state);
  }

  void UpdateCaptivePortalState(const base::Value::Dict& properties) {
    network_state_->UpdateCaptivePortalState(properties);
  }

  NetworkState::PortalState GetPortalState() {
    return network_state_->portal_state_;
  }

  std::unique_ptr<NetworkState> network_state_;

 private:
  void AddCellularDevice() {
    helper_.device_test()->AddDevice(
        kTestCellularDevicePath, shill::kTypeCellular, kTestCellularDeviceName);
    base::RunLoop().RunUntilIdle();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkStateTestHelper helper_{/*use_default_devices_and_services=*/false};

  base::Value::Dict properties_;
};

// Setting kNameProperty should set network name after call to
// InitialPropertiesReceived().
TEST_F(NetworkStateTest, NameAscii) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeVPN));

  std::string network_setname = "Name TEST";
  EXPECT_TRUE(SetStringProperty(shill::kNameProperty, network_setname));
  EXPECT_FALSE(SignalInitialPropertiesReceived());
  EXPECT_EQ(network_state_->name(), network_setname);
}

TEST_F(NetworkStateTest, NameAsciiWithNull) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeVPN));

  std::string network_setname = "Name TEST\x00xxx";
  std::string network_setname_result = "Name TEST";
  EXPECT_TRUE(SetStringProperty(shill::kNameProperty, network_setname));
  EXPECT_FALSE(SignalInitialPropertiesReceived());
  EXPECT_EQ(network_state_->name(), network_setname_result);
}

// Truncates invalid UTF-8. base::Value has a DCHECK against invalid UTF-8
// strings, which is why this is a release mode only test.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
TEST_F(NetworkStateTest, NameTruncateInvalid) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeVPN));

  std::string network_setname = "SSID TEST \x01\xff!";
  std::string network_setname_result = "SSID TEST \xEF\xBF\xBD\xEF\xBF\xBD!";
  EXPECT_TRUE(SetStringProperty(shill::kNameProperty, network_setname));
  EXPECT_TRUE(SignalInitialPropertiesReceived());
  EXPECT_EQ(network_state_->name(), network_setname_result);
}
#endif

// If HexSSID doesn't exist, fallback to NameProperty.
TEST_F(NetworkStateTest, SsidFromName) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeWifi));

  std::string wifi_utf8 = "UTF-8 \u3042\u3044\u3046";
  std::string wifi_utf8_result = "UTF-8 \xE3\x81\x82\xE3\x81\x84\xE3\x81\x86";
  EXPECT_TRUE(SetStringProperty(shill::kNameProperty, wifi_utf8));
  EXPECT_FALSE(SignalInitialPropertiesReceived());
  EXPECT_EQ(network_state_->name(), wifi_utf8_result);
}

// latin1 SSID -> UTF8 SSID (Hex)
TEST_F(NetworkStateTest, SsidLatin) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeWifi));

  std::string wifi_latin1 = "latin-1 \x54\xe9\x6c\xe9\x63\x6f\x6d";  // Télécom
  std::string wifi_latin1_hex = base::HexEncode(wifi_latin1);
  std::string wifi_latin1_result = "latin-1 T\xc3\xa9\x6c\xc3\xa9\x63om";
  EXPECT_TRUE(SetStringProperty(shill::kWifiHexSsid, wifi_latin1_hex));
  EXPECT_TRUE(SignalInitialPropertiesReceived());
  EXPECT_EQ(network_state_->name(), wifi_latin1_result);
}

// Hex SSID
TEST_F(NetworkStateTest, SsidHex) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeWifi));

  std::string wifi_hex_result = "This is HEX SSID!";
  std::string wifi_hex = base::HexEncode(wifi_hex_result);
  EXPECT_TRUE(SetStringProperty(shill::kWifiHexSsid, wifi_hex));
  EXPECT_TRUE(SignalInitialPropertiesReceived());
  EXPECT_EQ(wifi_hex_result, network_state_->name());

  // Check HexSSID via network state dictionary.
  base::Value::Dict dictionary;
  network_state_->GetStateProperties(&dictionary);
  std::string* value = dictionary.FindString(shill::kWifiHexSsid);
  EXPECT_NE(nullptr, value);
  EXPECT_EQ(wifi_hex, *value);
}

// Non-UTF-8 SSID should be preserved in |raw_ssid_| field.
TEST_F(NetworkStateTest, SsidNonUtf8) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeWifi));

  std::string non_utf8_ssid = "\xc0";
  ASSERT_FALSE(base::StreamingUtf8Validator::Validate(non_utf8_ssid));

  std::vector<uint8_t> non_utf8_ssid_bytes;
  non_utf8_ssid_bytes.push_back(static_cast<uint8_t>(non_utf8_ssid.data()[0]));

  std::string wifi_hex = base::HexEncode(non_utf8_ssid);
  EXPECT_TRUE(SetStringProperty(shill::kWifiHexSsid, wifi_hex));
  EXPECT_TRUE(SignalInitialPropertiesReceived());
  EXPECT_EQ(network_state_->raw_ssid(), non_utf8_ssid_bytes);
}

// Multiple updates for Hex SSID should work fine.
TEST_F(NetworkStateTest, SsidHexMultipleUpdates) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeWifi));

  std::string wifi_hex_result = "This is HEX SSID!";
  std::string wifi_hex = base::HexEncode(wifi_hex_result);
  EXPECT_TRUE(SetStringProperty(shill::kWifiHexSsid, wifi_hex));
  EXPECT_TRUE(SetStringProperty(shill::kWifiHexSsid, wifi_hex));
}

TEST_F(NetworkStateTest, CaptivePortalState) {
  std::string network_name = "test";
  network_state_->set_visible(true);

  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeWifi));
  EXPECT_TRUE(SetStringProperty(shill::kNameProperty, network_name));
  std::string hex_ssid = base::HexEncode(network_name);
  EXPECT_TRUE(SetStringProperty(shill::kWifiHexSsid, hex_ssid));

  // State != portal or online -> portal_state() == kUnknown
  EXPECT_TRUE(SetStringProperty(shill::kStateProperty, shill::kStateReady));
  SignalInitialPropertiesReceived();
  EXPECT_EQ(network_state_->GetPortalState(),
            NetworkState::PortalState::kUnknown);

  // State == online -> portal_state() == kOnline
  EXPECT_TRUE(SetStringProperty(shill::kStateProperty, shill::kStateOnline));
  SignalInitialPropertiesReceived();
  EXPECT_EQ(network_state_->GetPortalState(),
            NetworkState::PortalState::kOnline);

  // State == redirect-found -> portal_state() == kPortal
  EXPECT_TRUE(
      SetStringProperty(shill::kStateProperty, shill::kStateRedirectFound));
  SignalInitialPropertiesReceived();
  EXPECT_EQ(network_state_->GetPortalState(),
            NetworkState::PortalState::kPortal);

  // State == portal-suspected -> portal_state() == kPortalSuspected
  EXPECT_TRUE(
      SetStringProperty(shill::kStateProperty, shill::kStatePortalSuspected));
  SignalInitialPropertiesReceived();
  EXPECT_EQ(network_state_->GetPortalState(),
            NetworkState::PortalState::kPortalSuspected);

  // State == no-connectivity -> portal_state() == kOffline
  EXPECT_TRUE(
      SetStringProperty(shill::kStateProperty, shill::kStateNoConnectivity));
  SignalInitialPropertiesReceived();
  EXPECT_EQ(network_state_->GetPortalState(),
            NetworkState::PortalState::kNoInternet);
}

// Third-party VPN provider.
TEST_F(NetworkStateTest, VPNThirdPartyProvider) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeVPN));
  EXPECT_TRUE(SetStringProperty(shill::kNameProperty, "VPN"));

  auto provider =
      base::Value::Dict()
          .Set(shill::kTypeProperty, shill::kProviderThirdPartyVpn)
          .Set(shill::kHostProperty, "third-party-vpn-provider-extension-id");
  EXPECT_TRUE(
      SetProperty(shill::kProviderProperty, base::Value(std::move(provider))));
  SignalInitialPropertiesReceived();
  ASSERT_TRUE(network_state_->vpn_provider());
  EXPECT_EQ(network_state_->vpn_provider()->type,
            shill::kProviderThirdPartyVpn);
  EXPECT_EQ(network_state_->vpn_provider()->id,
            "third-party-vpn-provider-extension-id");
}

// Arc VPN provider.
TEST_F(NetworkStateTest, VPNArcProvider) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeVPN));
  EXPECT_TRUE(SetStringProperty(shill::kNameProperty, "VPN"));

  auto provider = base::Value::Dict()
                      .Set(shill::kTypeProperty, shill::kProviderArcVpn)
                      .Set(shill::kHostProperty, "package.name.foo");
  EXPECT_TRUE(
      SetProperty(shill::kProviderProperty, base::Value(std::move(provider))));
  SignalInitialPropertiesReceived();
  ASSERT_TRUE(network_state_->vpn_provider());
  EXPECT_EQ(network_state_->vpn_provider()->type, shill::kProviderArcVpn);
  EXPECT_EQ(network_state_->vpn_provider()->id, "package.name.foo");
}

TEST_F(NetworkStateTest, AllowRoaming) {
  EXPECT_FALSE(network_state_->allow_roaming());
  EXPECT_TRUE(
      SetProperty(shill::kCellularAllowRoamingProperty, base::Value(true)));
  EXPECT_TRUE(network_state_->allow_roaming());
}

TEST_F(NetworkStateTest, Visible) {
  EXPECT_FALSE(network_state_->visible());

  network_state_->set_visible(true);
  EXPECT_TRUE(network_state_->visible());

  network_state_->set_visible(false);
  EXPECT_FALSE(network_state_->visible());
}

TEST_F(NetworkStateTest, ConnectionState) {
  network_state_->set_visible(true);

  network_state_->SetConnectionState(shill::kStateConfiguration);
  EXPECT_EQ(network_state_->connection_state(), shill::kStateConfiguration);
  EXPECT_TRUE(network_state_->IsConnectingState());
  EXPECT_TRUE(network_state_->IsConnectingOrConnected());
  EXPECT_TRUE(network_state_->IsActive());
  // State change to configuration from idle should not set connect_requested
  // unless explicitly set by the UI.
  EXPECT_FALSE(network_state_->connect_requested());

  network_state_->SetConnectionState(shill::kStateOnline);
  EXPECT_EQ(network_state_->connection_state(), shill::kStateOnline);
  EXPECT_TRUE(network_state_->IsConnectedState());
  EXPECT_TRUE(network_state_->IsConnectingOrConnected());
  EXPECT_TRUE(network_state_->IsActive());

  network_state_->SetConnectionState(shill::kStateConfiguration);
  EXPECT_EQ(network_state_->connection_state(), shill::kStateConfiguration);
  EXPECT_TRUE(network_state_->IsConnectingState());
  // State change to configuration from a connected state should set
  // connect_requested.
  EXPECT_TRUE(network_state_->connect_requested());

  network_state_->SetConnectionState(shill::kStateOnline);
  EXPECT_TRUE(network_state_->IsConnectedState());
  // State change to connected should clear connect_requested.
  EXPECT_FALSE(network_state_->connect_requested());

  network_state_->SetConnectionState(shill::kStateIdle);
  EXPECT_EQ(network_state_->connection_state(), shill::kStateIdle);
  EXPECT_FALSE(network_state_->IsConnectedState());
  EXPECT_FALSE(network_state_->IsConnectingState());
  EXPECT_FALSE(network_state_->IsConnectingOrConnected());
  EXPECT_FALSE(network_state_->IsActive());

  EXPECT_TRUE(SetStringProperty(shill::kActivationStateProperty,
                                shill::kActivationStateActivating));
  EXPECT_FALSE(network_state_->IsConnectedState());
  EXPECT_FALSE(network_state_->IsConnectingState());
  EXPECT_FALSE(network_state_->IsConnectingOrConnected());
  EXPECT_TRUE(network_state_->IsActive());
}

TEST_F(NetworkStateTest, ConnectRequested) {
  network_state_->set_visible(true);

  network_state_->SetConnectionState(shill::kStateIdle);

  network_state_->set_connect_requested_for_testing(true);
  EXPECT_EQ(network_state_->connection_state(), shill::kStateIdle);
  EXPECT_FALSE(network_state_->IsConnectedState());
  EXPECT_TRUE(network_state_->IsConnectingState());
  EXPECT_TRUE(network_state_->IsConnectingOrConnected());

  network_state_->SetConnectionState(shill::kStateOnline);
  EXPECT_TRUE(network_state_->IsConnectedState());
  EXPECT_FALSE(network_state_->IsConnectingState());
}

TEST_F(NetworkStateTest, ConnectionStateNotVisible) {
  network_state_->set_visible(false);

  network_state_->SetConnectionState(shill::kStateConfiguration);
  EXPECT_EQ(network_state_->connection_state(), shill::kStateIdle);
  EXPECT_FALSE(network_state_->IsConnectingState());

  network_state_->SetConnectionState(shill::kStateOnline);
  EXPECT_EQ(network_state_->connection_state(), shill::kStateIdle);
  EXPECT_FALSE(network_state_->IsConnectedState());

  network_state_->SetConnectionState(shill::kStateConfiguration);
  EXPECT_EQ(network_state_->connection_state(), shill::kStateIdle);
  EXPECT_FALSE(network_state_->IsConnectingState());
}

TEST_F(NetworkStateTest, TetherProperties) {
  network_state_->set_type_for_testing(kTypeTether);
  network_state_->set_tether_carrier("Project Fi");
  network_state_->set_battery_percentage(85);
  network_state_->set_tether_has_connected_to_host(true);
  network_state_->set_signal_strength(75);

  base::Value::Dict dictionary;
  network_state_->GetStateProperties(&dictionary);

  std::optional<int> signal_strength =
      dictionary.FindInt(kTetherSignalStrength);
  EXPECT_TRUE(signal_strength.has_value());
  EXPECT_EQ(75, signal_strength.value());

  std::optional<int> battery_percentage =
      dictionary.FindInt(kTetherBatteryPercentage);
  EXPECT_TRUE(battery_percentage.has_value());
  EXPECT_EQ(85, battery_percentage.value());

  std::optional<bool> tether_has_connected_to_host =
      dictionary.FindBool(kTetherHasConnectedToHost);
  EXPECT_TRUE(tether_has_connected_to_host.has_value());
  EXPECT_TRUE(tether_has_connected_to_host.value());

  std::string* carrier = dictionary.FindString(kTetherCarrier);
  EXPECT_NE(nullptr, carrier);
  EXPECT_EQ("Project Fi", *carrier);
}

TEST_F(NetworkStateTest, CelularPaymentPortalPost) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeCellular));
  EXPECT_TRUE(SetStringProperty(shill::kNameProperty, "Test Cellular"));
  EXPECT_TRUE(SetStringProperty(shill::kNetworkTechnologyProperty,
                                shill::kNetworkTechnologyLteAdvanced));
  EXPECT_TRUE(SetStringProperty(shill::kActivationTypeProperty,
                                shill::kActivationTypeOTA));
  EXPECT_TRUE(SetStringProperty(shill::kActivationStateProperty,
                                shill::kActivationStateActivated));

  auto payment_portal =
      base::Value::Dict()
          .Set(shill::kPaymentPortalURL, "http://test-portal.com")
          .Set(shill::kPaymentPortalMethod, "POST")
          .Set(shill::kPaymentPortalPostData, "fake_data");

  EXPECT_TRUE(SetProperty(shill::kPaymentPortalProperty,
                          base::Value(std::move(payment_portal))));

  SignalInitialPropertiesReceived();
  EXPECT_EQ("Test Cellular", network_state_->name());
  EXPECT_EQ(shill::kNetworkTechnologyLteAdvanced,
            network_state_->network_technology());
  EXPECT_EQ(shill::kActivationTypeOTA, network_state_->activation_type());
  EXPECT_EQ(shill::kActivationStateActivated,
            network_state_->activation_state());
  EXPECT_EQ("http://test-portal.com", network_state_->payment_url());
  EXPECT_EQ("fake_data", network_state_->payment_post_data());
}

TEST_F(NetworkStateTest, CelularPaymentPortalGet) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeCellular));
  EXPECT_TRUE(SetStringProperty(shill::kNameProperty, "Test Cellular"));
  EXPECT_TRUE(SetStringProperty(shill::kNetworkTechnologyProperty,
                                shill::kNetworkTechnologyLteAdvanced));
  EXPECT_TRUE(SetStringProperty(shill::kActivationTypeProperty,
                                shill::kActivationTypeOTA));
  EXPECT_TRUE(SetStringProperty(shill::kActivationStateProperty,
                                shill::kActivationStateActivated));

  auto payment_portal =
      base::Value::Dict()
          .Set(shill::kPaymentPortalURL, "http://test-portal.com")
          .Set(shill::kPaymentPortalMethod, "GET")
          .Set(shill::kPaymentPortalPostData, "ignored");

  EXPECT_TRUE(SetProperty(shill::kPaymentPortalProperty,
                          base::Value(std::move(payment_portal))));

  SignalInitialPropertiesReceived();

  EXPECT_EQ("Test Cellular", network_state_->name());
  EXPECT_EQ(shill::kNetworkTechnologyLteAdvanced,
            network_state_->network_technology());
  EXPECT_EQ(shill::kActivationTypeOTA, network_state_->activation_type());
  EXPECT_EQ(shill::kActivationStateActivated,
            network_state_->activation_state());
  EXPECT_EQ("http://test-portal.com", network_state_->payment_url());
  EXPECT_EQ("", network_state_->payment_post_data());
}

TEST_F(NetworkStateTest, CellularSpecifier) {
  const char kTestCellularNetworkName[] = "cellular1";
  const char kTestIccid1[] = "1234567890";
  const char kTestIccid2[] = "0987654321";
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeCellular));
  EXPECT_TRUE(
      SetStringProperty(shill::kNameProperty, kTestCellularNetworkName));
  network_state_->set_update_received();

  // Verify that cellular network state with same name but different iccid
  // produce different specifier values.
  EXPECT_TRUE(SetStringProperty(shill::kIccidProperty, kTestIccid1));
  std::string specifier1 = network_state_->GetSpecifier();
  EXPECT_TRUE(SetStringProperty(shill::kIccidProperty, kTestIccid2));
  std::string specifier2 = network_state_->GetSpecifier();
  EXPECT_NE(specifier1, specifier2);
}

TEST_F(NetworkStateTest, NonShillCellular) {
  const char kTestIccid[] = "test_iccid";
  const char kTestEid[] = "test_eid";
  const char kTestGuid[] = "test_guid";

  std::unique_ptr<NetworkState> non_shill_cellular =
      NetworkState::CreateNonShillCellularNetwork(
          kTestIccid, kTestEid, kTestGuid, /*is_managed=*/false,
          GetCellularDevice()->path());
  EXPECT_EQ(kTestIccid, non_shill_cellular->iccid());
  EXPECT_EQ(kTestEid, non_shill_cellular->eid());
  EXPECT_EQ(kTestGuid, non_shill_cellular->guid());
  EXPECT_FALSE(non_shill_cellular->IsManagedByPolicy());

  base::Value::Dict dictionary;
  non_shill_cellular->GetStateProperties(&dictionary);
  EXPECT_EQ(kTestIccid, *dictionary.FindString(shill::kIccidProperty));
  EXPECT_EQ(kTestEid, *dictionary.FindString(shill::kEidProperty));
  EXPECT_EQ(kTestGuid, *dictionary.FindString(shill::kGuidProperty));

  non_shill_cellular = NetworkState::CreateNonShillCellularNetwork(
      kTestIccid, kTestEid, kTestGuid, /*is_managed=*/true,
      GetCellularDevice()->path());
  EXPECT_EQ(kTestIccid, non_shill_cellular->iccid());
  EXPECT_EQ(kTestEid, non_shill_cellular->eid());
  EXPECT_EQ(kTestGuid, non_shill_cellular->guid());
  EXPECT_TRUE(non_shill_cellular->IsManagedByPolicy());

  non_shill_cellular->GetStateProperties(&dictionary);
  EXPECT_EQ(kTestIccid, *dictionary.FindString(shill::kIccidProperty));
  EXPECT_EQ(kTestEid, *dictionary.FindString(shill::kEidProperty));
  EXPECT_EQ(kTestGuid, *dictionary.FindString(shill::kGuidProperty));
}

TEST_F(NetworkStateTest, UpdateCaptivePortalState) {
  base::Value::Dict shill_properties;

  network_state_->set_visible(true);
  EXPECT_EQ(GetPortalState(), NetworkState::PortalState::kUnknown);

  SetConnectionState(shill::kStateIdle);
  UpdateCaptivePortalState(shill_properties);
  EXPECT_EQ(GetPortalState(), NetworkState::PortalState::kUnknown);

  SetConnectionState(shill::kStateNoConnectivity);
  UpdateCaptivePortalState(shill_properties);
  EXPECT_EQ(GetPortalState(), NetworkState::PortalState::kNoInternet);

  SetConnectionState(shill::kStateRedirectFound);
  UpdateCaptivePortalState(shill_properties);
  EXPECT_EQ(GetPortalState(), NetworkState::PortalState::kPortal);

  SetConnectionState(shill::kStatePortalSuspected);
  UpdateCaptivePortalState(shill_properties);
  EXPECT_EQ(GetPortalState(), NetworkState::PortalState::kPortalSuspected);

  SetConnectionState(shill::kStateOnline);
  UpdateCaptivePortalState(shill_properties);
  EXPECT_EQ(GetPortalState(), NetworkState::PortalState::kOnline);
}

TEST_F(NetworkStateTest, UpdateNetworkConfig) {
  // This test only verifies that update of NetworkConfig can be reflected on
  // NetworkState. The parsing of the NetworkConfig dict is tested in
  // network_config_unittest.cc.
  base::Value::Dict properties;
  properties.Set(shill::kNetworkConfigIPv4AddressProperty, "1.2.3.4/24");

  network_state_->PropertyChanged(shill::kNetworkConfigProperty,
                                  base::Value(std::move(properties)));

  const NetworkConfig* config = network_state_->network_config();
  ASSERT_TRUE(config);
  EXPECT_EQ(config->ipv4_address->addr.ToString(), "1.2.3.4");
  EXPECT_EQ(config->ipv4_address->prefix_len, 24);
}

}  // namespace ash
