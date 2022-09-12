// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/cros_network_config.h"

#include <tuple>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_device_client.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/cellular_metrics_logger.h"
#include "chromeos/ash/components/network/fake_stub_cellular_networks_provider.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/network_certificate_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "chromeos/ash/components/network/prohibited_technologies_handler.h"
#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"
#include "chromeos/ash/components/network/system_token_cert_db_storage.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_observer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-shared.h"
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
const char kCellularTestIccid[] = "1234567890";

const char kCellularTestApn1[] = "TEST.APN1";
const char kCellularTestApnName1[] = "Test Apn 1";
const char kCellularTestApnUsername1[] = "Test User";
const char kCellularTestApnPassword1[] = "Test Pass";
const char kCellularTestApnAttach1[] = "";

const char kCellularTestApn2[] = "TEST.APN2";
const char kCellularTestApnName2[] = "Test Apn 2";
const char kCellularTestApnUsername2[] = "Test User";
const char kCellularTestApnPassword2[] = "Test Pass";
const char kCellularTestApnAttach2[] = "";

const char kCellularTestApn3[] = "TEST.APN3";
const char kCellularTestApnName3[] = "Test Apn 3";
const char kCellularTestApnUsername3[] = "Test User";
const char kCellularTestApnPassword3[] = "Test Pass";
const char kCellularTestApnAttach3[] = "attach";

// Escaped twice, as it will be embedded as part of a JSON string, which should
// have a single level of escapes still present.
const char kOpenVPNTLSAuthContents[] =
    "-----BEGIN OpenVPN Static key V1-----\\n"
    "83f8e7ccd99be189b4663e18615f9166\\n"
    "d885cdea6c8accb0ebf5be304f0b8081\\n"
    "5404f2a6574e029815d7a2fb65b83d0c\\n"
    "676850714c6a56b23415a78e06aad6b1\\n"
    "34900dd512049598382039e4816cb5ff\\n"
    "1848532b71af47578c9b4a14b5bca49f\\n"
    "99e0ae4dae2f4e5eadfea374aeb8fb1e\\n"
    "a6fdf02adc73ea778dfd43d64bf7bc75\\n"
    "7779d629498f8c2fbfd32812bfdf6df7\\n"
    "8cebafafef3e5496cb13202274f2768a\\n"
    "1959bc53d67a70945c4c8c6f34b63327\\n"
    "fb60dc84990ffec1243461e0b6310f61\\n"
    "e90aee1f11fb6292d6f5fcd7cd508aab\\n"
    "50d80f9963589c148cb4b933ec86128d\\n"
    "ed77d3fad6005b62f36369e2319f52bd\\n"
    "09c6d2e52cce2362a05009dc29b6b39a\\n"
    "-----END OpenVPN Static key V1-----\\n";

enum ComparisonType {
  INTEGER = 0,
  DOUBLE,
};

void CompareTrafficCounters(
    const std::vector<mojom::TrafficCounterPtr>& actual_traffic_counters,
    const base::Value* expected_traffic_counters,
    enum ComparisonType comparison_type) {
  EXPECT_EQ(actual_traffic_counters.size(),
            expected_traffic_counters->GetListDeprecated().size());
  for (size_t i = 0; i < actual_traffic_counters.size(); i++) {
    auto& actual_tc = actual_traffic_counters[i];
    auto& expected_tc = expected_traffic_counters->GetListDeprecated()[i];
    EXPECT_EQ(actual_tc->source,
              CrosNetworkConfig::GetTrafficCounterEnumForTesting(
                  expected_tc.FindKey("source")->GetString()));
    if (comparison_type == ComparisonType::INTEGER) {
      EXPECT_EQ(actual_tc->rx_bytes,
                (size_t)expected_tc.FindKey("rx_bytes")->GetInt());
      EXPECT_EQ(actual_tc->tx_bytes,
                (size_t)expected_tc.FindKey("tx_bytes")->GetInt());
    } else if (comparison_type == ComparisonType::DOUBLE) {
      EXPECT_EQ(actual_tc->rx_bytes,
                (size_t)expected_tc.FindKey("rx_bytes")->GetDouble());
      EXPECT_EQ(actual_tc->tx_bytes,
                (size_t)expected_tc.FindKey("tx_bytes")->GetDouble());
    }
  }
}

}  // namespace

class CrosNetworkConfigTest : public testing::Test {
 public:
  CrosNetworkConfigTest() {
    LoginState::Initialize();
    SystemTokenCertDbStorage::Initialize();
    NetworkCertLoader::Initialize();
    helper_ = std::make_unique<NetworkHandlerTestHelper>();
    helper_->AddDefaultProfiles();
    helper_->ResetDevicesAndServices();

    helper_->RegisterPrefs(user_prefs_.registry(), local_state_.registry());
    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());

    helper_->InitializePrefs(&user_prefs_, &local_state_);

    NetworkHandler* network_handler = NetworkHandler::Get();
    cros_network_config_ = std::make_unique<CrosNetworkConfig>(
        network_handler->network_state_handler(),
        network_handler->network_device_handler(),
        network_handler->cellular_inhibitor(),
        network_handler->cellular_esim_profile_handler(),
        network_handler->managed_network_configuration_handler(),
        network_handler->network_connection_handler(),
        network_handler->network_certificate_handler(),
        network_handler->network_profile_handler());
    SetupPolicy();
    SetupNetworks();
  }

  CrosNetworkConfigTest(const CrosNetworkConfigTest&) = delete;
  CrosNetworkConfigTest& operator=(const CrosNetworkConfigTest&) = delete;

  ~CrosNetworkConfigTest() override {
    cros_network_config_.reset();
    helper_.reset();
    NetworkCertLoader::Shutdown();
    SystemTokenCertDbStorage::Shutdown();
    LoginState::Shutdown();
  }

  void SetupPolicy() {
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler =
        NetworkHandler::Get()->managed_network_configuration_handler();
    managed_network_configuration_handler->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY,
        /*userhash=*/std::string(),
        /*network_configs_onc=*/base::ListValue(),
        /*global_network_config=*/base::DictionaryValue());

    const std::string user_policy_ssid = "wifi2";
    base::Value wifi2_onc = onc::ReadDictionaryFromJson(base::StringPrintf(
        R"({"GUID": "wifi2_guid", "Type": "WiFi",
                "Name": "wifi2", "Priority": 0,
                "WiFi": { "Passphrase": "fake", "SSID": "%s", "HexSSID": "%s",
                          "Security": "WPA-PSK", "AutoConnect": true}})",
        user_policy_ssid.c_str(),
        base::HexEncode(user_policy_ssid.c_str(), user_policy_ssid.size())
            .c_str()));

    base::Value wifi_eap_onc =
        onc::ReadDictionaryFromJson(R"({ "GUID": "wifi_eap",
      "Name": "wifi_eap",
      "Type": "WiFi",
      "WiFi": {
         "AutoConnect": true,
         "EAP": {
            "Inner": "MD5",
            "Outer": "PEAP",
            "SubjectAlternativeNameMatch": [
              { "Type": "DNS" , "Value" : "example.com"},
              {"Type" : "EMAIL", "Value" : "test@example.com"}],
            "DomainSuffixMatch": ["example1.com","example2.com"],
            "Recommended": [ "AnonymousIdentity", "Identity", "Password",
              "DomainSuffixMatch" , "SubjectAlternativeNameMatch"],
            "UseSystemCAs": true
         },
         "SSID": "wifi_eap",
         "Security": "WPA-EAP"
      }
}  )");

    base::Value openvpn_onc = onc::ReadDictionaryFromJson(base::StringPrintf(
        R"({ "GUID": "openvpn_guid", "Name": "openvpn", "Type": "VPN", "VPN": {
          "Host": "my.vpn.example.com", "Type": "OpenVPN", "OpenVPN": {
          "Auth": "MD5", "Cipher": "AES-192-CBC", "ClientCertType": "None",
          "CompressionAlgorithm": "LZO", "KeyDirection": "1",
          "TLSAuthContents": "%s"}}})",
        kOpenVPNTLSAuthContents));

    base::ListValue user_policy_onc;
    user_policy_onc.Append(std::move(wifi2_onc));
    user_policy_onc.Append(std::move(wifi_eap_onc));
    user_policy_onc.Append(std::move(openvpn_onc));
    managed_network_configuration_handler->SetPolicy(
        ::onc::ONC_SOURCE_USER_POLICY, helper()->UserHash(), user_policy_onc,
        /*global_network_config=*/base::DictionaryValue());
    base::RunLoop().RunUntilIdle();
  }

  void AddSimSlotInfoToList(base::Value::List& ordered_sim_slot_info_list,
                            const std::string& eid,
                            const std::string& iccid,
                            bool primary = false) {
    base::Value::Dict item;
    item.Set(shill::kSIMSlotInfoEID, eid);
    item.Set(shill::kSIMSlotInfoICCID, iccid);
    item.Set(shill::kSIMSlotInfoPrimary, primary);
    ordered_sim_slot_info_list.Append(std::move(item));
  }

  void SetupNetworks() {
    // Wifi device exists by default, add Ethernet and Cellular.
    helper()->device_test()->AddDevice("/device/stub_eth_device",
                                       shill::kTypeEthernet, "stub_eth_device");
    helper()->manager_test()->AddTechnology(shill::kTypeCellular,
                                            true /* enabled */);
    helper()->device_test()->AddDevice(
        kCellularDevicePath, shill::kTypeCellular, "stub_cellular_device");
    base::Value sim_value(base::Value::Type::DICTIONARY);
    sim_value.SetKey(shill::kSIMLockEnabledProperty, base::Value(true));
    sim_value.SetKey(shill::kSIMLockTypeProperty,
                     base::Value(shill::kSIMLockPin));
    sim_value.SetKey(shill::kSIMLockRetriesLeftProperty,
                     base::Value(kSimRetriesLeft));
    helper()->device_test()->SetDeviceProperty(
        kCellularDevicePath, shill::kSIMLockStatusProperty, sim_value,
        /*notify_changed=*/false);
    helper()->device_test()->SetDeviceProperty(kCellularDevicePath,
                                               shill::kIccidProperty,
                                               base::Value(kCellularTestIccid),
                                               /*notify_changed=*/false);
    helper()->device_test()->SetDeviceProperty(
        kCellularDevicePath, shill::kSIMPresentProperty, base::Value(true),
        /*notify_changed=*/false);

    // Setup SimSlotInfo
    base::Value::List ordered_sim_slot_info_list;
    AddSimSlotInfoToList(ordered_sim_slot_info_list, /*eid=*/"",
                         kCellularTestIccid,
                         /*primary=*/true);
    helper()->device_test()->SetDeviceProperty(
        kCellularDevicePath, shill::kSIMSlotInfoProperty,
        base::Value(std::move(ordered_sim_slot_info_list)),
        /*notify_changed=*/false);

    // Note: These are Shill dictionaries, not ONC.
    helper()->ConfigureService(
        R"({"GUID": "eth_guid", "Type": "ethernet", "State": "online"})");
    wifi1_path_ = helper()->ConfigureService(
        R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "ready",
            "Strength": 50, "AutoConnect": true, "WiFi.HiddenSSID": false,
            "TrafficCounterResetTime": 123456789987654})");
    helper()->ConfigureService(
        R"({"GUID": "wifi2_guid", "Type": "wifi", "SSID": "wifi2",
            "State": "idle", "SecurityClass": "psk", "Strength": 100,
            "Profile": "user_profile_path", "WiFi.HiddenSSID": true})");
    helper()->ConfigureService(base::StringPrintf(
        R"({"GUID": "cellular_guid", "Type": "cellular",  "State": "idle",
            "Strength": 0, "Cellular.NetworkTechnology": "LTE",
            "Cellular.ActivationState": "activated", "Cellular.ICCID": "%s",
            "Profile": "%s"})",
        kCellularTestIccid,
        NetworkProfileHandler::GetSharedProfilePath().c_str()));
    vpn_path_ = helper()->ConfigureService(
        R"({"GUID": "vpn_l2tp_guid", "Type": "vpn", "State": "association",
            "Provider": {"Type": "l2tpipsec"}})");
    helper()->ConfigureService(base::StringPrintf(
        R"({"GUID":"openvpn_guid", "Type": "vpn", "Name": "openvpn",
          "Provider.Host": "vpn.my.domain.com", "Provider.Type": "openvpn",
          "OpenVPN.Auth": "MD5", "OpenVPN.Cipher": "AES-192-CBC",
          "OpenVPN.Compress": "lzo", "OpenVPN.KeyDirection": "1",
          "OpenVPN.TLSAuthContents": "%s"})",
        kOpenVPNTLSAuthContents));
    helper()->ConfigureService(R"({"GUID": "vpn_ikev2_guid", "Type": "vpn",
            "State": "idle", "Provider": {"Type": "ikev2"}})");

    // Add a non visible configured wifi service.
    std::string wifi3_path = helper()->ConfigureService(
        R"({"GUID": "wifi3_guid", "Type": "wifi", "SecurityClass": "psk",
            "Visible": false})");
    helper()->profile_test()->AddService(
        NetworkProfileHandler::GetSharedProfilePath(), wifi3_path);

    // Syncable wifi network:
    std::string service_path = helper()->ConfigureService(
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
    apn_entry2.SetStringKey(shill::kApnAttachProperty, kCellularTestApnAttach2);
    apn_list.Append(std::move(apn_entry2));

    helper()->device_test()->SetDeviceProperty(
        kCellularDevicePath, shill::kCellularApnListProperty, apn_list,
        /*notify_changed=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetupEthernetEAP() {
    std::string eap_path = helper()->ConfigureService(
        R"({"GUID": "eth_eap_guid", "Type": "etherneteap",
            "State": "online", "EAP.EAP": "TTLS", "EAP.Identity": "user1"})");
    helper()->profile_test()->AddService(
        NetworkProfileHandler::GetSharedProfilePath(), eap_path);
    base::RunLoop().RunUntilIdle();
  }

  void SetupTestESimProfile(const std::string& eid,
                            const std::string& iccid,
                            const std::string& service_path,
                            const std::string& profile_name,
                            const std::string& profile_nickname) {
    const char kTestEuiccPath[] = "euicc_path";
    const char kTestESimProfilePath[] = "profile_path";

    helper()->hermes_manager_test()->AddEuicc(dbus::ObjectPath(kTestEuiccPath),
                                              eid, /*is_active=*/true,
                                              /*physical_slot=*/0);
    helper()->hermes_euicc_test()->AddCarrierProfile(
        dbus::ObjectPath(kTestESimProfilePath),
        dbus::ObjectPath(kTestEuiccPath), iccid, profile_name, profile_nickname,
        "service_provider", "activation_code", service_path,
        hermes::profile::State::kInactive,
        hermes::profile::ProfileClass::kOperational,
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);
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
               const absl::optional<std::string>& guid,
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
                           absl::optional<std::string> new_pin,
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

  mojom::AlwaysOnVpnPropertiesPtr GetAlwaysOnVpn() {
    mojom::AlwaysOnVpnPropertiesPtr result;
    base::RunLoop run_loop;
    cros_network_config()->GetAlwaysOnVpn(base::BindOnce(
        [](mojom::AlwaysOnVpnPropertiesPtr* result,
           base::OnceClosure quit_closure,
           mojom::AlwaysOnVpnPropertiesPtr properties) {
          *result = std::move(properties);
          std::move(quit_closure).Run();
        },
        &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  void SetAlwaysOnVpn(mojom::AlwaysOnVpnPropertiesPtr properties) {
    cros_network_config()->SetAlwaysOnVpn(std::move(properties));
    base::RunLoop().RunUntilIdle();
  }

  std::vector<std::string> GetSupportedVpnTypes() {
    std::vector<std::string> result;
    base::RunLoop run_loop;
    cros_network_config()->GetSupportedVpnTypes(base::BindOnce(
        [](std::vector<std::string>& result, base::OnceClosure quit_closure,
           const std::vector<std::string>& return_value) {
          result = std::move(return_value);
          std::move(quit_closure).Run();
        },
        std::ref(result), run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  bool ContainsVpnDeviceState(
      std::vector<mojom::DeviceStatePropertiesPtr> devices) {
    for (auto& device : devices) {
      if (device->type == mojom::NetworkType::kVPN) {
        return true;
      }
    }
    return false;
  }

  std::unique_ptr<CellularInhibitor::InhibitLock> InhibitCellularScanning(
      CellularInhibitor::InhibitReason inhibit_reason) {
    base::RunLoop run_loop;

    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock;
    NetworkHandler::Get()->cellular_inhibitor()->InhibitCellularScanning(
        inhibit_reason,
        base::BindLambdaForTesting(
            [&](std::unique_ptr<CellularInhibitor::InhibitLock> result) {
              inhibit_lock = std::move(result);
              run_loop.Quit();
            }));

    run_loop.Run();
    return inhibit_lock;
  }

  void RequestTrafficCountersAndCompareTrafficCounters(
      const std::string& guid,
      base::Value traffic_counters,
      ComparisonType comparison_type) {
    base::RunLoop run_loop;
    cros_network_config()->RequestTrafficCounters(
        guid,
        base::BindOnce(
            [](base::Value* expected_traffic_counters, ComparisonType* type,
               base::OnceClosure quit_closure,
               std::vector<mojom::TrafficCounterPtr> actual_traffic_counters) {
              CompareTrafficCounters(actual_traffic_counters,
                                     expected_traffic_counters, *type);
              std::move(quit_closure).Run();
            },
            &traffic_counters, &comparison_type, run_loop.QuitClosure()));
    run_loop.Run();
  }

  void SetTrafficCountersAutoResetAndCompare(const std::string& guid,
                                             bool auto_reset,
                                             mojom::UInt32ValuePtr day,
                                             bool expected_success,
                                             base::Value* expected_auto_reset,
                                             base::Value* expected_reset_day) {
    base::RunLoop run_loop;
    cros_network_config()->SetTrafficCountersAutoReset(
        guid, auto_reset, day ? std::move(day) : nullptr,
        base::BindOnce(
            [](const std::string* const guid, bool* expected_success,
               base::Value* expected_auto_reset,
               base::Value* expected_reset_day,
               NetworkMetadataStore* network_metadata_store,
               base::OnceClosure quit_closure, bool success) {
              EXPECT_EQ(*expected_success, success);
              const base::Value* actual_auto_reset =
                  network_metadata_store->GetEnableTrafficCountersAutoReset(
                      *guid);
              const base::Value* actual_reset_day =
                  network_metadata_store->GetDayOfTrafficCountersAutoReset(
                      *guid);
              if (expected_auto_reset) {
                EXPECT_TRUE(actual_auto_reset);
                EXPECT_EQ(*expected_auto_reset, *actual_auto_reset);
              } else {
                EXPECT_EQ(actual_auto_reset, nullptr);
              }
              if (expected_reset_day) {
                EXPECT_TRUE(actual_reset_day);
                EXPECT_EQ(*expected_reset_day, *actual_reset_day);
              } else {
                EXPECT_EQ(actual_reset_day, nullptr);
              }
              std::move(quit_closure).Run();
            },
            &guid, &expected_success, expected_auto_reset, expected_reset_day,
            network_metadata_store(), run_loop.QuitClosure()));
    run_loop.Run();
  }

  NetworkHandlerTestHelper* helper() { return helper_.get(); }
  CrosNetworkConfigTestObserver* observer() { return observer_.get(); }
  CrosNetworkConfig* cros_network_config() {
    return cros_network_config_.get();
  }
  ManagedNetworkConfigurationHandler* managed_network_configuration_handler() {
    return NetworkHandler::Get()->managed_network_configuration_handler();
  }
  NetworkCertificateHandler* network_certificate_handler() {
    return NetworkHandler::Get()->network_certificate_handler();
  }
  NetworkMetadataStore* network_metadata_store() {
    return NetworkHandler::Get()->network_metadata_store();
  }
  std::string wifi1_path() { return wifi1_path_; }
  std::string vpn_path() { return vpn_path_; }

 protected:
  sync_preferences::TestingPrefServiceSyncable user_prefs_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<NetworkHandlerTestHelper> helper_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<CrosNetworkConfig> cros_network_config_;
  std::unique_ptr<CrosNetworkConfigTestObserver> observer_;
  std::string wifi1_path_;
  std::string vpn_path_;
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
  EXPECT_EQ(false, network->type_state->get_wifi()->hidden_ssid);
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
  EXPECT_EQ(true, network->type_state->get_wifi()->hidden_ssid);
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
  EXPECT_EQ(kCellularTestIccid, cellular->iccid);
  EXPECT_EQ(mojom::OncSource::kDevice, network->source);
  EXPECT_TRUE(cellular->sim_locked);
  EXPECT_TRUE(cellular->sim_lock_enabled);
  EXPECT_EQ(shill::kSIMLockPin, cellular->sim_lock_type);

  network = GetNetworkState("vpn_l2tp_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ("vpn_l2tp_guid", network->guid);
  EXPECT_EQ(mojom::NetworkType::kVPN, network->type);
  EXPECT_EQ(mojom::ConnectionStateType::kConnecting, network->connection_state);
  ASSERT_TRUE(network->type_state);
  ASSERT_TRUE(network->type_state->is_vpn());
  EXPECT_EQ(mojom::VpnType::kL2TPIPsec, network->type_state->get_vpn()->type);
  EXPECT_EQ(mojom::OncSource::kNone, network->source);

  network = GetNetworkState("vpn_ikev2_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ("vpn_ikev2_guid", network->guid);
  EXPECT_EQ(mojom::NetworkType::kVPN, network->type);
  EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
            network->connection_state);
  ASSERT_TRUE(network->type_state);
  ASSERT_TRUE(network->type_state->is_vpn());
  EXPECT_EQ(mojom::VpnType::kIKEv2, network->type_state->get_vpn()->type);
  EXPECT_EQ(mojom::OncSource::kNone, network->source);

  // TODO(919691): Test ProxyMode once UIProxyConfigService logic is improved.
}

TEST_F(CrosNetworkConfigTest, PortalState) {
  mojom::NetworkStatePropertiesPtr network = GetNetworkState("eth_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ(mojom::ConnectionStateType::kOnline, network->connection_state);
  EXPECT_EQ(mojom::PortalState::kOnline, network->portal_state);

  helper()->ConfigureService(
      R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "portal-suspected",
          "Strength": 90, "AutoConnect": true})");
  network = GetNetworkState("wifi1_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ(mojom::ConnectionStateType::kPortal, network->connection_state);
  EXPECT_EQ(mojom::PortalState::kPortalSuspected, network->portal_state);

  helper()->ConfigureService(
      R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "redirect-found",
          "Strength": 90, "AutoConnect": true})");
  network = GetNetworkState("wifi1_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ(mojom::ConnectionStateType::kPortal, network->connection_state);
  EXPECT_EQ(mojom::PortalState::kPortal, network->portal_state);

  helper()->ConfigureService(
      R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "no-connectivity",
          "Strength": 90, "AutoConnect": true})");
  network = GetNetworkState("wifi1_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ(mojom::ConnectionStateType::kPortal, network->connection_state);
  EXPECT_EQ(mojom::PortalState::kNoInternet, network->portal_state);

  helper()->ConfigureService(
      R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "portal-suspected",
          "Strength": 90, "AutoConnect": true,
          "PortalDetectionFailedStatusCode": 407})");
  network = GetNetworkState("wifi1_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ(mojom::ConnectionStateType::kPortal, network->connection_state);
  EXPECT_EQ(mojom::PortalState::kProxyAuthRequired, network->portal_state);
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
  EXPECT_EQ("vpn_l2tp_guid", networks[2]->guid);

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
  ASSERT_EQ(5u, networks.size());
  EXPECT_EQ("wifi1_guid", networks[0]->guid);
  EXPECT_EQ("wifi2_guid", networks[1]->guid);
  EXPECT_EQ("wifi4_guid", networks[2]->guid);
  EXPECT_EQ("wifi_eap", networks[3]->guid);
  EXPECT_EQ("wifi3_guid", networks[4]->guid);

  // Visible wifi networks
  filter->filter = mojom::FilterType::kVisible;
  networks = GetNetworkStateList(filter.Clone());
  ASSERT_EQ(4u, networks.size());
  EXPECT_EQ("wifi1_guid", networks[0]->guid);
  EXPECT_EQ("wifi2_guid", networks[1]->guid);
  EXPECT_EQ("wifi4_guid", networks[2]->guid);
  EXPECT_EQ("wifi_eap", networks[3]->guid);

  // Configured wifi networks
  filter->filter = mojom::FilterType::kConfigured;
  networks = GetNetworkStateList(filter.Clone());
  ASSERT_EQ(4u, networks.size());
  EXPECT_EQ("wifi2_guid", networks[0]->guid);
  EXPECT_EQ("wifi4_guid", networks[1]->guid);
  EXPECT_EQ("wifi_eap", networks[2]->guid);
  EXPECT_EQ("wifi3_guid", networks[3]->guid);
}

TEST_F(CrosNetworkConfigTest, ESimAndPSimSlotInfo) {
  const char kTestEuiccPath1[] = "euicc_path_1";
  const char kTestEuiccPath2[] = "euicc_path_2";

  // pSIM slot info (existing ICCID).
  const char kTestPSimIccid[] = "test_psim_iccid";
  const int32_t psim_physical_slot = 1;

  // eSIM 1 slot info (existing ICCID).
  const char kTestEid1[] = "test_eid_1_esim_only";
  const char kTestESimIccid[] = "test_esim_iccid";
  const int32_t esim_1_physical_slot = 2;

  // eSIM 2 slot info (no ICCID).
  const char kTestEid2[] = "test_eid_2_esim_only";
  const int32_t esim_2_physical_slot = 3;

  // Add eSIM 1 and 2 info to Hermes.
  helper()->hermes_manager_test()->AddEuicc(
      dbus::ObjectPath(kTestEuiccPath1), kTestEid1, /*is_active=*/false,
      /*esim_1_physical_slot=*/esim_1_physical_slot);
  helper()->hermes_manager_test()->AddEuicc(
      dbus::ObjectPath(kTestEuiccPath2), kTestEid2, /*is_active=*/true,
      /*esim_1_physical_slot=*/esim_2_physical_slot);

  // Add pSIM and eSIM slot info to Shill.
  base::Value::List ordered_sim_slot_info_list;
  // Add pSIM first to correspond to |psim_physical_slot| index. Note that
  // pSIMs do not have EIDs.
  AddSimSlotInfoToList(ordered_sim_slot_info_list, /*eid=*/"", kTestPSimIccid,
                       /*primary=*/true);
  // Add eSIM next to correspond to |esim_1_physical_slot| index. Intentionally
  // exclude the EID; it's expected that Hermes will fill in the missing EID.
  AddSimSlotInfoToList(ordered_sim_slot_info_list, /*eid=*/"", kTestESimIccid);
  // Add eSIM next to correspond to |esim_2_physical_slot| index. Intentionally
  // exclude the EID and ICCID; it's expected that Hermes will still fill in
  // the missing EID.
  AddSimSlotInfoToList(ordered_sim_slot_info_list, /*eid=*/"", /*iccid=*/"");
  helper()->device_test()->SetDeviceProperty(
      kCellularDevicePath, shill::kSIMSlotInfoProperty,
      base::Value(std::move(ordered_sim_slot_info_list)),
      /*notify_changed=*/true);
  base::RunLoop().RunUntilIdle();

  mojom::DeviceStatePropertiesPtr cellular =
      GetDeviceStateFromList(mojom::NetworkType::kCellular);

  // Check pSIM slot info.
  EXPECT_EQ(psim_physical_slot, (*cellular->sim_infos)[0]->slot_id);
  ASSERT_TRUE((*cellular->sim_infos)[0]->eid.empty());
  EXPECT_EQ(kTestPSimIccid, (*cellular->sim_infos)[0]->iccid);
  EXPECT_TRUE((*cellular->sim_infos)[0]->is_primary);

  // Check eSIM 1 slot info.
  EXPECT_EQ(esim_1_physical_slot, (*cellular->sim_infos)[1]->slot_id);
  EXPECT_EQ(kTestEid1, (*cellular->sim_infos)[1]->eid);
  EXPECT_EQ(kTestESimIccid, (*cellular->sim_infos)[1]->iccid);
  EXPECT_FALSE((*cellular->sim_infos)[1]->is_primary);

  // Check eSIM 2 slot info. Note that the ICCID is empty here but the
  // EID still exists.
  EXPECT_EQ(esim_2_physical_slot, (*cellular->sim_infos)[2]->slot_id);
  EXPECT_EQ(kTestEid2, (*cellular->sim_infos)[2]->eid);
  ASSERT_TRUE((*cellular->sim_infos)[2]->iccid.empty());
  EXPECT_FALSE((*cellular->sim_infos)[2]->is_primary);
}

TEST_F(CrosNetworkConfigTest, ESimNetworkNameComesFromHermes) {
  const char kTestEuiccPath[] = "euicc_path";
  const char kTestProfileServicePath[] = "esim_service_path";
  const char kTestIccid[] = "iccid";

  const char kTestProfileName[] = "test_profile_name";
  const char kTestProfileNickname[] = "test_profile_nickname";
  const char kTestNameFromShill[] = "shill_network_name";

  // Add a fake eSIM with name kTestProfileName.
  helper()->hermes_manager_test()->AddEuicc(dbus::ObjectPath(kTestEuiccPath),
                                            "eid", /*is_active=*/true,
                                            /*physical_slot=*/0);
  helper()->hermes_euicc_test()->AddCarrierProfile(
      dbus::ObjectPath(kTestProfileServicePath),
      dbus::ObjectPath(kTestEuiccPath), kTestIccid, kTestProfileName,
      kTestProfileNickname, "service_provider", "activation_code",
      kTestProfileServicePath, hermes::profile::State::kInactive,
      hermes::profile::ProfileClass::kOperational,
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);
  base::RunLoop().RunUntilIdle();

  // Change the network's name in Shill. Now, Hermes and Shill have different
  // names associated with the profile.
  helper()->SetServiceProperty(kTestProfileServicePath, shill::kNameProperty,
                               base::Value(kTestNameFromShill));
  base::RunLoop().RunUntilIdle();

  // Fetch the Cellular network for the eSIM profile.
  std::string esim_guid = std::string("esim_guid") + kTestIccid;
  mojom::NetworkStatePropertiesPtr network = GetNetworkState(esim_guid);

  // The network's name should be the profile name (from Hermes), not the name
  // from Shill.
  EXPECT_EQ(kTestProfileNickname, network->name);
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
  NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), false, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  devices = GetDeviceStateList();
  ASSERT_EQ(4u, devices.size());
  EXPECT_EQ(mojom::NetworkType::kWiFi, devices[0]->type);
  EXPECT_EQ(mojom::DeviceStateType::kDisabled, devices[0]->device_state);
}

// Tests that no VPN device state is returned by GetDeviceStateList if no VPN
// services exist and built-in VPN is not prohibited.
TEST_F(CrosNetworkConfigTest, GetDeviceStateListNoVpnServices) {
  helper()->ClearServices();

  std::vector<std::string> prohibited_technologies =
      NetworkHandler::Get()
          ->prohibited_technologies_handler()
          ->GetCurrentlyProhibitedTechnologies();
  ASSERT_FALSE(base::Contains(prohibited_technologies, shill::kTypeVPN));

  EXPECT_FALSE(ContainsVpnDeviceState(GetDeviceStateList()));
}

// Tests that a VPN device state is returned by GetDeviceStateList if built-in
// VPN is not prohibited even if no VPN services exist.
TEST_F(CrosNetworkConfigTest, GetDeviceStateListNoVpnServicesAndVpnProhibited) {
  helper()->ClearServices();

  NetworkHandler::Get()
      ->prohibited_technologies_handler()
      ->AddGloballyProhibitedTechnology(shill::kTypeVPN);

  EXPECT_TRUE(ContainsVpnDeviceState(GetDeviceStateList()));
}

// Test a sampling of properties, ensuring that string property types are
// translated as strings and not enum values (See ManagedProperties definition
// in cros_network_config.mojom for details).
TEST_F(CrosNetworkConfigTest, GetManagedProperties) {
  SetTrafficCountersAutoResetAndCompare("eth_guid", /*auto_reset=*/true,
                                        /*day=*/mojom::UInt32Value::New(32),
                                        /*expected_success=*/false,
                                        /*expected_auto_reset=*/nullptr,
                                        /*expected_reset_day=*/nullptr);
  mojom::ManagedPropertiesPtr properties = GetManagedProperties("eth_guid");
  ASSERT_TRUE(properties);
  EXPECT_EQ("eth_guid", properties->guid);
  EXPECT_EQ(mojom::NetworkType::kEthernet, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kOnline, properties->connection_state);
  ASSERT_TRUE(properties->traffic_counter_properties);
  EXPECT_EQ(false, properties->traffic_counter_properties->auto_reset);
  EXPECT_EQ(static_cast<uint32_t>(1),
            properties->traffic_counter_properties->user_specified_reset_day);
  EXPECT_FALSE(properties->traffic_counter_properties->last_reset_time);

  base::Value expected_auto_reset(true);
  base::Value expected_reset_day(2);
  SetTrafficCountersAutoResetAndCompare(
      "wifi1_guid", /*auto_reset=*/true,
      /*day=*/mojom::UInt32Value::New(2),
      /*expected_success=*/true, &expected_auto_reset, &expected_reset_day);
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
  ASSERT_TRUE(properties->traffic_counter_properties &&
              properties->traffic_counter_properties->last_reset_time);
  EXPECT_EQ(123456789987654,
            properties->traffic_counter_properties->last_reset_time
                ->ToDeltaSinceWindowsEpoch()
                .InMilliseconds());
  EXPECT_EQ(true, properties->traffic_counter_properties->auto_reset);
  EXPECT_EQ(static_cast<uint32_t>(2),
            properties->traffic_counter_properties->user_specified_reset_day);

  SetTrafficCountersAutoResetAndCompare("wifi2_guid", /*auto_reset=*/true,
                                        /*day=*/nullptr,
                                        /*expected_success=*/false,
                                        /*expected_auto_reset=*/nullptr,
                                        /*expected_reset_day=*/nullptr);
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
  EXPECT_EQ(false, properties->traffic_counter_properties->auto_reset);
  EXPECT_EQ(static_cast<uint32_t>(1),
            properties->traffic_counter_properties->user_specified_reset_day);

  SetTrafficCountersAutoResetAndCompare("wifi3_guid", /*auto_reset=*/false,
                                        /*day=*/mojom::UInt32Value::New(2),
                                        /*expected_success=*/false,
                                        /*expected_auto_reset=*/nullptr,
                                        /*expected_reset_day=*/nullptr);
  properties = GetManagedProperties("wifi3_guid");
  ASSERT_TRUE(properties);
  EXPECT_EQ("wifi3_guid", properties->guid);
  EXPECT_EQ(mojom::NetworkType::kWiFi, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
            properties->connection_state);
  EXPECT_EQ(mojom::OncSource::kDevice, properties->source);
  EXPECT_EQ(false, properties->type_properties->get_wifi()->is_syncable);
  EXPECT_EQ(false, properties->traffic_counter_properties->auto_reset);
  EXPECT_EQ(static_cast<uint32_t>(1),
            properties->traffic_counter_properties->user_specified_reset_day);

  expected_auto_reset = base::Value(false);
  expected_reset_day = base::Value();
  SetTrafficCountersAutoResetAndCompare(
      "wifi4_guid", /*auto_reset=*/false,
      /*day=*/nullptr,
      /*expected_success=*/true, &expected_auto_reset, &expected_reset_day);
  properties = GetManagedProperties("wifi4_guid");
  ASSERT_TRUE(properties);
  EXPECT_EQ("wifi4_guid", properties->guid);
  EXPECT_EQ(mojom::NetworkType::kWiFi, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
            properties->connection_state);
  EXPECT_EQ(mojom::OncSource::kUser, properties->source);
  EXPECT_EQ(true, properties->type_properties->get_wifi()->is_syncable);
  EXPECT_EQ(false, properties->traffic_counter_properties->auto_reset);
  EXPECT_EQ(static_cast<uint32_t>(1),
            properties->traffic_counter_properties->user_specified_reset_day);

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
  EXPECT_TRUE(cellular->sim_locked);
  EXPECT_EQ(mojom::ActivationStateType::kActivated, cellular->activation_state);

  properties = GetManagedProperties("vpn_l2tp_guid");
  ASSERT_TRUE(properties);
  EXPECT_EQ("vpn_l2tp_guid", properties->guid);
  EXPECT_EQ(mojom::NetworkType::kVPN, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kConnecting,
            properties->connection_state);
  ASSERT_TRUE(properties->type_properties);
  ASSERT_TRUE(properties->type_properties->is_vpn());
  EXPECT_EQ(mojom::VpnType::kL2TPIPsec,
            properties->type_properties->get_vpn()->type);

  properties = GetManagedProperties("wifi_eap");
  ASSERT_TRUE(properties);
  EXPECT_EQ("wifi_eap", properties->guid);
  EXPECT_EQ(mojom::NetworkType::kWiFi, properties->type);
  ASSERT_TRUE(properties->type_properties);
  ASSERT_TRUE(properties->type_properties->is_wifi());
  mojom::ManagedEAPPropertiesPtr eap =
      std::move(properties->type_properties->get_wifi()->eap);
  ASSERT_TRUE(eap);
  ASSERT_TRUE(eap->subject_alt_name_match);
  ASSERT_EQ(2u, eap->subject_alt_name_match->active_value.size());
  EXPECT_EQ(mojom::SubjectAltName::Type::kDns,
            eap->subject_alt_name_match->active_value[0]->type);
  EXPECT_EQ("example.com", eap->subject_alt_name_match->active_value[0]->value);
  EXPECT_EQ(mojom::SubjectAltName::Type::kEmail,
            eap->subject_alt_name_match->active_value[1]->type);
  EXPECT_EQ("test@example.com",
            eap->subject_alt_name_match->active_value[1]->value);

  ASSERT_TRUE(eap->domain_suffix_match);
  ASSERT_EQ(2u, eap->domain_suffix_match->active_value.size());
  EXPECT_EQ("example1.com", eap->domain_suffix_match->active_value[0]);
  EXPECT_EQ("example2.com", eap->domain_suffix_match->active_value[1]);
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

  properties = GetManagedProperties("openvpn_guid");
  ASSERT_TRUE(properties);
  ASSERT_EQ("openvpn_guid", properties->guid);
  ASSERT_TRUE(properties->type_properties);
  mojom::ManagedVPNProperties* vpn =
      properties->type_properties->get_vpn().get();
  ASSERT_TRUE(vpn);
  ASSERT_TRUE(vpn->open_vpn);
  std::vector<std::tuple<mojom::ManagedString*, std::string>> checks = {
      {vpn->open_vpn->auth.get(), "MD5"},
      {vpn->open_vpn->cipher.get(), "AES-192-CBC"},
      {vpn->open_vpn->compression_algorithm.get(), "LZO"},
      {vpn->open_vpn->tls_auth_contents.get(), policy_util::kFakeCredential},
      {vpn->open_vpn->key_direction.get(), "1"}};
  for (const auto& [property, expected] : checks) {
    ASSERT_TRUE(property);
    EXPECT_EQ(expected, property->active_value);
    EXPECT_EQ(mojom::PolicySource::kUserPolicyEnforced,
              property->policy_source);
  }
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

  // Set Eap.SubjectAlternativeNameMatch and Eap.DomainSuffixMatch.
  config = mojom::ConfigProperties::New();
  config->type_config = mojom::NetworkTypeConfigProperties::NewWifi(
      mojom::WiFiConfigProperties::New());
  auto eap = mojom::EAPConfigProperties::New();
  eap->domain_suffix_match = {"example1.com", "example2.com"};
  auto san = mojom::SubjectAltName::New();
  san->type = mojom::SubjectAltName::Type::kDns;
  san->value = "test.example.com";
  eap->subject_alt_name_match.push_back(std::move(san));

  config->type_config->get_wifi()->eap = std::move(eap);
  config->guid = kGUID;
  success = SetProperties(kGUID, std::move(config));
  ASSERT_TRUE(success);
  properties = GetManagedProperties(kGUID);
  ASSERT_TRUE(properties);
  ASSERT_EQ(kGUID, properties->guid);
  ASSERT_TRUE(properties->type_properties);
  wifi = properties->type_properties->get_wifi().get();
  ASSERT_TRUE(wifi);
  ASSERT_TRUE(wifi->eap);
  EXPECT_EQ(2u, wifi->eap->domain_suffix_match->active_value.size());
  EXPECT_EQ("example1.com", wifi->eap->domain_suffix_match->active_value[0]);
  EXPECT_EQ("example2.com", wifi->eap->domain_suffix_match->active_value[1]);
  EXPECT_EQ(1u, wifi->eap->subject_alt_name_match->active_value.size());
  EXPECT_EQ(mojom::SubjectAltName::Type::kDns,
            wifi->eap->subject_alt_name_match->active_value[0]->type);
  EXPECT_EQ("test.example.com",
            wifi->eap->subject_alt_name_match->active_value[0]->value);
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
  new_apn->attach = kCellularTestApnAttach1;
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
  new_apn->attach = kCellularTestApnAttach3;
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
  ASSERT_EQ(kCellularTestApnName3, properties->type_properties->get_cellular()
                                       ->custom_apn_list->front()
                                       ->name);
  ASSERT_EQ(kCellularTestApnUsername3,
            properties->type_properties->get_cellular()
                ->custom_apn_list->front()
                ->username);
  ASSERT_EQ(kCellularTestApnPassword3,
            properties->type_properties->get_cellular()
                ->custom_apn_list->front()
                ->password);
  ASSERT_EQ(kCellularTestApnAttach3, properties->type_properties->get_cellular()
                                         ->custom_apn_list->front()
                                         ->attach);
}

TEST_F(CrosNetworkConfigTest, UnrecognizedAttachApnValue) {
  SetupAPNList();
  const char kUnrecognizedTestApnAttachStr[] = "unrecognized attach value";
  const char* kGUID = "cellular_guid";

  // Verify that custom APN list is updated properly.
  auto config = mojom::ConfigProperties::New();
  auto cellular_config = mojom::CellularConfigProperties::New();
  auto new_apn = mojom::ApnProperties::New();

  new_apn->attach = kUnrecognizedTestApnAttachStr;
  cellular_config->apn = std::move(new_apn);
  config->type_config = mojom::NetworkTypeConfigProperties::NewCellular(
      std::move(cellular_config));
  SetProperties(kGUID, std::move(config));

  // Unrecognized values are still saved without incident.
  ASSERT_EQ(kUnrecognizedTestApnAttachStr, GetManagedProperties(kGUID)
                                               ->type_properties->get_cellular()
                                               ->custom_apn_list->front()
                                               ->attach);
}

TEST_F(CrosNetworkConfigTest, AllowRoaming) {
  const char* kGUID = "cellular_guid";
  mojom::ManagedPropertiesPtr properties = GetManagedProperties(kGUID);

  ASSERT_FALSE(properties->type_properties->get_cellular()->allow_roaming);

  auto config = mojom::ConfigProperties::New();
  auto cellular_config = mojom::CellularConfigProperties::New();
  auto new_roaming = mojom::RoamingProperties::New();

  new_roaming->allow_roaming = true;
  cellular_config->roaming = std::move(new_roaming);
  config->type_config = mojom::NetworkTypeConfigProperties::NewCellular(
      std::move(cellular_config));
  ASSERT_TRUE(SetProperties(kGUID, std::move(config)));

  properties = GetManagedProperties(kGUID);

  ASSERT_TRUE(properties);
  ASSERT_EQ(kGUID, properties->guid);
  ASSERT_TRUE(properties->type_properties->is_cellular());
  ASSERT_TRUE(
      properties->type_properties->get_cellular()->allow_roaming->active_value);
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
  wifi->hidden_ssid = mojom::HiddenSsidMode::kDisabled;
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
  ASSERT_FALSE(network->type_state->get_wifi()->hidden_ssid);
}

TEST_F(CrosNetworkConfigTest, ConfigureNetwork_AutomaticHiddenSSID) {
  const std::string ssid = "new_wifi_ssid";
  auto config = mojom::ConfigProperties::New();
  config->name = ssid;
  auto wifi = mojom::WiFiConfigProperties::New();
  wifi->ssid = ssid;
  wifi->hidden_ssid = mojom::HiddenSsidMode::kAutomatic;
  config->type_config =
      mojom::NetworkTypeConfigProperties::NewWifi(std::move(wifi));
  std::string guid = ConfigureNetwork(std::move(config), true);
  EXPECT_FALSE(guid.empty());

  // Verify the configuration.
  mojom::NetworkStatePropertiesPtr network = GetNetworkState(guid);
  ASSERT_TRUE(network);
  EXPECT_EQ(guid, network->guid);

  // For the purposes of this test, the wifi network is considered "in range"
  // and therefore the fake platform will not set the network to hidden.
  ASSERT_FALSE(network->type_state->get_wifi()->hidden_ssid);
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
  wifi->hidden_ssid = mojom::HiddenSsidMode::kEnabled;
  config->type_config =
      mojom::NetworkTypeConfigProperties::NewWifi(std::move(wifi));
  std::string config_guid = ConfigureNetwork(std::move(config), shared);
  // The new guid should be the same as the existing guid.
  EXPECT_EQ(config_guid, guid);

  mojom::NetworkStatePropertiesPtr network = GetNetworkState(guid);
  ASSERT_TRUE(network->type_state->get_wifi()->hidden_ssid);
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

TEST_F(CrosNetworkConfigTest, CellularInhibitState) {
  mojom::DeviceStatePropertiesPtr cellular =
      GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular);
  EXPECT_EQ(mojom::DeviceStateType::kEnabled, cellular->device_state);
  EXPECT_EQ(mojom::InhibitReason::kNotInhibited, cellular->inhibit_reason);

  std::unique_ptr<CellularInhibitor::InhibitLock> lock =
      InhibitCellularScanning(
          CellularInhibitor::InhibitReason::kInstallingProfile);
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular);
  EXPECT_EQ(mojom::DeviceStateType::kEnabled, cellular->device_state);
  EXPECT_EQ(mojom::InhibitReason::kInstallingProfile, cellular->inhibit_reason);
}

TEST_F(CrosNetworkConfigTest, CellularInhibitState_Connecting) {
  const char kTestEuiccPath[] = "euicc_path";
  mojom::DeviceStatePropertiesPtr cellular =
      GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular);
  EXPECT_EQ(mojom::DeviceStateType::kEnabled, cellular->device_state);
  EXPECT_EQ(mojom::InhibitReason::kNotInhibited, cellular->inhibit_reason);

  // Set connect requested on cellular network.
  NetworkStateHandler* network_state_handler =
      NetworkHandler::Get()->network_state_handler();
  const NetworkState* network_state =
      network_state_handler->GetNetworkStateFromGuid("cellular_guid");
  network_state_handler->SetNetworkConnectRequested(network_state->path(),
                                                    true);

  // Verify the inhibit state is not set when connecting if there are no EUICC.
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular);
  EXPECT_EQ(mojom::DeviceStateType::kEnabled, cellular->device_state);
  EXPECT_EQ(mojom::InhibitReason::kNotInhibited, cellular->inhibit_reason);

  // Verify the adding EUICC sets the inhibit reason correctly.
  helper()->hermes_manager_test()->AddEuicc(dbus::ObjectPath(kTestEuiccPath),
                                            "eid", /*is_active=*/true,
                                            /*physical_slot=*/0);
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular);
  EXPECT_EQ(mojom::DeviceStateType::kEnabled, cellular->device_state);
  EXPECT_EQ(mojom::InhibitReason::kConnectingToProfile,
            cellular->inhibit_reason);
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
                                  /*new_pin=*/absl::nullopt,
                                  /*require_pin=*/false));

  // Sim should be unlocked, locking should still be enabled.
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_TRUE(cellular->sim_lock_status->lock_type.empty());

  // Set |require_pin| to false (disable locking).
  EXPECT_TRUE(SetCellularSimState(FakeShillDeviceClient::kDefaultSimPin,
                                  /*new_pin=*/absl::nullopt,
                                  /*require_pin=*/false));

  // Sim should be unlocked, locking should be disabled.
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_FALSE(cellular->sim_lock_status->lock_enabled);
  EXPECT_TRUE(cellular->sim_lock_status->lock_type.empty());

  // Set |require_pin| to true (enable locking).
  EXPECT_TRUE(SetCellularSimState(FakeShillDeviceClient::kDefaultSimPin,
                                  /*new_pin=*/absl::nullopt,
                                  /*require_pin=*/true));

  // Sim should remain unlocked, locking should be enabled.
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_TRUE(cellular->sim_lock_status->lock_type.empty());

  // Lock the sim. (Can not be done via the mojo API).
  helper()->device_test()->SetSimLocked(kCellularDevicePath, true);
  base::RunLoop().RunUntilIdle();
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  ASSERT_TRUE(cellular->sim_lock_status->lock_enabled);
  ASSERT_EQ(shill::kSIMLockPin, cellular->sim_lock_status->lock_type);

  // Attempt to unlock the sim with an incorrect pin. Call should fail.
  EXPECT_FALSE(SetCellularSimState("incorrect pin", /*new_pin=*/absl::nullopt,
                                   /*require_pin=*/false));

  // Ensure sim is still locked and retry count has decreased.
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_EQ(shill::kSIMLockPin, cellular->sim_lock_status->lock_type);
  EXPECT_EQ(retries - 1, cellular->sim_lock_status->retries_left);

  // Additional attempts should set the sim to puk locked.
  for (int i = retries - 1; i > 0; --i) {
    SetCellularSimState("incorrect pin", /*new_pin=*/absl::nullopt, false);
  }
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_EQ(shill::kSIMLockPuk, cellular->sim_lock_status->lock_type);

  // Attempt to unblock the sim with the incorrect puk. Call should fail.
  const std::string new_pin = "2222";
  EXPECT_FALSE(SetCellularSimState("incorrect puk",
                                   absl::make_optional(new_pin),
                                   /*require_pin=*/false));

  // Attempt to unblock the sim with np pin. Call should fail.
  EXPECT_FALSE(SetCellularSimState(FakeShillDeviceClient::kSimPuk,
                                   /*new_pin=*/absl::nullopt,
                                   /*require_pin=*/false));

  // Attempt to unlock the sim with the correct puk.
  EXPECT_TRUE(SetCellularSimState(FakeShillDeviceClient::kSimPuk,
                                  absl::make_optional(new_pin),
                                  /*require_pin=*/false));

  // Sim should be unlocked
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_type.empty());
}

TEST_F(CrosNetworkConfigTest, SelectCellularMobileNetwork) {
  // Create fake list of found networks.
  absl::optional<base::Value> found_networks_list =
      base::JSONReader::Read(base::StringPrintf(
          R"([{"network_id": "network1", "technology": "GSM",
               "status": "current"},
              {"network_id": "network2", "technology": "GSM",
               "status": "available"}])"));
  helper()->device_test()->SetDeviceProperty(
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
      ::onc::global_network_config::kAllowOnlyPolicyWiFiToConnect, false);
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
  EXPECT_EQ(false, policy->allow_cellular_sim_lock);
  EXPECT_EQ(false, policy->allow_only_policy_cellular_networks);
  EXPECT_EQ(true, policy->allow_only_policy_networks_to_autoconnect);
  EXPECT_EQ(false, policy->allow_only_policy_wifi_networks_to_connect);
  EXPECT_EQ(false,
            policy->allow_only_policy_wifi_networks_to_connect_if_available);
  ASSERT_EQ(2u, policy->blocked_hex_ssids.size());
  EXPECT_EQ("blocked_ssid1", policy->blocked_hex_ssids[0]);
  EXPECT_EQ("blocked_ssid2", policy->blocked_hex_ssids[1]);
}

TEST_F(CrosNetworkConfigTest, GlobalPolicyApplied) {
  SetupObserver();
  EXPECT_EQ(0, observer()->GetPolicyAppliedCount(/*userhash=*/std::string()));

  base::DictionaryValue global_config;
  global_config.SetBoolKey(::onc::global_network_config::kAllowCellularSimLock,
                           true);
  global_config.SetBoolKey(
      ::onc::global_network_config::kAllowOnlyPolicyCellularNetworks, true);
  global_config.SetBoolKey(
      ::onc::global_network_config::kAllowOnlyPolicyWiFiToConnect, false);
  managed_network_configuration_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
      base::ListValue(), global_config);
  base::RunLoop().RunUntilIdle();
  mojom::GlobalPolicyPtr policy = GetGlobalPolicy();
  ASSERT_TRUE(policy);
  EXPECT_EQ(true, policy->allow_cellular_sim_lock);
  EXPECT_EQ(true, policy->allow_only_policy_cellular_networks);
  EXPECT_EQ(false, policy->allow_only_policy_networks_to_autoconnect);
  EXPECT_EQ(false, policy->allow_only_policy_wifi_networks_to_connect);
  EXPECT_EQ(false,
            policy->allow_only_policy_wifi_networks_to_connect_if_available);
  EXPECT_EQ(1, observer()->GetPolicyAppliedCount(/*userhash=*/std::string()));
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
  helper()->ConfigureService(
      R"({"GUID": "wifi3_guid", "Type": "wifi", "State": "ready"})");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer()->network_state_list_changed());
}

TEST_F(CrosNetworkConfigTest, DeviceListChanged) {
  SetupObserver();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer()->device_state_list_changed());
  NetworkStateHandler* network_state_handler =
      NetworkHandler::Get()->network_state_handler();

  // Disable wifi
  network_state_handler->SetTechnologyEnabled(NetworkTypePattern::WiFi(), false,
                                              network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  // This will trigger three device list updates. First when wifi is in the
  // disabling state, next when it's actually disabled, and lastly when
  // Device::available_managed_network_path_ changes.
  EXPECT_EQ(3, observer()->device_state_list_changed());

  // Enable Tethering
  network_state_handler->SetTetherTechnologyState(
      NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, observer()->device_state_list_changed());

  // Tests that observers are notified of device state list change
  // when a tether scan begins for a device.
  network_state_handler->SetTetherScanState(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5, observer()->device_state_list_changed());

  // Tests that observers are notified of device state list change
  // when a tether scan completes.
  network_state_handler->SetTetherScanState(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(6, observer()->device_state_list_changed());

  // Test that observers are notified of device state list change
  // when a cellular network connection state changes.
  const NetworkState* network_state =
      network_state_handler->GetNetworkStateFromGuid("cellular_guid");
  network_state_handler->SetNetworkConnectRequested(network_state->path(),
                                                    /*connect_requested=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(7, observer()->device_state_list_changed());
}

TEST_F(CrosNetworkConfigTest, ActiveNetworksChanged) {
  SetupObserver();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer()->active_networks_changed());

  // Change a network state.
  helper()->SetServiceProperty(wifi1_path(), shill::kStateProperty,
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
  helper()->SetServiceProperty(wifi1_path(), shill::kSignalStrengthProperty,
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

TEST_F(CrosNetworkConfigTest, NetworkStateHasIccidAndEid) {
  const char kTestIccid[] = "iccid";
  const char kTestEid[] = "eid";

  // Add a fake eSIM network.
  SetupTestESimProfile(kTestEid, kTestIccid, "esim_service_path",
                       "test_profile_name", "test_profile_nickname");

  // Fetch the Cellular network's managed properties for the eSIM profile.
  std::string esim_guid = std::string("esim_guid") + kTestIccid;
  mojom::NetworkStatePropertiesPtr network = GetNetworkState(esim_guid);
  mojom::CellularStatePropertiesPtr& cellular =
      network->type_state->get_cellular();
  EXPECT_EQ(kTestIccid, cellular->iccid);
  EXPECT_EQ(kTestEid, cellular->eid);
}

TEST_F(CrosNetworkConfigTest, ESimManagedPropertiesNameComesFromHermes) {
  const char kTestProfileServicePath[] = "esim_service_path";
  const char kTestIccid[] = "iccid";
  const char kTestEid[] = "eid";
  const char kTestNameFromShill[] = "shill_network_name";
  const char kTestProfileName[] = "test_profile_name";
  const char kTestProfileNickname[] = "test_profile_nickname";

  // Add a fake eSIM with name kTestProfileName.
  SetupTestESimProfile(kTestEid, kTestIccid, kTestProfileServicePath,
                       kTestProfileName, kTestProfileNickname);

  // Change the network's name in Shill. Now, Hermes and Shill have different
  // names associated with the profile.
  helper()->SetServiceProperty(kTestProfileServicePath, shill::kNameProperty,
                               base::Value(kTestNameFromShill));
  base::RunLoop().RunUntilIdle();

  // Fetch the Cellular network's managed properties for the eSIM profile.
  std::string esim_guid = std::string("esim_guid") + kTestIccid;
  mojom::ManagedPropertiesPtr properties = GetManagedProperties(esim_guid);
  EXPECT_EQ(kTestProfileNickname, properties->name->active_value);
}

TEST_F(CrosNetworkConfigTest, GetAlwaysOnVpn) {
  mojom::AlwaysOnVpnPropertiesPtr properties;

  helper()->SetProfileProperty(helper()->ProfilePathUser(),
                               shill::kAlwaysOnVpnModeProperty,
                               base::Value("off"));
  helper()->SetProfileProperty(helper()->ProfilePathUser(),
                               shill::kAlwaysOnVpnServiceProperty,
                               base::Value(vpn_path()));
  properties = GetAlwaysOnVpn();
  EXPECT_EQ(mojom::AlwaysOnVpnMode::kOff, properties->mode);
  EXPECT_EQ("vpn_l2tp_guid", properties->service_guid);

  helper()->SetProfileProperty(helper()->ProfilePathUser(),
                               shill::kAlwaysOnVpnModeProperty,
                               base::Value("best-effort"));
  properties = GetAlwaysOnVpn();
  EXPECT_EQ(mojom::AlwaysOnVpnMode::kBestEffort, properties->mode);

  helper()->SetProfileProperty(helper()->ProfilePathUser(),
                               shill::kAlwaysOnVpnModeProperty,
                               base::Value("strict"));
  properties = GetAlwaysOnVpn();
  EXPECT_EQ(mojom::AlwaysOnVpnMode::kStrict, properties->mode);
}

TEST_F(CrosNetworkConfigTest, SetAlwaysOnVpn) {
  mojom::AlwaysOnVpnPropertiesPtr properties =
      mojom::AlwaysOnVpnProperties::New(mojom::AlwaysOnVpnMode::kBestEffort,
                                        "vpn_l2tp_guid");
  SetAlwaysOnVpn(std::move(properties));

  EXPECT_EQ("best-effort",
            helper()->GetProfileStringProperty(
                helper()->ProfilePathUser(), shill::kAlwaysOnVpnModeProperty));
  EXPECT_EQ(vpn_path(), helper()->GetProfileStringProperty(
                            helper()->ProfilePathUser(),
                            shill::kAlwaysOnVpnServiceProperty));

  properties = mojom::AlwaysOnVpnProperties::New(mojom::AlwaysOnVpnMode::kOff,
                                                 std::string());
  SetAlwaysOnVpn(std::move(properties));

  EXPECT_EQ("off",
            helper()->GetProfileStringProperty(
                helper()->ProfilePathUser(), shill::kAlwaysOnVpnModeProperty));
  EXPECT_EQ(vpn_path(), helper()->GetProfileStringProperty(
                            helper()->ProfilePathUser(),
                            shill::kAlwaysOnVpnServiceProperty));

  properties = mojom::AlwaysOnVpnProperties::New(mojom::AlwaysOnVpnMode::kOff,
                                                 "another_service");
  SetAlwaysOnVpn(std::move(properties));

  EXPECT_EQ("off",
            helper()->GetProfileStringProperty(
                helper()->ProfilePathUser(), shill::kAlwaysOnVpnModeProperty));
  EXPECT_EQ(vpn_path(), helper()->GetProfileStringProperty(
                            helper()->ProfilePathUser(),
                            shill::kAlwaysOnVpnServiceProperty));
}

TEST_F(CrosNetworkConfigTest, RequestTrafficCountersWithIntegerType) {
  base::Value traffic_counters(base::Value::Type::LIST);

  base::Value chrome_dict(base::Value::Type::DICTIONARY);
  chrome_dict.SetKey("source", base::Value(shill::kTrafficCounterSourceChrome));
  chrome_dict.SetKey("rx_bytes", base::Value(12));
  chrome_dict.SetKey("tx_bytes", base::Value(32));
  traffic_counters.Append(std::move(chrome_dict));

  base::Value user_dict(base::Value::Type::DICTIONARY);
  user_dict.SetKey("source", base::Value(shill::kTrafficCounterSourceUser));
  user_dict.SetKey("rx_bytes", base::Value(90));
  user_dict.SetKey("tx_bytes", base::Value(87));
  traffic_counters.Append(std::move(user_dict));

  ASSERT_TRUE(traffic_counters.is_list());
  ASSERT_EQ(traffic_counters.GetListDeprecated().size(), (size_t)2);
  helper()->service_test()->SetFakeTrafficCounters(traffic_counters.Clone());

  RequestTrafficCountersAndCompareTrafficCounters(
      "wifi1_guid", traffic_counters.Clone(), ComparisonType::INTEGER);
}

TEST_F(CrosNetworkConfigTest, RequestTrafficCountersWithDoubleType) {
  base::Value traffic_counters(base::Value::Type::LIST);

  base::Value chrome_dict(base::Value::Type::DICTIONARY);
  chrome_dict.SetKey("source", base::Value(shill::kTrafficCounterSourceChrome));
  chrome_dict.SetKey("rx_bytes", base::Value(123456789987.0));
  chrome_dict.SetKey("tx_bytes", base::Value(3211234567898.0));
  traffic_counters.Append(std::move(chrome_dict));

  base::Value user_dict(base::Value::Type::DICTIONARY);
  user_dict.SetKey("source", base::Value(shill::kTrafficCounterSourceUser));
  user_dict.SetKey("rx_bytes", base::Value(9000000000000000.0));
  user_dict.SetKey("tx_bytes", base::Value(8765432112345.0));
  traffic_counters.Append(std::move(user_dict));

  ASSERT_TRUE(traffic_counters.is_list());
  ASSERT_EQ(traffic_counters.GetListDeprecated().size(), (size_t)2);
  helper()->service_test()->SetFakeTrafficCounters(traffic_counters.Clone());

  RequestTrafficCountersAndCompareTrafficCounters(
      "wifi1_guid", traffic_counters.Clone(), ComparisonType::DOUBLE);
}

TEST_F(CrosNetworkConfigTest, GetSupportedVpnTypes) {
  std::vector<std::string> result = GetSupportedVpnTypes();
  ASSERT_EQ(result.size(), 0u);

  helper()->manager_test()->SetManagerProperty(
      shill::kSupportedVPNTypesProperty, base::Value("l2tpipsec,openvpn"));
  result = GetSupportedVpnTypes();
  ASSERT_EQ(result.size(), 2u);

  helper()->manager_test()->SetShouldReturnNullProperties(true);
  result = GetSupportedVpnTypes();
  ASSERT_EQ(result.size(), 0u);
  helper()->manager_test()->SetShouldReturnNullProperties(false);
}

TEST_F(CrosNetworkConfigTest, SetAutoReset) {
  SetTrafficCountersAutoResetAndCompare("wifi1_guid", /*auto_reset=*/true,
                                        /*day=*/mojom::UInt32Value::New(32),
                                        /*expected_success=*/false,
                                        /*expected_auto_reset=*/nullptr,
                                        /*expected_reset_day=*/nullptr);
  base::Value expected_auto_reset(true);
  base::Value expected_reset_day(2);
  SetTrafficCountersAutoResetAndCompare(
      "wifi1_guid", /*auto_reset=*/true,
      /*day=*/mojom::UInt32Value::New(2),
      /*expected_success=*/true, &expected_auto_reset, &expected_reset_day);
  // Auto reset prefs remains unchanged from last successful call.
  SetTrafficCountersAutoResetAndCompare(
      "wifi1_guid", /*auto_reset=*/true,
      /*day=*/mojom::UInt32Value::New(0),
      /*expected_success=*/false, &expected_auto_reset, &expected_reset_day);
  expected_auto_reset = base::Value(false);
  expected_reset_day = base::Value();
  SetTrafficCountersAutoResetAndCompare(
      "wifi1_guid", /*auto_reset=*/false,
      /*day=*/nullptr,
      /*expected_success=*/true,
      /*expected_auto_reset=*/&expected_auto_reset,
      /*expected_reset_day=*/&expected_reset_day);
  // Auto reset prefs remains unchanged from last successful call.
  SetTrafficCountersAutoResetAndCompare(
      "wifi1_guid", /*auto_reset=*/false,
      /*day=*/mojom::UInt32Value::New(10),
      /*expected_success=*/false, &expected_auto_reset, &expected_reset_day);
  // Auto reset prefs remains unchanged from last successful call.
  SetTrafficCountersAutoResetAndCompare(
      "wifi1_guid", /*auto_reset=*/true,
      /*day=*/nullptr,
      /*expected_success=*/false, &expected_auto_reset, &expected_reset_day);
}

// Make sure calling shutdown before cros_network_config destruction doesn't
// cause a crash.
TEST_F(CrosNetworkConfigTest, Shutdown) {
  SetupObserver();
  base::RunLoop().RunUntilIdle();

  NetworkHandler::Get()->network_state_handler()->Shutdown();
  NetworkHandler::Get()->managed_network_configuration_handler()->Shutdown();
}

}  // namespace network_config
}  // namespace chromeos
