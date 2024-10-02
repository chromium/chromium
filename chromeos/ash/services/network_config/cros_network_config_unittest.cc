// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/network_config/cros_network_config.h"

#include <tuple>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/carrier_lock/carrier_lock_manager.h"
#include "chromeos/ash/components/carrier_lock/fake_fcm_topic_subscriber.h"
#include "chromeos/ash/components/carrier_lock/fake_provisioning_config_fetcher.h"
#include "chromeos/ash/components/carrier_lock/fake_psm_claim_verifier.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_device_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/cellular_metrics_logger.h"
#include "chromeos/ash/components/network/fake_network_3gpp_handler.h"
#include "chromeos/ash/components/network/fake_stub_cellular_networks_provider.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
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
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "chromeos/ash/components/network/traffic_counters_handler.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_observer.h"
#include "chromeos/ash/services/network_config/test_apn_data.h"
#include "chromeos/ash/services/network_config/test_network_configuration_observer.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-shared.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-shared.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/onc/onc_constants.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash::network_config {

namespace {

namespace mojom = ::chromeos::network_config::mojom;
using ApnTypes = CellularNetworkMetricsLogger::ApnTypes;

constexpr int kSimRetriesLeft = 3;
constexpr char kCellularGuid[] = "cellular_guid";
constexpr char kCellularDevicePath[] = "/device/stub_cellular_device";
constexpr char kCellularTestIccid[] = "1234567890";
constexpr char kCellularTestImei[] = "1234567890";
constexpr char kCellularTestSerial[] = "ABCD";

constexpr char kCellularTestApn1[] = "TEST.APN1";
constexpr char kCellularTestApnName1[] = "Test Apn 1";
constexpr char kCellularTestApnUsername1[] = "Test User";
constexpr char kCellularTestApnPassword1[] = "Test Pass";
constexpr char kCellularTestApnAttach1[] = "";
constexpr char kCellularTestApnId1[] = "1";
constexpr char kCellularTestApnAuthenticationType1[] = "";
constexpr char kCellularTestApnTypes1[] = "Default";

constexpr char kCellularTestApn2[] = "TEST.APN2";
constexpr char kCellularTestApnName2[] = "Test Apn 2";
constexpr char kCellularTestApnUsername2[] = "Test User";
constexpr char kCellularTestApnPassword2[] = "Test Pass";
constexpr char kCellularTestApnAttach2[] = "";

constexpr char kCellularTestApn3[] = "TEST.APN3";
constexpr char kCellularTestApnName3[] = "Test Apn 3";
constexpr char kCellularTestApnUsername3[] = "Test User";
constexpr char kCellularTestApnPassword3[] = "Test Pass";
constexpr char kCellularTestApnAttach3[] = "attach";

constexpr char kTestApnCellularGuid[] = "test_apn_cellular_guid";
constexpr char kTestApnCellularShillDictFmt[] =
    R"({"GUID": "%s", "Type": "cellular",  "State": "%s",
            "Strength": 0, "Cellular.NetworkTechnology": "LTE",
            "Cellular.ActivationState": "activated", "Cellular.ICCID": "%s",
            "Profile": "%s", "Cellular.LastGoodAPN": %s})";

static const re2::RE2 kApnIdRegex("[0-9a-fA-F]{32}");

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

struct ApnHistogramCounts {
  size_t num_modify_success = 0u;
  size_t num_modify_failure = 0u;
  size_t num_modify_type_default = 0u;
  size_t num_modify_type_attach = 0u;
  size_t num_modify_type_default_and_attach = 0u;

  size_t num_enable_success = 0u;
  size_t num_enable_failure = 0u;
  size_t num_enable_type_default = 0u;
  size_t num_enable_type_attach = 0u;
  size_t num_enable_type_default_and_attach = 0u;

  size_t num_disable_success = 0u;
  size_t num_disable_failure = 0u;
  size_t num_disable_type_default = 0u;
  size_t num_disable_type_attach = 0u;
  size_t num_disable_type_default_and_attach = 0u;
};

void CompareTrafficCounters(
    const std::vector<mojom::TrafficCounterPtr>& actual_traffic_counters,
    const base::Value::List& expected_traffic_counters,
    enum ComparisonType comparison_type) {
  EXPECT_EQ(actual_traffic_counters.size(), expected_traffic_counters.size());
  for (size_t i = 0; i < actual_traffic_counters.size(); i++) {
    const auto& actual_tc = actual_traffic_counters[i];
    const auto& expected_tc = expected_traffic_counters[i].GetDict();
    EXPECT_EQ(actual_tc->source,
              CrosNetworkConfig::GetTrafficCounterEnumForTesting(
                  *expected_tc.FindString("source")));
    if (comparison_type == ComparisonType::INTEGER) {
      EXPECT_EQ(actual_tc->rx_bytes, (size_t)*expected_tc.FindInt("rx_bytes"));
      EXPECT_EQ(actual_tc->tx_bytes, (size_t)*expected_tc.FindInt("tx_bytes"));
    } else if (comparison_type == ComparisonType::DOUBLE) {
      EXPECT_EQ(actual_tc->rx_bytes,
                (size_t)*expected_tc.FindDouble("rx_bytes"));
      EXPECT_EQ(actual_tc->tx_bytes,
                (size_t)*expected_tc.FindDouble("tx_bytes"));
    }
  }
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

bool ContainsVpnDeviceState(
    std::vector<mojom::DeviceStatePropertiesPtr> devices) {
  for (auto& device : devices) {
    if (device->type == mojom::NetworkType::kVPN) {
      return true;
    }
  }
  return false;
}

std::string CreateApnShillDict() {
  TestApnData test_apn_data;
  test_apn_data.access_point_name = kCellularTestApn1;
  test_apn_data.name = kCellularTestApnName1;
  test_apn_data.username = kCellularTestApnUsername1;
  test_apn_data.password = kCellularTestApnPassword1;
  test_apn_data.attach = kCellularTestApnAttach1;
  test_apn_data.id = kCellularTestApnId1;
  test_apn_data.onc_authentication = kCellularTestApnAuthenticationType1;
  test_apn_data.onc_ip_type = ::onc::cellular_apn::kIpTypeIpv4;
  test_apn_data.onc_source = ::onc::cellular_apn::kSourceModb;
  test_apn_data.onc_apn_types.emplace_back(kCellularTestApnTypes1);
  return test_apn_data.AsApnShillDict();
}

mojom::ConfigPropertiesPtr CreateFakeVpnConfig(std::string name,
                                               std::string host,
                                               mojom::VpnType type) {
  auto vpn = mojom::VPNConfigProperties::New();
  vpn->host = host;
  vpn->type = mojom::VpnTypeConfig::New();
  vpn->type->value = type;

  auto config = mojom::ConfigProperties::New();
  config->name = name;
  config->type_config =
      mojom::NetworkTypeConfigProperties::NewVpn(std::move(vpn));
  return config;
}

bool OncApnHasId(const base::Value::Dict& apn) {
  if (const std::string* id = apn.FindString(::onc::cellular_apn::kId)) {
    return re2::RE2::FullMatch(*id, kApnIdRegex);
  }
  return false;
}

bool ApnListsMatch(const std::vector<TestApnData*>& expected_apns,
                   const base::Value::List& actual_apns,
                   bool has_state_field,
                   bool is_password_masked) {
  if (expected_apns.size() != actual_apns.size()) {
    return false;
  }

  for (size_t i = 0; i < expected_apns.size(); i++) {
    DCHECK(actual_apns[i].is_dict());
    const base::Value::Dict& actual_apn = actual_apns[i].GetDict();
    if (!OncApnHasId(actual_apn)) {
      return false;
    }
    if (!expected_apns[i]->OncApnEquals(actual_apn, has_state_field,
                                        is_password_masked)) {
      return false;
    }
  }
  return true;
}

bool MojoApnHasId(const mojom::ApnPropertiesPtr& apn) {
  if (!apn->id.has_value()) {
    return false;
  }
  return re2::RE2::FullMatch(*apn->id, kApnIdRegex);
}

}  // namespace

class CrosNetworkConfigTest : public testing::Test {
 public:
  CrosNetworkConfigTest() {
    // TODO(b/278643115) Remove LoginState dependency.
    LoginState::Initialize();
    SystemTokenCertDbStorage::Initialize();

    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<user_manager::FakeUserManager>());

    NetworkCertLoader::Initialize();
    helper_ = std::make_unique<NetworkHandlerTestHelper>();
    helper_->AddDefaultProfiles();
    helper_->ResetDevicesAndServices();

    helper_->RegisterPrefs(user_prefs_.registry(), local_state_.registry());
    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());

    helper_->InitializePrefs(&user_prefs_, &local_state_);
    NetworkHandler* network_handler = NetworkHandler::Get();
    cros_network_config_test_helper_ =
        std::make_unique<CrosNetworkConfigTestHelper>(true);
    SetupNetworkConfig(network_handler);
  }

  CrosNetworkConfigTest(const CrosNetworkConfigTest&) = delete;
  CrosNetworkConfigTest& operator=(const CrosNetworkConfigTest&) = delete;

  ~CrosNetworkConfigTest() override {
    carrier_lock_manager_.reset();
    cros_network_config_test_helper_.reset();
    cros_network_config_.reset();
    helper_.reset();
    if (traffic_counters::TrafficCountersHandler::IsInitialized()) {
      traffic_counters::TrafficCountersHandler::Shutdown();
    }
    NetworkCertLoader::Shutdown();
    scoped_user_manager_.reset();
    SystemTokenCertDbStorage::Shutdown();
    LoginState::Shutdown();
  }

  void SetupNetworkConfig(NetworkHandler* network_handler) {
    cros_network_config_ = std::make_unique<CrosNetworkConfig>(
        network_handler->network_state_handler(),
        network_handler->network_device_handler(),
        network_handler->cellular_inhibitor(),
        network_handler->cellular_esim_profile_handler(),
        network_handler->managed_network_configuration_handler(),
        network_handler->network_connection_handler(),
        network_handler->network_certificate_handler(),
        network_handler->network_profile_handler(),
        network_handler->technology_state_controller());
    SetupPolicy();
    SetupNetworks();
  }

  void SetupCarrierLock(bool is_locked) {
    ash::carrier_lock::CarrierLockManager::RegisterLocalPrefs(
        local_state_.registry());
    if (is_locked) {
      local_state_.SetBoolean(carrier_lock::kDisableManagerPref, false);
      local_state_.SetString(carrier_lock::kFcmTopicPref, "testtopic");
    }
    fake_modem_handler_ = std::make_unique<FakeNetwork3gppHandler>();
    fake_config_fetcher_ =
        std::make_unique<carrier_lock::FakeProvisioningConfigFetcher>();
    fake_psm_verifier_ = std::make_unique<carrier_lock::FakePsmClaimVerifier>();
    fake_fcm_subscriber_ =
        std::make_unique<carrier_lock::FakeFcmTopicSubscriber>();

    carrier_lock_manager_ = carrier_lock::CarrierLockManager::CreateForTesting(
        &local_state_, fake_modem_handler_.get(),
        std::move(fake_fcm_subscriber_), std::move(fake_psm_verifier_),
        std::move(fake_config_fetcher_));
  }

  void SetupPolicy() {
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler =
        NetworkHandler::Get()->managed_network_configuration_handler();
    managed_network_configuration_handler->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY,
        /*userhash=*/std::string(),
        /*network_configs_onc=*/base::Value::List(),
        /*global_network_config=*/base::Value::Dict());

    const std::string user_policy_ssid = "wifi2";
    std::optional<base::Value::Dict> wifi2_onc =
        chromeos::onc::ReadDictionaryFromJson(base::StringPrintf(
            R"({"GUID": "wifi2_guid", "Type": "WiFi",
                "Name": "wifi2", "Priority": 0,
                "WiFi": { "Passphrase": "fake", "SSID": "%s", "HexSSID": "%s",
                          "Security": "WPA-PSK", "AutoConnect": true}})",
            user_policy_ssid.c_str(),
            base::HexEncode(user_policy_ssid).c_str()));
    ASSERT_TRUE(wifi2_onc.has_value());

    std::optional<base::Value::Dict> wifi_eap_onc =
        chromeos::onc::ReadDictionaryFromJson(
            R"({ "GUID": "wifi_eap",
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
           })");
    ASSERT_TRUE(wifi_eap_onc.has_value());

    std::optional<base::Value::Dict> openvpn_onc =
        chromeos::onc::ReadDictionaryFromJson(base::StringPrintf(
            R"({ "GUID": "openvpn_guid", "Name": "openvpn", "Type": "VPN", "VPN": {
          "Host": "my.vpn.example.com", "Type": "OpenVPN", "OpenVPN": {
          "Auth": "MD5", "Cipher": "AES-192-CBC", "ClientCertType": "None",
          "CompressionAlgorithm": "LZO", "KeyDirection": "1",
          "TLSAuthContents": "%s"}}})",
            kOpenVPNTLSAuthContents));
    ASSERT_TRUE(openvpn_onc.has_value());

    base::Value::List user_policy_onc;
    user_policy_onc.Append(std::move(*wifi2_onc));
    user_policy_onc.Append(std::move(*wifi_eap_onc));
    user_policy_onc.Append(std::move(*openvpn_onc));
    managed_network_configuration_handler->SetPolicy(
        ::onc::ONC_SOURCE_USER_POLICY, helper()->UserHash(), user_policy_onc,
        /*global_network_config=*/base::Value::Dict());
    base::RunLoop().RunUntilIdle();
  }

  void SetupNetworks() {
    // Wifi device exists by default, add Ethernet and Cellular.
    helper()->device_test()->AddDevice("/device/stub_eth_device",
                                       shill::kTypeEthernet, "stub_eth_device");
    helper()->manager_test()->AddTechnology(shill::kTypeCellular,
                                            true /* enabled */);
    helper()->device_test()->AddDevice(
        kCellularDevicePath, shill::kTypeCellular, "stub_cellular_device");
    base::Value::Dict sim_value;
    sim_value.Set(shill::kSIMLockEnabledProperty, true);
    sim_value.Set(shill::kSIMLockTypeProperty, shill::kSIMLockPin);
    sim_value.Set(shill::kSIMLockRetriesLeftProperty, kSimRetriesLeft);
    helper()->device_test()->SetDeviceProperty(
        kCellularDevicePath, shill::kSIMLockStatusProperty,
        base::Value(std::move(sim_value)),
        /*notify_changed=*/false);
    helper()->device_test()->SetDeviceProperty(kCellularDevicePath,
                                               shill::kIccidProperty,
                                               base::Value(kCellularTestIccid),
                                               /*notify_changed=*/false);
    helper()->device_test()->SetDeviceProperty(kCellularDevicePath,
                                               shill::kImeiProperty,
                                               base::Value(kCellularTestImei),
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
        R"({"GUID": "%s", "Type": "cellular",  "State": "idle",
            "Strength": 0, "Cellular.NetworkTechnology": "LTE",
            "Cellular.ActivationState": "activated", "Cellular.ICCID": "%s",
            "Profile": "%s"})",
        kCellularGuid, kCellularTestIccid,
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

  void SetCellularFlashing(bool flashing) {
    helper()->device_test()->SetDeviceProperty(
        kCellularDevicePath, shill::kFlashingProperty, base::Value(flashing),
        /*notify_changed=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetupAPNList() {
    base::Value::List apn_entries;
    TestApnData apn_entry1;
    apn_entry1.access_point_name = kCellularTestApn1;
    apn_entry1.name = kCellularTestApnName1;
    apn_entry1.username = kCellularTestApnUsername1;
    apn_entry1.password = kCellularTestApnPassword1;
    apn_entries.Append(apn_entry1.AsShillApn());

    TestApnData apn_entry2;
    apn_entry2.access_point_name = kCellularTestApn2;
    apn_entry2.name = kCellularTestApnName2;
    apn_entry2.username = kCellularTestApnUsername2;
    apn_entry2.password = kCellularTestApnPassword2;
    apn_entry2.attach = kCellularTestApnAttach2;
    apn_entries.Append(apn_entry2.AsShillApn());

    helper()->device_test()->SetDeviceProperty(
        kCellularDevicePath, shill::kCellularApnListProperty,
        base::Value(std::move(apn_entries)),
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
               const std::optional<std::string>& guid,
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
                           std::optional<std::string> new_pin,
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

  void SetArcAlwaysOnUserPrefs(std::string package_name,
                               bool vpn_configured_allowed = false) {
    user_prefs_.SetUserPref(arc::prefs::kAlwaysOnVpnPackage,
                            base::Value(package_name));
    user_prefs_.SetUserPref(prefs::kVpnConfigAllowed,
                            base::Value(vpn_configured_allowed));
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
      const base::Value::List& traffic_counters,
      ComparisonType comparison_type) {
    base::RunLoop run_loop;
    cros_network_config()->RequestTrafficCounters(
        guid,
        base::BindOnce(
            [](const base::Value::List* expected_traffic_counters,
               ComparisonType type, base::OnceClosure quit_closure,
               std::vector<mojom::TrafficCounterPtr> actual_traffic_counters) {
              CompareTrafficCounters(actual_traffic_counters,
                                     *expected_traffic_counters, type);
              std::move(quit_closure).Run();
            },
            &traffic_counters, comparison_type, run_loop.QuitClosure()));
    run_loop.Run();
  }

  void SetTrafficCountersResetDayAndCompare(const std::string& guid,
                                            mojom::UInt32ValuePtr day,
                                            bool expected_success,
                                            base::Value* expected_reset_day) {
    base::RunLoop run_loop;
    cros_network_config()->SetTrafficCountersResetDay(
        guid, day ? std::move(day) : nullptr,
        base::BindOnce(
            [](const std::string* const guid, bool* expected_success,
               base::Value* expected_reset_day,
               NetworkMetadataStore* network_metadata_store,
               base::OnceClosure quit_closure, bool success) {
              EXPECT_EQ(*expected_success, success);
              const base::Value* actual_reset_day =
                  network_metadata_store->GetDayOfTrafficCountersAutoReset(
                      *guid);
              if (expected_reset_day) {
                EXPECT_TRUE(actual_reset_day);
                EXPECT_EQ(*expected_reset_day, *actual_reset_day);
              } else {
                EXPECT_EQ(actual_reset_day, nullptr);
              }
              std::move(quit_closure).Run();
            },
            &guid, &expected_success, expected_reset_day,
            network_metadata_store(), run_loop.QuitClosure()));
    run_loop.Run();
  }

  bool CreateCustomApn(const std::string& guid, mojom::ApnPropertiesPtr apn) {
    bool success = false;
    base::RunLoop run_loop;
    cros_network_config()->CreateCustomApn(
        guid, std::move(apn),
        base::BindOnce(
            [](bool* successp, base::OnceClosure quit_closure, bool success) {
              *successp = success;
              std::move(quit_closure).Run();
            },
            &success, run_loop.QuitClosure()));
    run_loop.Run();
    return success;
  }

  bool CreateExclusivelyEnabledCustomApn(const std::string& guid,
                                         mojom::ApnPropertiesPtr apn) {
    bool success = false;
    base::RunLoop run_loop;
    cros_network_config()->CreateExclusivelyEnabledCustomApn(
        guid, std::move(apn),
        base::BindOnce(
            [](bool* successp, base::OnceClosure quit_closure, bool success) {
              *successp = success;
              std::move(quit_closure).Run();
            },
            &success, run_loop.QuitClosure()));
    run_loop.Run();
    return success;
  }

  void RemoveCustomApn(const std::string& guid, const std::string& apn_id) {
    cros_network_config()->RemoveCustomApn(guid, apn_id);
    base::RunLoop().RunUntilIdle();
  }

  void ModifyCustomApn(const std::string& guid, mojom::ApnPropertiesPtr apn) {
    cros_network_config()->ModifyCustomApn(guid, std::move(apn));
    base::RunLoop().RunUntilIdle();
  }

  void SetAllowApnModification(bool allow_apn_modification) {
    base::Value::Dict global_config;
    global_config.Set(::onc::global_network_config::kAllowAPNModification,
                      allow_apn_modification);
    managed_network_configuration_handler()->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
        /*network_configs_onc=*/base::Value::List(), global_config);
    base::RunLoop().RunUntilIdle();
  }

  bool CustomApnsInNetworkMetadataStoreMatch(
      const std::string& guid,
      const std::vector<TestApnData*>& expected_apns) {
    if (const base::Value::List* custom_apns =
            network_metadata_store()->GetCustomApnList(guid)) {
      return ApnListsMatch(expected_apns, *custom_apns,
                           /*has_state_field=*/true,
                           /*is_password_masked=*/false);
    }
    return expected_apns.empty();
  }

  bool CustomApnsInCellularConfigMatch(
      const std::string& guid,
      const std::vector<TestApnData*>& expected_apns,
      const TestNetworkConfigurationObserver& observer) {
    const base::Value::Dict* user_settings = observer.GetUserSettings(guid);
    if (!user_settings) {
      return expected_apns.empty();
    }

    const base::Value::Dict* cellular_settings =
        user_settings->FindDict(::onc::network_type::kCellular);
    if (!cellular_settings) {
      return false;
    }

    const base::Value::List* custom_apns =
        cellular_settings->FindList(::onc::cellular::kCustomAPNList);
    if (!custom_apns) {
      return false;
    }

    return ApnListsMatch(expected_apns, *custom_apns,
                         /*has_state_field=*/true,
                         /*is_password_masked=*/true);
  }

  bool CustomApnsInManagedPropertiesMatch(
      const std::string& guid,
      const std::vector<TestApnData*>& expected_apns) {
    mojom::ManagedPropertiesPtr props = GetManagedProperties(guid);
    if (!props) {
      return expected_apns.empty();
    }
    if (!props->type_properties->is_cellular()) {
      return false;
    }
    if (!props->type_properties->get_cellular()->custom_apn_list.has_value()) {
      return expected_apns.empty();
    }

    const std::vector<mojom::ApnPropertiesPtr>& mojo_apn_list =
        props->type_properties->get_cellular()->custom_apn_list.value();
    if (expected_apns.size() != mojo_apn_list.size()) {
      return false;
    }

    for (size_t i = 0; i < expected_apns.size(); i++) {
      if (!MojoApnHasId(mojo_apn_list[i])) {
        return false;
      }
      if (!expected_apns[i]->MojoApnEquals(*mojo_apn_list[i])) {
        return false;
      }
    }
    return true;
  }

  void AssertCreateCustomApnResultBucketCount(size_t num_success,
                                              size_t num_failure) {
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kCreateCustomApnResultHistogram, true,
        num_success);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kCreateCustomApnResultHistogram, false,
        num_failure);
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::
            kCreateCustomApnAuthenticationTypeHistogram,
        num_success);
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::kCreateCustomApnIpTypeHistogram,
        num_success);
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::kCreateCustomApnApnTypesHistogram,
        num_success);
  }

  void AssertCreateCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType auth_type,
      size_t auth_type_count,
      mojom::ApnIpType ip_type,
      size_t ip_type_count,
      ApnTypes apn_types,
      size_t apn_types_count) {
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::
            kCreateCustomApnAuthenticationTypeHistogram,
        auth_type, auth_type_count);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kCreateCustomApnIpTypeHistogram, ip_type,
        ip_type_count);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kCreateCustomApnApnTypesHistogram,
        apn_types, apn_types_count);
  }

  void AssertCreateExclusivelyEnabledCustomApnResultBucketCount(
      size_t num_success,
      size_t num_failure) {
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::
            kCreateExclusivelyEnabledCustomApnResultHistogram,
        true, num_success);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::
            kCreateExclusivelyEnabledCustomApnResultHistogram,
        false, num_failure);
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::
            kCreateExclusivelyEnabledCustomApnAuthenticationTypeHistogram,
        num_success);
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::
            kCreateExclusivelyEnabledCustomApnIpTypeHistogram,
        num_success);
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::
            kCreateExclusivelyEnabledCustomApnApnTypesHistogram,
        num_success);
  }

  void AssertCreateExclusivelyEnabledCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType auth_type,
      size_t auth_type_count,
      mojom::ApnIpType ip_type,
      size_t ip_type_count,
      ApnTypes apn_types,
      size_t apn_types_count) {
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::
            kCreateExclusivelyEnabledCustomApnAuthenticationTypeHistogram,
        auth_type, auth_type_count);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::
            kCreateExclusivelyEnabledCustomApnIpTypeHistogram,
        ip_type, ip_type_count);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::
            kCreateExclusivelyEnabledCustomApnApnTypesHistogram,
        apn_types, apn_types_count);
  }

  void AssertRemoveCustomApnResultBucketCount(size_t num_success,
                                              size_t num_failure) {
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kRemoveCustomApnResultHistogram, true,
        num_success);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kRemoveCustomApnResultHistogram, false,
        num_failure);
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::kRemoveCustomApnApnTypesHistogram,
        num_success);
  }

  void AssertRemoveCustomApnPropertiesBucketCount(ApnTypes apn_types,
                                                  size_t apn_types_count) {
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kRemoveCustomApnApnTypesHistogram,
        apn_types, apn_types_count);
  }

  void AssertApnHistogramCounts(const ApnHistogramCounts& count) {
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kModifyCustomApnResultHistogram, true,
        count.num_modify_success);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kModifyCustomApnResultHistogram, false,
        count.num_modify_failure);
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::kModifyCustomApnApnTypesHistogram,
        count.num_modify_success);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kModifyCustomApnApnTypesHistogram,
        ApnTypes::kAttach, count.num_modify_type_attach);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kModifyCustomApnApnTypesHistogram,
        ApnTypes::kDefault, count.num_modify_type_default);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kModifyCustomApnApnTypesHistogram,
        ApnTypes::kDefaultAndAttach, count.num_modify_type_default_and_attach);

    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kEnableCustomApnResultHistogram, true,
        count.num_enable_success);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kEnableCustomApnResultHistogram, false,
        count.num_enable_failure);
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::kEnableCustomApnApnTypesHistogram,
        count.num_enable_success);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kEnableCustomApnApnTypesHistogram,
        ApnTypes::kAttach, count.num_enable_type_attach);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kEnableCustomApnApnTypesHistogram,
        ApnTypes::kDefault, count.num_enable_type_default);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kEnableCustomApnApnTypesHistogram,
        ApnTypes::kDefaultAndAttach, count.num_enable_type_default_and_attach);

    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kDisableCustomApnResultHistogram, true,
        count.num_disable_success);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kDisableCustomApnResultHistogram, false,
        count.num_disable_failure);
    histogram_tester_.ExpectTotalCount(
        CellularNetworkMetricsLogger::kDisableCustomApnApnTypesHistogram,
        count.num_disable_success);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kDisableCustomApnApnTypesHistogram,
        ApnTypes::kAttach, count.num_disable_type_attach);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kDisableCustomApnApnTypesHistogram,
        ApnTypes::kDefault, count.num_disable_type_default);
    histogram_tester_.ExpectBucketCount(
        CellularNetworkMetricsLogger::kDisableCustomApnApnTypesHistogram,
        ApnTypes::kDefaultAndAttach, count.num_disable_type_default_and_attach);
  }

  void AssertCellularAllowTextMessages(
      const std::string& guid,
      std::optional<bool> expected_active_value,
      std::optional<bool> expected_policy_value,
      ::chromeos::network_config::mojom::PolicySource policy_source) {
    mojom::ManagedPropertiesPtr properties = GetManagedProperties(guid);
    mojom::GlobalPolicyPtr policy = GetGlobalPolicy();

    ASSERT_TRUE(properties);
    ASSERT_EQ(guid, properties->guid);
    ASSERT_TRUE(properties->type_properties->is_cellular());
    // If there isn't an expected policy or actual value, that means the
    // property is undefined.
    if (!expected_active_value.has_value() &&
        !expected_policy_value.has_value()) {
      EXPECT_FALSE(
          properties->type_properties->get_cellular()->allow_text_messages);
      EXPECT_EQ(mojom::SuppressionType::kUnset, policy->allow_text_messages);
      return;
    }

    ASSERT_TRUE(
        properties->type_properties->get_cellular()->allow_text_messages);
    EXPECT_EQ(policy_source, properties->type_properties->get_cellular()
                                 ->allow_text_messages->policy_source);

    if (expected_policy_value.has_value()) {
      EXPECT_EQ(*expected_policy_value,
                properties->type_properties->get_cellular()
                    ->allow_text_messages->policy_value);

      mojom::SuppressionType expected_global_policy_type =
          expected_policy_value.value() ? mojom::SuppressionType::kAllow
                                        : mojom::SuppressionType::kSuppress;
      EXPECT_EQ(expected_global_policy_type, policy->allow_text_messages);
    } else {
      EXPECT_EQ(mojom::SuppressionType::kUnset, policy->allow_text_messages);
    }

    if (expected_active_value.has_value()) {
      EXPECT_EQ(*expected_active_value,
                properties->type_properties->get_cellular()
                    ->allow_text_messages->active_value);
    }
  }

  void SetSerialNumber(const std::string& serial_number) {
    cros_network_config_test_helper_->SetSerialNumber(serial_number);
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
  NetworkConfigurationHandler* network_configuration_handler() {
    return NetworkHandler::Get()->network_configuration_handler();
  }
  std::string wifi1_path() { return wifi1_path_; }
  std::string vpn_path() { return vpn_path_; }

 protected:
  base::HistogramTester histogram_tester_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<NetworkHandlerTestHelper> helper_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<CrosNetworkConfig> cros_network_config_;
  std::unique_ptr<CrosNetworkConfigTestHelper> cros_network_config_test_helper_;
  std::unique_ptr<CrosNetworkConfigTestObserver> observer_;
  std::unique_ptr<carrier_lock::CarrierLockManager> carrier_lock_manager_;
  std::unique_ptr<FakeNetwork3gppHandler> fake_modem_handler_;
  std::unique_ptr<carrier_lock::FakeFcmTopicSubscriber> fake_fcm_subscriber_;
  std::unique_ptr<carrier_lock::FakePsmClaimVerifier> fake_psm_verifier_;
  std::unique_ptr<carrier_lock::FakeProvisioningConfigFetcher>
      fake_config_fetcher_;

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
  EXPECT_FALSE(network->type_state->get_wifi()->hidden_ssid);
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
  EXPECT_TRUE(network->type_state->get_wifi()->hidden_ssid);
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

  network = GetNetworkState(kCellularGuid);
  ASSERT_TRUE(network);
  EXPECT_EQ(kCellularGuid, network->guid);
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

  // TODO(crbug.com/41434332): Test ProxyMode once UIProxyConfigService logic is
  // improved.
}

TEST_F(CrosNetworkConfigTest, PortalState) {
  mojom::NetworkStatePropertiesPtr network = GetNetworkState("eth_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ(mojom::ConnectionStateType::kOnline, network->connection_state);
  EXPECT_EQ(mojom::PortalState::kOnline, network->portal_state);
  EXPECT_FALSE(network->portal_probe_url);

  helper()->ConfigureService(
      R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "portal-suspected",
          "Strength": 90, "AutoConnect": true})");
  network = GetNetworkState("wifi1_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ(mojom::ConnectionStateType::kPortal, network->connection_state);
  EXPECT_EQ(mojom::PortalState::kPortalSuspected, network->portal_state);
  ASSERT_TRUE(network->portal_probe_url);
  EXPECT_EQ(captive_portal::CaptivePortalDetector::kDefaultURL,
            *network->portal_probe_url);

  helper()->ConfigureService(
      R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "redirect-found",
          "Strength": 90, "AutoConnect": true, "ProbeUrl": "http://foo.com"})");
  network = GetNetworkState("wifi1_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ(mojom::ConnectionStateType::kPortal, network->connection_state);
  EXPECT_EQ(mojom::PortalState::kPortal, network->portal_state);
  ASSERT_TRUE(network->portal_probe_url);
  EXPECT_EQ("http://foo.com/", network->portal_probe_url->spec());

  helper()->ConfigureService(
      R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "no-connectivity",
          "Strength": 90, "AutoConnect": true})");
  network = GetNetworkState("wifi1_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ(mojom::ConnectionStateType::kPortal, network->connection_state);
  EXPECT_EQ(mojom::PortalState::kNoInternet, network->portal_state);
  EXPECT_FALSE(network->portal_probe_url);
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
  base::RunLoop().RunUntilIdle();

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
  EXPECT_EQ(kCellularTestImei, cellular->imei);
  EXPECT_EQ(std::nullopt, cellular->serial);

  mojom::DeviceStateProperties* vpn = devices[3].get();
  EXPECT_EQ(mojom::NetworkType::kVPN, vpn->type);
  EXPECT_EQ(mojom::DeviceStateType::kEnabled, vpn->device_state);

  // Disable WiFi
  NetworkHandler::Get()->technology_state_controller()->SetTechnologiesEnabled(
      NetworkTypePattern::WiFi(), false, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  devices = GetDeviceStateList();
  ASSERT_EQ(4u, devices.size());
  EXPECT_EQ(mojom::NetworkType::kWiFi, devices[0]->type);
  EXPECT_EQ(mojom::DeviceStateType::kDisabled, devices[0]->device_state);
}

TEST_F(CrosNetworkConfigTest, GetDeviceStateListSerial) {
  SetSerialNumber(kCellularTestSerial);
  NetworkHandler* network_handler = NetworkHandler::Get();
  SetupNetworkConfig(network_handler);

  std::vector<mojom::DeviceStatePropertiesPtr> devices = GetDeviceStateList();
  ASSERT_EQ(4u, devices.size());
  mojom::DeviceStateProperties* cellular = devices[2].get();
  EXPECT_EQ(mojom::NetworkType::kCellular, cellular->type);
  EXPECT_EQ(mojom::DeviceStateType::kEnabled, cellular->device_state);
  EXPECT_FALSE(cellular->sim_absent);
  ASSERT_TRUE(cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_EQ(shill::kSIMLockPin, cellular->sim_lock_status->lock_type);
  EXPECT_EQ(3, cellular->sim_lock_status->retries_left);
  EXPECT_EQ(kCellularTestImei, cellular->imei);
  EXPECT_EQ(kCellularTestSerial, cellular->serial);
}

TEST_F(CrosNetworkConfigTest, GetDeviceStateListCarrierLocked) {
  SetupCarrierLock(true);

  std::vector<mojom::DeviceStatePropertiesPtr> devices = GetDeviceStateList();
  ASSERT_EQ(4u, devices.size());

  mojom::DeviceStateProperties* cellular = devices[2].get();
  EXPECT_EQ(mojom::NetworkType::kCellular, cellular->type);
  EXPECT_EQ(mojom::DeviceStateType::kEnabled, cellular->device_state);
  EXPECT_FALSE(cellular->sim_absent);
  ASSERT_TRUE(cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_EQ(shill::kSIMLockPin, cellular->sim_lock_status->lock_type);
  EXPECT_EQ(3, cellular->sim_lock_status->retries_left);
  EXPECT_EQ(kCellularTestImei, cellular->imei);
  ASSERT_TRUE(cellular->is_carrier_locked);
}

TEST_F(CrosNetworkConfigTest, GetDeviceStateListCarrierUnlocked) {
  SetupCarrierLock(false);

  std::vector<mojom::DeviceStatePropertiesPtr> devices = GetDeviceStateList();
  ASSERT_EQ(4u, devices.size());

  mojom::DeviceStateProperties* cellular = devices[2].get();
  EXPECT_EQ(mojom::NetworkType::kCellular, cellular->type);
  EXPECT_EQ(mojom::DeviceStateType::kEnabled, cellular->device_state);
  EXPECT_FALSE(cellular->sim_absent);
  ASSERT_TRUE(cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_EQ(shill::kSIMLockPin, cellular->sim_lock_status->lock_type);
  EXPECT_EQ(3, cellular->sim_lock_status->retries_left);
  EXPECT_EQ(kCellularTestImei, cellular->imei);
  ASSERT_FALSE(cellular->is_carrier_locked);
}

TEST_F(CrosNetworkConfigTest, GetDeviceStateListFlashing) {
  SetCellularFlashing(true);

  std::vector<mojom::DeviceStatePropertiesPtr> devices = GetDeviceStateList();
  ASSERT_EQ(4u, devices.size());

  mojom::DeviceStateProperties* cellular = devices[2].get();
  EXPECT_EQ(mojom::NetworkType::kCellular, cellular->type);
  ASSERT_TRUE(cellular->is_flashing);

  SetCellularFlashing(false);

  devices = GetDeviceStateList();
  ASSERT_EQ(4u, devices.size());

  cellular = devices[2].get();
  EXPECT_EQ(mojom::NetworkType::kCellular, cellular->type);
  ASSERT_FALSE(cellular->is_flashing);
}

TEST_F(CrosNetworkConfigTest, GetManagedPropertiesCellularProvider) {
  auto set_home_provider = [this](std::string_view name, std::string_view code,
                                  std::string_view country) {
    base::Value::Dict home_provider;
    home_provider.Set("name", name);
    home_provider.Set("code", code);
    home_provider.Set("country", country);
    helper()->device_test()->SetDeviceProperty(
        kCellularDevicePath, shill::kHomeProviderProperty,
        base::Value(home_provider.Clone()),
        /*notify_changed=*/true);
    base::RunLoop().RunUntilIdle();
  };

  auto check_home_provider = [this](std::string_view name,
                                    std::string_view code,
                                    std::string_view country) {
    mojom::ManagedPropertiesPtr properties =
        GetManagedProperties(kCellularGuid);
    ASSERT_TRUE(properties);

    const mojom::ManagedCellularPropertiesPtr& cellular =
        properties->type_properties->get_cellular();
    ASSERT_TRUE(cellular);
    const mojom::CellularProviderPropertiesPtr& provider =
        cellular->home_provider;
    ASSERT_TRUE(provider);
    EXPECT_EQ(name, provider->name);
    EXPECT_EQ(code, provider->code);
    ASSERT_TRUE(provider->country.has_value());
    EXPECT_EQ(country, *provider->country);
  };

  const std::string kDefaultName = "MobileNetwork";
  const std::string kDefaultCode = "000000";
  set_home_provider(/*name=*/"", /*code=*/"", /*country=*/"");
  check_home_provider(kDefaultName, kDefaultCode, /*country=*/"");

  const std::string kName = "ProviderName";
  const std::string kCode = "ProviderCode";
  const std::string kCountry = "ProviderCountry";
  set_home_provider(kName, kCode, kCountry);
  check_home_provider(kName, kCode, kCountry);
}

TEST_F(CrosNetworkConfigTest, GetManagedPropertiesCarrierLocked) {
  /* Lock the SIM using network-pin */
  base::Value::Dict sim_value;
  sim_value.Set(shill::kSIMLockEnabledProperty, true);
  sim_value.Set(shill::kSIMLockTypeProperty, shill::kSIMLockNetworkPin);
  sim_value.Set(shill::kSIMLockRetriesLeftProperty, kSimRetriesLeft);
  helper()->device_test()->SetDeviceProperty(kCellularDevicePath,
                                             shill::kSIMLockStatusProperty,
                                             base::Value(std::move(sim_value)),
                                             /*notify_changed=*/true);
  base::RunLoop().RunUntilIdle();

  mojom::ManagedPropertiesPtr properties = GetManagedProperties(kCellularGuid);
  ASSERT_TRUE(properties);
  EXPECT_EQ(kCellularGuid, properties->guid);
  EXPECT_EQ(mojom::NetworkType::kCellular, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
            properties->connection_state);
  ASSERT_TRUE(properties->type_properties);
  mojom::ManagedCellularPropertiesPtr& cellular =
      properties->type_properties->get_cellular();
  ASSERT_TRUE(cellular);
  EXPECT_TRUE(cellular->sim_locked);
  EXPECT_EQ(shill::kSIMLockNetworkPin, cellular->sim_lock_type);
}

TEST_F(CrosNetworkConfigTest, GetManagedPropertiesCarrierLockedDisabled) {
  /* Lock the SIM using network-pin */
  base::Value::Dict sim_value;
  sim_value.Set(shill::kSIMLockEnabledProperty, true);
  sim_value.Set(shill::kSIMLockTypeProperty, shill::kSIMLockPin);
  sim_value.Set(shill::kSIMLockRetriesLeftProperty, kSimRetriesLeft);
  helper()->device_test()->SetDeviceProperty(kCellularDevicePath,
                                             shill::kSIMLockStatusProperty,
                                             base::Value(std::move(sim_value)),
                                             /*notify_changed=*/true);
  base::RunLoop().RunUntilIdle();

  mojom::ManagedPropertiesPtr properties = GetManagedProperties(kCellularGuid);
  ASSERT_TRUE(properties);
  EXPECT_EQ(kCellularGuid, properties->guid);
  EXPECT_EQ(mojom::NetworkType::kCellular, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
            properties->connection_state);
  ASSERT_TRUE(properties->type_properties);
  mojom::ManagedCellularPropertiesPtr& cellular =
      properties->type_properties->get_cellular();
  ASSERT_TRUE(cellular);
  EXPECT_TRUE(cellular->sim_locked);
  EXPECT_EQ(shill::kSIMLockPin, cellular->sim_lock_type);
}

TEST_F(CrosNetworkConfigTest, SimStateCarrierLocked) {
  /* Lock the SIM using network-pin */
  base::Value::Dict sim_value;
  sim_value.Set(shill::kSIMLockEnabledProperty, true);
  sim_value.Set(shill::kSIMLockTypeProperty, shill::kSIMLockNetworkPin);
  sim_value.Set(shill::kSIMLockRetriesLeftProperty, kSimRetriesLeft);
  helper()->device_test()->SetDeviceProperty(kCellularDevicePath,
                                             shill::kSIMLockStatusProperty,
                                             base::Value(std::move(sim_value)),
                                             /*notify_changed=*/true);
  base::RunLoop().RunUntilIdle();

  mojom::DeviceStatePropertiesPtr cellular =
      GetDeviceStateFromList(mojom::NetworkType::kCellular);

  EXPECT_EQ(mojom::NetworkType::kCellular, cellular->type);
  EXPECT_EQ(mojom::DeviceStateType::kEnabled, cellular->device_state);
  ASSERT_TRUE(cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_EQ(shill::kSIMLockNetworkPin, cellular->sim_lock_status->lock_type);
  EXPECT_EQ(3, cellular->sim_lock_status->retries_left);

  // Any attempt to unlock carrier locked sim with the pin should fail and
  // should not change the carrier lock status.
  EXPECT_FALSE(SetCellularSimState(FakeShillDeviceClient::kDefaultSimPin,
                                   /*new_pin=*/std::nullopt,
                                   /*require_pin=*/false));

  // Sim should continue to be carrier locked.
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_EQ(shill::kSIMLockNetworkPin, cellular->sim_lock_status->lock_type);
  EXPECT_EQ(3, cellular->sim_lock_status->retries_left);
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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kTrafficCountersEnabled,
                            features::kTrafficCountersForWiFiTesting},
      /*disabled_features=*/{});
  traffic_counters::TrafficCountersHandler::InitializeForTesting();
  SetTrafficCountersResetDayAndCompare("eth_guid",
                                       /*day=*/mojom::UInt32Value::New(32),
                                       /*expected_success=*/false,
                                       /*expected_reset_day=*/nullptr);
  mojom::ManagedPropertiesPtr properties = GetManagedProperties("eth_guid");
  ASSERT_TRUE(properties);
  EXPECT_EQ("eth_guid", properties->guid);
  EXPECT_EQ(mojom::NetworkType::kEthernet, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kOnline, properties->connection_state);
  // Traffic counters are not presented for Ethernet networks.
  ASSERT_FALSE(properties->traffic_counter_properties);

  helper()->SetServiceProperty(wifi1_path(), shill::kStateProperty,
                               base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();

  base::Value expected_reset_day(2);
  SetTrafficCountersResetDayAndCompare("wifi1_guid",
                                       /*day=*/mojom::UInt32Value::New(2),
                                       /*expected_success=*/true,
                                       &expected_reset_day);
  properties = GetManagedProperties("wifi1_guid");
  ASSERT_TRUE(properties);
  EXPECT_EQ("wifi1_guid", properties->guid);
  EXPECT_EQ(mojom::NetworkType::kWiFi, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kOnline, properties->connection_state);
  ASSERT_TRUE(properties->type_properties);
  ASSERT_TRUE(properties->type_properties->is_wifi());
  EXPECT_EQ(50, properties->type_properties->get_wifi()->signal_strength);
  EXPECT_EQ(mojom::OncSource::kNone, properties->source);
  EXPECT_FALSE(properties->type_properties->get_wifi()->is_syncable);
  ASSERT_TRUE(properties->traffic_counter_properties &&
              properties->traffic_counter_properties->last_reset_time);
  EXPECT_EQ(123456789987654,
            properties->traffic_counter_properties->last_reset_time
                ->ToDeltaSinceWindowsEpoch()
                .InMilliseconds());
  EXPECT_EQ(static_cast<uint32_t>(2),
            properties->traffic_counter_properties->user_specified_reset_day);

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
  EXPECT_FALSE(properties->type_properties->get_wifi()->is_syncable);

  properties = GetManagedProperties(kCellularGuid);
  ASSERT_TRUE(properties);
  EXPECT_EQ(kCellularGuid, properties->guid);
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

TEST_F(CrosNetworkConfigTest, FillInCustomAPNList) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(/*enabled_features=*/
                                       {features::kApnRevamp,
                                        features::kAllowApnModificationPolicy},
                                       /*disabled_features=*/{});

  TestApnData test_apn1;
  test_apn1.access_point_name = kCellularTestApn1;
  test_apn1.name = kCellularTestApnName1;
  test_apn1.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault};
  test_apn1.onc_state = ::onc::cellular_apn::kStateEnabled;
  test_apn1.id = "apn_id_1";

  auto populated_apn_list = base::Value::List().Append(test_apn1.AsOncApn());

  NetworkHandler::Get()->network_metadata_store()->SetCustomApnList(
      kCellularGuid, populated_apn_list.Clone());

  std::string service_path = helper()->ConfigureService(base::StringPrintf(
      kTestApnCellularShillDictFmt, kCellularGuid, shill::kStateIdle,
      kCellularTestIccid, NetworkProfileHandler::GetSharedProfilePath().c_str(),
      CreateApnShillDict().c_str()));

  std::optional<base::Value::List> shill_custom_apns =
      helper()->GetServiceListProperty(service_path,
                                       shill::kCellularCustomApnListProperty);
  ASSERT_FALSE(shill_custom_apns.has_value());

  auto config = mojom::ConfigProperties::New();
  auto cellular_config = mojom::CellularConfigProperties::New();
  auto new_roaming = mojom::RoamingProperties::New();
  new_roaming->allow_roaming = false;
  cellular_config->roaming = std::move(new_roaming);
  config->type_config = mojom::NetworkTypeConfigProperties::NewCellular(
      std::move(cellular_config));
  SetProperties(kCellularGuid, std::move(config));

  shill_custom_apns = helper()->GetServiceListProperty(
      service_path, shill::kCellularCustomApnListProperty);

  ASSERT_TRUE(shill_custom_apns.has_value());
  EXPECT_EQ(1u, shill_custom_apns->size());
  const std::string* apn_name =
      shill_custom_apns->front().GetDict().FindString(shill::kApnNameProperty);
  EXPECT_EQ(kCellularTestApnName1, *apn_name);
  const std::string* apn_type =
      shill_custom_apns->front().GetDict().FindString(shill::kApnTypesProperty);
  EXPECT_EQ(shill::kApnTypeDefault, *apn_type);
}

TEST_F(CrosNetworkConfigTest, CustomAPN) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kApnRevamp);
  SetupAPNList();
  // Verify that setting APN to an entry that already exists in apn list
  // does not update the custom apn list.
  auto config = mojom::ConfigProperties::New();
  auto cellular_config = mojom::CellularConfigProperties::New();
  TestApnData test_apn_data1;
  test_apn_data1.access_point_name = kCellularTestApn1;
  test_apn_data1.name = kCellularTestApnName1;
  test_apn_data1.username = kCellularTestApnUsername1;
  test_apn_data1.password = kCellularTestApnPassword1;
  test_apn_data1.attach = kCellularTestApnAttach1;
  cellular_config->apn = test_apn_data1.AsMojoApn();
  config->type_config = mojom::NetworkTypeConfigProperties::NewCellular(
      std::move(cellular_config));
  SetProperties(kCellularGuid, std::move(config));
  const base::Value::List* apn_list =
      NetworkHandler::Get()->network_metadata_store()->GetCustomApnList(
          kCellularGuid);
  ASSERT_FALSE(apn_list);

  // Verify that custom APN list is updated properly.
  config = mojom::ConfigProperties::New();
  cellular_config = mojom::CellularConfigProperties::New();
  TestApnData test_apn_data3;
  test_apn_data3.access_point_name = kCellularTestApn3;
  test_apn_data3.name = kCellularTestApnName3;
  test_apn_data3.username = kCellularTestApnUsername3;
  test_apn_data3.password = kCellularTestApnPassword3;
  test_apn_data3.attach = kCellularTestApnAttach3;
  cellular_config->apn = test_apn_data3.AsMojoApn();
  config->type_config = mojom::NetworkTypeConfigProperties::NewCellular(
      std::move(cellular_config));
  SetProperties(kCellularGuid, std::move(config));
  apn_list = NetworkHandler::Get()->network_metadata_store()->GetCustomApnList(
      kCellularGuid);
  ASSERT_TRUE(apn_list);

  // Verify that custom APN list is returned properly in managed properties.
  mojom::ManagedPropertiesPtr properties = GetManagedProperties(kCellularGuid);
  ASSERT_TRUE(properties);
  ASSERT_EQ(kCellularGuid, properties->guid);
  ASSERT_TRUE(properties->type_properties->is_cellular());
  ASSERT_TRUE(
      properties->type_properties->get_cellular()->custom_apn_list.has_value());
  ASSERT_EQ(
      1u, properties->type_properties->get_cellular()->custom_apn_list->size());
  const mojom::ApnPropertiesPtr& first_apn =
      properties->type_properties->get_cellular()->custom_apn_list->front();
  EXPECT_TRUE(test_apn_data3.MojoApnEquals(*first_apn));
}

TEST_F(CrosNetworkConfigTest,
       CanCreateDisabledAttachApnWithoutExistingDefaultApn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  // Register an observer to capture values sent to Shill
  TestNetworkConfigurationObserver network_config_observer(
      network_configuration_handler());

  ASSERT_FALSE(network_metadata_store()->GetCustomApnList(kCellularGuid));

  // CreateCustomApn with an exclusively attach APN in the disabled state, and
  // verify that it is created even though a default APN does not exist.
  TestApnData test_apn1;
  test_apn1.mojo_state = mojom::ApnState::kDisabled;
  test_apn1.onc_state = ::onc::cellular_apn::kStateDisabled;
  test_apn1.access_point_name = kCellularTestApn1;
  test_apn1.name = kCellularTestApnName1;
  test_apn1.username = kCellularTestApnUsername1;
  test_apn1.password = kCellularTestApnPassword1;
  test_apn1.attach = kCellularTestApnAttach1;
  test_apn1.mojo_apn_types = {mojom::ApnType::kAttach};
  test_apn1.onc_apn_types = {::onc::cellular_apn::kApnTypeAttach};
  EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn1.AsMojoApn()));
  ASSERT_TRUE(network_metadata_store()->GetCustomApnList(kCellularGuid));
  {
    std::vector<TestApnData*> expected_apns({&test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertCreateCustomApnResultBucketCount(/*num_success=*/1, /*num_failure=*/0);
  AssertCreateCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType::kAutomatic,
      /*auth_type_count=*/1, mojom::ApnIpType::kAutomatic,
      /*ip_type_count=*/1, ApnTypes::kAttach, /*apn_types_count=*/1);
}

TEST_F(CrosNetworkConfigTest, CreateCustomApnList) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  // Register an observer to capture values sent to Shill
  TestNetworkConfigurationObserver network_config_observer(
      network_configuration_handler());

  const base::Value::List* custom_apns =
      network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_FALSE(custom_apns);

  // CreateCustomApn with attach only and verify that it doesn't get added
  // because its missing a default.
  TestApnData test_apn1;
  test_apn1.access_point_name = kCellularTestApn1;
  test_apn1.name = kCellularTestApnName1;
  test_apn1.username = kCellularTestApnUsername1;
  test_apn1.password = kCellularTestApnPassword1;
  test_apn1.attach = kCellularTestApnAttach1;
  test_apn1.mojo_apn_types = {mojom::ApnType::kAttach};
  test_apn1.onc_apn_types = {::onc::cellular_apn::kApnTypeAttach};
  EXPECT_FALSE(CreateCustomApn(kCellularGuid, test_apn1.AsMojoApn()));

  EXPECT_EQ(0u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> empty_apn_list({});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, empty_apn_list));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, empty_apn_list,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, empty_apn_list));
  }
  AssertCreateCustomApnResultBucketCount(/*num_success=*/0, /*num_failure=*/0);
  AssertCreateCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType::kAutomatic,
      /*auth_type_count=*/0, mojom::ApnIpType::kAutomatic,
      /*ip_type_count=*/0, ApnTypes::kAttach, /*apn_types_count=*/0);

  // CreateCustomApn with attach and default and mock a failure.
  ShillServiceClient::Get()
      ->GetTestInterface()
      ->SetErrorForNextSetPropertiesAttempt("Error.NotReady");
  TestApnData test_apn2;
  test_apn2.access_point_name = kCellularTestApn1;
  test_apn2.name = kCellularTestApnName1;
  test_apn2.username = kCellularTestApnUsername1;
  test_apn2.password = kCellularTestApnPassword1;
  test_apn2.attach = kCellularTestApnAttach1;
  test_apn2.mojo_apn_types = {mojom::ApnType::kDefault,
                              mojom::ApnType::kAttach};
  test_apn2.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault,
                             ::onc::cellular_apn::kApnTypeAttach};
  EXPECT_FALSE(CreateCustomApn(kCellularGuid, test_apn2.AsMojoApn()));
  EXPECT_EQ(0u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> empty_apn_list({});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, empty_apn_list));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, empty_apn_list,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, empty_apn_list));
  }
  AssertCreateCustomApnResultBucketCount(/*num_success=*/0, /*num_failure=*/1);
  AssertCreateCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType::kAutomatic,
      /*auth_type_count=*/0, mojom::ApnIpType::kAutomatic,
      /*ip_type_count=*/0, ApnTypes::kDefaultAndAttach, /*apn_types_count=*/0);

  // Try again to create the APN without mocking a failure.
  EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn2.AsMojoApn()));

  EXPECT_EQ(1u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn2});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertCreateCustomApnResultBucketCount(/*num_success=*/1, /*num_failure=*/1);
  AssertCreateCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType::kAutomatic,
      /*auth_type_count=*/1, mojom::ApnIpType::kAutomatic,
      /*ip_type_count=*/1, ApnTypes::kDefaultAndAttach, /*apn_types_count=*/1);

  // CreateCustomApn with attach and make sure that it gets added because
  // there’s an APN with default added in |test_apn2|.
  TestApnData test_apn3;
  test_apn3.access_point_name = kCellularTestApn1;
  test_apn3.name = kCellularTestApnName1;
  test_apn3.username = kCellularTestApnUsername1;
  test_apn3.password = kCellularTestApnPassword1;
  test_apn3.attach = kCellularTestApnAttach1;
  test_apn3.mojo_apn_types = {mojom::ApnType::kAttach};
  test_apn3.onc_apn_types = {::onc::cellular_apn::kApnTypeAttach};
  EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn3.AsMojoApn()));

  EXPECT_EQ(2u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn3, &test_apn2});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertCreateCustomApnResultBucketCount(/*num_success=*/2, /*num_failure=*/1);
  AssertCreateCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType::kAutomatic,
      /*auth_type_count=*/2, mojom::ApnIpType::kAutomatic,
      /*ip_type_count=*/2, ApnTypes::kAttach, /*apn_types_count=*/1);
}

TEST_F(CrosNetworkConfigTest, CreateExclusivelyEnabledCustomApnList) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  // Register an observer to capture values sent to Shill
  TestNetworkConfigurationObserver network_config_observer(
      network_configuration_handler());

  const base::Value::List* custom_apns =
      network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_FALSE(custom_apns);

  // CreateExclusivelyEnabledCustomApn with attach only and verify that it
  // doesn't get added because its missing a default.
  TestApnData test_apn1;
  test_apn1.access_point_name = kCellularTestApn1;
  test_apn1.name = kCellularTestApnName1;
  test_apn1.username = kCellularTestApnUsername1;
  test_apn1.password = kCellularTestApnPassword1;
  test_apn1.attach = kCellularTestApnAttach1;
  test_apn1.mojo_apn_types = {mojom::ApnType::kAttach};
  test_apn1.onc_apn_types = {::onc::cellular_apn::kApnTypeAttach};
  EXPECT_FALSE(
      CreateExclusivelyEnabledCustomApn(kCellularGuid, test_apn1.AsMojoApn()));

  EXPECT_EQ(0u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> empty_apn_list({});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, empty_apn_list));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, empty_apn_list,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, empty_apn_list));
  }
  AssertCreateExclusivelyEnabledCustomApnResultBucketCount(/*num_success=*/0,
                                                           /*num_failure=*/0);
  AssertCreateExclusivelyEnabledCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType::kAutomatic,
      /*auth_type_count=*/0, mojom::ApnIpType::kAutomatic,
      /*ip_type_count=*/0, ApnTypes::kAttach, /*apn_types_count=*/0);

  // CreateExclusivelyEnabledCustomApn with attach and default and mock a
  // failure.
  ShillServiceClient::Get()
      ->GetTestInterface()
      ->SetErrorForNextSetPropertiesAttempt("Error.NotReady");
  TestApnData test_apn2;
  test_apn2.access_point_name = kCellularTestApn1;
  test_apn2.name = kCellularTestApnName1;
  test_apn2.username = kCellularTestApnUsername1;
  test_apn2.password = kCellularTestApnPassword1;
  test_apn2.attach = kCellularTestApnAttach1;
  test_apn2.mojo_apn_types = {mojom::ApnType::kDefault,
                              mojom::ApnType::kAttach};
  test_apn2.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault,
                             ::onc::cellular_apn::kApnTypeAttach};
  EXPECT_FALSE(
      CreateExclusivelyEnabledCustomApn(kCellularGuid, test_apn2.AsMojoApn()));
  EXPECT_EQ(0u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> empty_apn_list({});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, empty_apn_list));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, empty_apn_list,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, empty_apn_list));
  }
  AssertCreateExclusivelyEnabledCustomApnResultBucketCount(/*num_success=*/0,
                                                           /*num_failure=*/1);
  AssertCreateExclusivelyEnabledCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType::kAutomatic,
      /*auth_type_count=*/0, mojom::ApnIpType::kAutomatic,
      /*ip_type_count=*/0, ApnTypes::kDefaultAndAttach, /*apn_types_count=*/0);

  // Try again to create the APN without mocking a failure.
  EXPECT_TRUE(
      CreateExclusivelyEnabledCustomApn(kCellularGuid, test_apn2.AsMojoApn()));

  EXPECT_EQ(1u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn2});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertCreateExclusivelyEnabledCustomApnResultBucketCount(/*num_success=*/1,
                                                           /*num_failure=*/1);
  AssertCreateExclusivelyEnabledCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType::kAutomatic,
      /*auth_type_count=*/1, mojom::ApnIpType::kAutomatic,
      /*ip_type_count=*/1, ApnTypes::kDefaultAndAttach, /*apn_types_count=*/1);

  // CreateExclusivelyEnabledCustomApn with attach and make sure that it gets
  // added because there’s an APN with default added in |test_apn2|.
  TestApnData test_apn3;
  test_apn3.access_point_name = kCellularTestApn1;
  test_apn3.name = kCellularTestApnName1;
  test_apn3.username = kCellularTestApnUsername1;
  test_apn3.password = kCellularTestApnPassword1;
  test_apn3.attach = kCellularTestApnAttach1;
  test_apn3.mojo_apn_types = {mojom::ApnType::kAttach};
  test_apn3.onc_apn_types = {::onc::cellular_apn::kApnTypeAttach};
  EXPECT_TRUE(
      CreateExclusivelyEnabledCustomApn(kCellularGuid, test_apn3.AsMojoApn()));

  // All other APNs should be disabled.
  test_apn2.mojo_state = mojom::ApnState::kDisabled;
  test_apn2.onc_state = ::onc::cellular_apn::kStateDisabled;

  EXPECT_EQ(2u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn3, &test_apn2});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertCreateExclusivelyEnabledCustomApnResultBucketCount(/*num_success=*/2,
                                                           /*num_failure=*/1);
  AssertCreateExclusivelyEnabledCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType::kAutomatic,
      /*auth_type_count=*/2, mojom::ApnIpType::kAutomatic,
      /*ip_type_count=*/2, ApnTypes::kAttach, /*apn_types_count=*/1);
}

TEST_F(CrosNetworkConfigTest, RemoveCustomApnList) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(ash::features::kApnRevamp);

  // Register an observer to capture values sent to Shill
  TestNetworkConfigurationObserver network_config_observer(
      network_configuration_handler());

  const base::Value::List* custom_apns =
      network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_FALSE(custom_apns);

  // Create a custom default APN.
  TestApnData test_apn1;
  test_apn1.access_point_name = kCellularTestApn1;
  test_apn1.name = kCellularTestApnName1;
  test_apn1.username = kCellularTestApnUsername1;
  test_apn1.password = kCellularTestApnPassword1;
  test_apn1.attach = kCellularTestApnAttach1;
  test_apn1.mojo_apn_types = {mojom::ApnType::kDefault};
  test_apn1.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault};
  EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn1.AsMojoApn()));

  EXPECT_EQ(1u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertCreateCustomApnResultBucketCount(/*num_success=*/1, /*num_failure=*/0);
  AssertCreateCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType::kAutomatic,
      /*auth_type_count=*/1, mojom::ApnIpType::kAutomatic,
      /*ip_type_count=*/1, ApnTypes::kDefault, /*apn_types_count=*/1);

  custom_apns = network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_TRUE(custom_apns);
  ASSERT_EQ(1u, custom_apns->size());
  const std::string first_apn_id = std::string(
      *custom_apns->front().GetDict().FindString(::onc::cellular_apn::kId));

  // Create a new custom attach APN.
  TestApnData test_apn2;
  test_apn2.access_point_name = kCellularTestApn2;
  test_apn2.name = kCellularTestApnName2;
  test_apn2.username = kCellularTestApnUsername2;
  test_apn2.password = kCellularTestApnPassword2;
  test_apn2.attach = "attach";
  test_apn2.mojo_apn_types = {mojom::ApnType::kAttach};
  test_apn2.onc_apn_types = {::onc::cellular_apn::kApnTypeAttach};
  EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn2.AsMojoApn()));

  EXPECT_EQ(2u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn2, &test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertCreateCustomApnResultBucketCount(/*num_success=*/2, /*num_failure=*/0);
  AssertCreateCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType::kAutomatic,
      /*auth_type_count=*/2, mojom::ApnIpType::kAutomatic,
      /*ip_type_count=*/2, ApnTypes::kAttach, /*apn_types_count=*/1);
  custom_apns = network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_TRUE(custom_apns);
  ASSERT_EQ(2u, custom_apns->size());
  const std::string second_apn_id = std::string(
      *custom_apns->front().GetDict().FindString(::onc::cellular_apn::kId));

  // Try to remove the default APN |test_apn1| which will not work because there
  // would only be an attach APN left.
  RemoveCustomApn(kCellularGuid, first_apn_id);
  EXPECT_EQ(2u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn2, &test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertRemoveCustomApnResultBucketCount(/*num_success=*/0, /*num_failure=*/0);
  AssertRemoveCustomApnPropertiesBucketCount(ApnTypes::kDefault,
                                             /*apn_types_count=*/0);
  custom_apns = network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_EQ(2u, custom_apns->size());

  // Create another new custom default APN.
  TestApnData test_apn3;
  test_apn3.access_point_name = kCellularTestApn3;
  test_apn3.name = kCellularTestApnName3;
  test_apn3.username = kCellularTestApnUsername3;
  test_apn3.password = kCellularTestApnPassword3;
  test_apn3.attach = "";
  test_apn3.mojo_apn_types = {mojom::ApnType::kDefault};
  test_apn3.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault};
  EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn3.AsMojoApn()));

  EXPECT_EQ(3u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns(
        {&test_apn3, &test_apn2, &test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertCreateCustomApnResultBucketCount(/*num_success=*/3, /*num_failure=*/0);
  AssertCreateCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType::kAutomatic,
      /*auth_type_count=*/3, mojom::ApnIpType::kAutomatic,
      /*ip_type_count=*/3, ApnTypes::kDefault, /*apn_types_count=*/2);
  custom_apns = network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_TRUE(custom_apns);
  ASSERT_EQ(3u, custom_apns->size());
  const std::string third_apn_id = std::string(
      *custom_apns->front().GetDict().FindString(::onc::cellular_apn::kId));

  // Try to remove the default APN |test_apn3| which is OK because there is
  // another APN that is default but mock a failure.
  ShillServiceClient::Get()
      ->GetTestInterface()
      ->SetErrorForNextSetPropertiesAttempt("Error.NotReady");
  RemoveCustomApn(kCellularGuid, third_apn_id);
  EXPECT_EQ(3u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns(
        {&test_apn3, &test_apn2, &test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertRemoveCustomApnResultBucketCount(/*num_success=*/0, /*num_failure=*/1);
  AssertRemoveCustomApnPropertiesBucketCount(ApnTypes::kDefault,
                                             /*apn_types_count=*/0);
  custom_apns = network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_TRUE(custom_apns);
  ASSERT_EQ(3u, custom_apns->size());

  // Try again to remove the APN which is OK because there is another APN that
  // is default and we did not mock a failure.
  RemoveCustomApn(kCellularGuid, third_apn_id);
  EXPECT_EQ(4u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn2, &test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertRemoveCustomApnResultBucketCount(/*num_success=*/1, /*num_failure=*/1);
  AssertRemoveCustomApnPropertiesBucketCount(ApnTypes::kDefault,
                                             /*apn_types_count=*/1);
  custom_apns = network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_TRUE(custom_apns);
  ASSERT_EQ(2u, custom_apns->size());

  // Remove the custom attach APN which is OK because there is a default APN.
  RemoveCustomApn(kCellularGuid, second_apn_id);
  EXPECT_EQ(5u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertRemoveCustomApnResultBucketCount(/*num_success=*/2, /*num_failure=*/1);
  AssertRemoveCustomApnPropertiesBucketCount(ApnTypes::kAttach,
                                             /*apn_types_count=*/1);
  custom_apns = network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_EQ(1u, custom_apns->size());

  // Remove the custom default APN which is OK because there is no other APN.
  RemoveCustomApn(kCellularGuid, first_apn_id);
  EXPECT_EQ(6u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> empty_apn_list({});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, empty_apn_list));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, empty_apn_list,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, empty_apn_list));
  }
  AssertRemoveCustomApnResultBucketCount(/*num_success=*/3, /*num_failure=*/1);
  AssertRemoveCustomApnPropertiesBucketCount(ApnTypes::kDefault,
                                             /*apn_types_count=*/2);
  custom_apns = network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_EQ(0u, custom_apns->size());
}

TEST_F(CrosNetworkConfigTest, CreateCustomApn_NoListSaved) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  // Register an observer to capture values sent to Shill
  TestNetworkConfigurationObserver network_config_observer(
      network_configuration_handler());

  const base::Value::List* custom_apns =
      network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_FALSE(custom_apns);

  TestApnData test_apn1;
  test_apn1.access_point_name = kCellularTestApn1;
  test_apn1.name = kCellularTestApnName1;
  test_apn1.username = kCellularTestApnUsername1;
  test_apn1.password = kCellularTestApnPassword1;
  test_apn1.attach = kCellularTestApnAttach1;
  test_apn1.mojo_apn_types = {mojom::ApnType::kDefault};
  test_apn1.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault};
  EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn1.AsMojoApn()));

  // Verify that the API called sent the right values to Shill
  EXPECT_EQ(1u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertCreateCustomApnResultBucketCount(/*num_success=*/1, /*num_failure=*/0);
  AssertCreateCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType::kAutomatic,
      /*auth_type_count=*/1, mojom::ApnIpType::kAutomatic,
      /*ip_type_count=*/1, ApnTypes::kDefault, /*apn_types_count=*/1);
}

TEST_F(CrosNetworkConfigTest, ModifyCustomApnList) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(ash::features::kApnRevamp);

  // Register an observer to capture values sent to Shill
  TestNetworkConfigurationObserver network_config_observer(
      network_configuration_handler());

  const base::Value::List* custom_apns =
      network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_FALSE(custom_apns);

  // Create a custom default APN.
  TestApnData test_apn1;
  test_apn1.access_point_name = kCellularTestApn1;
  test_apn1.name = kCellularTestApnName1;
  test_apn1.username = kCellularTestApnUsername1;
  test_apn1.password = kCellularTestApnPassword1;
  test_apn1.attach = kCellularTestApnAttach1;
  test_apn1.mojo_apn_types = {mojom::ApnType::kDefault};
  test_apn1.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault};
  EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn1.AsMojoApn()));
  EXPECT_EQ(1u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }

  // Try to modify the APN type to be attach which will not work because there
  // would be no default APN left.
  custom_apns = network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_TRUE(custom_apns);
  ASSERT_EQ(1u, custom_apns->size());
  const std::string first_apn_id =
      *custom_apns->front().GetDict().FindString(::onc::cellular_apn::kId);

  TestApnData test_apn2;
  test_apn2.access_point_name = kCellularTestApn2;
  test_apn2.name = kCellularTestApnName2;
  test_apn2.username = kCellularTestApnUsername2;
  test_apn2.password = kCellularTestApnPassword2;
  test_apn2.attach = "attach";
  test_apn2.mojo_apn_types = {mojom::ApnType::kAttach};
  test_apn2.onc_apn_types = {::onc::cellular_apn::kApnTypeAttach};
  test_apn2.id = first_apn_id;

  ModifyCustomApn(kCellularGuid, test_apn2.AsMojoApn());
  EXPECT_EQ(1u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }

  // Create a custom attach APN.
  TestApnData test_apn3;
  test_apn3.access_point_name = kCellularTestApn3;
  test_apn3.name = kCellularTestApnName3;
  test_apn3.username = kCellularTestApnUsername3;
  test_apn3.password = kCellularTestApnPassword3;
  test_apn3.attach = kCellularTestApnAttach1;
  test_apn3.mojo_apn_types = {mojom::ApnType::kAttach};
  test_apn3.onc_apn_types = {::onc::cellular_apn::kApnTypeAttach};
  EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn3.AsMojoApn()));
  EXPECT_EQ(2u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn3, &test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertCreateCustomApnResultBucketCount(/*num_success=*/2, /*num_failure=*/0);
  AssertCreateCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType::kAutomatic,
      /*auth_type_count=*/2, mojom::ApnIpType::kAutomatic,
      /*ip_type_count=*/2, ApnTypes::kAttach, /*apn_types_count=*/1);

  // Try to modify default APN to be attach which will not work because there
  // would be no default APN left.
  ModifyCustomApn(kCellularGuid, test_apn2.AsMojoApn());
  EXPECT_EQ(2u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn3, &test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }

  // Try to modify attach APN type to default which is OK but mock a failure.
  custom_apns = network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_TRUE(custom_apns);
  ASSERT_EQ(2u, custom_apns->size());
  const std::string second_apn_id =
      *custom_apns->front().GetDict().FindString(::onc::cellular_apn::kId);

  ShillServiceClient::Get()
      ->GetTestInterface()
      ->SetErrorForNextSetPropertiesAttempt("Error.NotReady");
  TestApnData test_apn4;
  test_apn4.access_point_name = "TEST.APN4";
  test_apn4.name = "Test Apn 4";
  test_apn4.username = kCellularTestApnUsername1;
  test_apn4.password = kCellularTestApnPassword1;
  test_apn4.attach = "";
  test_apn4.mojo_apn_types = {mojom::ApnType::kDefault};
  test_apn4.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault};
  test_apn4.id = second_apn_id;

  ApnHistogramCounts counts;
  AssertApnHistogramCounts(counts);
  ModifyCustomApn(kCellularGuid, test_apn4.AsMojoApn());
  EXPECT_EQ(2u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn3, &test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  counts.num_modify_failure++;
  AssertApnHistogramCounts(counts);

  // Try again to modify attach APN type to default which is OK without mocking
  // a failure.
  ModifyCustomApn(kCellularGuid, test_apn4.AsMojoApn());
  EXPECT_EQ(3u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn4, &test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  counts.num_modify_success++;
  counts.num_modify_type_attach++;
  AssertApnHistogramCounts(counts);

  // Modify first default APN to be attach which is OK.
  TestApnData test_apn5;
  test_apn5.access_point_name = "TEST.APN5";
  test_apn5.name = "Test Apn 5";
  test_apn5.username = kCellularTestApnUsername1;
  test_apn5.password = kCellularTestApnPassword1;
  test_apn5.attach = "attach";
  test_apn5.mojo_apn_types = {mojom::ApnType::kAttach};
  test_apn5.onc_apn_types = {::onc::cellular_apn::kApnTypeAttach};
  test_apn5.id = first_apn_id;

  ModifyCustomApn(kCellularGuid, test_apn5.AsMojoApn());
  EXPECT_EQ(4u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn4, &test_apn5});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
}

TEST_F(CrosNetworkConfigTest, CreateCustomApn_EmptyList) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  // Register an observer to capture values sent to Shill
  TestNetworkConfigurationObserver network_config_observer(
      network_configuration_handler());

  network_metadata_store()->SetCustomApnList(kCellularGuid,
                                             base::Value::List());

  EXPECT_TRUE(CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, {}));
  EXPECT_EQ(0u, network_config_observer.GetOnConfigurationModifiedCallCount());

  // Call the API to create a new user APN
  TestApnData test_apn1;
  test_apn1.access_point_name = kCellularTestApn1;
  test_apn1.name = kCellularTestApnName1;
  test_apn1.username = kCellularTestApnUsername1;
  test_apn1.password = kCellularTestApnPassword1;
  test_apn1.attach = kCellularTestApnAttach1;
  test_apn1.mojo_ip_type = mojom::ApnIpType::kIpv4;
  test_apn1.onc_ip_type = ::onc::cellular_apn::kIpTypeIpv4;
  test_apn1.mojo_source = mojom::ApnSource::kUi;
  test_apn1.onc_source = ::onc::cellular_apn::kSourceUi;
  test_apn1.mojo_authentication = mojom::ApnAuthenticationType::kPap;
  test_apn1.onc_authentication = ::onc::cellular_apn::kAuthenticationPap;
  test_apn1.mojo_apn_types = {mojom::ApnType::kDefault,
                              mojom::ApnType::kAttach};
  test_apn1.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault,
                             ::onc::cellular_apn::kApnTypeAttach};
  EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn1.AsMojoApn()));

  // Verify that the API called sent the right values to Shill
  EXPECT_EQ(1u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertCreateCustomApnResultBucketCount(/*num_success=*/1, /*num_failure=*/0);
  AssertCreateCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType::kPap,
      /*auth_type_count=*/1, mojom::ApnIpType::kIpv4,
      /*ip_type_count=*/1, ApnTypes::kDefaultAndAttach, /*apn_types_count=*/1);

  // Call the API to create a second user APN
  TestApnData test_apn2;
  test_apn2.access_point_name = kCellularTestApn2;
  test_apn2.name = kCellularTestApnName2;
  test_apn2.username = kCellularTestApnUsername2;
  test_apn2.password = kCellularTestApnPassword2;
  test_apn2.attach = kCellularTestApnAttach2;
  test_apn2.mojo_ip_type = mojom::ApnIpType::kIpv4Ipv6;
  test_apn2.onc_ip_type = ::onc::cellular_apn::kIpTypeIpv4Ipv6;
  test_apn2.onc_source = ::onc::cellular_apn::kSourceUi;
  test_apn2.mojo_source = mojom::ApnSource::kUi;
  test_apn2.mojo_authentication = mojom::ApnAuthenticationType::kChap;
  test_apn2.onc_authentication = ::onc::cellular_apn::kAuthenticationChap;
  test_apn2.mojo_apn_types = {mojom::ApnType::kDefault,
                              mojom::ApnType::kAttach};
  test_apn2.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault,
                             ::onc::cellular_apn::kApnTypeAttach};
  EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn2.AsMojoApn()));

  // Verify that the API called sent the right values to Shill
  EXPECT_EQ(2u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn2, &test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertCreateCustomApnResultBucketCount(/*num_success=*/2, /*num_failure=*/0);
  AssertCreateCustomApnPropertiesBucketCount(
      mojom::ApnAuthenticationType::kChap,
      /*auth_type_count=*/1, mojom::ApnIpType::kIpv4Ipv6,
      /*ip_type_count=*/1, ApnTypes::kDefaultAndAttach, /*apn_types_count=*/2);
}

TEST_F(CrosNetworkConfigTest, CreateCustomApn_InvalidGuid) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  // Register an observer to capture values sent to Shill
  TestNetworkConfigurationObserver network_config_observer(
      network_configuration_handler());

  const std::string guid = "invalid";
  const base::Value::List* custom_apns =
      network_metadata_store()->GetCustomApnList(guid);
  ASSERT_FALSE(custom_apns);

  TestApnData test_apn1;
  test_apn1.access_point_name = kCellularTestApn1;
  test_apn1.name = kCellularTestApnName1;
  test_apn1.username = kCellularTestApnUsername1;
  test_apn1.password = kCellularTestApnPassword1;
  test_apn1.attach = kCellularTestApnAttach1;
  test_apn1.mojo_apn_types = {mojom::ApnType::kDefault,
                              mojom::ApnType::kAttach};
  test_apn1.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault,
                             ::onc::cellular_apn::kApnTypeAttach};
  EXPECT_FALSE(CreateCustomApn(guid, test_apn1.AsMojoApn()));

  // Verify that no values were sent to Shill
  EXPECT_EQ(0u, network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns;
    EXPECT_TRUE(CustomApnsInNetworkMetadataStoreMatch(guid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(guid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(CustomApnsInManagedPropertiesMatch(guid, expected_apns));
  }
  AssertCreateCustomApnResultBucketCount(/*num_success=*/0, /*num_failure=*/1);
}

TEST_F(CrosNetworkConfigTest, RemoveCustomApn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  // Register an observer to capture values sent to Shill
  TestNetworkConfigurationObserver network_config_observer(
      network_configuration_handler());

  ASSERT_FALSE(network_metadata_store()->GetCustomApnList(kCellularGuid));
  // Verify RemoveCustomApn reports an error and return when the
  // network_metadata_store has a nullptr custom APN list
  size_t expected_network_config_calls = 0u;
  std::string id_to_delete("apn_id_1");
  RemoveCustomApn(kCellularGuid, id_to_delete);
  EXPECT_EQ(expected_network_config_calls,
            network_config_observer.GetOnConfigurationModifiedCallCount());
  ASSERT_FALSE(network_metadata_store()->GetCustomApnList(kCellularGuid));
  AssertRemoveCustomApnResultBucketCount(/*num_success=*/0, /*num_failure=*/1);

  TestApnData test_apn1;
  test_apn1.access_point_name = kCellularTestApn1;
  test_apn1.name = kCellularTestApnName1;
  test_apn1.username = kCellularTestApnUsername1;
  test_apn1.password = kCellularTestApnPassword1;
  test_apn1.attach = kCellularTestApnAttach1;
  test_apn1.mojo_apn_types = {mojom::ApnType::kDefault};
  test_apn1.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault};

  TestApnData test_apn2;
  test_apn2.access_point_name = kCellularTestApn2;
  test_apn2.name = kCellularTestApnName2;
  test_apn2.username = kCellularTestApnUsername2;
  test_apn2.password = kCellularTestApnPassword2;
  test_apn2.attach = kCellularTestApnAttach2;
  test_apn2.mojo_apn_types = {mojom::ApnType::kDefault,
                              mojom::ApnType::kAttach};
  test_apn2.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault,
                             ::onc::cellular_apn::kApnTypeAttach};

  // Add two custom APNs using the official API
  {
    EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn1.AsMojoApn()));
    EXPECT_EQ(++expected_network_config_calls,
              network_config_observer.GetOnConfigurationModifiedCallCount());
    EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn2.AsMojoApn()));
    EXPECT_EQ(++expected_network_config_calls,
              network_config_observer.GetOnConfigurationModifiedCallCount());
  }

  // Verify that RemoveCustomApn deletes the second custom APN
  const base::Value::List* custom_apns =
      network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_TRUE(custom_apns);
  ASSERT_EQ(2u, custom_apns->size());
  const std::string* second_apn_id =
      custom_apns->front().GetDict().FindString(::onc::cellular_apn::kId);
  ASSERT_TRUE(second_apn_id);

  id_to_delete = std::string(*second_apn_id);
  RemoveCustomApn(kCellularGuid, id_to_delete);
  EXPECT_EQ(++expected_network_config_calls,
            network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertRemoveCustomApnResultBucketCount(/*num_success=*/1, /*num_failure=*/1);
  AssertRemoveCustomApnPropertiesBucketCount(ApnTypes::kDefaultAndAttach,
                                             /*apn_types_count=*/1);

  // Try to remove an ID not found in the list, API should do nothing
  RemoveCustomApn(kCellularGuid, id_to_delete);
  EXPECT_EQ(expected_network_config_calls,
            network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertRemoveCustomApnResultBucketCount(/*num_success=*/1, /*num_failure=*/2);

  // Remove the first test APN
  custom_apns = network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_TRUE(custom_apns);
  ASSERT_EQ(1u, custom_apns->size());
  const std::string* first_apn_id =
      custom_apns->front().GetDict().FindString(::onc::cellular_apn::kId);
  ASSERT_TRUE(first_apn_id);
  id_to_delete = std::string(*first_apn_id);
  RemoveCustomApn(kCellularGuid, id_to_delete);
  EXPECT_EQ(++expected_network_config_calls,
            network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns;
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertRemoveCustomApnResultBucketCount(/*num_success=*/2, /*num_failure=*/2);
  AssertRemoveCustomApnPropertiesBucketCount(ApnTypes::kDefault,
                                             /*apn_types_count=*/1);

  // Try to delete an APN when the custom APN list is empty, it should do
  // nothing
  RemoveCustomApn(kCellularGuid, id_to_delete);
  EXPECT_EQ(expected_network_config_calls,
            network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns;
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  AssertRemoveCustomApnResultBucketCount(/*num_success=*/2, /*num_failure=*/3);
}

TEST_F(CrosNetworkConfigTest, CreateCustomApn_MaxAmountAllowed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  // Register an observer to capture values sent to Shill
  TestNetworkConfigurationObserver network_config_observer(
      network_configuration_handler());

  const base::Value::List* custom_apns =
      network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_FALSE(custom_apns);

  TestApnData test_apn1;
  test_apn1.access_point_name = kCellularTestApn1;
  test_apn1.name = kCellularTestApnName1;
  test_apn1.username = kCellularTestApnUsername1;
  test_apn1.password = kCellularTestApnPassword1;
  test_apn1.attach = kCellularTestApnAttach1;
  test_apn1.mojo_apn_types = {mojom::ApnType::kDefault, mojom::ApnType::kAttach,
                              mojom::ApnType::kTether};
  test_apn1.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault,
                             ::onc::cellular_apn::kApnTypeAttach,
                             ::onc::cellular_apn::kApnTypeTether};

  // Verify that the API creates as many APNs as allowed
  for (size_t i = 0; i < mojom::kMaxNumCustomApns; ++i) {
    EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn1.AsMojoApn()));
    EXPECT_EQ(i + 1,
              network_config_observer.GetOnConfigurationModifiedCallCount());
  }
  // Verify that the next call does nothing
  EXPECT_FALSE(CreateCustomApn(kCellularGuid, test_apn1.AsMojoApn()));
  EXPECT_EQ(mojom::kMaxNumCustomApns,
            network_config_observer.GetOnConfigurationModifiedCallCount());

  custom_apns = network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_TRUE(custom_apns);
  EXPECT_EQ(mojom::kMaxNumCustomApns, custom_apns->size());
}

TEST_F(CrosNetworkConfigTest, ModifyCustomApn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  // Register an observer to capture values sent to Shill
  TestNetworkConfigurationObserver network_config_observer(
      network_configuration_handler());

  ApnHistogramCounts counts;
  ASSERT_FALSE(network_metadata_store()->GetCustomApnList(kCellularGuid));
  // Verify ModifyCustomApn reports an error and return when the
  // network_metadata_store has a nullptr custom APN list
  size_t expected_network_config_calls = 0u;
  TestApnData test_apn1;
  test_apn1.access_point_name = kCellularTestApn1;
  test_apn1.name = kCellularTestApnName1;
  test_apn1.username = kCellularTestApnUsername1;
  test_apn1.password = kCellularTestApnPassword1;
  test_apn1.attach = kCellularTestApnAttach1;
  test_apn1.mojo_apn_types = {mojom::ApnType::kDefault,
                              mojom::ApnType::kAttach};
  test_apn1.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault,
                             ::onc::cellular_apn::kApnTypeAttach};
  test_apn1.mojo_state = mojom::ApnState::kEnabled;
  test_apn1.onc_state = ::onc::cellular_apn::kStateEnabled;

  test_apn1.id = "apn_id_1";
  AssertApnHistogramCounts(counts);
  ModifyCustomApn(kCellularGuid, test_apn1.AsMojoApn());
  EXPECT_EQ(expected_network_config_calls,
            network_config_observer.GetOnConfigurationModifiedCallCount());
  ASSERT_FALSE(network_metadata_store()->GetCustomApnList(kCellularGuid));
  counts.num_modify_failure++;
  AssertApnHistogramCounts(counts);
  // Try to replace an APN when the custom APN list is empty, it should do
  // nothing
  network_metadata_store()->SetCustomApnList(kCellularGuid,
                                             base::Value::List());
  ModifyCustomApn(kCellularGuid, test_apn1.AsMojoApn());
  EXPECT_EQ(expected_network_config_calls,
            network_config_observer.GetOnConfigurationModifiedCallCount());
  EXPECT_TRUE(CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, {}));
  counts.num_modify_failure++;
  AssertApnHistogramCounts(counts);

  TestApnData test_apn2;
  test_apn2.access_point_name = kCellularTestApn2;
  test_apn2.name = kCellularTestApnName2;
  test_apn2.username = kCellularTestApnUsername2;
  test_apn2.password = kCellularTestApnPassword2;
  test_apn2.attach = kCellularTestApnAttach2;
  test_apn2.mojo_apn_types = {mojom::ApnType::kDefault,
                              mojom::ApnType::kAttach};
  test_apn2.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault,
                             ::onc::cellular_apn::kApnTypeAttach};
  test_apn2.mojo_state = mojom::ApnState::kEnabled;
  test_apn2.onc_state = ::onc::cellular_apn::kStateEnabled;

  // Add two custom APNs using the official API
  {
    EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn1.AsMojoApn()));
    EXPECT_EQ(++expected_network_config_calls,
              network_config_observer.GetOnConfigurationModifiedCallCount());
    EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn2.AsMojoApn()));
    EXPECT_EQ(++expected_network_config_calls,
              network_config_observer.GetOnConfigurationModifiedCallCount());
  }

  // Verify that ModifyCustomApn replaces the first custom APN
  const base::Value::List* custom_apns =
      network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_TRUE(custom_apns);
  ASSERT_EQ(2u, custom_apns->size());
  const std::string first_apn_id =
      *custom_apns->front().GetDict().FindString(::onc::cellular_apn::kId);

  TestApnData test_apn3;
  test_apn3.access_point_name = kCellularTestApn3;
  test_apn3.name = kCellularTestApnName3;
  test_apn3.username = kCellularTestApnUsername3;
  test_apn3.password = kCellularTestApnPassword3;
  test_apn3.attach = kCellularTestApnAttach3;
  test_apn3.mojo_apn_types = {mojom::ApnType::kAttach};
  test_apn3.onc_apn_types = {::onc::cellular_apn::kApnTypeAttach};
  test_apn3.mojo_state = mojom::ApnState::kEnabled;
  test_apn3.onc_state = ::onc::cellular_apn::kStateEnabled;

  // Verify that ModifyCustomApn does nothing if the input APN does not have an
  // ID.
  ModifyCustomApn(kCellularGuid, test_apn3.AsMojoApn());
  EXPECT_EQ(expected_network_config_calls,
            network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn2, &test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  counts.num_modify_failure++;
  AssertApnHistogramCounts(counts);
  // Verify that ModifyCustomApn replaces the first custom APN
  test_apn3.id = first_apn_id;
  ModifyCustomApn(kCellularGuid, test_apn3.AsMojoApn());
  EXPECT_EQ(++expected_network_config_calls,
            network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn3, &test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));

    counts.num_modify_success++;
    counts.num_modify_type_default_and_attach++;
    AssertApnHistogramCounts(counts);
  }

  // Try to update an ID not found in the list, API should do nothing
  test_apn3.id = "invalid_apn_id";
  ModifyCustomApn(kCellularGuid, test_apn3.AsMojoApn());
  EXPECT_EQ(expected_network_config_calls,
            network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn3, &test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  counts.num_modify_failure++;
  AssertApnHistogramCounts(counts);

  // Verify that disabling a custom APN changes its ApnState to disabled and
  // logs metrics
  test_apn3.id = first_apn_id;
  test_apn3.mojo_state = mojom::ApnState::kDisabled;
  test_apn3.onc_state = ::onc::cellular_apn::kStateDisabled;
  ModifyCustomApn(kCellularGuid, test_apn3.AsMojoApn());
  EXPECT_EQ(++expected_network_config_calls,
            network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn3, &test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  counts.num_modify_success++;
  counts.num_modify_type_attach++;
  counts.num_disable_success++;
  counts.num_disable_type_attach++;
  AssertApnHistogramCounts(counts);

  // Verify that enabling a custom APN changes its ApnState to enabled and
  // logs metrics
  test_apn3.id = first_apn_id;
  test_apn3.mojo_state = mojom::ApnState::kEnabled;
  test_apn3.onc_state = ::onc::cellular_apn::kStateEnabled;
  test_apn3.mojo_apn_types = {mojom::ApnType::kDefault};
  test_apn3.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault};
  ModifyCustomApn(kCellularGuid, test_apn3.AsMojoApn());
  EXPECT_EQ(++expected_network_config_calls,
            network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn3, &test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  counts.num_modify_success++;
  counts.num_modify_type_attach++;
  counts.num_enable_success++;
  counts.num_enable_type_attach++;
  AssertApnHistogramCounts(counts);

  // Verify that changing the APN type logs an event when changing from default
  // APN type to a different type
  test_apn3.id = first_apn_id;
  test_apn3.mojo_apn_types = {mojom::ApnType::kAttach};
  test_apn3.onc_apn_types = {::onc::cellular_apn::kApnTypeAttach};
  test_apn3.mojo_state = mojom::ApnState::kDisabled;
  test_apn3.onc_state = ::onc::cellular_apn::kStateDisabled;
  ModifyCustomApn(kCellularGuid, test_apn3.AsMojoApn());
  EXPECT_EQ(++expected_network_config_calls,
            network_config_observer.GetOnConfigurationModifiedCallCount());
  {
    std::vector<TestApnData*> expected_apns({&test_apn3, &test_apn1});
    EXPECT_TRUE(
        CustomApnsInNetworkMetadataStoreMatch(kCellularGuid, expected_apns));
    EXPECT_TRUE(CustomApnsInCellularConfigMatch(kCellularGuid, expected_apns,
                                                network_config_observer));
    EXPECT_TRUE(
        CustomApnsInManagedPropertiesMatch(kCellularGuid, expected_apns));
  }
  counts.num_modify_success++;
  counts.num_modify_type_default++;
  counts.num_disable_success++;
  counts.num_disable_type_default++;
  AssertApnHistogramCounts(counts);
}

TEST_F(CrosNetworkConfigTest,
       ApnOperationsDisallowApnModificationFlagDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list
      .InitWithFeatures(/*enabled_features=*/
                        {features::kApnRevamp},
                        /*disabled_features=*/{
                            features::kAllowApnModificationPolicy});

  // Register an observer to capture values sent to Shill.
  TestNetworkConfigurationObserver network_config_observer(
      network_configuration_handler());

  const base::Value::List* custom_apns =
      network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_FALSE(custom_apns);

  // Set AllowAPNModification to false.
  SetAllowApnModification(false);

  // Create APN with kAllowApnModificationPolicy flag disabled should succeed.
  TestApnData test_apn1;
  test_apn1.access_point_name = kCellularTestApn1;
  test_apn1.name = kCellularTestApnName1;
  test_apn1.username = kCellularTestApnUsername1;
  test_apn1.password = kCellularTestApnPassword1;
  test_apn1.mojo_apn_types = {mojom::ApnType::kDefault};
  test_apn1.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault};
  EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn1.AsMojoApn()));
  EXPECT_EQ(1u, network_config_observer.GetOnConfigurationModifiedCallCount());

  custom_apns = network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_TRUE(custom_apns);
  ASSERT_EQ(1u, custom_apns->size());
  const std::string apn_id =
      *custom_apns->front().GetDict().FindString(::onc::cellular_apn::kId);

  // Modifying the APN should succeed.
  test_apn1.id = apn_id;
  ModifyCustomApn(kCellularGuid, test_apn1.AsMojoApn());
  EXPECT_EQ(2u, network_config_observer.GetOnConfigurationModifiedCallCount());

  // Removing the APN should succeed.
  RemoveCustomApn(kCellularGuid, apn_id);
  EXPECT_EQ(3u, network_config_observer.GetOnConfigurationModifiedCallCount());
}

TEST_F(CrosNetworkConfigTest, ApnOperationsDisallowApnModification) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(/*enabled_features=*/
                                       {features::kApnRevamp,
                                        features::kAllowApnModificationPolicy},
                                       /*disabled_features=*/{});

  // Register an observer to capture values sent to Shill.
  TestNetworkConfigurationObserver network_config_observer(
      network_configuration_handler());

  const base::Value::List* custom_apns =
      network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_FALSE(custom_apns);

  // APN operations with AllowAPNModification unset should succeed.
  TestApnData test_apn1;
  test_apn1.access_point_name = kCellularTestApn1;
  test_apn1.name = kCellularTestApnName1;
  test_apn1.username = kCellularTestApnUsername1;
  test_apn1.password = kCellularTestApnPassword1;
  test_apn1.mojo_apn_types = {mojom::ApnType::kDefault};
  test_apn1.onc_apn_types = {::onc::cellular_apn::kApnTypeDefault};
  EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn1.AsMojoApn()));
  EXPECT_EQ(1u, network_config_observer.GetOnConfigurationModifiedCallCount());

  custom_apns = network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_TRUE(custom_apns);
  ASSERT_EQ(1u, custom_apns->size());
  std::string apn_id =
      *custom_apns->front().GetDict().FindString(::onc::cellular_apn::kId);

  // Modifying the APN should succeed.
  test_apn1.id = apn_id;
  ModifyCustomApn(kCellularGuid, test_apn1.AsMojoApn());
  EXPECT_EQ(2u, network_config_observer.GetOnConfigurationModifiedCallCount());

  // Removing the APN should succeed.
  RemoveCustomApn(kCellularGuid, apn_id);
  EXPECT_EQ(3u, network_config_observer.GetOnConfigurationModifiedCallCount());

  // Set AllowAPNModification to true. Operations should succeed.
  SetAllowApnModification(true);
  EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn1.AsMojoApn()));
  EXPECT_EQ(5u, network_config_observer.GetOnConfigurationModifiedCallCount());

  custom_apns = network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_TRUE(custom_apns);
  ASSERT_EQ(1u, custom_apns->size());
  apn_id = *custom_apns->front().GetDict().FindString(::onc::cellular_apn::kId);

  // Modifying the APN should succeed.
  test_apn1.id = apn_id;
  ModifyCustomApn(kCellularGuid, test_apn1.AsMojoApn());
  EXPECT_EQ(6u, network_config_observer.GetOnConfigurationModifiedCallCount());

  // Removing the APN should succeed.
  RemoveCustomApn(kCellularGuid, apn_id);
  EXPECT_EQ(7u, network_config_observer.GetOnConfigurationModifiedCallCount());

  // Add another custom APN.
  EXPECT_TRUE(CreateCustomApn(kCellularGuid, test_apn1.AsMojoApn()));
  EXPECT_EQ(8u, network_config_observer.GetOnConfigurationModifiedCallCount());

  custom_apns = network_metadata_store()->GetCustomApnList(kCellularGuid);
  ASSERT_TRUE(custom_apns);
  ASSERT_EQ(1u, custom_apns->size());
  apn_id = *custom_apns->front().GetDict().FindString(::onc::cellular_apn::kId);

  // Set AllowAPNModification to false. Operations should not succeed and custom
  // apn list should be set to empty.
  SetAllowApnModification(false);
  EXPECT_FALSE(CreateCustomApn(kCellularGuid, test_apn1.AsMojoApn()));
  EXPECT_EQ(9u, network_config_observer.GetOnConfigurationModifiedCallCount());
  histogram_tester_.ExpectBucketCount(
      "Network.Ash.Cellular.Apn.CreateCustomApn.AllowApnModification", false,
      1);

  // Modifying the APN shouldn't succeed.
  test_apn1.id = apn_id;
  ModifyCustomApn(kCellularGuid, test_apn1.AsMojoApn());
  EXPECT_EQ(9u, network_config_observer.GetOnConfigurationModifiedCallCount());
  histogram_tester_.ExpectBucketCount(
      "Network.Ash.Cellular.Apn.ModifyCustomApn.AllowApnModification", false,
      1);

  // Removing the APN shouldn't succeed.
  RemoveCustomApn(kCellularGuid, apn_id);
  EXPECT_EQ(9u, network_config_observer.GetOnConfigurationModifiedCallCount());
  histogram_tester_.ExpectBucketCount(
      "Network.Ash.Cellular.Apn.RemoveCustomApn.AllowApnModification", false,
      1);
}

TEST_F(CrosNetworkConfigTest, ConnectedAPN_ApnRevampEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  // Configure a cellular network with a last good APN and disconnected
  // as connection status
  helper()->ConfigureService(base::StringPrintf(
      kTestApnCellularShillDictFmt, kTestApnCellularGuid, shill::kStateIdle,
      kCellularTestIccid, NetworkProfileHandler::GetSharedProfilePath().c_str(),
      CreateApnShillDict().c_str()));

  // Verify the connection state
  mojom::ManagedPropertiesPtr properties =
      GetManagedProperties(kTestApnCellularGuid);
  ASSERT_TRUE(properties);
  EXPECT_EQ(kTestApnCellularGuid, properties->guid);
  EXPECT_EQ(mojom::NetworkType::kCellular, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
            properties->connection_state);
  ASSERT_TRUE(properties->type_properties->is_cellular());
  mojom::ManagedCellularProperties* cellular_props =
      properties->type_properties->get_cellular().get();
  ASSERT_TRUE(cellular_props);

  // Check that last_good_apn was set, but not the connected_apn
  EXPECT_TRUE(cellular_props->last_good_apn);
  EXPECT_FALSE(cellular_props->connected_apn);

  // Simulate an update where Shill was able to connect to the cellular network
  helper()->ConfigureService(base::StringPrintf(
      kTestApnCellularShillDictFmt, kTestApnCellularGuid, shill::kStateReady,
      kCellularTestIccid, NetworkProfileHandler::GetSharedProfilePath().c_str(),
      CreateApnShillDict().c_str()));

  // Verify the new connection state
  properties = GetManagedProperties(kTestApnCellularGuid);
  ASSERT_TRUE(properties);
  EXPECT_EQ(kTestApnCellularGuid, properties->guid);
  EXPECT_EQ(mojom::NetworkType::kCellular, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kConnected,
            properties->connection_state);
  EXPECT_TRUE(properties->type_properties->is_cellular());

  // Check now that last_good_apn is set, and matches with connected_apn
  cellular_props = properties->type_properties->get_cellular().get();
  ASSERT_TRUE(cellular_props);
  EXPECT_TRUE(cellular_props->last_good_apn);
  const mojom::ApnPropertiesPtr& connected_apn = cellular_props->connected_apn;
  EXPECT_EQ(connected_apn, cellular_props->last_good_apn);
  EXPECT_EQ(kCellularTestApn1, connected_apn->access_point_name);
  EXPECT_EQ(kCellularTestApnName1, connected_apn->name);
  EXPECT_EQ(kCellularTestApnUsername1, connected_apn->username);
  EXPECT_EQ(kCellularTestApnPassword1, connected_apn->password);
  EXPECT_EQ(kCellularTestApnAttach1, connected_apn->attach);
}

TEST_F(CrosNetworkConfigTest, ConnectedAPN_ApnRevampDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kApnRevamp);
  // Configure a cellular network with a last good APN and disconnected
  // as connection status
  helper()->ConfigureService(base::StringPrintf(
      kTestApnCellularShillDictFmt, kTestApnCellularGuid, shill::kStateIdle,
      kCellularTestIccid, NetworkProfileHandler::GetSharedProfilePath().c_str(),
      CreateApnShillDict().c_str()));

  // Verify the connection state
  mojom::ManagedPropertiesPtr properties =
      GetManagedProperties(kTestApnCellularGuid);
  ASSERT_TRUE(properties);
  EXPECT_EQ(kTestApnCellularGuid, properties->guid);
  EXPECT_EQ(mojom::NetworkType::kCellular, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
            properties->connection_state);
  ASSERT_TRUE(properties->type_properties->is_cellular());
  mojom::ManagedCellularProperties* cellular_props =
      properties->type_properties->get_cellular().get();
  ASSERT_TRUE(cellular_props);

  // Check that last_good_apn was set, but not the connected_apn
  EXPECT_TRUE(cellular_props->last_good_apn);
  EXPECT_FALSE(cellular_props->connected_apn);

  // Simulate an update where Shill was able to connect to the cellular network
  helper()->ConfigureService(base::StringPrintf(
      kTestApnCellularShillDictFmt, kTestApnCellularGuid, shill::kStateReady,
      kCellularTestIccid, NetworkProfileHandler::GetSharedProfilePath().c_str(),
      CreateApnShillDict().c_str()));

  // Verify the new connection state
  properties = GetManagedProperties(kTestApnCellularGuid);
  ASSERT_TRUE(properties);
  EXPECT_EQ(kTestApnCellularGuid, properties->guid);
  EXPECT_EQ(mojom::NetworkType::kCellular, properties->type);
  EXPECT_EQ(mojom::ConnectionStateType::kConnected,
            properties->connection_state);
  EXPECT_TRUE(properties->type_properties->is_cellular());

  // Check that last_good_apn was set, and the connected_apn is still not set
  cellular_props = properties->type_properties->get_cellular().get();
  ASSERT_TRUE(cellular_props);
  EXPECT_TRUE(cellular_props->last_good_apn);
  EXPECT_FALSE(cellular_props->connected_apn);
}

TEST_F(CrosNetworkConfigTest, UnrecognizedAttachApnValue) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kApnRevamp);
  SetupAPNList();
  const char kUnrecognizedTestApnAttachStr[] = "unrecognized attach value";

  // Verify that custom APN list is updated properly.
  auto config = mojom::ConfigProperties::New();
  auto cellular_config = mojom::CellularConfigProperties::New();
  auto new_apn = mojom::ApnProperties::New();

  new_apn->attach = kUnrecognizedTestApnAttachStr;
  cellular_config->apn = std::move(new_apn);
  config->type_config = mojom::NetworkTypeConfigProperties::NewCellular(
      std::move(cellular_config));
  SetProperties(kCellularGuid, std::move(config));

  // Unrecognized values are still saved without incident.
  ASSERT_EQ(kUnrecognizedTestApnAttachStr, GetManagedProperties(kCellularGuid)
                                               ->type_properties->get_cellular()
                                               ->custom_apn_list->front()
                                               ->attach);
}

TEST_F(CrosNetworkConfigTest, AllowRoaming) {
  mojom::ManagedPropertiesPtr properties = GetManagedProperties(kCellularGuid);

  ASSERT_FALSE(properties->type_properties->get_cellular()->allow_roaming);

  auto config = mojom::ConfigProperties::New();
  auto cellular_config = mojom::CellularConfigProperties::New();
  auto new_roaming = mojom::RoamingProperties::New();

  new_roaming->allow_roaming = true;
  cellular_config->roaming = std::move(new_roaming);
  config->type_config = mojom::NetworkTypeConfigProperties::NewCellular(
      std::move(cellular_config));
  ASSERT_TRUE(SetProperties(kCellularGuid, std::move(config)));

  properties = GetManagedProperties(kCellularGuid);

  ASSERT_TRUE(properties);
  ASSERT_EQ(kCellularGuid, properties->guid);
  ASSERT_TRUE(properties->type_properties->is_cellular());
  ASSERT_TRUE(
      properties->type_properties->get_cellular()->allow_roaming->active_value);
}

TEST_F(CrosNetworkConfigTest,
       AllowTextMessagesWithSuppressTextMessagesFlagEnabled) {
  // When never set, allow_text_messages will be true.
  AssertCellularAllowTextMessages(kCellularGuid, /*expected_active_value=*/true,
                                  /*expected_policy_value=*/std::nullopt,
                                  mojom::PolicySource::kNone);

  // When text message state is set to false, the value will be updated to
  // false.
  auto config = mojom::ConfigProperties::New();
  auto cellular_config = mojom::CellularConfigProperties::New();
  auto new_text_message_state = mojom::TextMessagesAllowState::New();

  new_text_message_state->allow_text_messages = false;
  cellular_config->text_message_allow_state = std::move(new_text_message_state);
  config->type_config = mojom::NetworkTypeConfigProperties::NewCellular(
      std::move(cellular_config));
  ASSERT_TRUE(SetProperties(kCellularGuid, std::move(config)));
  AssertCellularAllowTextMessages(
      kCellularGuid, /*expected_active_value=*/false,
      /*expected_policy_value=*/std::nullopt, mojom::PolicySource::kNone);

  // When text message state is undefined, this will not update the last saved
  // value of false.
  config = mojom::ConfigProperties::New();
  config->type_config = mojom::NetworkTypeConfigProperties::NewCellular(
      mojom::CellularConfigProperties::New());
  ASSERT_TRUE(SetProperties(kCellularGuid, std::move(config)));
  AssertCellularAllowTextMessages(
      kCellularGuid, /*expected_active_value=*/false,
      /*expected_policy_value=*/std::nullopt, mojom::PolicySource::kNone);

  // When text message state is set to true, the value will be updated to true.
  config = mojom::ConfigProperties::New();
  cellular_config = mojom::CellularConfigProperties::New();
  new_text_message_state = mojom::TextMessagesAllowState::New();

  new_text_message_state->allow_text_messages = true;
  cellular_config->text_message_allow_state = std::move(new_text_message_state);
  config->type_config = mojom::NetworkTypeConfigProperties::NewCellular(
      std::move(cellular_config));

  ASSERT_TRUE(SetProperties(kCellularGuid, std::move(config)));
  AssertCellularAllowTextMessages(kCellularGuid, /*expected_active_value=*/true,
                                  /*expected_policy_value=*/std::nullopt,
                                  mojom::PolicySource::kNone);

  // When text message state is undefined, this will not update the last saved
  // value of true.
  config = mojom::ConfigProperties::New();
  config->type_config = mojom::NetworkTypeConfigProperties::NewCellular(
      mojom::CellularConfigProperties::New());
  ASSERT_TRUE(SetProperties(kCellularGuid, std::move(config)));
  AssertCellularAllowTextMessages(kCellularGuid, /*expected_active_value=*/true,
                                  /*expected_policy_value=*/std::nullopt,
                                  mojom::PolicySource::kNone);
}

TEST_F(CrosNetworkConfigTest,
       AllowTextMessagesPolicyValueWithSuppressTextMessagesFlagEnabled) {
  base::Value::Dict global_config;

  // When the policy is explicitly Suppress, the managed boolean policy value
  // should return false and the policy source should be device enforced.
  global_config.Set(::onc::global_network_config::kAllowTextMessages,
                    ::onc::cellular::kTextMessagesSuppress);

  managed_network_configuration_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
      /*network_configs_onc=*/base::Value::List(), global_config);
  base::RunLoop().RunUntilIdle();

  AssertCellularAllowTextMessages(kCellularGuid,
                                  /*expected_active_value=*/false,
                                  /*expected_policy_value=*/false,
                                  mojom::PolicySource::kDevicePolicyEnforced);

  // When the policy is explicitly Allow, the managed boolean policy value
  // should return true and the policy source should be device enforced.
  global_config.Set(::onc::global_network_config::kAllowTextMessages,
                    ::onc::cellular::kTextMessagesAllow);
  managed_network_configuration_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
      /*network_configs_onc=*/base::Value::List(), global_config);
  base::RunLoop().RunUntilIdle();

  AssertCellularAllowTextMessages(kCellularGuid,
                                  /*expected_active_value=*/true,
                                  /*expected_policy_value=*/true,
                                  mojom::PolicySource::kDevicePolicyEnforced);

  // When the policy is explicitly Unset, we default to the user set value.
  global_config.Set(::onc::global_network_config::kAllowTextMessages,
                    ::onc::cellular::kTextMessagesUnset);
  managed_network_configuration_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
      /*network_configs_onc=*/base::Value::List(), global_config);
  base::RunLoop().RunUntilIdle();

  AssertCellularAllowTextMessages(kCellularGuid,
                                  /*expected_active_value=*/true,
                                  /*expected_policy_value=*/std::nullopt,
                                  mojom::PolicySource::kNone);

  // When global network configuration is not set, we treat it as unset.
  managed_network_configuration_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
      /*network_configs_onc=*/base::Value::List(),
      /*global_network_config=*/base::Value::Dict());
  base::RunLoop().RunUntilIdle();

  AssertCellularAllowTextMessages(kCellularGuid,
                                  /*expected_active_value=*/true,
                                  /*expected_policy_value=*/std::nullopt,
                                  mojom::PolicySource::kNone);
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
      network_state_handler->GetNetworkStateFromGuid(kCellularGuid);
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
                                  /*new_pin=*/std::nullopt,
                                  /*require_pin=*/false));

  // Sim should be unlocked, locking should still be enabled.
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_TRUE(cellular->sim_lock_status->lock_type.empty());

  // Set |require_pin| to false (disable locking).
  EXPECT_TRUE(SetCellularSimState(FakeShillDeviceClient::kDefaultSimPin,
                                  /*new_pin=*/std::nullopt,
                                  /*require_pin=*/false));

  // Sim should be unlocked, locking should be disabled.
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_FALSE(cellular->sim_lock_status->lock_enabled);
  EXPECT_TRUE(cellular->sim_lock_status->lock_type.empty());

  // Set |require_pin| to true (enable locking).
  EXPECT_TRUE(SetCellularSimState(FakeShillDeviceClient::kDefaultSimPin,
                                  /*new_pin=*/std::nullopt,
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
  EXPECT_FALSE(SetCellularSimState("incorrect pin", /*new_pin=*/std::nullopt,
                                   /*require_pin=*/false));

  // Ensure sim is still locked and retry count has decreased.
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_EQ(shill::kSIMLockPin, cellular->sim_lock_status->lock_type);
  EXPECT_EQ(retries - 1, cellular->sim_lock_status->retries_left);

  // Additional attempts should set the sim to puk locked.
  for (int i = retries - 1; i > 0; --i) {
    SetCellularSimState("incorrect pin", /*new_pin=*/std::nullopt, false);
  }
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_EQ(shill::kSIMLockPuk, cellular->sim_lock_status->lock_type);

  // Attempt to unblock the sim with the incorrect puk. Call should fail.
  const std::string new_pin = "2222";
  EXPECT_FALSE(SetCellularSimState("incorrect puk", std::make_optional(new_pin),
                                   /*require_pin=*/false));

  // Attempt to unblock the sim with np pin. Call should fail.
  EXPECT_FALSE(SetCellularSimState(FakeShillDeviceClient::kSimPuk,
                                   /*new_pin=*/std::nullopt,
                                   /*require_pin=*/false));

  // Attempt to unlock the sim with the correct puk.
  EXPECT_TRUE(SetCellularSimState(FakeShillDeviceClient::kSimPuk,
                                  std::make_optional(new_pin),
                                  /*require_pin=*/false));

  // Sim should be unlocked
  cellular = GetDeviceStateFromList(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_type.empty());
}

TEST_F(CrosNetworkConfigTest, SelectCellularMobileNetwork) {
  // Create fake list of found networks.
  std::optional<base::Value> found_networks_list =
      base::JSONReader::Read(base::StringPrintf(
          R"([{"network_id": "network1", "technology": "GSM",
               "status": "current"},
              {"network_id": "network2", "technology": "GSM",
               "status": "available"}])"));
  helper()->device_test()->SetDeviceProperty(
      kCellularDevicePath, shill::kFoundNetworksProperty, *found_networks_list,
      /*notify_changed=*/true);

  // Assert initial state
  mojom::ManagedPropertiesPtr properties = GetManagedProperties(kCellularGuid);
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
  EXPECT_TRUE(SelectCellularMobileNetwork(kCellularGuid, "network2"));
  properties = GetManagedProperties(kCellularGuid);
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
    raw_ptr<CrosNetworkConfig> cros_network_config_;
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
  base::Value::Dict global_config;
  global_config.Set(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      true);
  global_config.Set(::onc::global_network_config::kAllowOnlyPolicyWiFiToConnect,
                    false);
  base::Value::List blocked;
  blocked.Append("blocked_ssid1");
  blocked.Append("blocked_ssid2");
  global_config.Set(::onc::global_network_config::kBlockedHexSSIDs,
                    std::move(blocked));
  managed_network_configuration_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
      /*network_configs_onc=*/base::Value::List(), global_config);
  base::RunLoop().RunUntilIdle();
  mojom::GlobalPolicyPtr policy = GetGlobalPolicy();
  ASSERT_TRUE(policy);
  EXPECT_TRUE(policy->allow_apn_modification);
  EXPECT_TRUE(policy->allow_cellular_sim_lock);
  EXPECT_FALSE(policy->allow_only_policy_cellular_networks);
  EXPECT_TRUE(policy->allow_only_policy_networks_to_autoconnect);
  EXPECT_FALSE(policy->allow_only_policy_wifi_networks_to_connect);
  EXPECT_FALSE(policy->allow_only_policy_wifi_networks_to_connect_if_available);
  EXPECT_FALSE(policy->dns_queries_monitored);
  EXPECT_FALSE(policy->report_xdr_events_enabled);
  ASSERT_EQ(2u, policy->blocked_hex_ssids.size());
  EXPECT_EQ("blocked_ssid1", policy->blocked_hex_ssids[0]);
  EXPECT_EQ("blocked_ssid2", policy->blocked_hex_ssids[1]);
  EXPECT_FALSE(policy->recommended_values_are_ephemeral);
  EXPECT_FALSE(policy->user_created_network_configurations_are_ephemeral);
  EXPECT_EQ(mojom::SuppressionType::kUnset, policy->allow_text_messages);
}

TEST_F(CrosNetworkConfigTest, GlobalPolicyApplied) {
  SetupObserver();
  EXPECT_EQ(0, observer()->GetPolicyAppliedCount(/*userhash=*/std::string()));

  base::Value::Dict global_config;
  global_config.Set(::onc::global_network_config::kAllowAPNModification, false);
  global_config.Set(::onc::global_network_config::kAllowCellularSimLock, false);
  global_config.Set(::onc::global_network_config::kAllowCellularHotspot, false);
  global_config.Set(
      ::onc::global_network_config::kAllowOnlyPolicyCellularNetworks, true);
  global_config.Set(::onc::global_network_config::kAllowOnlyPolicyWiFiToConnect,
                    false);
  managed_network_configuration_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
      /*network_configs_onc=*/base::Value::List(), global_config);
  base::RunLoop().RunUntilIdle();
  mojom::GlobalPolicyPtr policy = GetGlobalPolicy();
  ASSERT_TRUE(policy);
  EXPECT_FALSE(policy->allow_apn_modification);
  EXPECT_FALSE(policy->allow_cellular_sim_lock);
  EXPECT_FALSE(policy->allow_cellular_hotspot);
  EXPECT_TRUE(policy->allow_only_policy_cellular_networks);
  EXPECT_FALSE(policy->allow_only_policy_networks_to_autoconnect);
  EXPECT_FALSE(policy->allow_only_policy_wifi_networks_to_connect);
  EXPECT_FALSE(policy->allow_only_policy_wifi_networks_to_connect_if_available);
  EXPECT_FALSE(policy->dns_queries_monitored);
  EXPECT_FALSE(policy->report_xdr_events_enabled);
  EXPECT_FALSE(policy->recommended_values_are_ephemeral);
  EXPECT_FALSE(policy->user_created_network_configurations_are_ephemeral);
  EXPECT_EQ(mojom::SuppressionType::kUnset, policy->allow_text_messages);

  EXPECT_EQ(1, observer()->GetPolicyAppliedCount(/*userhash=*/std::string()));

  policy = GetGlobalPolicy();
  EXPECT_FALSE(policy->allow_apn_modification);
}

TEST_F(CrosNetworkConfigTest,
       GetGlobalPolicy_EphemeralNetworkPolicies_Disabled) {
  base::Value::Dict global_config;
  global_config.Set(
      ::onc::global_network_config::kRecommendedValuesAreEphemeral, true);
  global_config.Set(::onc::global_network_config::
                        kUserCreatedNetworkConfigurationsAreEphemeral,
                    true);
  managed_network_configuration_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
      /*network_configs_onc=*/base::Value::List(), global_config);
  base::RunLoop().RunUntilIdle();
  mojom::GlobalPolicyPtr policy = GetGlobalPolicy();
  ASSERT_TRUE(policy);
  EXPECT_FALSE(policy->recommended_values_are_ephemeral);
  EXPECT_FALSE(policy->user_created_network_configurations_are_ephemeral);
}

TEST_F(CrosNetworkConfigTest,
       GetGlobalPolicy_EphemeralNetworkPolicies_Enabled) {
  policy_util::SetEphemeralNetworkPoliciesEnabled();

  base::Value::Dict global_config;
  global_config.Set(
      ::onc::global_network_config::kRecommendedValuesAreEphemeral, true);
  managed_network_configuration_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
      /*network_configs_onc=*/base::Value::List(), global_config);
  base::RunLoop().RunUntilIdle();
  {
    mojom::GlobalPolicyPtr policy = GetGlobalPolicy();
    ASSERT_TRUE(policy);
    EXPECT_TRUE(policy->recommended_values_are_ephemeral);
    EXPECT_FALSE(policy->user_created_network_configurations_are_ephemeral);
  }

  global_config.Set(::onc::global_network_config::
                        kUserCreatedNetworkConfigurationsAreEphemeral,
                    true);
  managed_network_configuration_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
      /*network_configs_onc=*/base::Value::List(), global_config);
  base::RunLoop().RunUntilIdle();
  {
    mojom::GlobalPolicyPtr policy = GetGlobalPolicy();
    ASSERT_TRUE(policy);
    EXPECT_TRUE(policy->recommended_values_are_ephemeral);
    EXPECT_TRUE(policy->user_created_network_configurations_are_ephemeral);
  }
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
  NetworkHandler::Get()->technology_state_controller()->SetTechnologiesEnabled(
      NetworkTypePattern::WiFi(), false, network_handler::ErrorCallback());
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
      network_state_handler->GetNetworkStateFromGuid(kCellularGuid);
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

TEST_F(CrosNetworkConfigTest, PolicyEnforcedProxyMode) {
  // Proxies enforced by policy and/or extension are set in the kProxy
  // preference.
  user_prefs_.SetUserPref(proxy_config::prefs::kProxy,
                          ProxyConfigDictionary::CreateAutoDetect());

  mojom::NetworkStatePropertiesPtr network = GetNetworkState("wifi2_guid");
  ASSERT_TRUE(network);
  EXPECT_EQ(network->proxy_mode, mojom::ProxyMode::kAutoDetect);
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

// Tests that the Passpoint identifier of a Wi-Fi network is reflected to its
// network state.
TEST_F(CrosNetworkConfigTest, NetworkStateHasPasspointId) {
  const char kWifiGuid[] = "wifi_pp_guid";
  const char kPasspointId[] = "passpoint_id";
  helper()->ConfigureService(base::StringPrintf(
      R"({"GUID": "%s", "Type": "wifi", "State": "idle",
          "Strength": 90, "AutoConnect": true, "Connectable": true,
          "Passpoint.ID": "%s"})",
      kWifiGuid, kPasspointId));
  mojom::NetworkStatePropertiesPtr network = GetNetworkState(kWifiGuid);
  ASSERT_TRUE(network);
  EXPECT_EQ(mojom::NetworkType::kWiFi, network->type);
  EXPECT_EQ(kPasspointId, network->type_state->get_wifi()->passpoint_id);
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

TEST_F(CrosNetworkConfigTest, IsProhibitedFromConfiguringVpn) {
  arc::prefs::RegisterProfilePrefs(user_prefs_.registry());
  user_prefs_.registry()->RegisterBooleanPref(prefs::kVpnConfigAllowed, true);

  for (const std::string& package_name : {"", "package_name"}) {
      for (const bool vpn_configure_allowed : {true, false}) {
        SetArcAlwaysOnUserPrefs(package_name, vpn_configure_allowed);
        const std::string guid = ConfigureNetwork(
            CreateFakeVpnConfig("name", "host", mojom::VpnType::kArc),
            /*shared=*/true);
        if (package_name.empty() || vpn_configure_allowed) {
          EXPECT_FALSE(guid.empty());
          continue;
        }
        EXPECT_TRUE(guid.empty());
      }
  }
}

TEST_F(CrosNetworkConfigTest, RequestTrafficCountersWithIntegerType) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kTrafficCountersEnabled,
                            features::kTrafficCountersForWiFiTesting},
      /*disabled_features=*/{});
  traffic_counters::TrafficCountersHandler::InitializeForTesting();
  base::Value::List traffic_counters;

  base::Value::Dict chrome_dict;
  chrome_dict.Set("source", shill::kTrafficCounterSourceChrome);
  chrome_dict.Set("rx_bytes", 12);
  chrome_dict.Set("tx_bytes", 32);
  traffic_counters.Append(std::move(chrome_dict));

  base::Value::Dict user_dict;
  user_dict.Set("source", shill::kTrafficCounterSourceUser);
  user_dict.Set("rx_bytes", 90);
  user_dict.Set("tx_bytes", 87);
  traffic_counters.Append(std::move(user_dict));

  ASSERT_EQ(traffic_counters.size(), 2u);
  helper()->service_test()->SetFakeTrafficCounters(traffic_counters.Clone());

  RequestTrafficCountersAndCompareTrafficCounters(
      "wifi1_guid", traffic_counters, ComparisonType::INTEGER);
}

TEST_F(CrosNetworkConfigTest, RequestTrafficCountersWithDoubleType) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kTrafficCountersEnabled,
                            features::kTrafficCountersForWiFiTesting},
      /*disabled_features=*/{});
  traffic_counters::TrafficCountersHandler::InitializeForTesting();
  base::Value::List traffic_counters;

  base::Value::Dict chrome_dict;
  chrome_dict.Set("source", shill::kTrafficCounterSourceChrome);
  chrome_dict.Set("rx_bytes", 123456789987.0);
  chrome_dict.Set("tx_bytes", 3211234567898.0);
  traffic_counters.Append(std::move(chrome_dict));

  base::Value::Dict user_dict;
  user_dict.Set("source", shill::kTrafficCounterSourceUser);
  user_dict.Set("rx_bytes", 9000000000000000.0);
  user_dict.Set("tx_bytes", 8765432112345.0);
  traffic_counters.Append(std::move(user_dict));

  ASSERT_EQ(traffic_counters.size(), 2u);
  helper()->service_test()->SetFakeTrafficCounters(traffic_counters.Clone());

  RequestTrafficCountersAndCompareTrafficCounters(
      "wifi1_guid", traffic_counters, ComparisonType::DOUBLE);
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

TEST_F(CrosNetworkConfigTest, SetResetDay) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kTrafficCountersEnabled,
                            features::kTrafficCountersForWiFiTesting},
      /*disabled_features=*/{});
  traffic_counters::TrafficCountersHandler::InitializeForTesting();
  SetTrafficCountersResetDayAndCompare("wifi1_guid",
                                       /*day=*/mojom::UInt32Value::New(32),
                                       /*expected_success=*/false,
                                       /*expected_reset_day=*/nullptr);
  base::Value expected_reset_day(2);
  SetTrafficCountersResetDayAndCompare("wifi1_guid",
                                       /*day=*/mojom::UInt32Value::New(2),
                                       /*expected_success=*/true,
                                       &expected_reset_day);
  // Auto reset prefs remains unchanged from last successful call.
  SetTrafficCountersResetDayAndCompare("wifi1_guid",
                                       /*day=*/mojom::UInt32Value::New(0),
                                       /*expected_success=*/false,
                                       &expected_reset_day);
}

// Make sure calling shutdown before cros_network_config destruction doesn't
// cause a crash.
TEST_F(CrosNetworkConfigTest, Shutdown) {
  SetupObserver();
  base::RunLoop().RunUntilIdle();

  NetworkHandler::Get()->network_state_handler()->Shutdown();
  NetworkHandler::Get()->managed_network_configuration_handler()->Shutdown();
}

}  // namespace ash::network_config
