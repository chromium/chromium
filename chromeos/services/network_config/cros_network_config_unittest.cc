// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/cros_network_config.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/shill/fake_shill_device_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_certificate_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_metadata_store.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_observer.h"
#include "components/onc/onc_constants.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {
namespace network_config {

namespace {

const int kSimRetriesLeft = 3;
const char kCellularDevicePath[] = "/device/stub_cellular_device";

const char kCellularTestApn1[] = "TEST.APN1";
const char kCellularTestApnName1[] = "Test Apn 1";
const char kCellularTestApnUsername1[] = "Test User";
const char kCellularTestApnPassword1[] = "Test Pass";

const char kCellularTestApn2[] = "TEST.APN2";
const char kCellularTestApnName2[] = "Test Apn 2";
const char kCellularTestApnUsername2[] = "Test User";
const char kCellularTestApnPassword2[] = "Test Pass";

const char kCellularTestApn3[] = "TEST.APN3";
const char kCellularTestApnName3[] = "Test Apn 3";
const char kCellularTestApnUsername3[] = "Test User";
const char kCellularTestApnPassword3[] = "Test Pass";

}  // namespace

class CrosNetworkConfigTest : public testing::Test {
 public:
  CrosNetworkConfigTest() {
    LoginState::Initialize();
    NetworkCertLoader::Initialize();
    NetworkHandler::Initialize();
    network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();
    network_device_handler_ = NetworkDeviceHandler::InitializeForTesting(
        helper_.network_state_handler());
    network_configuration_handler_ =
        base::WrapUnique<NetworkConfigurationHandler>(
            NetworkConfigurationHandler::InitializeForTest(
                helper_.network_state_handler(),
                network_device_handler_.get()));

    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
    ::onc::RegisterProfilePrefs(user_prefs_.registry());
    ::onc::RegisterPrefs(local_state_.registry());
    NetworkMetadataStore::RegisterPrefs(user_prefs_.registry());
    NetworkMetadataStore::RegisterPrefs(local_state_.registry());
    NetworkHandler::Get()->InitializePrefServices(&user_prefs_, &local_state_);

    ui_proxy_config_service_ = std::make_unique<chromeos::UIProxyConfigService>(
        &user_prefs_, &local_state_, helper_.network_state_handler(),
        network_profile_handler_.get());

    managed_network_configuration_handler_ =
        ManagedNetworkConfigurationHandler::InitializeForTesting(
            helper_.network_state_handler(), network_profile_handler_.get(),
            network_device_handler_.get(), network_configuration_handler_.get(),
            ui_proxy_config_service_.get());
    network_connection_handler_ =
        NetworkConnectionHandler::InitializeForTesting(
            helper_.network_state_handler(),
            network_configuration_handler_.get(),
            managed_network_configuration_handler_.get());
    network_certificate_handler_ =
        std::make_unique<NetworkCertificateHandler>();
    cros_network_config_ = std::make_unique<CrosNetworkConfig>(
        helper_.network_state_handler(), network_device_handler_.get(),
        managed_network_configuration_handler_.get(),
        network_connection_handler_.get(), network_certificate_handler_.get());
    SetupPolicy();
    SetupNetworks();
  }

  ~CrosNetworkConfigTest() override {
    cros_network_config_.reset();
    network_certificate_handler_.reset();
    network_connection_handler_.reset();
    managed_network_configuration_handler_.reset();
    network_configuration_handler_.reset();
    network_device_handler_.reset();
    network_profile_handler_.reset();
    ui_proxy_config_service_.reset();
    NetworkHandler::Shutdown();
    NetworkCertLoader::Shutdown();
    LoginState::Shutdown();
  }

  void SetupPolicy() {
    managed_network_configuration_handler_->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY,
        /*userhash=*/std::string(),
        /*network_configs_onc=*/base::ListValue(),
        /*global_network_config=*/base::DictionaryValue());

    const std::string user_policy_ssid = "wifi2";
    base::Value wifi2_onc = base::Value::FromUniquePtrValue(
        onc::ReadDictionaryFromJson(base::StringPrintf(
            R"({"GUID": "wifi2_guid", "Type": "WiFi",
                "Name": "wifi2", "Priority": 0,
                "WiFi": { "Passphrase": "fake", "SSID": "%s", "HexSSID": "%s",
                          "Security": "WPA-PSK", "AutoConnect": true}})",
            user_policy_ssid.c_str(),
            base::HexEncode(user_policy_ssid.c_str(), user_policy_ssid.size())
                .c_str())));
    base::ListValue user_policy_onc;
    user_policy_onc.Append(std::move(wifi2_onc));
    managed_network_configuration_handler_->SetPolicy(
        ::onc::ONC_SOURCE_USER_POLICY, helper().UserHash(), user_policy_onc,
        /*global_network_config=*/base::DictionaryValue());
    base::RunLoop().RunUntilIdle();
  }

  void SetupNetworks() {
    // Wifi device exists by default, add Ethernet and Cellular.
    helper().device_test()->AddDevice("/device/stub_eth_device",
                                      shill::kTypeEthernet, "stub_eth_device");
    helper().manager_test()->AddTechnology(shill::kTypeCellular,
                                           true /* enabled */);
    helper().device_test()->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                                      "stub_cellular_device");
    base::Value sim_value(base::Value::Type::DICTIONARY);
    sim_value.SetKey(shill::kSIMLockEnabledProperty, base::Value(true));
    sim_value.SetKey(shill::kSIMLockTypeProperty,
                     base::Value(shill::kSIMLockPin));
    sim_value.SetKey(shill::kSIMLockRetriesLeftProperty,
                     base::Value(kSimRetriesLeft));
    helper().device_test()->SetDeviceProperty(
        kCellularDevicePath, shill::kSIMLockStatusProperty, sim_value,
        /*notify_changed=*/false);
    helper().device_test()->SetDeviceProperty(
        kCellularDevicePath, shill::kSIMPresentProperty, base::Value(true),
        /*notify_changed=*/false);

    // Note: These are Shill dictionaries, not ONC.
    helper().ConfigureService(
        R"({"GUID": "eth_guid", "Type": "ethernet", "State": "online"})");
    wifi1_path_ = helper().ConfigureService(
        R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "ready",
            "Strength": 50, "AutoConnect": true})");
    helper().ConfigureService(
        R"({"GUID": "wifi2_guid", "Type": "wifi", "SSID": "wifi2",
            "State": "idle", "SecurityClass": "psk", "Strength": 100,
            "Profile": "user_profile_path"})");
    helper().ConfigureService(base::StringPrintf(
        R"({"GUID": "cellular_guid", "Type": "cellular",  "State": "idle",
            "Strength": 0, "Cellular.NetworkTechnology": "LTE",
            "Cellular.ActivationState": "activated",
            "Profile": "%s"})",
        NetworkProfileHandler::GetSharedProfilePath().c_str()));
    helper().ConfigureService(
        R"({"GUID": "vpn_guid", "Type": "vpn", "State": "association",
            "Provider": {"Type": "l2tpipsec"}})");

    // Add a non visible configured wifi service.
    std::string wifi3_path = helper().ConfigureService(
        R"({"GUID": "wifi3_guid", "Type": "wifi", "SecurityClass": "psk",
            "Visible": false})");
    helper().profile_test()->AddService(
        NetworkProfileHandler::GetSharedProfilePath(), wifi3_path);

    // Syncable wifi network:
    std::string service_path = helper().ConfigureService(
        R"({"GUID": "wifi4_guid", "Type": "wifi", "SSID": "wifi4",
            "State": "idle", "SecurityClass": "psk", "Strength": 100,
            "Profile": "user_profile_path", "Connectable": true})");
    NetworkHandler::Get()->network_metadata_store()->ConnectSucceeded(
        service_path);

    base::RunLoop().RunUntilIdle();
  }

  void SetupAPNList() {
    base::Value apn_list(base::Value::Type::LIST);
    base::Value apn_entry1(base::Value::Type::DICTIONARY);
    apn_entry1.SetStringKey(shill::kApnNameProperty, kCellularTestApnName1);
    apn_entry1.SetStringKey(shill::kApnProperty, kCellularTestApn1);
    apn_entry1.SetStringKey(shill::kApnUsernameProperty,
                            kCellularTestApnUsername1);
    apn_entry1.SetStringKey(shill::kApnPasswordProperty,
                            kCellularTestApnPassword1);
    apn_list.Append(std::move(apn_entry1));
    base::Value apn_entry2(base::Value::Type::DICTIONARY);
    apn_entry2.SetStringKey(shill::kApnNameProperty, kCellularTestApnName2);
    apn_entry2.SetStringKey(shill::kApnProperty, kCellularTestApn2);
    apn_entry2.SetStringKey(shill::kApnUsernameProperty,
                            kCellularTestApnUsername2);
    apn_entry2.SetStringKey(shill::kApnPasswordProperty,
                            kCellularTestApnPassword2);
    apn_list.Append(std::move(apn_entry2));

    helper().device_test()->SetDeviceProperty(
        kCellularDevicePath, shill::kCellularApnListProperty, apn_list,
        /*notify_changed=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetupEthernetEAP() {
    std::string eap_path = helper().ConfigureService(
        R"({"GUID": "eth_eap_guid", "Type": "etherneteap",
            "State": "online", "EAP.EAP": "TTLS", "EAP.Identity": "user1"})");
    helper().profile_test()->AddService(
        NetworkProfileHandler::GetSharedProfilePath(), eap_path);
    base::RunLoop().RunUntilIdle();
  }

  void SetupObserver() {
    observer_ = std::make_unique<CrosNetworkConfigTestObserver>();
    cros_network_config_->AddObserver(observer_->GenerateRemote());
  }

  mojom::NetworkStatePropertiesPtr GetNetworkState(const std::string& guid) {
    mojom::NetworkStatePropertiesPtr result;
    base::RunLoop run_loop;
    cros_network_config()->GetNetworkState(
        guid, base::BindOnce(
                  [](mojom::NetworkStatePropertiesPtr* result,
                     base::OnceClosure quit_closure,
                     mojom::NetworkStatePropertiesPtr network) {
                    *result = std::move(network);
                    std::move(quit_closure).Run();
                  },
                  &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  std::vector<mojom::NetworkStatePropertiesPtr> GetNetworkStateList(
      mojom::NetworkFilterPtr filter) {
    std::vector<mojom::NetworkStatePropertiesPtr> result;
    base::RunLoop run_loop;
    cros_network_config()->GetNetworkStateList(
        std::move(filter),
        base::BindOnce(
            [](std::vector<mojom::NetworkStatePropertiesPtr>* result,
               base::OnceClosure quit_closure,
               std::vector<mojom::NetworkStatePropertiesPtr> networks) {
              for (auto& network : networks)
                result->push_back(std::move(network));
              std::move(quit_closure).Run();
            },
            &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  std::vector<mojom::DeviceStatePropertiesPtr> GetDeviceStateList() {
    std::vector<mojom::DeviceStatePropertiesPtr> result;
    base::RunLoop run_loop;
    cros_network_config()->GetDeviceStateList(base::BindOnce(
        [](std::vector<mojom::DeviceStatePropertiesPtr>* result,
           base::OnceClosure quit_closure,
           std::vector<mojom::DeviceStatePropertiesPtr> devices) {
          for (auto& device : devices)
            result->push_back(std::move(device));
          std::move(quit_closure).Run();
        },
        &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  mojom::DeviceStatePropertiesPtr GetDeviceStateFromList(
      mojom::NetworkType type) {
    std::vector<mojom::DeviceStatePropertiesPtr> devices = GetDeviceStateList();
    for (auto& device : devices) {
      if (device->type == type)
        return std::move(device);
    }
    return nullptr;
  }

  mojom::ManagedPropertiesPtr GetManagedProperties(const std::string& guid) {
    mojom::ManagedPropertiesPtr result;
    base::RunLoop run_loop;
    cros_network_config()->GetManagedProperties(
        guid, base::BindOnce(
                  [](mojom::ManagedPropertiesPtr* result,
                     base::OnceClosure quit_closure,
                     mojom::ManagedPropertiesPtr properties) {
                    *result = std::move(properties);
                    std::move(quit_closure).Run();
                  },
                  &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  bool SetProperties(const std::string& guid,
                     mojom::ConfigPropertiesPtr properties) {
    bool success = false;
    base::RunLoop run_loop;
    cros_network_config()->SetProperties(
        guid, std::move(properties),
        base::BindOnce(
            [](bool* successp, base::OnceClosure quit_closure, bool success,
               const std::string& message) {
              *successp = success;
              std::move(quit_closure).Run();
            },
            &success, run_loop.QuitClosure()));
    run_loop.Run();
    return success;
  }

  std::string ConfigureNetwork(mojom::ConfigPropertiesPtr properties,
                               bool shared) {
    std::string guid;
    base::RunLoop run_loop;
    cros_network_config()->ConfigureNetwork(
        std::move(properties), shared,
        base::BindOnce(
            [](std::string* guidp, base::OnceClosure quit_closure,
               const base::Optional<std::string>& guid,
               const std::string& message) {
              if (guid)
                *guidp = *guid;
              std::move(quit_closure).Run();
            },
            &guid, run_loop.QuitClosure()));
    run_loop.Run();
    return guid;
  }

  bool ForgetNetwork(const std::string& guid) {
    bool success = false;
    base::RunLoop run_loop;
    cros_network_config()->ForgetNetwork(
        guid,
        base::BindOnce(
            [](bool* successp, base::OnceClosure quit_closure, bool success) {
              *successp = success;
              std::move(quit_closure).Run();
            },
            &success, run_loop.QuitClosure()));
    run_loop.Run();
    return success;
  }

  bool SetCellularSimState(const std::string& current_pin_or_puk,
                           base::Optional<std::string> new_pin,
                           bool require_pin) {
    bool success = false;
    base::RunLoop run_loop;
    cros_network_config()->SetCellularSimState(
        mojom::CellularSimState::New(current_pin_or_puk, new_pin, require_pin),
        base::BindOnce(
            [](bool* successp, base::OnceClosure quit_closure, bool success) {
              *successp = success;
              std::move(quit_closure).Run();
            },
            &success, run_loop.QuitClosure()));
    run_loop.Run();
    return success;
  }

  bool SelectCellularMobileNetwork(const std::string& guid,
                                   const std::string& network_id) {
    bool success = false;
    base::RunLoop run_loop;
    cros_network_config()->SelectCellularMobileNetwork(
        guid, network_id,
        base::BindOnce(
            [](bool* successp, base::OnceClosure quit_closure, bool success) {
              *successp = success;
              std::move(quit_closure).Run();
            },
            &success, run_loop.QuitClosure()));
    run_loop.Run();
    return success;
  }

  mojom::GlobalPolicyPtr GetGlobalPolicy() {
    mojom::GlobalPolicyPtr result;
    base::RunLoop run_loop;
    cros_network_config()->GetGlobalPolicy(base::BindOnce(
        [](mojom::GlobalPolicyPtr* result, base::OnceClosure quit_closure,
           mojom::GlobalPolicyPtr global_policy) {
          *result = std::move(global_policy);
          std::move(quit_closure).Run();
        },
        &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  mojom::StartConnectResult StartConnect(const std::string& guid) {
    mojom::StartConnectResult result;
    base::RunLoop run_loop;
    cros_network_config()->StartConnect(
        guid,
        base::BindOnce(
            [](mojom::StartConnectResult* resultp,
               base::OnceClosure quit_closure, mojom::StartConnectResult result,
               const std::string& message) {
              *resultp = result;
              std::move(quit_closure).Run();
            },
            &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  bool StartDisconnect(const std::string& guid) {
    bool success = false;
    base::RunLoop run_loop;
    cros_network_config()->StartDisconnect(
        guid,
        base::BindOnce(
            [](bool* successp, base::OnceClosure quit_closure, bool success) {
              *successp = success;
              std::move(quit_closure).Run();
            },
            &success, run_loop.QuitClosure()));
    run_loop.Run();
    return success;
  }

  std::vector<mojom::VpnProviderPtr> GetVpnProviders() {
    std::vector<mojom::VpnProviderPtr> result;
    base::RunLoop run_loop;
    cros_network_config()->GetVpnProviders(base::BindOnce(
        [](std::vector<mojom::VpnProviderPtr>* result,
           base::OnceClosure quit_closure,
           std::vector<mojom::VpnProviderPtr> providers) {
          *result = std::move(providers);
          std::move(quit_closure).Run();
        },
        &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  void GetNetworkCertificates(
      std::vector<mojom::NetworkCertificatePtr>* server_cas,
      std::vector<mojom::NetworkCertificatePtr>* user_certs) {
    base::RunLoop run_loop;
    cros_network_config()->GetNetworkCertificates(base::BindOnce(
        [](std::vector<mojom::NetworkCertificatePtr>* server_cas_result,
           std::vector<mojom::NetworkCertificatePtr>* user_certs_result,
           base::OnceClosure quit_closure,
           std::vector<mojom::NetworkCertificatePtr> server_cas,
           std::vector<mojom::NetworkCertificatePtr> user_certs) {
          *server_cas_result = std::move(server_cas);
          *user_certs_result = std::move(user_certs);
          std::move(quit_closure).Run();
        },
        server_cas, user_certs, run_loop.QuitClosure()));
    run_loop.Run();
  }

  NetworkStateTestHelper& helper() { return helper_; }
  CrosNetworkConfigTestObserver* observer() { return observer_.get(); }
  CrosNetworkConfig* cros_network_config() {
    return cros_network_config_.get();
  }
  ManagedNetworkConfigurationHandler* managed_network_configuration_handler() {
    return managed_network_configuration_handler_.get();
  }
  NetworkCertificateHandler* network_certificate_handler() {
    return network_certificate_handler_.get();
  }
  std::string wifi1_path() { return wifi1_path_; }

 protected:
  sync_preferences::TestingPrefServiceSyncable user_prefs_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkStateTestHelper helper_{false /* use_default_devices_and_services */};
  std::unique_ptr<NetworkCertificateHandler> network_certificate_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  std::unique_ptr<NetworkConnectionHandler> network_connection_handler_;
  std::unique_ptr<chromeos::UIProxyConfigService> ui_proxy_config_service_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<CrosNetworkConfig> cros_network_config_;
  std::unique_ptr<CrosNetworkConfigTestObserver> observer_;
  std::string wifi1_path_;

  DISALLOW_COPY_AND_ASSIGN(CrosNetworkConfigTest);
};

TEST_F(CrosNetworkConfigTest, GetNetworkState) {
  mojom::NetworkStatePropertiesPtr network = GetNetworkState("eth_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ("eth_guid", network->guid);
  EXPECT_EQ(mojom::NetworkType::kEthernet, network->type);
  EXPECT_EQ(mojom::ConnectionStateType::kOnline, network->connection_state);
  EXPECT_EQ(mojom::OncSource::kNone, network->source);

  network = GetNetworkState("wifi1_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ("wifi1_guid", network->guid);
  EXPECT_EQ(mojom::NetworkType::kWiFi, network->type);
  EXPECT_EQ(mojom::ConnectionStateType::kConnected, network->connection_state);
  ASSERT_TRUE(network->type_state);
  ASSERT_TRUE(network->type_state->is_wifi());
  EXPECT_EQ(mojom::SecurityType::kNone,
            network->type_state->get_wifi()->security);
  EXPECT_EQ(50, network->type_state->get_wifi()->signal_strength);
  EXPECT_EQ(mojom::OncSource::kNone, network->source);

  network = GetNetworkState("wifi2_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ("wifi2_guid", network->guid);
  EXPECT_EQ(mojom::NetworkType::kWiFi, network->type);
  EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
            network->connection_state);
  ASSERT_TRUE(network->type_state);
  ASSERT_TRUE(network->type_state->is_wifi());
  EXPECT_EQ(mojom::SecurityType::kWpaPsk,
            network->type_state->get_wifi()->security);
  EXPECT_EQ(100, network->type_state->get_wifi()->signal_strength);
  EXPECT_EQ(mojom::OncSource::kUserPolicy, network->source);

  network = GetNetworkState("wifi3_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ("wifi3_guid", network->guid);
  EXPECT_EQ(mojom::NetworkType::kWiFi, network->type);
  EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
            network->connection_state);
  ASSERT_TRUE(network->type_state);
  ASSERT_TRUE(network->type_state->is_wifi());
  EXPECT_EQ(mojom::SecurityType::kWpaPsk,
            network->type_state->get_wifi()->security);
  EXPECT_EQ(0, network->type_state->get_wifi()->signal_strength);
  EXPECT_EQ(mojom::OncSource::kDevice, network->source);

  network = GetNetworkState("cellular_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ("cellular_guid", network->guid);
  EXPECT_EQ(mojom::NetworkType::kCellular, network->type);
  EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
            network->connection_state);
  ASSERT_TRUE(network->type_state);
  ASSERT_TRUE(network->type_state->is_cellular());
  mojom::CellularStatePropertiesPtr& cellular =
      network->type_state->get_cellular();
  EXPECT_EQ(0, cellular->signal_strength);
  EXPECT_EQ("LTE", cellular->network_technology);
  EXPECT_EQ(mojom::ActivationStateType::kActivated, cellular->activation_state);
  EXPECT_EQ(mojom::OncSource::kDevice, network->source);
  EXPECT_TRUE(cellular->sim_locked);

  network = GetNetworkState("vpn_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ("vpn_guid", network->guid);
  EXPECT_EQ(mojom::NetworkType::kVPN, network->type);
  EXPECT_EQ(mojom::ConnectionStateType::kConnecting, network->connection_state);
  ASSERT_TRUE(network->type_state);
  ASSERT_TRUE(network->type_state->is_vpn());
  EXPECT_EQ(mojom::VpnType::kL2TPIPsec, network->type_state->get_vpn()->type);
  EXPECT_EQ(mojom::OncSource::kNone, network->source);

  // TODO(919691): Test ProxyMode once UIProxyConfigService logic is improved.
}

TEST_F(CrosNetworkConfigTest, GetNetworkStateList) {
  mojom::NetworkFilterPtr filter = mojom::NetworkFilter::New();
  // All active networks
  filter->filter = mojom::FilterType::kActive;
  filter->network_type = mojom::NetworkType::kAll;
  filter->limit = mojom::kNoLimit;
  std::vector<mojom::NetworkStatePropertiesPtr> networks =
      GetNetworkStateList(filter.Clone());
  ASSERT_EQ(3u, networks.size());
  EXPECT_EQ("eth_guid", networks[0]->guid);
  EXPECT_EQ("wifi1_guid", networks[1]->guid);
  EXPECT_EQ("vpn_guid", networks[2]->guid);

  // First active network
  filter->limit = 1;
  networks = GetNetworkStateList(filter.Clone());
  ASSERT_EQ(1u, networks.size());
  EXPECT_EQ("eth_guid", networks[0]->guid);

  // All wifi networks
  filter->filter = mojom::FilterType::kAll;
  filter->network_type = mojom::NetworkType::kWiFi;
  filter->limit = mojom::kNoLimit;
  networks = GetNetworkStateList(filter.Clone());
  ASSERT_EQ(4u, networks.size());
  EXPECT_EQ("wifi1_guid", networks[0]->guid);
  EXPECT_EQ("wifi2_guid", networks[1]->guid);
  EXPECT_EQ("wifi4_guid", networks[2]->guid);
  EXPECT_EQ("wifi3_guid", networks[3]->guid);

  // Visible wifi networks
  filter->filter = mojom::FilterType::kVisible;
  networks = GetNetworkStateList(filter.Clone());
  ASSERT_EQ(3u, networks.size());
  EXPECT_EQ("wifi1_guid", networks[0]->guid);
  EXPECT_EQ("wifi2_guid", networks[1]->guid);
  EXPECT_EQ("wifi4_guid", networks[2]->guid);

  // Configured wifi networks
  filter->filter = mojom::FilterType::kConfigured;
  networks = GetNetworkStateList(filter.Clone());
  ASSERT_EQ(3u, networks.size());
  EXPECT_EQ("wifi2_guid", networks[0]->guid);
  EXPECT_EQ("wifi4_guid", networks[1]->guid);
  EXPECT_EQ("wifi3_guid", networks[2]->guid);
}

TEST_F(CrosNetworkConfigTest, GetDeviceStateList) {
  std::vector<mojom::DeviceStatePropertiesPtr> devices = GetDeviceStateList();
  ASSERT_EQ(4u, devices.size());
  EXPECT_EQ(mojom::NetworkType::kWiFi, devices[0]->type);
  EXPECT_EQ(mojom::DeviceStateType::kEnabled, devices[0]->device_state);

  // IP address match those set up in FakeShillManagerClient::
  // SetupDefaultEnvironment(). TODO(stevenjb): Support setting
  // expectations explicitly in NetworkStateTestHelper.
  net::IPAddress ipv4_expected;
  ASSERT_TRUE(ipv4_expected.AssignFromIPLiteral("100.0.0.1"));
  EXPECT_EQ(ipv4_expected, devices[0]->ipv4_address);
  net::IPAddress ipv6_expected;
  ASSERT_TRUE(ipv6_expected.AssignFromIPLiteral("0:0:0:0:100:0:0:1"));
  EXPECT_EQ(ipv6_expected, devices[0]->ipv6_address);

  EXPECT_EQ(mojom::NetworkType::kEthernet, devices[1]->type);
  EXPECT_EQ(mojom::DeviceStateType::kEnabled, devices[1]->device_state);

  mojom::DeviceStateProperties* cellular = devices[2].get();
  EXPECT_EQ(mojom::NetworkType::kCellular, cellular->type);
  EXPECT_EQ(mojom::DeviceStateType::kEnabled, cellular->device_state);
  EXPECT_FALSE(cellular->sim_absent);
  ASSERT_TRUE(cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_EQ(shill::kSIMLockPin, cellular->sim_lock_status->lock_type);
  EXPECT_EQ(3, cellular->sim_lock_status->retries_left);

  mojom::DeviceStateProperties* vpn = devices[3].get();
  EXPECT_EQ(mojom::NetworkType::kVPN, vpn->type);
  EXPECT_EQ(mojom::DeviceStateType::kEnabled, vpn->device_state);

  // Disable WiFi
  helper().network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), false, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  devices = GetDeviceStateList();
  ASSERT_EQ(4u, devices.size());
  EXPECT_EQ(mojom::NetworkType::kWiFi, devices[0]->type);
  EXPECT_EQ(mojom::DeviceStateType::kDisabled, devices[0]->device_state);
}

// Test a sampling of properties, ensuring that string property types are
// translated as strings and not enum values (See ManagedProperties definition
// in cros_network_config.mojom for details).
TEST_F(CrosNetworkConfigTest, GetManagedProperties) {
  mojom::ManagedPropertiesPtr properties = GetManagedProperties("eth_guid");
  ASSERT_TRUE(properties);
  EXPECT_EQ("eth_guid", properties->guid);
  EXPECT_EQ(mojom::NetworkType::kEthernet, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kOnline, properties->connection_state);

  properties = GetManagedProperties("wifi1_guid");
  ASSERT_TRUE(properties);
  EXPECT_EQ("wifi1_guid", properties->guid);
  EXPECT_EQ(mojom::NetworkType::kWiFi, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kConnected,
            properties->connection_state);
  ASSERT_TRUE(properties->type_properties);
  ASSERT_TRUE(properties->type_properties->is_wifi());
  EXPECT_EQ(50, properties->type_properties->get_wifi()->signal_strength);
  EXPECT_EQ(mojom::OncSource::kNone, properties->source);
  EXPECT_EQ(false, properties->type_properties->get_wifi()->is_syncable);

  properties = GetManagedProperties("wifi2_guid");
  ASSERT_TRUE(properties);
  EXPECT_EQ("wifi2_guid", properties->guid);
  EXPECT_EQ(mojom::NetworkType::kWiFi, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
            properties->connection_state);
  ASSERT_TRUE(properties->type_properties);
  mojom::ManagedWiFiPropertiesPtr& wifi =
      properties->type_properties->get_wifi();
  ASSERT_TRUE(wifi);
  EXPECT_EQ(mojom::SecurityType::kWpaPsk, wifi->security);
  EXPECT_EQ(100, wifi->signal_strength);
  EXPECT_EQ(mojom::OncSource::kUserPolicy, properties->source);
  EXPECT_EQ(false, properties->type_properties->get_wifi()->is_syncable);

  properties = GetManagedProperties("wifi3_guid");
  ASSERT_TRUE(properties);
  EXPECT_EQ("wifi3_guid", properties->guid);
  EXPECT_EQ(mojom::NetworkType::kWiFi, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
            properties->connection_state);
  EXPECT_EQ(mojom::OncSource::kDevice, properties->source);
  EXPECT_EQ(false, properties->type_properties->get_wifi()->is_syncable);

  properties = GetManagedProperties("wifi4_guid");
  ASSERT_TRUE(properties);
  EXPECT_EQ("wifi4_guid", properties->guid);
  EXPECT_EQ(mojom::NetworkType::kWiFi, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
            properties->connection_state);
  EXPECT_EQ(mojom::OncSource::kUser, properties->source);
  EXPECT_EQ(true, properties->type_properties->get_wifi()->is_syncable);

  properties = GetManagedProperties("cellular_guid");
  ASSERT_TRUE(properties);
  EXPECT_EQ("cellular_guid", properties->guid);
  EXPECT_EQ(mojom::NetworkType::kCellular, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
            properties->connection_state);
  ASSERT_TRUE(properties->type_properties);
  mojom::ManagedCellularPropertiesPtr& cellular =
      properties->type_properties->get_cellular();
  ASSERT_TRUE(cellular);
  EXPECT_EQ(0, cellular->signal_strength);
  EXPECT_EQ("LTE", cellular->network_technology);
  EXPECT_EQ(mojom::ActivationStateType::kActivated, cellular->activation_state);

  properties = GetManagedProperties("vpn_guid");
  ASSERT_TRUE(properties);
  EXPECT_EQ("vpn_guid", properties->guid);
  EXPECT_EQ(mojom::NetworkType::kVPN, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kConnecting,
            properties->connection_state);
  ASSERT_TRUE(properties->type_properties);
  ASSERT_TRUE(properties->type_properties->is_vpn());
  EXPECT_EQ(mojom::VpnType::kL2TPIPsec,
            properties->type_properties->get_vpn()->type);
}

// Test managed property policy values.
TEST_F(CrosNetworkConfigTest, GetManagedPropertiesPolicy) {
  mojom::ManagedPropertiesPtr properties = GetManagedProperties("wifi1_guid");
  ASSERT_TRUE(properties);
  ASSERT_EQ("wifi1_guid", properties->guid);
  ASSERT_TRUE(properties->type_properties);
  mojom::ManagedWiFiProperties* wifi =
      properties->type_properties->get_wifi().get();
  ASSERT_TRUE(wifi);
  ASSERT_TRUE(wifi->auto_connect);
  EXPECT_TRUE(wifi->auto_connect->active_value);
  EXPECT_EQ(mojom::PolicySource::kNone, wifi->auto_connect->policy_source);

  properties = GetManagedProperties("wifi2_guid");
  ASSERT_TRUE(properties);
  ASSERT_EQ("wifi2_guid", properties->guid);
  ASSERT_TRUE(properties->type_properties);
  wifi = properties->type_properties->get_wifi().get();
  ASSERT_TRUE(wifi);
  ASSERT_TRUE(wifi->auto_connect);
  EXPECT_TRUE(wifi->auto_connect->active_value);
  EXPECT_EQ(mojom::PolicySource::kUserPolicyEnforced,
            wifi->auto_connect->policy_source);
  EXPECT_TRUE(wifi->auto_connect->policy_value);
  ASSERT_TRUE(properties->name);
  EXPECT_EQ("wifi2", properties->name->active_value);
  EXPECT_EQ(mojom::PolicySource::kUserPolicyEnforced,
            properties->name->policy_source);
  EXPECT_EQ("wifi2", properties->name->policy_value);
  ASSERT_TRUE(properties->priority);
  EXPECT_EQ(0, properties->priority->active_value);
  EXPECT_EQ(mojom::PolicySource::kUserPolicyEnforced,
            properties->priority->policy_source);
  EXPECT_EQ(0, properties->priority->policy_value);
}

// Test managed EAP properties which are merged from a separate EthernetEAP
// Shill service.
TEST_F(CrosNetworkConfigTest, GetManagedPropertiesEAP) {
  SetupEthernetEAP();
  mojom::ManagedPropertiesPtr properties = GetManagedProperties("eth_guid");
  ASSERT_TRUE(properties);
  EXPECT_EQ("eth_guid", properties->guid);
  EXPECT_EQ(mojom::NetworkType::kEthernet, properties->type);
  ASSERT_TRUE(properties->type_properties);
  mojom::ManagedEthernetProperties* ethernet =
      properties->type_properties->get_ethernet().get();
  ASSERT_TRUE(ethernet);
  ASSERT_TRUE(ethernet->authentication);
  EXPECT_EQ("8021X", ethernet->authentication->active_value);
  ASSERT_TRUE(ethernet->eap);
  ASSERT_TRUE(ethernet->eap->identity);
  EXPECT_EQ("user1", ethernet->eap->identity->active_value);
}

TEST_F(CrosNetworkConfigTest, SetProperties) {
  // Use wifi3 since it has a profile path (i.e. it is 'saved'). and is not
  // policy controoled.
  const char* kGUID = "wifi3_guid";
  // Assert initial state.
  mojom::ManagedPropertiesPtr properties = GetManagedProperties(kGUID);
  ASSERT_TRUE(properties);
  ASSERT_EQ(kGUID, properties->guid);
  ASSERT_TRUE(properties->type_properties);
  mojom::ManagedWiFiProperties* wifi =
      properties->type_properties->get_wifi().get();
  ASSERT_TRUE(wifi);
  ASSERT_FALSE(wifi->auto_connect);
  ASSERT_FALSE(properties->priority);

  // Set priority.
  auto config = mojom::ConfigProperties::New();
  config->type_config = mojom::NetworkTypeConfigProperties::NewWifi(
      mojom::WiFiConfigProperties::New());
  config->priority = mojom::PriorityConfig::New();
  config->priority->value = 1;
  bool success = SetProperties(kGUID, std::move(config));
  ASSERT_TRUE(success);
  properties = GetManagedProperties(kGUID);
  ASSERT_TRUE(properties);
  ASSERT_EQ(kGUID, properties->guid);
  ASSERT_TRUE(properties->type_properties);
  wifi = properties->type_properties->get_wifi().get();
  ASSERT_TRUE(wifi);
  ASSERT_FALSE(wifi->auto_connect);
  ASSERT_TRUE(properties->priority);
  EXPECT_EQ(1, properties->priority->active_value);

  // Set auto connect only. Priority should remain unchanged. Also provide a
  // matching |guid|.
  config = mojom::ConfigProperties::New();
  config->type_config = mojom::NetworkTypeConfigProperties::NewWifi(
      mojom::WiFiConfigProperties::New());
  config->auto_connect = mojom::AutoConnectConfig::New();
  config->auto_connect->value = true;
  config->guid = kGUID;
  success = SetProperties(kGUID, std::move(config));
  ASSERT_TRUE(success);
  properties = GetManagedProperties(kGUID);
  ASSERT_TRUE(properties);
  ASSERT_EQ(kGUID, properties->guid);
  ASSERT_TRUE(properties->type_properties);
  wifi = properties->type_properties->get_wifi().get();
  ASSERT_TRUE(wifi);
  ASSERT_TRUE(wifi->auto_connect);
  EXPECT_TRUE(wifi->auto_connect->active_value);
  ASSERT_TRUE(properties->priority);
  EXPECT_EQ(1, properties->priority->active_value);

  // Set auto connect with a mismatched guid; call should fail.
  config = mojom::ConfigProperties::New();
  config->type_config = mojom::NetworkTypeConfigProperties::NewWifi(
      mojom::WiFiConfigProperties::New());
  config->auto_connect = mojom::AutoConnectConfig::New();
  config->auto_connect->value = false;
  config->guid = "Mismatched guid";
  success = SetProperties(kGUID, std::move(config));
  EXPECT_FALSE(success);
}

TEST_F(CrosNetworkConfigTest, CustomAPN) {
  SetupAPNList();
  const char* kGUID = "cellular_guid";
  // Verify that setting APN to an entry that already exists in apn list
  // does not update the custom apn list.
  auto config = mojom::ConfigProperties::New();
  auto cellular_config = mojom::CellularConfigProperties::New();
  auto new_apn = mojom::ApnProperties::New();
  new_apn->access_point_name = kCellularTestApn1;
  new_apn->name = kCellularTestApnName1;
  new_apn->username = kCellularTestApnUsername1;
  new_apn->password = kCellularTestApnPassword1;
  cellular_config->apn = std::move(new_apn);
  config->type_config = mojom::NetworkTypeConfigProperties::NewCellular(
      std::move(cellular_config));
  SetProperties(kGUID, std::move(config));
  const base::Value* apn_list =
      NetworkHandler::Get()->network_metadata_store()->GetCustomAPNList(kGUID);
  ASSERT_FALSE(apn_list);

  // Verify that custom APN list is updated properly.
  config = mojom::ConfigProperties::New();
  cellular_config = mojom::CellularConfigProperties::New();
  new_apn = mojom::ApnProperties::New();
  new_apn->access_point_name = kCellularTestApn3;
  new_apn->name = kCellularTestApnName3;
  new_apn->username = kCellularTestApnUsername3;
  new_apn->password = kCellularTestApnPassword3;
  cellular_config->apn = std::move(new_apn);
  config->type_config = mojom::NetworkTypeConfigProperties::NewCellular(
      std::move(cellular_config));
  SetProperties(kGUID, std::move(config));
  apn_list =
      NetworkHandler::Get()->network_metadata_store()->GetCustomAPNList(kGUID);
  ASSERT_TRUE(apn_list);
  ASSERT_TRUE(apn_list->is_list());

  // Verify that custom APN list is returned properly in managed properties.
  mojom::ManagedPropertiesPtr properties = GetManagedProperties(kGUID);
  ASSERT_TRUE(properties);
  ASSERT_EQ(kGUID, properties->guid);
  ASSERT_TRUE(properties->type_properties->is_cellular());
  ASSERT_TRUE(
      properties->type_properties->get_cellular()->custom_apn_list.has_value());
  ASSERT_EQ(
      1u, properties->type_properties->get_cellular()->custom_apn_list->size());
  ASSERT_EQ(kCellularTestApn3, properties->type_properties->get_cellular()
                                   ->custom_apn_list->front()
                                   ->access_point_name);
}

TEST_F(CrosNetworkConfigTest, ConfigureNetwork) {
  // Note: shared = false requires a UserManager instance.
  bool shared = true;
  const std::string ssid = "new_wifi_ssid";
  // Configure a new wifi network.
  auto config = mojom::ConfigProperties::New();
  config->name = ssid;
  auto wifi = mojom::WiFiConfigProperties::New();
  wifi->ssid = ssid;
  config->type_config =
      mojom::NetworkTypeConfigProperties::NewWifi(std::move(wifi));
  std::string guid = ConfigureNetwork(std::move(config), shared);
  EXPECT_FALSE(guid.empty());

  // Verify the configuration.
  mojom::NetworkStatePropertiesPtr network = GetNetworkState(guid);
  ASSERT_TRUE(network);
  EXPECT_EQ(guid, network->guid);
  EXPECT_EQ(mojom::NetworkType::kWiFi, network->type);
  EXPECT_EQ(mojom::OncSource::kDevice, network->source);
  ASSERT_TRUE(network->type_state);
  ASSERT_TRUE(network->type_state->is_wifi());
  EXPECT_EQ(ssid, network->type_state->get_wifi()->ssid);
}

TEST_F(CrosNetworkConfigTest, ConfigureNetworkExistingGuid) {
  // Note: shared = false requires a UserManager instance.
  bool shared = true;
  const std::string guid = "new_wifi_guid";
  const std::string ssid = "new_wifi_ssid";
  // Configure a new wifi network with an existing guid.
  auto config = mojom::ConfigProperties::New();
  config->guid = guid;
  config->name = ssid;
  auto wifi = mojom::WiFiConfigProperties::New();
  wifi->ssid = ssid;
  config->type_config =
      mojom::NetworkTypeConfigProperties::NewWifi(std::move(wifi));
  std::string config_guid = ConfigureNetwork(std::move(config), shared);
  // The new guid should be the same as the existing guid.
  EXPECT_EQ(config_guid, guid);
}

TEST_F(CrosNetworkConfigTest, ForgetNetwork) {
  // Use a non visible configured network.
  const std::string kGUID = "wifi3_guid";

  // Verify the configuration exists.
  mojom::ManagedPropertiesPtr properties = GetManagedProperties(kGUID);
  ASSERT_TRUE(properties);
  ASSERT_EQ(kGUID, properties->guid);

  // Forget the network and verify the configuration no longer exists.
  bool result = ForgetNetwork(kGUID);
  EXPECT_TRUE(result);
  properties = GetManagedProperties(kGUID);
  ASSERT_FALSE(properties);
}

TEST_F(CrosNetworkConfigTest, SetNetworkTypeEnabledState) {
  std::vector<mojom::DeviceStatePropertiesPtr> devices = GetDeviceStateList();
  ASSERT_EQ(4u, devices.size());
  EXPECT_EQ(mojom::NetworkType::kWiFi, devices[0]->type);
  EXPECT_EQ(mojom::DeviceStateType::kEnabled, devices[0]->device_state);

  // Disable WiFi
  bool succeeded = false;
  cros_network_config()->SetNetworkTypeEnabledState(
      mojom::NetworkType::kWiFi, false,
      base::BindOnce(
          [](bool* succeeded, bool success) { *succeeded = success; },
          &succeeded));
  // Wait for callback to complete; this test does not use mojo bindings.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(succeeded);
  devices = GetDeviceStateList();
  ASSERT_EQ(4u, devices.size());
  EXPECT_EQ(mojom::NetworkType::kWiFi, devices[0]->type);
  EXPECT_EQ(mojom::DeviceStateType::kDisabled, devices[0]->device_state);
}

TEST_F(CrosNetworkConfigTest, SetCellularSimState) {
  // Assert initial state.
  mojom::DeviceStatePropertiesPtr cellular =
      GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular);
  ASSERT_FALSE(cellular->sim_absent);
  ASSERT_TRUE(cellular->sim_lock_status);
  ASSERT_TRUE(cellular->sim_lock_status->lock_enabled);
  ASSERT_EQ(shill::kSIMLockPin, cellular->sim_lock_status->lock_type);
  const int retries = FakeShillDeviceClient::kSimPinRetryCount;
  ASSERT_EQ(retries, cellular->sim_lock_status->retries_left);

  // Unlock the sim with the correct pin. |require_pin| should be ignored.
  EXPECT_TRUE(SetCellularSimState(FakeShillDeviceClient::kDefaultSimPin,
                                  /*new_pin=*/base::nullopt,
                                  /*require_pin=*/false));

  // Sim should be unlocked, locking should still be enabled.
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_TRUE(cellular->sim_lock_status->lock_type.empty());

  // Set |require_pin| to false (disable locking).
  EXPECT_TRUE(SetCellularSimState(FakeShillDeviceClient::kDefaultSimPin,
                                  /*new_pin=*/base::nullopt,
                                  /*require_pin=*/false));

  // Sim should be unlocked, locking should be disabled.
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_FALSE(cellular->sim_lock_status->lock_enabled);
  EXPECT_TRUE(cellular->sim_lock_status->lock_type.empty());

  // Set |require_pin| to true (enable locking).
  EXPECT_TRUE(SetCellularSimState(FakeShillDeviceClient::kDefaultSimPin,
                                  /*new_pin=*/base::nullopt,
                                  /*require_pin=*/true));

  // Sim should remain unlocked, locking should be enabled.
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_TRUE(cellular->sim_lock_status->lock_type.empty());

  // Lock the sim. (Can not be done via the mojo API).
  helper().device_test()->SetSimLocked(kCellularDevicePath, true);
  base::RunLoop().RunUntilIdle();
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  ASSERT_TRUE(cellular->sim_lock_status->lock_enabled);
  ASSERT_EQ(shill::kSIMLockPin, cellular->sim_lock_status->lock_type);

  // Attempt to unlock the sim with an incorrect pin. Call should fail.
  EXPECT_FALSE(SetCellularSimState("incorrect pin", /*new_pin=*/base::nullopt,
                                   /*require_pin=*/false));

  // Ensure sim is still locked and retry count has decreased.
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_EQ(shill::kSIMLockPin, cellular->sim_lock_status->lock_type);
  EXPECT_EQ(retries - 1, cellular->sim_lock_status->retries_left);

  // Additional attempts should set the sim to puk locked.
  for (int i = retries - 1; i > 0; --i) {
    SetCellularSimState("incorrect pin", /*new_pin=*/base::nullopt, false);
  }
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_EQ(shill::kSIMLockPuk, cellular->sim_lock_status->lock_type);

  // Attempt to unblock the sim with the incorrect puk. Call should fail.
  const std::string new_pin = "2222";
  EXPECT_FALSE(SetCellularSimState("incorrect puk",
                                   base::make_optional(new_pin),
                                   /*require_pin=*/false));

  // Attempt to unblock the sim with np pin. Call should fail.
  EXPECT_FALSE(SetCellularSimState(FakeShillDeviceClient::kSimPuk,
                                   /*new_pin=*/base::nullopt,
                                   /*require_pin=*/false));

  // Attempt to unlock the sim with the correct puk.
  EXPECT_TRUE(SetCellularSimState(FakeShillDeviceClient::kSimPuk,
                                  base::make_optional(new_pin),
                                  /*require_pin=*/false));

  // Sim should be unlocked
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_type.empty());
}

TEST_F(CrosNetworkConfigTest, SelectCellularMobileNetwork) {
  // Create fake list of found networks.
  base::Optional<base::Value> found_networks_list =
      base::JSONReader::Read(base::StringPrintf(
          R"([{"network_id": "network1", "technology": "GSM",
               "status": "current"},
              {"network_id": "network2", "technology": "GSM",
               "status": "available"}])"));
  helper().device_test()->SetDeviceProperty(
      kCellularDevicePath, shill::kFoundNetworksProperty, *found_networks_list,
      /*notify_changed=*/true);

  // Assert initial state
  mojom::ManagedPropertiesPtr properties =
      GetManagedProperties("cellular_guid");
  mojom::ManagedCellularProperties* cellular =
      properties->type_properties->get_cellular().get();
  ASSERT_TRUE(cellular);
  ASSERT_TRUE(cellular->found_networks);
  const std::vector<mojom::FoundNetworkPropertiesPtr>& found_networks1 =
      *(cellular->found_networks);
  ASSERT_EQ(2u, found_networks1.size());
  EXPECT_EQ("current", found_networks1[0]->status);
  EXPECT_EQ("available", found_networks1[1]->status);

  // Select "network2"
  EXPECT_TRUE(SelectCellularMobileNetwork("cellular_guid", "network2"));
  properties = GetManagedProperties("cellular_guid");
  cellular = properties->type_properties->get_cellular().get();
  ASSERT_TRUE(cellular);
  ASSERT_TRUE(cellular->found_networks);
  const std::vector<mojom::FoundNetworkPropertiesPtr>& found_networks2 =
      *(cellular->found_networks);
  ASSERT_EQ(2u, found_networks2.size());
  EXPECT_EQ("available", found_networks2[0]->status);
  EXPECT_EQ("current", found_networks2[1]->status);
}

TEST_F(CrosNetworkConfigTest, RequestNetworkScan) {
  // Observe device state list changes and track when the wifi scanning state
  // gets set to true. Note: In the test the scan will complete immediately and
  // the scanning state will get set back to false, so ignore that change.
  class ScanningObserver : public CrosNetworkConfigTestObserver {
   public:
    explicit ScanningObserver(CrosNetworkConfig* cros_network_config)
        : cros_network_config_(cros_network_config) {}
    void OnDeviceStateListChanged() override {
      cros_network_config_->GetDeviceStateList(base::BindOnce(
          [](bool* wifi_scanning,
             std::vector<mojom::DeviceStatePropertiesPtr> devices) {
            for (auto& device : devices) {
              if (device->type == mojom::NetworkType::kWiFi && device->scanning)
                *wifi_scanning = true;
            }
          },
          &wifi_scanning_));
    }
    CrosNetworkConfig* cros_network_config_;
    bool wifi_scanning_ = false;
  };
  ScanningObserver observer(cros_network_config());
  cros_network_config()->AddObserver(observer.GenerateRemote());
  base::RunLoop().RunUntilIdle();

  cros_network_config()->RequestNetworkScan(mojom::NetworkType::kWiFi);
  base::RunLoop().RunUntilIdle();
  observer.FlushForTesting();
  EXPECT_TRUE(observer.wifi_scanning_);
}

TEST_F(CrosNetworkConfigTest, GetGlobalPolicy) {
  base::DictionaryValue global_config;
  global_config.SetBoolKey(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      true);
  global_config.SetBoolKey(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToConnect, false);
  base::Value blocked(base::Value::Type::LIST);
  blocked.Append(base::Value("blocked_ssid1"));
  blocked.Append(base::Value("blocked_ssid2"));
  global_config.SetKey(::onc::global_network_config::kBlockedHexSSIDs,
                       std::move(blocked));
  managed_network_configuration_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
      base::ListValue(), global_config);
  base::RunLoop().RunUntilIdle();
  mojom::GlobalPolicyPtr policy = GetGlobalPolicy();
  ASSERT_TRUE(policy);
  EXPECT_EQ(true, policy->allow_only_policy_networks_to_autoconnect);
  EXPECT_EQ(false, policy->allow_only_policy_networks_to_connect);
  EXPECT_EQ(false, policy->allow_only_policy_networks_to_connect_if_available);
  ASSERT_EQ(2u, policy->blocked_hex_ssids.size());
  EXPECT_EQ("blocked_ssid1", policy->blocked_hex_ssids[0]);
  EXPECT_EQ("blocked_ssid2", policy->blocked_hex_ssids[1]);
}

TEST_F(CrosNetworkConfigTest, StartConnect) {
  // wifi1 is already connected, StartConnect should fail.
  mojom::StartConnectResult result = StartConnect("wifi1_guid");
  EXPECT_EQ(mojom::StartConnectResult::kInvalidState, result);

  // wifi2 is not connected, StartConnect should succeed and connection_state
  // should change to connecting.
  mojom::NetworkStatePropertiesPtr network = GetNetworkState("wifi2_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
            network->connection_state);
  result = StartConnect("wifi2_guid");
  EXPECT_EQ(mojom::StartConnectResult::kSuccess, result);
  network = GetNetworkState("wifi2_guid");
  EXPECT_EQ(mojom::ConnectionStateType::kConnecting, network->connection_state);
  // Wait for disconnect to complete.
  base::RunLoop().RunUntilIdle();
  network = GetNetworkState("wifi2_guid");
  EXPECT_EQ(mojom::ConnectionStateType::kOnline, network->connection_state);
}

TEST_F(CrosNetworkConfigTest, StartDisconnect) {
  // wifi1 is connected, StartDisconnect should succeed and connection_state
  // should change to disconnected.
  mojom::NetworkStatePropertiesPtr network = GetNetworkState("wifi1_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ(mojom::ConnectionStateType::kConnected, network->connection_state);
  bool success = StartDisconnect("wifi1_guid");
  EXPECT_TRUE(success);
  // Wait for disconnect to complete.
  base::RunLoop().RunUntilIdle();
  network = GetNetworkState("wifi1_guid");
  EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
            network->connection_state);

  // wifi1 is now disconnected, StartDisconnect should fail.
  success = StartDisconnect("wifi1_guid");
  EXPECT_FALSE(success);
}

TEST_F(CrosNetworkConfigTest, VpnProviders) {
  SetupObserver();
  ASSERT_EQ(0, observer()->vpn_providers_changed());

  mojom::VpnProvider provider1(mojom::VpnType::kExtension, "provider1",
                               "provider_name1", "", base::Time());
  mojom::VpnProvider provider2(mojom::VpnType::kArc, "provider2",
                               "provider_name2", "app2", base::Time());
  std::vector<mojom::VpnProviderPtr> providers;
  providers.push_back(provider1.Clone());
  providers.push_back(provider2.Clone());
  cros_network_config()->SetVpnProviders(std::move(providers));

  std::vector<mojom::VpnProviderPtr> providers_received = GetVpnProviders();
  ASSERT_EQ(2u, providers_received.size());
  EXPECT_TRUE(providers_received[0]->Equals(provider1));
  EXPECT_TRUE(providers_received[1]->Equals(provider2));

  base::RunLoop().RunUntilIdle();  // Ensure observers run.
  ASSERT_EQ(1, observer()->vpn_providers_changed());
}

TEST_F(CrosNetworkConfigTest, NetworkCertificates) {
  SetupObserver();
  ASSERT_EQ(0, observer()->network_certificates_changed());

  std::vector<mojom::NetworkCertificatePtr> server_cas;
  std::vector<mojom::NetworkCertificatePtr> user_certs;
  GetNetworkCertificates(&server_cas, &user_certs);
  EXPECT_EQ(0u, server_cas.size());
  EXPECT_EQ(0u, user_certs.size());

  network_certificate_handler()->AddAuthorityCertificateForTest(
      "authority_cert");
  base::RunLoop().RunUntilIdle();  // Ensure observers run.
  ASSERT_EQ(1, observer()->network_certificates_changed());

  GetNetworkCertificates(&server_cas, &user_certs);
  EXPECT_EQ(1u, server_cas.size());
  EXPECT_EQ(0u, user_certs.size());
}

TEST_F(CrosNetworkConfigTest, NetworkListChanged) {
  SetupObserver();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer()->network_state_list_changed());

  // Add a wifi network.
  helper().ConfigureService(
      R"({"GUID": "wifi3_guid", "Type": "wifi", "State": "ready"})");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer()->network_state_list_changed());
}

TEST_F(CrosNetworkConfigTest, DeviceListChanged) {
  SetupObserver();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer()->device_state_list_changed());

  // Disable wifi
  helper().network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), false, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  // This will trigger three device list updates. First when wifi is in the
  // disabling state, next when it's actually disabled, and lastly when
  // Device::available_managed_network_path_ changes.
  EXPECT_EQ(3, observer()->device_state_list_changed());
}

TEST_F(CrosNetworkConfigTest, ActiveNetworksChanged) {
  SetupObserver();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer()->active_networks_changed());

  // Change a network state.
  helper().SetServiceProperty(wifi1_path(), shill::kStateProperty,
                              base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer()->active_networks_changed());
}

TEST_F(CrosNetworkConfigTest, NetworkStateChanged) {
  SetupObserver();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer()->GetNetworkChangedCount("wifi1_guid"));
  EXPECT_EQ(0, observer()->GetNetworkChangedCount("wifi2_guid"));

  // Change a network state.
  helper().SetServiceProperty(wifi1_path(), shill::kSignalStrengthProperty,
                              base::Value(10));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer()->GetNetworkChangedCount("wifi1_guid"));
  EXPECT_EQ(0, observer()->GetNetworkChangedCount("wifi2_guid"));
}

// Do not forward information about proxies set by policy.
// |NetworkStatePropertiesPtr::proxy_mode| is used to show a privacy warning in
// the system tray. This warning should not be shown for managed networks.
TEST_F(CrosNetworkConfigTest, PolicyEnforcedProxyMode) {
  // Proxies enforced by policy and/or extension are set in the kProxy
  // preference.
  base::Value policy_prefs_config = ProxyConfigDictionary::CreateAutoDetect();
  user_prefs_.SetUserPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(policy_prefs_config)));

  mojom::NetworkStatePropertiesPtr network = GetNetworkState("wifi2_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ(network->proxy_mode, mojom::ProxyMode::kDirect);
}

}  // namespace network_config
}  // namespace chromeos
