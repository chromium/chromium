// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/hermes/hermes_clients.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/cellular_connection_handler.h"
#include "chromeos/ash/components/network/cellular_esim_installer.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/cellular_policy_handler.h"
#include "chromeos/ash/components/network/fake_network_connection_handler.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler_impl.h"
#include "chromeos/ash/components/network/mock_network_metadata_store.h"
#include "chromeos/ash/components/network/mock_network_state_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_policy_observer.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "chromeos/ash/components/network/prohibited_technologies_handler.h"
#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "chromeos/ash/components/network/test_cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/text_message_suppression_state.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/components/onc/onc_validator.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace test_utils = ::chromeos::onc::test_utils;
using base::test::DictionaryHasValue;
using base::test::DictionaryHasValues;

namespace ash {

using testing::ElementsAre;
using testing::IsEmpty;
using ::testing::Return;

namespace {

constexpr char kUser1[] = "user1";
constexpr char kUser1ProfilePath[] = "/profile/user1/shill";

// The GUID used by chromeos/components/test/data/onc/policy/*.{json,onc} files
// for a VPN.
constexpr char kTestGuidVpn[] = "{a3860e83-f03d-4cb1-bafa-b22c9e746950}";

// The GUID used by chromeos/components/test/data/onc/policy/*.{json,onc} files
// for a managed Wifi service.
constexpr char kTestGuidManagedWifi[] = "policy_wifi1";

// The GUID used by chromeos/components/test/data/onc/policy/policy_cellular.onc
// files for a managed Cellular service.
constexpr char kTestGuidManagedCellular[] = "policy_cellular";

// The GUID used by
// chromeos/components/test/data/onc/policy/policy_cellular_with_iccid.onc files
// for a managed Cellular service.
constexpr char kTestGuidManagedCellular2[] = "policy_cellular2";

// The GUID used by
// chromeos/components/test/data/onc/policy/policy_cellular_with_no_smdp.onc
// files for a managed Cellular service.
constexpr char kTestGuidManagedCellular3[] = "policy_cellular3";

// The GUID used by chromeos/components/test/data/onc/policy/*.{json,onc} files
// for an unmanaged Wifi service.
constexpr char kTestGuidUnmanagedWifi2[] = "wifi2";

// The GUID used by chromeos/components/test/data/onc/policy/*.{json,onc} files
// for a Wifi service.
constexpr char kTestGuidEthernetEap[] = "policy_ethernet_eap";

constexpr char kTestEuiccPath[] = "/org/chromium/Hermes/Euicc/0";
constexpr char kTestEid[] = "12345678901234567890123456789012";
constexpr char kTestCellularServicePath[] = "cellular_service_path";
constexpr char kTestCellularGuid[] = "cellular_guid";

// A valid but empty (no networks and no certificates) and unencrypted
// configuration.
constexpr char kEmptyUnencryptedConfiguration[] =
    "{\"Type\":\"UnencryptedConfiguration\",\"NetworkConfigurations\":[],"
    "\"Certificates\":[]}";

void ErrorCallback(const std::string& error_name) {
  ADD_FAILURE() << "Unexpected error: " << error_name;
}

class TestNetworkPolicyObserver : public NetworkPolicyObserver {
 public:
  TestNetworkPolicyObserver() = default;

  TestNetworkPolicyObserver(const TestNetworkPolicyObserver&) = delete;
  TestNetworkPolicyObserver& operator=(const TestNetworkPolicyObserver&) =
      delete;

  void PoliciesApplied(const std::string& userhash) override {
    policies_applied_count_++;
  }

  void PoliciesChanged(const std::string& userhash) override {
    if (userhash.empty() && on_shared_profile_policies_changed_) {
      std::move(on_shared_profile_policies_changed_).Run();
    }
  }

  int GetPoliciesAppliedCountAndReset() {
    int count = policies_applied_count_;
    policies_applied_count_ = 0;
    return count;
  }

  void RunOnSharedProfilePoliciesChanged(base::OnceClosure action) {
    on_shared_profile_policies_changed_ = std::move(action);
  }

 private:
  int policies_applied_count_ = 0;
  base::OnceClosure on_shared_profile_policies_changed_;
};

}  // namespace

class ManagedNetworkConfigurationHandlerTest : public testing::Test {
 public:
  ManagedNetworkConfigurationHandlerTest() = default;
  ManagedNetworkConfigurationHandlerTest(
      const ManagedNetworkConfigurationHandlerTest&) = delete;
  ManagedNetworkConfigurationHandlerTest& operator=(
      const ManagedNetworkConfigurationHandlerTest&) = delete;

  ~ManagedNetworkConfigurationHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    LoginState::Initialize();

    shill_clients::InitializeFakes();
    hermes_clients::InitializeFakes();

    ShillManagerClient::Get()
        ->GetTestInterface()
        ->SetWifiServicesVisibleByDefault(false);

    network_state_handler_ = MockNetworkStateHandler::InitializeForTest();
    network_device_handler_ = NetworkDeviceHandler::InitializeForTesting(
        network_state_handler_.get());
    network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();
    technology_state_controller_ =
        std::make_unique<TechnologyStateController>();
    technology_state_controller_->Init(network_state_handler_.get());
    network_configuration_handler_ =
        NetworkConfigurationHandler::InitializeForTest(
            network_state_handler_.get(), network_device_handler_.get());
    network_connection_handler_ =
        std::make_unique<FakeNetworkConnectionHandler>();
    cellular_inhibitor_ = std::make_unique<CellularInhibitor>();
    cellular_inhibitor_->Init(network_state_handler_.get(),
                              network_device_handler_.get());
    cellular_esim_profile_handler_ =
        std::make_unique<TestCellularESimProfileHandler>();
    cellular_esim_profile_handler_->Init(network_state_handler_.get(),
                                         cellular_inhibitor_.get());
    cellular_connection_handler_ =
        std::make_unique<CellularConnectionHandler>();
    cellular_connection_handler_->Init(network_state_handler_.get(),
                                       cellular_inhibitor_.get(),
                                       cellular_esim_profile_handler_.get());
    cellular_esim_installer_ = std::make_unique<CellularESimInstaller>();
    // TODO(crbug.com/1248229): Create fake cellular esim installer for test
    // setup.
    cellular_esim_installer_->Init(
        cellular_connection_handler_.get(), cellular_inhibitor_.get(),
        network_connection_handler_.get(), network_profile_handler_.get(),
        network_state_handler_.get());
    cellular_policy_handler_ = std::make_unique<CellularPolicyHandler>();
    // ProhibitedTechnologiesHandler's ctor is private.
    prohibited_technologies_handler_.reset(new ProhibitedTechnologiesHandler);

    managed_cellular_pref_handler_ =
        std::make_unique<ManagedCellularPrefHandler>();
    managed_cellular_pref_handler_->Init(network_state_handler_.get());
    ManagedCellularPrefHandler::RegisterLocalStatePrefs(
        device_prefs_.registry());
    managed_cellular_pref_handler_->SetDevicePrefs(&device_prefs_);

    // ManagedNetworkConfigurationHandlerImpl's ctor is private.
    managed_network_configuration_handler_.reset(
        new ManagedNetworkConfigurationHandlerImpl());

    network_metadata_store_ =
        base::WrapUnique(new testing::NiceMock<MockNetworkMetadataStore>());
    managed_network_configuration_handler_
        ->set_network_metadata_store_for_testing(network_metadata_store_.get());

    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
    ::onc::RegisterProfilePrefs(user_prefs_.registry());
    ::onc::RegisterPrefs(local_state_.registry());

    ui_proxy_config_service_ = std::make_unique<UIProxyConfigService>(
        &user_prefs_, &local_state_, network_state_handler_.get(),
        network_profile_handler_.get());
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    NetworkHandler* network_handler = NetworkHandler::Get();
    managed_network_configuration_handler_->Init(
        cellular_policy_handler_.get(), managed_cellular_pref_handler_.get(),
        network_state_handler_.get(), network_profile_handler_.get(),
        network_configuration_handler_.get(), network_device_handler_.get(),
        prohibited_technologies_handler_.get(),
        network_handler->hotspot_controller());
    managed_network_configuration_handler_->set_ui_proxy_config_service(
        ui_proxy_config_service_.get());
    managed_network_configuration_handler_->set_user_prefs(&user_prefs_);
    managed_network_configuration_handler_->AddObserver(&policy_observer_);
    cellular_policy_handler_->Init(
        cellular_esim_profile_handler_.get(), cellular_esim_installer_.get(),
        cellular_inhibitor_.get(), network_profile_handler_.get(),
        network_state_handler_.get(), managed_cellular_pref_handler_.get(),
        managed_network_configuration_handler_.get());
    prohibited_technologies_handler_->Init(
        managed_network_configuration_handler_.get(),
        network_state_handler_.get(), technology_state_controller_.get());

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    // Run remaining tasks.
    base::RunLoop().RunUntilIdle();
    ResetManagedNetworkConfigurationHandler();
    network_handler_test_helper_.reset();
    cellular_policy_handler_.reset();
    cellular_esim_installer_.reset();
    cellular_esim_profile_handler_.reset();
    cellular_connection_handler_.reset();
    cellular_inhibitor_.reset();
    managed_cellular_pref_handler_.reset();
    network_configuration_handler_.reset();
    ui_proxy_config_service_.reset();
    technology_state_controller_.reset();
    network_profile_handler_.reset();
    network_device_handler_.reset();
    network_state_handler_.reset();
    network_connection_handler_.reset();

    hermes_clients::Shutdown();
    shill_clients::Shutdown();

    LoginState::Shutdown();
  }

  TestNetworkPolicyObserver* policy_observer() { return &policy_observer_; }

  ManagedNetworkConfigurationHandler* managed_handler() {
    return managed_network_configuration_handler_.get();
  }

  ShillServiceClient::TestInterface* GetShillServiceClient() {
    return ShillServiceClient::Get()->GetTestInterface();
  }

  ShillProfileClient::TestInterface* GetShillProfileClient() {
    return ShillProfileClient::Get()->GetTestInterface();
  }

  void InitializeStandardProfiles() {
    GetShillProfileClient()->AddProfile(kUser1ProfilePath, kUser1);
    GetShillProfileClient()->AddProfile(
        NetworkProfileHandler::GetSharedProfilePath(),
        std::string() /* no userhash */);
  }

  void InitializeEuicc() {
    HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
    HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath), kTestEid, /*is_active=*/true,
        /*physical_slot=*/0);
    cellular_esim_profile_handler_->SetHasRefreshedProfilesForEuicc(
        kTestEid, dbus::ObjectPath(kTestEuiccPath), /*has_refreshed=*/true);
    base::RunLoop().RunUntilIdle();
  }

  bool SetPolicy(::onc::ONCSource onc_source,
                 const std::string& userhash,
                 const std::string& path_to_onc) {
    if (path_to_onc.empty()) {
      std::optional<base::Value::Dict> policy =
          chromeos::onc::ReadDictionaryFromJson(kEmptyUnencryptedConfiguration);
      if (!policy.has_value()) {
        return false;
      }
      return SetPolicy(onc_source, userhash, std::move(policy.value()));
    }
    base::Value::Dict policy_value =
        test_utils::ReadTestDictionary(path_to_onc);
    return SetPolicy(onc_source, userhash, std::move(policy_value));
  }

  bool SetPolicy(::onc::ONCSource onc_source,
                 const std::string& userhash,
                 base::Value::Dict policy) {
    chromeos::onc::Validator validator(/*error_on_unknown_field=*/true,
                                       /*error_on_wrong_recommended=*/true,
                                       /*error_on_missing_field=*/false,
                                       /*managed_onc=*/true,
                                       /*log_warnings=*/true);
    validator.SetOncSource(onc_source);
    chromeos::onc::Validator::Result validation_result;
    std::optional<base::Value::Dict> validated_policy =
        validator.ValidateAndRepairObject(
            &chromeos::onc::kToplevelConfigurationSignature, policy,
            &validation_result);
    if (validation_result == chromeos::onc::Validator::INVALID) {
      ADD_FAILURE() << "Network configuration invalid.";
      return false;
    }

    base::Value::List network_configs;
    const base::Value::List* found_network_configs = validated_policy->FindList(
        ::onc::toplevel_config::kNetworkConfigurations);
    if (found_network_configs) {
      for (const auto& network_config : *found_network_configs) {
        network_configs.Append(network_config.Clone());
      }
    }

    base::Value::Dict global_config;
    const base::Value::Dict* found_global_config = validated_policy->FindDict(
        ::onc::toplevel_config::kGlobalNetworkConfiguration);
    if (found_global_config) {
      global_config = found_global_config->Clone();
    }

    managed_network_configuration_handler_->SetPolicy(
        onc_source, userhash, network_configs, global_config);
    return true;
  }

  void SetUpEntry(const std::string& path_to_shill_json,
                  const std::string& profile_path,
                  const std::string& entry_path) {
    base::Value::Dict entry =
        test_utils::ReadTestDictionary(path_to_shill_json);
    GetShillProfileClient()->AddEntry(profile_path, entry_path, entry);
  }

  void ResetManagedNetworkConfigurationHandler() {
    if (!managed_network_configuration_handler_)
      return;
    prohibited_technologies_handler_.reset();
    managed_network_configuration_handler_->RemoveObserver(&policy_observer_);
    managed_network_configuration_handler_.reset();
  }

  NetworkHandlerTestHelper* network_handler_test_helper() {
    return network_handler_test_helper_.get();
  }

  bool PropertiesMatch(const base::Value::Dict& v1,
                       const base::Value::Dict& v2) {
    if (v1 == v2)
      return true;
    // EXPECT_EQ does not recursively log dictionaries, so use LOG instead.
    LOG(ERROR) << "v1=" << v1;
    LOG(ERROR) << "v2=" << v2;
    return false;
  }

  void FastForwardProfileRefreshDelay() {
    const base::TimeDelta kProfileRefreshCallbackDelay =
        base::Milliseconds(150);

    // Connect can result in two profile refresh calls before and after
    // enabling profile. Fast forward by delay after refresh.
    task_environment_.FastForwardBy(2 * kProfileRefreshCallbackDelay);
  }

  void FastForwardAutoConnectWaiting() {
    task_environment_.FastForwardBy(
        CellularConnectionHandler::kWaitingForAutoConnectTimeout);
  }

  void SetArcAlwaysOnUserPrefs(std::string package_name,
                               bool vpn_configured_allowed = false) {
    user_prefs_.SetUserPref(arc::prefs::kAlwaysOnVpnPackage,
                            base::Value(package_name));
    user_prefs_.SetUserPref(prefs::kVpnConfigAllowed,
                            base::Value(vpn_configured_allowed));
  }

  ProhibitedTechnologiesHandler* prohibited_technologies_handler() {
    return prohibited_technologies_handler_.get();
  }

  void ConfigureCellularService(const std::string& service_path,
                                const std::string& type) {
    base::Value::Dict properties;
    shill_property_util::SetSSID(service_path, &properties);
    properties.Set(shill::kNameProperty, service_path);
    properties.Set(shill::kGuidProperty, kTestCellularGuid);
    properties.Set(shill::kTypeProperty, type);
    properties.Set(shill::kStateProperty, shill::kStateIdle);
    properties.Set(shill::kProfileProperty,
                   NetworkProfileHandler::GetSharedProfilePath());

    network_configuration_handler_->CreateShillConfiguration(
        std::move(properties), base::DoNothing(),
        base::BindOnce(&ErrorCallback));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;

  TestNetworkPolicyObserver policy_observer_;
  std::unique_ptr<MockNetworkStateHandler> network_state_handler_;
  std::unique_ptr<TechnologyStateController> technology_state_controller_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<UIProxyConfigService> ui_proxy_config_service_;
  std::unique_ptr<ManagedCellularPrefHandler> managed_cellular_pref_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandlerImpl>
      managed_network_configuration_handler_;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<CellularConnectionHandler> cellular_connection_handler_;
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<TestCellularESimProfileHandler>
      cellular_esim_profile_handler_;
  std::unique_ptr<FakeNetworkConnectionHandler> network_connection_handler_;
  std::unique_ptr<CellularESimInstaller> cellular_esim_installer_;
  std::unique_ptr<CellularPolicyHandler> cellular_policy_handler_;
  std::unique_ptr<ProhibitedTechnologiesHandler>
      prohibited_technologies_handler_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<MockNetworkMetadataStore> network_metadata_store_;

  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_, device_prefs_;
};

TEST_F(ManagedNetworkConfigurationHandlerTest, RemoveIrrelevantFields) {
  InitializeStandardProfiles();
  base::Value::Dict expected_shill_properties = test_utils::ReadTestDictionary(
      "policy/shill_policy_on_unconfigured_wifi1.json");

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1_with_redundant_fields.onc"));
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(expected_shill_properties));
}

// A network policy uses a variable expansion which is set after the policy has
// been initially applied.
TEST_F(ManagedNetworkConfigurationHandlerTest, VariableSetAfterPolicy) {
  InitializeStandardProfiles();

  // Initial policy application.
  const char* const onc_policy = R"(
      {
        "NetworkConfigurations": [
          {
            "GUID": "policy_wifi1",
            "Type": "WiFi",
            "Name": "Managed wifi1",
            "WiFi": {
              "Recommended": [ "AutoConnect"],
              "SSID": "wifi1",
              "Security": "WPA-EAP",
              "EAP": {
                "Outer": "PEAP",
                "Identity": "${LOGIN_ID}",
                "Recommended": [
                  "AnonymousIdentity",
                ]
              }
            }
          }
        ],
        "Type": "UnencryptedConfiguration"
      })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        base::test::ParseJsonDict(onc_policy)));
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());

  // Expect that the variable has not been resolved because it didn't have a
  // value.
  {
    const base::Value::Dict* properties =
        GetShillServiceClient()->GetServiceProperties(service_path);
    ASSERT_TRUE(properties);
    const std::string* identity =
        properties->FindString(shill::kEapIdentityProperty);
    ASSERT_TRUE(identity);
    EXPECT_EQ(*identity, "${LOGIN_ID}");
  }

  // Set a value for the variable.
  managed_handler()->SetProfileWideVariableExpansions(
      kUser1, {{"LOGIN_ID", "VarValue"}});

  // Expect that a policy re-application happens and the variable gets resolved.
  EXPECT_TRUE(managed_handler()->IsAnyPolicyApplicationRunning());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(managed_handler()->IsAnyPolicyApplicationRunning());

  {
    const base::Value::Dict* properties =
        GetShillServiceClient()->GetServiceProperties(service_path);
    ASSERT_TRUE(properties);
    const std::string* identity =
        properties->FindString(shill::kEapIdentityProperty);
    ASSERT_TRUE(identity);
    EXPECT_EQ(*identity, "VarValue");
  }
}

// A network policy uses a variable expansion which is set before the policy has
// been initially applied.
TEST_F(ManagedNetworkConfigurationHandlerTest, VariableSetBeforePolicy) {
  InitializeStandardProfiles();

  // Set a value for the variable.
  managed_handler()->SetProfileWideVariableExpansions(
      kUser1, {{"LOGIN_ID", "VarValue"}});
  // Initial policy application.
  const char* const onc_policy = R"(
      {
        "NetworkConfigurations": [
          {
            "GUID": "policy_wifi1",
            "Type": "WiFi",
            "Name": "Managed wifi1",
            "WiFi": {
              "Recommended": [ "AutoConnect"],
              "SSID": "wifi1",
              "Security": "WPA-EAP",
              "EAP": {
                "Outer": "PEAP",
                "Identity": "${LOGIN_ID}",
                "Recommended": [
                  "AnonymousIdentity",
                ]
              }
            }
          }
        ],
        "Type": "UnencryptedConfiguration"
      })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        base::test::ParseJsonDict(onc_policy)));
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());

  // Expect that the variable has been resolved.
  {
    const base::Value::Dict* properties =
        GetShillServiceClient()->GetServiceProperties(service_path);
    ASSERT_TRUE(properties);
    const std::string* identity =
        properties->FindString(shill::kEapIdentityProperty);
    ASSERT_TRUE(identity);
    EXPECT_EQ(*identity, "VarValue");
  }
}

// A variable expansion is changed which does not affect any network.
TEST_F(ManagedNetworkConfigurationHandlerTest, VariableDoesNotAffectPolicy) {
  InitializeStandardProfiles();

  // Initial policy application.
  const char* const onc_policy = R"(
      {
        "NetworkConfigurations": [
          {
            "GUID": "policy_wifi1",
            "Type": "WiFi",
            "Name": "Managed wifi1",
            "WiFi": {
              "Recommended": [ "AutoConnect"],
              "SSID": "wifi1",
              "Security": "WPA-EAP",
              "EAP": {
                "Outer": "PEAP",
                "Identity": "no_variable",
                "Recommended": [
                  "AnonymousIdentity",
                ]
              }
            }
          }
        ],
        "Type": "UnencryptedConfiguration"
      })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        base::test::ParseJsonDict(onc_policy)));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(managed_handler()->IsAnyPolicyApplicationRunning());

  // Set a value for a variable which is not referenced by any network.
  managed_handler()->SetProfileWideVariableExpansions(
      kUser1, {{"LOGIN_ID", "VarValue"}});

  // No policy re-application should be in progress.
  EXPECT_FALSE(managed_handler()->IsAnyPolicyApplicationRunning());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyProhibitedTechnology) {
  const char* const empty =
      "policy/policy_empty_global_network_configuration.onc";
  const char* const prohibit_wifi =
      "policy/policy_global_network_configuration_prohibit_wifi.onc";

  // Technologies prohibited by policy are only enforced if the user policy has
  // been applied and we are in an active user session.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);
  prohibited_technologies_handler()->PoliciesApplied(kUser1);

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(), empty));
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      prohibited_technologies_handler()->GetCurrentlyProhibitedTechnologies(),
      IsEmpty());

  EXPECT_TRUE(
      SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(), prohibit_wifi));
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      prohibited_technologies_handler()->GetCurrentlyProhibitedTechnologies(),
      ElementsAre(shill::kTypeWifi));

  // Not explicitly prohibiting any technology should result in all
  // technologies being explicitly allowed.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(), empty));
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      prohibited_technologies_handler()->GetCurrentlyProhibitedTechnologies(),
      IsEmpty());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, ModifyCustomApns) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(/*enabled_features=*/
                                       {features::kApnRevamp,
                                        features::kAllowApnModificationPolicy},
                                       /*disabled_features=*/{});
  ConfigureCellularService(kTestCellularServicePath, shill::kTypeCellular);

  auto custom_apn_list = base::Value::List().Append(
      base::Value::Dict()
          .Set(::onc::cellular_apn::kAccessPointName, "apn1")
          .Set(::onc::cellular_apn::kState, ::onc::cellular_apn::kStateEnabled)
          .Set(::onc::cellular_apn::kApnTypes,
               base::Value::List().Append(
                   ::onc::cellular_apn::kApnTypeDefault)));
  EXPECT_CALL(*(network_metadata_store_.get()),
              GetCustomApnList(kTestCellularGuid))
      .WillRepeatedly(Return(&custom_apn_list));

  // Set 'AllowApnModification' policy.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        "policy/policy_allow_apn_modification.onc"));
  base::RunLoop().RunUntilIdle();

  std::optional<base::Value::List> shill_custom_apns =
      network_handler_test_helper()->GetServiceListProperty(
          kTestCellularServicePath, shill::kCellularCustomApnListProperty);
  ASSERT_FALSE(shill_custom_apns.has_value());

  EXPECT_TRUE(SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
      "managed_cellular_no_recommended_allow_apn_modification_true.onc"));
  base::RunLoop().RunUntilIdle();

  shill_custom_apns = network_handler_test_helper()->GetServiceListProperty(
      "service_path_for_cellular_guid", shill::kCellularCustomApnListProperty);
  ASSERT_TRUE(shill_custom_apns.has_value());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyManagedCellular) {
  InitializeStandardProfiles();
  InitializeEuicc();

  base::Value::Dict expected_shill_properties = test_utils::ReadTestDictionary(
      "policy/shill_policy_on_unconfigured_cellular.json");

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        "policy/policy_cellular.onc"));
  FastForwardProfileRefreshDelay();
  FastForwardAutoConnectWaiting();
  base::RunLoop().RunUntilIdle();

  std::string service_path = GetShillServiceClient()->FindServiceMatchingGUID(
      kTestGuidManagedCellular);
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(expected_shill_properties));
  const std::string* iccid = properties->FindString(shill::kIccidProperty);
  ASSERT_TRUE(iccid);
  EXPECT_TRUE(managed_cellular_pref_handler_->GetESimMetadata(*iccid));

  // Verify that applying a new cellular policy with same ICCID should update
  // the old shill configuration.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        "policy/policy_cellular_with_iccid.onc"));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(std::string(), GetShillServiceClient()->FindServiceMatchingGUID(
                               kTestGuidManagedCellular));
  service_path = GetShillServiceClient()->FindServiceMatchingGUID(
      kTestGuidManagedCellular2);
  const base::Value::Dict* properties2 =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties2);
  std::optional<bool> auto_connect =
      properties2->FindBool(shill::kAutoConnectProperty);
  ASSERT_TRUE(*auto_connect);
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyIgnoreNoSmdpManagedCellular) {
  InitializeStandardProfiles();
  InitializeEuicc();
  // Verify that applying managed eSIM policy with no SMDP address in the ONC
  // should not create a new shill configuration for it.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        "policy/policy_cellular_with_no_smdp.onc"));
  FastForwardProfileRefreshDelay();
  base::RunLoop().RunUntilIdle();
  std::string service_path = GetShillServiceClient()->FindServiceMatchingGUID(
      kTestGuidManagedCellular3);
  ASSERT_EQ(service_path, std::string());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyManageUnconfigured) {
  InitializeStandardProfiles();
  base::Value::Dict expected_shill_properties = test_utils::ReadTestDictionary(
      "policy/shill_policy_on_unconfigured_wifi1.json");

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(expected_shill_properties));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, EnableManagedCredentialsWiFi) {
  InitializeStandardProfiles();
  base::Value::Dict expected_shill_properties = test_utils::ReadTestDictionary(
      "policy/shill_policy_autoconnect_on_unconfigured_wifi1.json");

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1_autoconnect.onc"));
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(expected_shill_properties));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, EnableManagedCredentialsVPN) {
  InitializeStandardProfiles();
  base::Value::Dict expected_shill_properties = test_utils::ReadTestDictionary(
      "policy/shill_policy_autoconnect_on_unconfigured_vpn.json");

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_vpn_autoconnect.onc"));
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidVpn);
  ASSERT_FALSE(service_path.empty());
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_EQ(expected_shill_properties, *properties);
}

// Ensure that EAP settings for ethernet are matched with the right profile
// entry and written to the dedicated EthernetEAP service.
TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyManageUnmanagedEthernetEAP) {
  InitializeStandardProfiles();
  base::Value::Dict expected_shill_properties = test_utils::ReadTestDictionary(
      "policy/"
      "shill_policy_on_unmanaged_ethernet_eap.json");

  GetShillServiceClient()->AddService(
      "eth_entry", std::string() /* guid */, std::string() /* name */,
      "etherneteap", std::string() /* state */, true /* visible */);
  GetShillProfileClient()->AddService(kUser1ProfilePath, "eth_entry");
  SetUpEntry("policy/shill_unmanaged_ethernet_eap.json", kUser1ProfilePath,
             "eth_entry");

  // Also setup an unrelated WiFi configuration to verify that the right entry
  // is matched.
  GetShillServiceClient()->AddService(
      "wifi_entry", std::string() /* guid */, "wifi1", shill::kTypeWifi,
      std::string() /* state */, true /* visible */);
  SetUpEntry("policy/shill_unmanaged_wifi1.json", kUser1ProfilePath,
             "wifi_entry");

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_ethernet_eap.onc"));
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidEthernetEap);
  ASSERT_FALSE(service_path.empty());
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(expected_shill_properties));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyIgnoreUnmodified) {
  InitializeStandardProfiles();

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());

  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json", kUser1ProfilePath,
             "some_entry_path");

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, PolicyApplicationRunning) {
  InitializeStandardProfiles();

  EXPECT_FALSE(managed_handler()->IsAnyPolicyApplicationRunning());

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  managed_handler()->SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY,
                               /*userhash=*/std::string(),
                               /*network_configs_onc=*/base::Value::List(),
                               /*global_network_config=*/base::Value::Dict());

  EXPECT_TRUE(managed_handler()->IsAnyPolicyApplicationRunning());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(managed_handler()->IsAnyPolicyApplicationRunning());

  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json", kUser1ProfilePath,
             "some_entry_path");

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1_update.onc"));
  EXPECT_TRUE(managed_handler()->IsAnyPolicyApplicationRunning());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(managed_handler()->IsAnyPolicyApplicationRunning());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, UpdatePolicyAfterFinished) {
  InitializeStandardProfiles();

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());

  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json", kUser1ProfilePath,
             "some_entry_path");

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1_update.onc"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, UpdatePolicyBeforeFinished) {
  InitializeStandardProfiles();

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  // Usually the first call will cause a profile entry to be created, which we
  // don't fake here.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1_update.onc"));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());
}

// Regression test for b/240237232: A shill profile disappears before triggering
// policy application and the actual policy application run.
TEST_F(ManagedNetworkConfigurationHandlerTest,
       ProfileDisappearsAfterPolicySet) {
  InitializeStandardProfiles();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));

  // Pretend that NetworkProfileHandler doesn't know the network profile
  // anymore.
  network_profile_handler_->OnPropertyChanged(
      shill::kProfilesProperty, base::Value(base::Value::Type::LIST));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, policy_observer()->GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyManageUnmanaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json", kUser1ProfilePath,
             "old_entry_path");

  base::Value::Dict expected_shill_properties = test_utils::ReadTestDictionary(
      "policy/shill_policy_on_unmanaged_wifi1.json");

  // Before setting policy, old_entry_path should exist.
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  base::RunLoop().RunUntilIdle();

  // Verify old_entry_path is deleted.
  EXPECT_FALSE(GetShillProfileClient()->HasService("old_entry_path"));

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(expected_shill_properties));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUpdateManagedNewGUID) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_wifi1.json", kUser1ProfilePath,
             "old_entry_path");

  // Note that this test case expects that the UIData user settings are copied
  // to the entry with the new GUID.
  base::Value::Dict expected_shill_properties = test_utils::ReadTestDictionary(
      "policy/shill_policy_on_unmanaged_wifi1.json");

  // The passphrase isn't sent again, because it's configured by the user and
  // Shill doesn't send it on GetProperties calls.
  expected_shill_properties.Remove(shill::kPassphraseProperty);
  expected_shill_properties.Remove(shill::kPassphraseRequiredProperty);

  // Before setting policy, old_entry_path should exist.
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  base::RunLoop().RunUntilIdle();

  // Verify old_entry_path is deleted.
  EXPECT_FALSE(GetShillProfileClient()->HasService("old_entry_path"));

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(expected_shill_properties));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUpdateManagedVPN) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_vpn.json", kUser1ProfilePath, "entry_path");

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_vpn.onc"));
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidVpn);
  ASSERT_FALSE(service_path.empty());
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  base::Value::Dict expected_shill_properties =
      test_utils::ReadTestDictionary("policy/shill_policy_on_managed_vpn.json");
  EXPECT_EQ(expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyUpdateManagedVPNOpenVPNPlusUi) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_vpn.json", kUser1ProfilePath, "entry_path");

  // Apply a policy that does not provide an authentication type.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_vpn_no_auth.onc"));
  base::RunLoop().RunUntilIdle();

  // Apply additional configuration (e.g. from the UI). This includes password
  // and OTP which should be allowed when authentication type is not explicitly
  // set. See https://crbug.com/817617 for details.
  const NetworkState* network_state =
      network_state_handler_->GetNetworkStateFromGuid(kTestGuidVpn);
  ASSERT_TRUE(network_state);
  base::Value::Dict ui_config =
      test_utils::ReadTestDictionary("policy/policy_vpn_ui.json");
  managed_network_configuration_handler_->SetProperties(
      network_state->path(), ui_config, base::DoNothing(),
      base::BindOnce(&ErrorCallback));
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidVpn);
  ASSERT_FALSE(service_path.empty());
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  base::Value::Dict expected_shill_properties = test_utils::ReadTestDictionary(
      "policy/shill_policy_on_managed_vpn_plus_ui.json");
  EXPECT_EQ(expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyUpdateManagedVPNL2TPIPsecPlusUi) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_vpn_ipsec.json", kUser1ProfilePath,
             "entry_path");

  // Apply the VPN L2TP-IPsec policy that will be updated.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_vpn_ipsec.onc"));
  base::RunLoop().RunUntilIdle();

  // Update the VPN L2TP-IPsec policy.
  const NetworkState* network_state =
      network_state_handler_->GetNetworkStateFromGuid(kTestGuidVpn);
  ASSERT_TRUE(network_state);
  base::Value::Dict ui_config =
      test_utils::ReadTestDictionary("policy/policy_vpn_ipsec_ui.json");
  managed_network_configuration_handler_->SetProperties(
      network_state->path(), ui_config, base::DoNothing(),
      base::BindOnce(&ErrorCallback));
  base::RunLoop().RunUntilIdle();

  // Get shill service properties after the update.
  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidVpn);
  ASSERT_FALSE(service_path.empty());
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);

  base::Value::Dict expected_shill_properties = test_utils::ReadTestDictionary(
      "policy/shill_policy_on_managed_vpn_ipsec_plus_ui.json");
  EXPECT_EQ(expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyUpdateManagedVPNNoUserAuthType) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_vpn.json", kUser1ProfilePath, "entry_path");

  base::Value::Dict expected_shill_properties =
      test_utils::ReadTestDictionary("policy/shill_policy_on_managed_vpn.json");

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_vpn_no_user_auth_type.onc"));
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidVpn);
  ASSERT_FALSE(service_path.empty());
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_EQ(expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyReapplyToManaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json", kUser1ProfilePath,
             "old_entry_path");

  base::Value::Dict expected_shill_properties = test_utils::ReadTestDictionary(
      "policy/shill_policy_on_unmanaged_wifi1.json");

  // The passphrase isn't sent again, because it's configured by the user and
  // Shill doesn't send it on GetProperties calls.
  expected_shill_properties.Remove(shill::kPassphraseProperty);
  expected_shill_properties.Remove(shill::kPassphraseRequiredProperty);

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  base::RunLoop().RunUntilIdle();

  {
    std::string service_path =
        GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
    ASSERT_FALSE(service_path.empty());
    const base::Value::Dict* properties =
        GetShillServiceClient()->GetServiceProperties(service_path);
    ASSERT_TRUE(properties);
    EXPECT_THAT(*properties, DictionaryHasValues(expected_shill_properties));
  }

  // If we apply the policy again, without change, then the Shill profile will
  // not be modified.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  base::RunLoop().RunUntilIdle();

  {
    std::string service_path =
        GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
    ASSERT_FALSE(service_path.empty());
    const base::Value::Dict* properties =
        GetShillServiceClient()->GetServiceProperties(service_path);
    ASSERT_TRUE(properties);
    EXPECT_THAT(*properties, DictionaryHasValues(expected_shill_properties));
  }
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUnmanageManaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json", kUser1ProfilePath,
             "old_entry_path");

  // Before setting policy, old_entry_path should exist.
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        std::string() /* path_to_onc */));
  base::RunLoop().RunUntilIdle();

  // Verify old_entry_path is deleted.
  EXPECT_FALSE(GetShillProfileClient()->HasService("old_entry_path"));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetEmptyPolicyIgnoreUnmanaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json", kUser1ProfilePath,
             "old_entry_path");

  // Before setting policy, old_entry_path should exist.
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        std::string() /* path_to_onc */));
  base::RunLoop().RunUntilIdle();

  // Verify old_entry_path is kept.
  EXPECT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyIgnoreUnmanaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi2.json", kUser1ProfilePath,
             "wifi2_entry_path");

  base::Value::Dict expected_shill_properties = test_utils::ReadTestDictionary(
      "policy/shill_policy_on_unconfigured_wifi1.json");

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(expected_shill_properties));
}

// Regression test for b/237657704.
// Profile entries that don't have a "Profile" property don't break application
// of new policy-provided networks.
TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyIgnoreNetworkWithoutProfile) {
  InitializeStandardProfiles();

  // This shill entry is missing the "Profile" property.
  // It has a "wifi2" SSID.
  base::Value::Dict wifi_without_profile_property =
      base::test::ParseJsonDict(R"(
    {
      "AutoConnect": true,
      "GUID": "wifi2",
      "Mode": "managed",
      "Passphrase": "user's passphrase",
      "PassphraseRequired": false,
      "SecurityClass": "psk",
      "Type": "wifi",
      "WiFi.HexSSID": "7769666932"
    })");
  GetShillProfileClient()->AddEntry(kUser1ProfilePath,
                                    "wifi_without_profile_prop_entry_path",
                                    std::move(wifi_without_profile_property));

  // Apply a policy which:
  // - Disallows unmanaged networks (such as wifi2 above) to auto-connect
  //   This will trigger policy_applicator.cc to try to modify wifi2
  // - Apply a new network (policy_wifi1).
  const char* const onc_policy = R"(
    {
      "GlobalNetworkConfiguration": {
        "AllowOnlyPolicyNetworksToAutoconnect": true
      },
      "NetworkConfigurations": [
        {
          "GUID": "policy_wifi1",
          "Type": "WiFi",
          "Name": "Managed wifi1",
          "WiFi": {
            "HexSSID": "7769666931", // "wifi1"
            "Passphrase": "policy's passphrase",
            "SSID": "wifi1",
            "Security": "WPA-PSK"
          }
        }
      ],
      "Type": "UnencryptedConfiguration"
    })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        base::test::ParseJsonDict(onc_policy)));
  base::RunLoop().RunUntilIdle();

  // Expect that "policy_wifi1" policy has been applied by checking that the
  // GUID exists and it has properties from the above policy.
  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID("policy_wifi1");
  ASSERT_FALSE(service_path.empty());
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValue(shill::kWifiHexSsid,
                                              base::Value("7769666931")));
  EXPECT_THAT(*properties,
              DictionaryHasValue(shill::kPassphraseProperty,
                                 base::Value("policy's passphrase")));
}

// There is a policy with a Recommended field.
// The RecommendedValuesAreEphemeralAccessor policy is not enabled.
// Tests that initial policy application does not reset "Recommended" fields,
// even when `TriggerEphemeralNetworkConfigActions` is called.
TEST_F(ManagedNetworkConfigurationHandlerTest,
       ResetRecommendedFields_Disabled_Initial) {
  policy_util::SetEphemeralNetworkPoliciesEnabled();

  InitializeStandardProfiles();
  const std::string kOriginalEntryPath = "orig_entry_path";
  base::Value::Dict original_wifi_config = base::test::ParseJsonDict(R"(
    {
      "AutoConnect": true,
      "GUID": "guid_wifi1",
      "Mode": "managed",
      "EAP.EAP": "PEAP",
      "EAP.Identity": "user_identity",
      "EAP.Password": "user_password",
      "Profile": "/profile/default",
      "SecurityClass": "802_1x",
      "SaveCredentials": true,
      "Type": "wifi",
      "WiFi.HexSSID": "7769666931",
      "UIData": "{\"onc_source\":\"device_policy\"}"
    })");
  GetShillProfileClient()->AddEntry(
      NetworkProfileHandler::GetSharedProfilePath(), kOriginalEntryPath,
      std::move(original_wifi_config));

  // Call TriggerEphemeralNetworkConfigActions when policies are available
  // In production code, EphemeralNetworkConfigHandler will do this.
  policy_observer_.RunOnSharedProfilePoliciesChanged(base::BindOnce(
      &ManagedNetworkConfigurationHandlerImpl::
          TriggerEphemeralNetworkConfigActions,
      base::Unretained(managed_network_configuration_handler_.get())));

  const char* const onc_policy = R"(
    {
      "GlobalNetworkConfiguration": {
      },
      "NetworkConfigurations": [
        {
          "GUID": "guid_wifi1",
          "Type": "WiFi",
          "Name": "Managed wifi1",
          "WiFi": {
            "HexSSID": "7769666931", // "wifi1"
            "SSID": "wifi1",
            "Security": "WPA-EAP",
            "EAP": {
              "Outer": "PEAP",
              "Inner": "MSCHAPv2",
              "SaveCredentials": true,
              "Recommended": ["Identity", "Password"]
            }
          }
        }
      ],
      "Type": "UnencryptedConfiguration"
    })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy)));
  base::RunLoop().RunUntilIdle();

  // The entry still exists and has kept the user-provided Passphrase.
  std::string profile_path;
  std::optional<base::Value::Dict> resulting_properties =
      GetShillProfileClient()->GetService(kOriginalEntryPath, &profile_path);
  ASSERT_TRUE(resulting_properties);
  EXPECT_THAT(*resulting_properties,
              DictionaryHasValue(shill::kEapPasswordProperty,
                                 base::Value("user_password")));
}

// There is a policy with a Recommended field.
// The RecommendedValuesAreEphemeralAccessor policy is enabled.
// Tests that initial policy application resets "Recommended" fields by
// re-creating the configuration.
TEST_F(ManagedNetworkConfigurationHandlerTest,
       ResetRecommendedFields_Enabled_Initial) {
  policy_util::SetEphemeralNetworkPoliciesEnabled();

  InitializeStandardProfiles();
  const std::string kOriginalEntryPath = "orig_entry_path";
  base::Value::Dict original_wifi_config = base::test::ParseJsonDict(R"(
    {
      "AutoConnect": true,
      "GUID": "guid_wifi1",
      "Mode": "managed",
      "EAP.EAP": "PEAP",
      "EAP.Identity": "user_identity",
      "EAP.Password": "user_password",
      "Profile": "/profile/default",
      "SecurityClass": "802_1x",
      "SaveCredentials": true,
      "Type": "wifi",
      "WiFi.HexSSID": "7769666931",
      "UIData": "{\"onc_source\":\"device_policy\"}"
    })");
  GetShillProfileClient()->AddEntry(
      NetworkProfileHandler::GetSharedProfilePath(), kOriginalEntryPath,
      std::move(original_wifi_config));

  // Call TriggerEphemeralNetworkConfigActions when policies are available
  // In production code, EphemeralNetworkConfigHandler will do this.
  policy_observer_.RunOnSharedProfilePoliciesChanged(base::BindOnce(
      &ManagedNetworkConfigurationHandlerImpl::
          TriggerEphemeralNetworkConfigActions,
      base::Unretained(managed_network_configuration_handler_.get())));

  const char* const onc_policy = R"(
    {
      "GlobalNetworkConfiguration": {
        "RecommendedValuesAreEphemeral": true
      },
      "NetworkConfigurations": [
        {
          "GUID": "guid_wifi1",
          "Type": "WiFi",
          "Name": "Managed wifi1",
          "WiFi": {
            "HexSSID": "7769666931", // "wifi1"
            "SSID": "wifi1",
            "Security": "WPA-EAP",
            "EAP": {
              "Outer": "PEAP",
              "Inner": "MSCHAPv2",
              "SaveCredentials": true,
              "Recommended": ["Identity", "Password"]
            }
          }
        }
      ],
      "Type": "UnencryptedConfiguration"
    })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy)));
  base::RunLoop().RunUntilIdle();

  // The original entry has been wiped.
  EXPECT_FALSE(GetShillProfileClient()->HasService(kOriginalEntryPath));

  // A new one has been created.
  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID("guid_wifi1");
  ASSERT_FALSE(service_path.empty());
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(properties->FindString(shill::kEapPasswordProperty),
              testing::IsNull());
}

// There is a policy with a Recommended field.
// The RecommendedValuesAreEphemeralAccessor policy is enabled.
// Tests that a `TriggerEphemeralNetworkConfigActions` call triggered after the
// initial policy application leads to clearing of the Recommended fields by
// re-creating the configuration.
TEST_F(ManagedNetworkConfigurationHandlerTest,
       ResetRecommendedFields_Enabled_AfterInitialApplication) {
  policy_util::SetEphemeralNetworkPoliciesEnabled();

  const std::string kOncWifiGuid = "guid_wifi1";
  const std::string kTestPassword = "test_password";

  InitializeStandardProfiles();
  const std::string onc_policy = base::StringPrintf(R"(
    {
      "GlobalNetworkConfiguration": {
        "RecommendedValuesAreEphemeral": true
      },
      "NetworkConfigurations": [
        {
          "GUID": "%s",
          "Type": "WiFi",
          "Name": "Managed wifi1",
          "WiFi": {
            "HexSSID": "7769666931", // "wifi1"
            "SSID": "wifi1",
            "Security": "WPA-EAP",
            "EAP": {
              "Outer": "PEAP",
              "Inner": "MSCHAPv2",
              "SaveCredentials": true,
              "Recommended": ["Identity", "Password"]
            }
          }
        }
      ],
      "Type": "UnencryptedConfiguration"
    })",
                                                    kOncWifiGuid.c_str());
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy)));
  base::RunLoop().RunUntilIdle();

  // Set a recommended field.
  std::string initial_service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kOncWifiGuid);
  EXPECT_TRUE(GetShillServiceClient()->SetServiceProperty(
      initial_service_path, shill::kEapPasswordProperty,
      base::Value(kTestPassword)));

  managed_network_configuration_handler_
      ->TriggerEphemeralNetworkConfigActions();
  base::RunLoop().RunUntilIdle();

  // The config does not have the recommended field value anymore.
  std::string new_service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kOncWifiGuid);
  {
    const base::Value::Dict* properties =
        GetShillServiceClient()->GetServiceProperties(new_service_path);
    ASSERT_TRUE(properties);
    EXPECT_THAT(properties->FindString(shill::kEapPasswordProperty),
                testing::IsNull());
  }

  // Set a recommended field again.
  EXPECT_TRUE(GetShillServiceClient()->SetServiceProperty(
      new_service_path, shill::kEapPasswordProperty,
      base::Value(kTestPassword)));

  // Re-apply policy.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy)));
  base::RunLoop().RunUntilIdle();

  // The re-application of policy (without TriggerEphemeralNetworkConfigActions)
  // did not wipe the recommended field or re-create the entry.
  {
    const base::Value::Dict* properties =
        GetShillServiceClient()->GetServiceProperties(new_service_path);
    ASSERT_TRUE(properties);
    EXPECT_THAT(*properties, DictionaryHasValue(shill::kEapPasswordProperty,
                                                base::Value(kTestPassword)));
  }
}

// There is a policy with a Recommended field.
// The RecommendedValuesAreEphemeralAccessor policy is enabled.
// The feature flags/policies guarding it are however disabled.
TEST_F(ManagedNetworkConfigurationHandlerTest,
       ResetRecommendedFields_Enabled_FeatureOff) {
  InitializeStandardProfiles();
  const std::string kOriginalEntryPath = "orig_entry_path";
  base::Value::Dict original_wifi_config = base::test::ParseJsonDict(R"(
    {
      "AutoConnect": true,
      "GUID": "guid_wifi1",
      "Mode": "managed",
      "EAP.EAP": "PEAP",
      "EAP.Identity": "user_identity",
      "EAP.Password": "user_password",
      "Profile": "/profile/default",
      "SecurityClass": "802_1x",
      "SaveCredentials": true,
      "Type": "wifi",
      "WiFi.HexSSID": "7769666931",
      "UIData": "{\"onc_source\":\"device_policy\"}"
    })");
  GetShillProfileClient()->AddEntry(
      NetworkProfileHandler::GetSharedProfilePath(), kOriginalEntryPath,
      std::move(original_wifi_config));

  // Don't call TriggerEphemeralNetworkConfigActions - it will only be called in
  // production code if the feature is enabled.
  const char* const onc_policy = R"(
    {
      "GlobalNetworkConfiguration": {
        "RecommendedValuesAreEphemeral": true
      },
      "NetworkConfigurations": [
        {
          "GUID": "guid_wifi1",
          "Type": "WiFi",
          "Name": "Managed wifi1",
          "WiFi": {
            "HexSSID": "7769666931", // "wifi1"
            "SSID": "wifi1",
            "Security": "WPA-EAP",
            "EAP": {
              "Outer": "PEAP",
              "Inner": "MSCHAPv2",
              "SaveCredentials": true,
              "Recommended": ["Identity", "Password"]
            }
          }
        }
      ],
      "Type": "UnencryptedConfiguration"
    })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy)));
  base::RunLoop().RunUntilIdle();

  // The entry still exists and has kept the user-provided Passphrase.
  std::string profile_path;
  std::optional<base::Value::Dict> resulting_properties =
      GetShillProfileClient()->GetService(kOriginalEntryPath, &profile_path);
  ASSERT_TRUE(resulting_properties);
  EXPECT_THAT(*resulting_properties,
              DictionaryHasValue(shill::kEapPasswordProperty,
                                 base::Value("user_password")));
}

// There is a policy with no Recommended field.
// The RecommendedValuesAreEphemeralAccessor policy is enabled.
// Tests that initial policy application does not attempt to re-create the
// configuration (because no field in there is Recommended).
TEST_F(ManagedNetworkConfigurationHandlerTest,
       ResetRecommendedFields_Enabled_NoFieldRecommended_Initial) {
  policy_util::SetEphemeralNetworkPoliciesEnabled();

  InitializeStandardProfiles();
  const std::string kOriginalEntryPath = "orig_entry_path";
  base::Value::Dict original_wifi_config = base::test::ParseJsonDict(R"(
    {
      "AutoConnect": true,
      "GUID": "guid_wifi1",
      "Mode": "managed",
      "EAP.EAP": "PEAP",
      "EAP.Identity": "user_identity",
      "EAP.Password": "user_password",
      "Profile": "/profile/default",
      "SecurityClass": "802_1x",
      "SaveCredentials": true,
      "Type": "wifi",
      "WiFi.HexSSID": "7769666931",
      "UIData": "{\"onc_source\":\"device_policy\"}"
    })");
  GetShillProfileClient()->AddEntry(
      NetworkProfileHandler::GetSharedProfilePath(), kOriginalEntryPath,
      std::move(original_wifi_config));

  // Call TriggerEphemeralNetworkConfigActions when policies are available
  // In production code, EphemeralNetworkConfigHandler will do this.
  policy_observer_.RunOnSharedProfilePoliciesChanged(base::BindOnce(
      &ManagedNetworkConfigurationHandlerImpl::
          TriggerEphemeralNetworkConfigActions,
      base::Unretained(managed_network_configuration_handler_.get())));

  const char* const onc_policy = R"(
    {
      "GlobalNetworkConfiguration": {
        "RecommendedValuesAreEphemeral": true
      },
      "NetworkConfigurations": [
        {
          "GUID": "guid_wifi1",
          "Type": "WiFi",
          "Name": "Managed wifi1",
          "WiFi": {
            "HexSSID": "7769666931", // "wifi1"
            "SSID": "wifi1",
            "Security": "WPA-EAP",
            "EAP": {
              "Outer": "PEAP",
              "Inner": "MSCHAPv2",
              "SaveCredentials": true,
              "Identity": "user_identity",
              "Password": "user_password"
            }
          }
        }
      ],
      "Type": "UnencryptedConfiguration"
    })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy)));
  base::RunLoop().RunUntilIdle();

  // The original entry has been preserved.
  EXPECT_TRUE(GetShillProfileClient()->HasService(kOriginalEntryPath));
}

// There is an unmanaged entry.
// The UserCreatedNetworkConfigurationsAreEphemeral policy is not enabled.
// Tests that initial policy application does not delete the entry even when
// `TriggerEphemeralNetworkConfigActions` is called.
TEST_F(ManagedNetworkConfigurationHandlerTest,
       RemoveUnmanagedConfigs_Disabled_Initial) {
  policy_util::SetEphemeralNetworkPoliciesEnabled();

  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json",
             NetworkProfileHandler::GetSharedProfilePath(), "old_entry_path");
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  policy_observer_.RunOnSharedProfilePoliciesChanged(base::BindOnce(
      &ManagedNetworkConfigurationHandlerImpl::
          TriggerEphemeralNetworkConfigActions,
      base::Unretained(managed_network_configuration_handler_.get())));

  const char* const onc_policy = R"(
    {
      "GlobalNetworkConfiguration": {
      },
      "Type": "UnencryptedConfiguration"
    })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy)));
  base::RunLoop().RunUntilIdle();

  // The entry still exists.
  EXPECT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));
}

// There is an unmanaged entry.
// The UserCreatedNetworkConfigurationsAreEphemeral policy is enabled.
// Tests that initial policy application deletes the entry when
// `TriggerEphemeralNetworkConfigActions` is called.
TEST_F(ManagedNetworkConfigurationHandlerTest,
       RemoveUnmanagedConfigs_Enabled_Initial) {
  policy_util::SetEphemeralNetworkPoliciesEnabled();

  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json",
             NetworkProfileHandler::GetSharedProfilePath(), "old_entry_path");

  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  policy_observer_.RunOnSharedProfilePoliciesChanged(base::BindOnce(
      &ManagedNetworkConfigurationHandlerImpl::
          TriggerEphemeralNetworkConfigActions,
      base::Unretained(managed_network_configuration_handler_.get())));

  const char* const onc_policy = R"(
    {
      "GlobalNetworkConfiguration": {
        "UserCreatedNetworkConfigurationsAreEphemeral": true
      },
      "Type": "UnencryptedConfiguration"
    })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy)));
  base::RunLoop().RunUntilIdle();

  // The entry has been removed.
  EXPECT_FALSE(GetShillProfileClient()->HasService("old_entry_path"));
}

// There is an unmanaged entry.
// The UserCreatedNetworkConfigurationsAreEphemeral policy is enabled.
// The feature flags/policies guarding it are however disabled.
TEST_F(ManagedNetworkConfigurationHandlerTest,
       RemoveUnmanagedConfigs_Enabled_FeatureOff) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json",
             NetworkProfileHandler::GetSharedProfilePath(), "old_entry_path");

  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  // Don't call TriggerEphemeralNetworkConfigActions - it will only be called in
  // production code if the feature is enabled.

  const char* const onc_policy = R"(
    {
      "GlobalNetworkConfiguration": {
        "UserCreatedNetworkConfigurationsAreEphemeral": true
      },
      "Type": "UnencryptedConfiguration"
    })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy)));
  base::RunLoop().RunUntilIdle();

  // The entry is still there
  EXPECT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));
}

// There is an unmanaged entry.
// The UserCreatedNetworkConfigurationsAreEphemeral policy is enabled.
// Tests that a `TriggerEphemeralNetworkConfigActions` call triggered after the
// initial policy application leads to deletion of the unmanaged entry.
TEST_F(ManagedNetworkConfigurationHandlerTest,
       RemoveUnmanagedConfigs_Enabled_AfterInitialApplication) {
  policy_util::SetEphemeralNetworkPoliciesEnabled();

  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json",
             NetworkProfileHandler::GetSharedProfilePath(), "old_entry_path");
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  const char* const onc_policy = R"(
    {
      "GlobalNetworkConfiguration": {
        "UserCreatedNetworkConfigurationsAreEphemeral": true
      },
      "Type": "UnencryptedConfiguration"
    })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy)));
  base::RunLoop().RunUntilIdle();

  // The entry is still there.
  EXPECT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  managed_network_configuration_handler_
      ->TriggerEphemeralNetworkConfigActions();
  base::RunLoop().RunUntilIdle();

  // The entry has been removed.
  EXPECT_FALSE(GetShillProfileClient()->HasService("old_entry_path"));

  // Re-create it and test that re-applying policies does not trigger the
  // "ephemeral network config" actions.
  SetUpEntry("policy/shill_unmanaged_wifi1.json", kUser1ProfilePath,
             "old_entry_path");
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy)));
  base::RunLoop().RunUntilIdle();

  // The entry is still there
  EXPECT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));
}

// There is a policy with a Recommended field.
// The RecommendedValuesAreEphemeralAccessor policy is enabled.
// Tests that initial policy application does not reset "Recommended" fields,
// if `TriggerEphemeralNetworkConfigActions` is not called.
TEST_F(ManagedNetworkConfigurationHandlerTest,
       NoEphemeralNetworkConfigActionsTriggered) {
  // Don't call `TriggerEphemeralNetworkConfigActions`.
  const std::string original_entry_path = "orig_entry_path";
  base::Value::Dict original_wifi_config = base::test::ParseJsonDict(R"(
    {
      "AutoConnect": true,
      "GUID": "guid_wifi1",
      "Mode": "managed",
      "EAP.EAP": "PEAP",
      "EAP.Identity": "user_identity",
      "EAP.Password": "user_password",
      "Profile": "/profile/default",
      "SecurityClass": "802_1x",
      "SaveCredentials": true,
      "Type": "wifi",
      "WiFi.HexSSID": "7769666931",
      "UIData": "{\"onc_source\":\"device_policy\"}"
    })");
  GetShillProfileClient()->AddEntry(
      NetworkProfileHandler::GetSharedProfilePath(), original_entry_path,
      std::move(original_wifi_config));

  const char* const onc_policy = R"(
    {
      "GlobalNetworkConfiguration": {
      },
      "NetworkConfigurations": [
        {
          "GUID": "guid_wifi1",
          "Type": "WiFi",
          "Name": "Managed wifi1",
          "WiFi": {
            "HexSSID": "7769666931", // "wifi1"
            "SSID": "wifi1",
            "Security": "WPA-EAP",
            "EAP": {
              "Outer": "PEAP",
              "Inner": "MSCHAPv2",
              "SaveCredentials": true,
              "Recommended": ["Identity", "Password"]
            }
          }
        }
      ],
      "Type": "UnencryptedConfiguration"
    })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy)));
  base::RunLoop().RunUntilIdle();

  // The entry still exists and has kept the user-provided Passphrase.
  std::string profile_path;
  std::optional<base::Value::Dict> resulting_properties =
      GetShillProfileClient()->GetService(original_entry_path, &profile_path);
  ASSERT_TRUE(resulting_properties);
  EXPECT_THAT(*resulting_properties,
              DictionaryHasValue(shill::kEapPasswordProperty,
                                 base::Value("user_password")));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, AutoConnectDisallowed) {
  InitializeStandardProfiles();

  // Setup an unmanaged network.
  SetUpEntry("policy/shill_unmanaged_wifi2.json", kUser1ProfilePath,
             "wifi2_entry_path");

  base::Value::Dict expected_shill_properties = test_utils::ReadTestDictionary(
      "policy/shill_disallow_autoconnect_on_unmanaged_wifi2.json");

  // Apply the user policy with global autoconnect config and expect that
  // autoconnect is disabled in the network's profile entry.
  EXPECT_TRUE(
      SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                "policy/policy_allow_only_policy_networks_to_autoconnect.onc"));
  base::RunLoop().RunUntilIdle();

  std::string wifi2_service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidUnmanagedWifi2);
  ASSERT_FALSE(wifi2_service_path.empty());
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(wifi2_service_path);
  ASSERT_TRUE(properties);
  EXPECT_TRUE(PropertiesMatch(expected_shill_properties, *properties));

  // Verify that GetManagedProperties correctly augments the properties with the
  // global config from the user policy.
  // GetManagedProperties requires the device policy to be set or explicitly
  // unset.
  managed_handler()->SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY,
                               /*userhash=*/std::string(),
                               /*network_configs_onc=*/base::Value::List(),
                               /*global_network_config=*/base::Value::Dict());

  base::RunLoop get_properties_run_loop;
  std::optional<base::Value::Dict> dictionary;
  managed_handler()->GetManagedProperties(
      kUser1, wifi2_service_path,
      base::BindOnce(
          [](std::optional<base::Value::Dict>* dictionary_out,
             base::RepeatingClosure quit_closure,
             const std::string& service_path,
             std::optional<base::Value::Dict> dictionary,
             std::optional<std::string> error) {
            if (dictionary) {
              *dictionary_out = std::move(*dictionary);
            } else {
              FAIL();
            }
            quit_closure.Run();
          },
          &dictionary, get_properties_run_loop.QuitClosure()));

  get_properties_run_loop.Run();

  ASSERT_TRUE(dictionary.has_value());
  base::Value::Dict expected_managed_onc = test_utils::ReadTestDictionary(
      "policy/"
      "managed_onc_disallow_autoconnect_on_unmanaged_wifi2.onc");
  EXPECT_TRUE(PropertiesMatch(expected_managed_onc, dictionary.value()));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, LateProfileLoading) {
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  base::RunLoop().RunUntilIdle();

  base::Value::Dict expected_shill_properties = test_utils::ReadTestDictionary(
      "policy/shill_policy_on_unconfigured_wifi1.json");

  InitializeStandardProfiles();
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(expected_shill_properties));
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       ShutdownDuringPolicyApplication) {
  InitializeStandardProfiles();

  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));

  // Reset the network configuration manager after setting policy and before
  // calling RunUntilIdle to simulate shutdown during policy application.
  ResetManagedNetworkConfigurationHandler();
  base::RunLoop().RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, AllowOnlyPolicyWiFiToConnect) {
  InitializeStandardProfiles();

  // Check transfer to NetworkStateHandler
  EXPECT_CALL(
      *network_state_handler_,
      UpdateBlockedWifiNetworks(true, false, std::vector<std::string>()))
      .Times(1);

  // Set 'AllowOnlyPolicyWiFiToConnect' policy and another arbitrary user
  // policy.
  EXPECT_TRUE(
      SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                "policy/policy_allow_only_policy_networks_to_connect.onc"));
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_TRUE(managed_handler()->AllowCellularSimLock());
  EXPECT_TRUE(managed_handler()->AllowCellularHotspot());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_TRUE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_TRUE(managed_handler()->GetBlockedHexSSIDs().empty());
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       AllowOnlyPolicyWiFiToConnectIfAvailable) {
  InitializeStandardProfiles();

  // Check transfer to NetworkStateHandler
  EXPECT_CALL(
      *network_state_handler_,
      UpdateBlockedWifiNetworks(false, true, std::vector<std::string>()))
      .Times(1);

  // Set 'AllowOnlyPolicyWiFiToConnectIfAvailable' policy and another
  // arbitrary user policy.
  EXPECT_TRUE(SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
      "policy/"
      "policy_allow_only_policy_networks_to_connect_if_available.onc"));
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_TRUE(managed_handler()->AllowCellularSimLock());
  EXPECT_TRUE(managed_handler()->AllowCellularHotspot());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_TRUE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_TRUE(managed_handler()->GetBlockedHexSSIDs().empty());
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       AllowOnlyPolicyNetworksToAutoconnect) {
  InitializeStandardProfiles();

  // Check transfer to NetworkStateHandler
  EXPECT_CALL(
      *network_state_handler_,
      UpdateBlockedWifiNetworks(false, false, std::vector<std::string>()))
      .Times(1);

  // Set 'AllowOnlyPolicyNetworksToAutoconnect' policy and another arbitrary
  // user policy.
  EXPECT_TRUE(
      SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                "policy/policy_allow_only_policy_networks_to_autoconnect.onc"));
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_TRUE(managed_handler()->AllowCellularSimLock());
  EXPECT_TRUE(managed_handler()->AllowCellularHotspot());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_TRUE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_TRUE(managed_handler()->GetBlockedHexSSIDs().empty());
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       AllowOnlyPolicyCellularNetworksToConnect) {
  InitializeStandardProfiles();
  InitializeEuicc();

  // Check transfer to NetworkStateHandler.
  EXPECT_CALL(*network_state_handler_, UpdateBlockedCellularNetworks(true))
      .Times(1);

  // Set 'AllowOnlyPolicyCellularNetworks' policy.
  EXPECT_TRUE(
      SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                "policy/policy_allow_only_policy_cellular_networks.onc"));
  FastForwardProfileRefreshDelay();
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_TRUE(managed_handler()->AllowCellularSimLock());
  EXPECT_TRUE(managed_handler()->AllowCellularHotspot());
  EXPECT_TRUE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_TRUE(managed_handler()->GetBlockedHexSSIDs().empty());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, DisconnectWiFiOnEthernet) {
  policy_util::SetEphemeralNetworkPoliciesEnabled();

  const char* const onc_policy_connected = R"(
      {
        "GlobalNetworkConfiguration": {
          "DisconnectWiFiOnEthernet": "WhenConnected"
        },
        "Type": "UnencryptedConfiguration"
      })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy_connected)));
  FastForwardProfileRefreshDelay();
  base::RunLoop().RunUntilIdle();
  auto properties =
      ShillManagerClient::Get()->GetTestInterface()->GetStubProperties();
  EXPECT_NE(properties.FindString(shill::kDisconnectWiFiOnEthernetProperty),
            nullptr);
  EXPECT_EQ(*properties.FindString(shill::kDisconnectWiFiOnEthernetProperty),
            std::string(shill::kDisconnectWiFiOnEthernetConnected));

  // Unknown policy value should reset property value to Off.
  const char* const onc_policy_invalid = R"(
      {
        "GlobalNetworkConfiguration": {
          "DisconnectWiFiOnEthernet": "Unknown"
        },
        "Type": "UnencryptedConfiguration"
      })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy_invalid)));
  FastForwardProfileRefreshDelay();
  base::RunLoop().RunUntilIdle();
  properties =
      ShillManagerClient::Get()->GetTestInterface()->GetStubProperties();
  EXPECT_NE(properties.FindString(shill::kDisconnectWiFiOnEthernetProperty),
            nullptr);
  EXPECT_EQ(*properties.FindString(shill::kDisconnectWiFiOnEthernetProperty),
            std::string(shill::kDisconnectWiFiOnEthernetOff));

  const char* const onc_policy_online = R"(
      {
        "GlobalNetworkConfiguration": {
          "DisconnectWiFiOnEthernet": "WhenOnline"
        },
        "Type": "UnencryptedConfiguration"
      })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy_online)));
  FastForwardProfileRefreshDelay();
  base::RunLoop().RunUntilIdle();
  properties =
      ShillManagerClient::Get()->GetTestInterface()->GetStubProperties();
  EXPECT_NE(properties.FindString(shill::kDisconnectWiFiOnEthernetProperty),
            nullptr);
  EXPECT_EQ(*properties.FindString(shill::kDisconnectWiFiOnEthernetProperty),
            std::string(shill::kDisconnectWiFiOnEthernetOnline));

  // Field not existing in policy should leave property value unchanged.
  const char* const onc_policy_off = R"(
      {
        "GlobalNetworkConfiguration": {},
        "Type": "UnencryptedConfiguration"
      })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy_off)));
  FastForwardProfileRefreshDelay();
  base::RunLoop().RunUntilIdle();
  properties =
      ShillManagerClient::Get()->GetTestInterface()->GetStubProperties();
  EXPECT_NE(properties.FindString(shill::kDisconnectWiFiOnEthernetProperty),
            nullptr);
  EXPECT_EQ(*properties.FindString(shill::kDisconnectWiFiOnEthernetProperty),
            std::string(shill::kDisconnectWiFiOnEthernetOnline));
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       RecommendedValuesAreEphemeralAccessor) {
  policy_util::SetEphemeralNetworkPoliciesEnabled();

  EXPECT_FALSE(managed_handler()->RecommendedValuesAreEphemeral());

  const char* const onc_policy = R"(
      {
        "GlobalNetworkConfiguration": {
          "RecommendedValuesAreEphemeral": true
        },
        "Type": "UnencryptedConfiguration"
      })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy)));
  FastForwardProfileRefreshDelay();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(managed_handler()->RecommendedValuesAreEphemeral());
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       UserCreatedNetworkConfigurationsAreEphemeral) {
  policy_util::SetEphemeralNetworkPoliciesEnabled();

  EXPECT_FALSE(
      managed_handler()->UserCreatedNetworkConfigurationsAreEphemeral());

  const char* const onc_policy = R"(
      {
        "GlobalNetworkConfiguration": {
          "UserCreatedNetworkConfigurationsAreEphemeral": true
        },
        "Type": "UnencryptedConfiguration"
      })";
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        base::test::ParseJsonDict(onc_policy)));
  FastForwardProfileRefreshDelay();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      managed_handler()->UserCreatedNetworkConfigurationsAreEphemeral());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, AllowApnModification) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(/*enabled_features=*/
                                       {features::kApnRevamp,
                                        features::kAllowApnModificationPolicy},
                                       /*disabled_features=*/{});

  // TODO(b/333100319): When feature is fully enabled, test
  // AllowApnModification() in other unit tests to be consistent.
  EXPECT_TRUE(managed_handler()->AllowApnModification());

  // Set 'AllowApnModification' policy.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        "policy/policy_allow_apn_modification.onc"));
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_FALSE(managed_handler()->AllowApnModification());
  EXPECT_TRUE(managed_handler()->AllowCellularHotspot());
  EXPECT_TRUE(managed_handler()->AllowCellularSimLock());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_TRUE(managed_handler()->GetBlockedHexSSIDs().empty());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, AllowCellularSimLock) {
  // Set 'AllowCellularSimLock' policy.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        "policy/policy_allow_cellular_sim_lock.onc"));
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_TRUE(managed_handler()->AllowCellularHotspot());
  EXPECT_FALSE(managed_handler()->AllowCellularSimLock());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_TRUE(managed_handler()->GetBlockedHexSSIDs().empty());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, AllowTextMessages) {
  EXPECT_TRUE(
      SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                "policy/policy_empty_global_network_configuration.onc"));
  // Check that the field returns Unset when it isn't set.
  EXPECT_EQ(managed_handler()->GetAllowTextMessages(),
            PolicyTextMessageSuppressionState::kUnset);

  // Set 'AllowTextMessages' policy to Suppress.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        "policy/policy_allow_text_messages_suppress.onc"));

  // Check that the field is updated to Suppress.
  EXPECT_EQ(managed_handler()->GetAllowTextMessages(),
            PolicyTextMessageSuppressionState::kSuppress);

  // Set 'AllowTextMessages' policy to Unset.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        "policy/policy_allow_text_messages_unset.onc"));

  // Check that the field is updated to Unset.
  EXPECT_EQ(managed_handler()->GetAllowTextMessages(),
            PolicyTextMessageSuppressionState::kUnset);

  // Set 'AllowTextMessages' policy to Allow.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        "policy/policy_allow_text_messages_allow.onc"));

  // Check that the field is updated to Allow.
  EXPECT_EQ(managed_handler()->GetAllowTextMessages(),
            PolicyTextMessageSuppressionState::kAllow);

  // Check other ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_TRUE(managed_handler()->AllowCellularSimLock());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_TRUE(managed_handler()->GetBlockedHexSSIDs().empty());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, AllowCellularHotspot) {
  // Set 'AllowCellularHotspot' policy.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        "policy/policy_allow_cellular_hotspot.onc"));
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_FALSE(managed_handler()->AllowCellularHotspot());
  EXPECT_TRUE(managed_handler()->AllowCellularSimLock());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_TRUE(managed_handler()->GetBlockedHexSSIDs().empty());
}

// Test deprecated BlacklistedHexSSIDs property.
TEST_F(ManagedNetworkConfigurationHandlerTest, GetBlacklistedHexSSIDs) {
  InitializeStandardProfiles();
  std::vector<std::string> blocked = {"476F6F676C65477565737450534B"};

  // Check transfer to NetworkStateHandler
  EXPECT_CALL(*network_state_handler_,
              UpdateBlockedWifiNetworks(false, false, blocked))
      .Times(1);

  // Set 'BlacklistedHexSSIDs' policy and another arbitrary user policy.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        "policy/policy_deprecated_blacklisted_hex_ssids.onc"));
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_TRUE(managed_handler()->AllowCellularSimLock());
  EXPECT_TRUE(managed_handler()->AllowCellularHotspot());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_EQ(blocked, managed_handler()->GetBlockedHexSSIDs());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, GetBlockedHexSSIDs) {
  InitializeStandardProfiles();
  std::vector<std::string> blocked = {"476F6F676C65477565737450534B"};

  // Check transfer to NetworkStateHandler
  EXPECT_CALL(*network_state_handler_,
              UpdateBlockedWifiNetworks(false, false, blocked))
      .Times(1);

  // Set 'BlockedHexSSIDs' policy and another arbitrary user policy.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                        "policy/policy_blocked_hex_ssids.onc"));
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_TRUE(managed_handler()->AllowCellularSimLock());
  EXPECT_TRUE(managed_handler()->AllowCellularHotspot());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_EQ(blocked, managed_handler()->GetBlockedHexSSIDs());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, WipeGlobalNetworkConfiguration) {
  InitializeStandardProfiles();

  // A user policy must be present to apply some global config, e.g. blocked
  // SSIDs, even though they are actually given in device policy. It does not
  // really matter which user policy is configured for this test.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1.onc"));

  // Step 1: Apply a device policy which sets all possible entries in
  // GlobalNetworkConfiguration.
  EXPECT_CALL(*network_state_handler_,
              UpdateBlockedWifiNetworks(
                  /*only_managed=*/true, /*available_only=*/true,
                  std::vector<std::string>({"blocked_ssid"})))
      .Times(1);

  EXPECT_TRUE(
      SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                "policy/policy_exhaustive_global_network_configuration.onc"));
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(network_state_handler_.get());
  EXPECT_TRUE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_TRUE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_TRUE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_TRUE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_THAT(managed_handler()->GetBlockedHexSSIDs(),
              testing::ElementsAre("blocked_ssid"));
  // TODO(b/219568567): Also test that DisableNetworkTypes are propagated to
  // ProhibitedTechnologiesHandler.

  // Step 2: Now apply a device policy with an empty GlobalNetworkConfiguration.
  EXPECT_CALL(*network_state_handler_,
              UpdateBlockedWifiNetworks(
                  /*only_managed=*/false, /*available_only=*/false,
                  std::vector<std::string>()))
      .Times(1);
  EXPECT_TRUE(
      SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                "policy/policy_empty_global_network_configuration.onc"));
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(network_state_handler_.get());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_THAT(managed_handler()->GetBlockedHexSSIDs(), testing::IsEmpty());
}

// Proxy settings can come from different sources. Proxy enforced by user policy
// (provided by kProxy prefence) should have precedence over configurations set
// by ONC policy. This test verifies that the order of preference is respected.
TEST_F(ManagedNetworkConfigurationHandlerTest, ActiveProxySettingsPreference) {
  // Configure network.
  InitializeStandardProfiles();
  GetShillServiceClient()->AddService(
      "wifi_entry", std::string() /* guid */, "wifi1", shill::kTypeWifi,
      std::string() /* state */, true /* visible */);

  // Use proxy configured network.
  EXPECT_TRUE(SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
                        "policy/policy_wifi1_proxy.onc"));
  base::RunLoop().RunUntilIdle();

  std::string wifi_service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(wifi_service_path.empty());

  const base::Value::Dict* properties =
      GetShillServiceClient()->GetServiceProperties(wifi_service_path);
  ASSERT_TRUE(properties);

  managed_handler()->SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY,
                               /*userhash=*/std::string(),
                               /*network_configs_onc=*/base::Value::List(),
                               /*global_network_config=*/base::Value::Dict());

  std::optional<base::Value::Dict> dictionary_before_pref;
  std::optional<base::Value::Dict> dictionary_after_pref;

  base::RunLoop get_initial_properties_run_loop;
  // Get properties and verify that proxy is used.
  managed_handler()->GetManagedProperties(
      kUser1, wifi_service_path,
      base::BindOnce(
          [](std::optional<base::Value::Dict>* dictionary_out,
             base::RepeatingClosure quit_closure,
             const std::string& service_path,
             std::optional<base::Value::Dict> dictionary,
             std::optional<std::string> error) {
            if (dictionary) {
              *dictionary_out = std::move(*dictionary);
            } else {
              ADD_FAILURE() << error.value_or("Failed");
            }
            quit_closure.Run();
          },
          &dictionary_before_pref,
          get_initial_properties_run_loop.QuitClosure()));

  get_initial_properties_run_loop.Run();

  std::string* policy_before_pref =
      dictionary_before_pref->FindStringByDottedPath(
          "ProxySettings.Type.UserPolicy");
  ASSERT_TRUE(dictionary_before_pref.has_value());
  ASSERT_EQ(*policy_before_pref, "PAC");

  // Set pref not to use proxy.
  user_prefs_.SetManagedPref(proxy_config::prefs::kProxy,
                             ProxyConfigDictionary::CreateDirect());

  base::RunLoop get_merged_properties_run_loop;
  // Fetch managed properties after preference is set.
  managed_handler()->GetManagedProperties(
      kUser1, wifi_service_path,
      base::BindOnce(
          [](std::optional<base::Value::Dict>* dictionary_out,
             base::RepeatingClosure quit_closure,
             const std::string& service_path,
             std::optional<base::Value::Dict> dictionary,
             std::optional<std::string> error) {
            if (dictionary) {
              *dictionary_out = std::move(*dictionary);
            } else {
              ADD_FAILURE() << error.value_or("Failed");
            }
            quit_closure.Run();
          },
          &dictionary_after_pref,
          get_merged_properties_run_loop.QuitClosure()));

  get_merged_properties_run_loop.Run();

  std::string* policy_after_pref =
      dictionary_after_pref->FindStringByDottedPath(
          "ProxySettings.Type.UserPolicy");

  ASSERT_TRUE(dictionary_after_pref.has_value());
  ASSERT_NE(dictionary_before_pref, dictionary_after_pref);
  ASSERT_EQ(*policy_after_pref, "Direct");
}

TEST_F(ManagedNetworkConfigurationHandlerTest, IsProhibitedFromConfiguringVpn) {
  arc::prefs::RegisterProfilePrefs(user_prefs_.registry());
  user_prefs_.registry()->RegisterBooleanPref(prefs::kVpnConfigAllowed, true);

  for (const std::string& package_name : {"", "package_name"}) {
    for (const bool vpn_configure_allowed : {true, false}) {
      SetArcAlwaysOnUserPrefs(package_name, vpn_configure_allowed);
      if (package_name.empty() || vpn_configure_allowed) {
        EXPECT_FALSE(managed_network_configuration_handler_
                         ->IsProhibitedFromConfiguringVpn());
        continue;
      }
      EXPECT_TRUE(managed_network_configuration_handler_
                      ->IsProhibitedFromConfiguringVpn());
    }
  }
}

}  // namespace ash
