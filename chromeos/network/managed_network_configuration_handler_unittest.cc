// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/managed_network_configuration_handler_impl.h"
#include "chromeos/network/mock_network_state_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_policy_observer.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/onc/onc_validator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace test_utils = ::chromeos::onc::test_utils;
using base::test::DictionaryHasValues;

namespace chromeos {

namespace {

constexpr char kUser1[] = "user1";
constexpr char kUser1ProfilePath[] = "/profile/user1/shill";

// The GUID used by chromeos/test/data/network/policy/*.{json,onc} files for a
// VPN.
constexpr char kTestGuidVpn[] = "{a3860e83-f03d-4cb1-bafa-b22c9e746950}";

// The GUID used by chromeos/test/data/network/policy/*.{json,onc} files for a
// managed Wifi service.
constexpr char kTestGuidManagedWifi[] = "policy_wifi1";

// The GUID used by chromeos/test/data/network/policy/*.{json,onc} files for an
// unmanaged Wifi service.
constexpr char kTestGuidUnmanagedWifi2[] = "wifi2";

// The GUID used by chromeos/test/data/network/policy/*.{json,onc} files for a
// Wifi service.
constexpr char kTestGuidEthernetEap[] = "policy_ethernet_eap";

std::string PrettyJson(const base::DictionaryValue& value) {
  std::string pretty;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &pretty);
  return pretty;
}

void ErrorCallback(const std::string& error_name,
                   std::unique_ptr<base::DictionaryValue> error_data) {
  ADD_FAILURE() << "Unexpected error: " << error_name
                << " with associated data: \n"
                << PrettyJson(*error_data);
}

class TestNetworkProfileHandler : public NetworkProfileHandler {
 public:
  TestNetworkProfileHandler() {
    Init();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNetworkProfileHandler);
};

class TestNetworkPolicyObserver : public NetworkPolicyObserver {
 public:
  TestNetworkPolicyObserver() = default;

  void PoliciesApplied(const std::string& userhash) override {
    policies_applied_count_++;
  }

  int GetPoliciesAppliedCountAndReset() {
    int count = policies_applied_count_;
    policies_applied_count_ = 0;
    return count;
  }

 private:
  int policies_applied_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkPolicyObserver);
};

}  // namespace

class ManagedNetworkConfigurationHandlerTest : public testing::Test {
 public:
  ManagedNetworkConfigurationHandlerTest() {
    shill_clients::InitializeFakes();

    network_state_handler_ = MockNetworkStateHandler::InitializeForTest();
    network_profile_handler_ = std::make_unique<TestNetworkProfileHandler>();
    network_configuration_handler_.reset(
        NetworkConfigurationHandler::InitializeForTest(
            network_state_handler_.get(),
            nullptr /* no NetworkDeviceHandler */));

    // ManagedNetworkConfigurationHandlerImpl's ctor is private.
    managed_network_configuration_handler_.reset(
        new ManagedNetworkConfigurationHandlerImpl());
    managed_network_configuration_handler_->Init(
        network_state_handler_.get(), network_profile_handler_.get(),
        network_configuration_handler_.get(), nullptr /* no DeviceHandler */,
        nullptr /* no ProhibitedTechnologiesHandler */);
    managed_network_configuration_handler_->AddObserver(&policy_observer_);

    base::RunLoop().RunUntilIdle();
  }

  ~ManagedNetworkConfigurationHandlerTest() override {
    network_state_handler_->Shutdown();
    ResetManagedNetworkConfigurationHandler();
    network_configuration_handler_.reset();
    network_profile_handler_.reset();
    network_state_handler_.reset();
    shill_clients::Shutdown();
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

  void SetPolicy(::onc::ONCSource onc_source,
                 const std::string& userhash,
                 const std::string& path_to_onc) {
    std::unique_ptr<base::DictionaryValue> policy =
        path_to_onc.empty()
            ? base::DictionaryValue::From(onc::ReadDictionaryFromJson(
                  onc::kEmptyUnencryptedConfiguration))
            : test_utils::ReadTestDictionary(path_to_onc);

    base::ListValue validated_network_configs;
    const base::Value* found_network_configs =
        policy->FindKeyOfType(::onc::toplevel_config::kNetworkConfigurations,
                              base::Value::Type::LIST);
    if (found_network_configs) {
      for (const auto& network_config : found_network_configs->GetList()) {
        onc::Validator validator(true,    // error_on_unknown_field
                                 true,    // error_on_wrong_recommended
                                 false,   // error_on_missing_field
                                 true,    // managed_onc
                                 false);  // log_warnings
        validator.SetOncSource(onc_source);
        onc::Validator::Result validation_result;
        std::unique_ptr<base::DictionaryValue> validated_network_config =
            validator.ValidateAndRepairObject(
                &onc::kNetworkConfigurationSignature, network_config,
                &validation_result);
        if (validation_result == onc::Validator::INVALID) {
          ADD_FAILURE() << "Network configuration invalid.";
          return;
        }
        validated_network_configs.Append(
            std::move(*(validated_network_config)));
      }
    }

    base::DictionaryValue global_config;
    const base::Value* found_global_config = policy->FindKeyOfType(
        ::onc::toplevel_config::kGlobalNetworkConfiguration,
        base::Value::Type::DICTIONARY);
    if (found_global_config) {
      global_config = std::move(*base::DictionaryValue::From(
          base::Value::ToUniquePtrValue(found_global_config->Clone())));
    }

    managed_network_configuration_handler_->SetPolicy(
        onc_source, userhash, validated_network_configs, global_config);
  }

  void SetUpEntry(const std::string& path_to_shill_json,
                  const std::string& profile_path,
                  const std::string& entry_path) {
    std::unique_ptr<base::DictionaryValue> entry =
        test_utils::ReadTestDictionary(path_to_shill_json);
    GetShillProfileClient()->AddEntry(profile_path, entry_path, *entry);
  }

  void ResetManagedNetworkConfigurationHandler() {
    if (!managed_network_configuration_handler_)
      return;
    managed_network_configuration_handler_->RemoveObserver(&policy_observer_);
    managed_network_configuration_handler_.reset();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  TestNetworkPolicyObserver policy_observer_;
  std::unique_ptr<MockNetworkStateHandler> network_state_handler_;
  std::unique_ptr<TestNetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandlerImpl>
      managed_network_configuration_handler_;

  DISALLOW_COPY_AND_ASSIGN(ManagedNetworkConfigurationHandlerTest);
};

TEST_F(ManagedNetworkConfigurationHandlerTest, RemoveIrrelevantFields) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY,
            kUser1,
            "policy/policy_wifi1_with_redundant_fields.onc");
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties,
              DictionaryHasValues(expected_shill_properties->Clone()));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyManageUnconfigured) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties,
              DictionaryHasValues(expected_shill_properties->Clone()));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, EnableManagedCredentialsWiFi) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_autoconnect_on_unconfigured_wifi1.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_wifi1_autoconnect.onc");
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties,
              DictionaryHasValues(expected_shill_properties->Clone()));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, EnableManagedCredentialsVPN) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_autoconnect_on_unconfigured_vpn.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_vpn_autoconnect.onc");
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidVpn);
  ASSERT_FALSE(service_path.empty());
  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_EQ(*expected_shill_properties, *properties);
}

// Ensure that EAP settings for ethernet are matched with the right profile
// entry and written to the dedicated EthernetEAP service.
TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyManageUnmanagedEthernetEAP) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/"
          "shill_policy_on_unmanaged_ethernet_eap.json");

  GetShillServiceClient()->AddService(
      "eth_entry", std::string() /* guid */, std::string() /* name */,
      "etherneteap", std::string() /* state */, true /* visible */);
  GetShillProfileClient()->AddService(kUser1ProfilePath, "eth_entry");
  SetUpEntry("policy/shill_unmanaged_ethernet_eap.json",
             kUser1ProfilePath,
             "eth_entry");

  // Also setup an unrelated WiFi configuration to verify that the right entry
  // is matched.
  GetShillServiceClient()->AddService(
      "wifi_entry", std::string() /* guid */, "wifi1", shill::kTypeWifi,
      std::string() /* state */, true /* visible */);
  SetUpEntry("policy/shill_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "wifi_entry");

  SetPolicy(
      ::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_ethernet_eap.onc");
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidEthernetEap);
  ASSERT_FALSE(service_path.empty());
  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties,
              DictionaryHasValues(expected_shill_properties->Clone()));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyIgnoreUnmodified) {
  InitializeStandardProfiles();

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());

  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "some_entry_path");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, PolicyApplicationRunning) {
  InitializeStandardProfiles();

  EXPECT_FALSE(managed_handler()->IsAnyPolicyApplicationRunning());

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  managed_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY,
      std::string(),             // no userhash
      base::ListValue(),         // no device network policy
      base::DictionaryValue());  // no device global config

  EXPECT_TRUE(managed_handler()->IsAnyPolicyApplicationRunning());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(managed_handler()->IsAnyPolicyApplicationRunning());

  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "some_entry_path");

  SetPolicy(
      ::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1_update.onc");
  EXPECT_TRUE(managed_handler()->IsAnyPolicyApplicationRunning());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(managed_handler()->IsAnyPolicyApplicationRunning());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, UpdatePolicyAfterFinished) {
  InitializeStandardProfiles();

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());

  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "some_entry_path");

  SetPolicy(
      ::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1_update.onc");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, UpdatePolicyBeforeFinished) {
  InitializeStandardProfiles();

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  // Usually the first call will cause a profile entry to be created, which we
  // don't fake here.
  SetPolicy(
      ::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1_update.onc");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyManageUnmanaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_wifi1.json");

  // Before setting policy, old_entry_path should exist.
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  // Verify old_entry_path is deleted.
  EXPECT_FALSE(GetShillProfileClient()->HasService("old_entry_path"));

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties,
              DictionaryHasValues(expected_shill_properties->Clone()));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUpdateManagedNewGUID) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_wifi1.json");

  // The passphrase isn't sent again, because it's configured by the user and
  // Shill doesn't send it on GetProperties calls.
  expected_shill_properties->RemoveWithoutPathExpansion(
      shill::kPassphraseProperty, nullptr);
  expected_shill_properties->RemoveWithoutPathExpansion(
      shill::kPassphraseRequiredProperty, nullptr);

  // Before setting policy, old_entry_path should exist.
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  // Verify old_entry_path is deleted.
  EXPECT_FALSE(GetShillProfileClient()->HasService("old_entry_path"));

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties,
              DictionaryHasValues(expected_shill_properties->Clone()));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUpdateManagedVPN) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_vpn.json", kUser1ProfilePath, "entry_path");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_vpn.onc");
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidVpn);
  ASSERT_FALSE(service_path.empty());
  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary("policy/shill_policy_on_managed_vpn.json");
  EXPECT_EQ(*expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyUpdateManagedVPNPlusUi) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_vpn.json", kUser1ProfilePath, "entry_path");

  // Apply a policy that does not provide an authenticaiton type.
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_vpn_no_auth.onc");
  base::RunLoop().RunUntilIdle();

  // Apply additional configuration (e.g. from the UI). This includes password
  // and OTP which should be allowed when authentication type is not explicitly
  // set. See https://crbug.com/817617 for details.
  const NetworkState* network_state =
      network_state_handler_->GetNetworkStateFromGuid(kTestGuidVpn);
  ASSERT_TRUE(network_state);
  std::unique_ptr<base::DictionaryValue> ui_config =
      test_utils::ReadTestDictionary("policy/policy_vpn_ui.json");
  managed_network_configuration_handler_->SetProperties(
      network_state->path(), *ui_config, base::DoNothing(),
      base::Bind(&ErrorCallback));
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidVpn);
  ASSERT_FALSE(service_path.empty());
  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_managed_vpn_plus_ui.json");
  EXPECT_EQ(*expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyUpdateManagedVPNNoUserAuthType) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_vpn.json", kUser1ProfilePath, "entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary("policy/shill_policy_on_managed_vpn.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_vpn_no_user_auth_type.onc");
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidVpn);
  ASSERT_FALSE(service_path.empty());
  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_EQ(*expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyReapplyToManaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_wifi1.json");

  // The passphrase isn't sent again, because it's configured by the user and
  // Shill doesn't send it on GetProperties calls.
  expected_shill_properties->RemoveWithoutPathExpansion(
      shill::kPassphraseProperty, nullptr);
  expected_shill_properties->RemoveWithoutPathExpansion(
      shill::kPassphraseRequiredProperty, nullptr);

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  {
    std::string service_path =
        GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
    ASSERT_FALSE(service_path.empty());
    const base::DictionaryValue* properties =
        GetShillServiceClient()->GetServiceProperties(service_path);
    ASSERT_TRUE(properties);
    EXPECT_THAT(*properties,
                DictionaryHasValues(expected_shill_properties->Clone()));
  }

  // If we apply the policy again, without change, then the Shill profile will
  // not be modified.
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  {
    std::string service_path =
        GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
    ASSERT_FALSE(service_path.empty());
    const base::DictionaryValue* properties =
        GetShillServiceClient()->GetServiceProperties(service_path);
    ASSERT_TRUE(properties);
    EXPECT_THAT(*properties,
                DictionaryHasValues(expected_shill_properties->Clone()));
  }
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUnmanageManaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  // Before setting policy, old_entry_path should exist.
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            std::string() /* path_to_onc */);
  base::RunLoop().RunUntilIdle();

  // Verify old_entry_path is deleted.
  EXPECT_FALSE(GetShillProfileClient()->HasService("old_entry_path"));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetEmptyPolicyIgnoreUnmanaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  // Before setting policy, old_entry_path should exist.
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            std::string() /* path_to_onc */);
  base::RunLoop().RunUntilIdle();

  // Verify old_entry_path is kept.
  EXPECT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyIgnoreUnmanaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi2.json",
             kUser1ProfilePath,
             "wifi2_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties,
              DictionaryHasValues(expected_shill_properties->Clone()));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, AutoConnectDisallowed) {
  InitializeStandardProfiles();

  // Setup an unmanaged network.
  SetUpEntry("policy/shill_unmanaged_wifi2.json",
             kUser1ProfilePath,
             "wifi2_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_disallow_autoconnect_on_unmanaged_wifi2.json");

  // Apply the user policy with global autoconnect config and expect that
  // autoconnect is disabled in the network's profile entry.
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_allow_only_policy_networks_to_autoconnect.onc");
  base::RunLoop().RunUntilIdle();

  std::string wifi2_service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidUnmanagedWifi2);
  ASSERT_FALSE(wifi2_service_path.empty());
  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties(wifi2_service_path);
  ASSERT_TRUE(properties);
  EXPECT_EQ(*expected_shill_properties, *properties);

  // Verify that GetManagedProperties correctly augments the properties with the
  // global config from the user policy.
  // GetManagedProperties requires the device policy to be set or explicitly
  // unset.
  managed_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY,
      std::string(),             // no userhash
      base::ListValue(),         // no device network policy
      base::DictionaryValue());  // no device global config

  std::unique_ptr<base::DictionaryValue> dictionary;
  managed_handler()->GetManagedProperties(
      kUser1, wifi2_service_path,
      base::Bind(
          [](std::unique_ptr<base::DictionaryValue>* dictionary_out,
             const std::string& service_path,
             const base::DictionaryValue& dictionary) {
            *dictionary_out = base::DictionaryValue::From(
                base::Value::ToUniquePtrValue(dictionary.Clone()));
          },
          &dictionary),
      base::Bind(
          [](const std::string& error_name,
             std::unique_ptr<base::DictionaryValue> error_data) { FAIL(); }));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(dictionary.get());
  std::unique_ptr<base::DictionaryValue> expected_managed_onc =
      test_utils::ReadTestDictionary(
          "policy/"
          "managed_onc_disallow_autoconnect_on_unmanaged_wifi2.onc");
  EXPECT_EQ(*expected_managed_onc, *dictionary);
}

TEST_F(ManagedNetworkConfigurationHandlerTest, LateProfileLoading) {
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  InitializeStandardProfiles();
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties,
              DictionaryHasValues(expected_shill_properties->Clone()));
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       ShutdownDuringPolicyApplication) {
  InitializeStandardProfiles();

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");

  // Reset the network configuration manager after setting policy and before
  // calling RunUntilIdle to simulate shutdown during policy application.
  ResetManagedNetworkConfigurationHandler();
  base::RunLoop().RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       AllowOnlyPolicyNetworksToConnect) {
  InitializeStandardProfiles();

  // Check transfer to NetworkStateHandler
  EXPECT_CALL(
      *network_state_handler_,
      UpdateBlockedWifiNetworks(true, false, std::vector<std::string>()))
      .Times(1);

  // Set 'AllowOnlyPolicyNetworksToConnect' policy and a random user policy.
  SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
            "policy/policy_allow_only_policy_networks_to_connect.onc");
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_TRUE(managed_handler()->AllowOnlyPolicyNetworksToConnect());
  EXPECT_FALSE(
      managed_handler()->AllowOnlyPolicyNetworksToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_TRUE(managed_handler()->GetBlacklistedHexSSIDs().empty());
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       AllowOnlyPolicyNetworksToConnectIfAvailable) {
  InitializeStandardProfiles();

  // Check transfer to NetworkStateHandler
  EXPECT_CALL(
      *network_state_handler_,
      UpdateBlockedWifiNetworks(false, true, std::vector<std::string>()))
      .Times(1);

  // Set 'AllowOnlyPolicyNetworksToConnect' policy and a random user policy.
  SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
      "policy/policy_allow_only_policy_networks_to_connect_if_available.onc");
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToConnect());
  EXPECT_TRUE(managed_handler()->AllowOnlyPolicyNetworksToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_TRUE(managed_handler()->GetBlacklistedHexSSIDs().empty());
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       AllowOnlyPolicyNetworksToAutoconnect) {
  InitializeStandardProfiles();

  // Check transfer to NetworkStateHandler
  EXPECT_CALL(
      *network_state_handler_,
      UpdateBlockedWifiNetworks(false, false, std::vector<std::string>()))
      .Times(1);

  // Set 'AllowOnlyPolicyNetworksToAutoconnect' policy and a random user policy.
  SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
            "policy/policy_allow_only_policy_networks_to_autoconnect.onc");
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToConnect());
  EXPECT_FALSE(
      managed_handler()->AllowOnlyPolicyNetworksToConnectIfAvailable());
  EXPECT_TRUE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_TRUE(managed_handler()->GetBlacklistedHexSSIDs().empty());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, GetBlacklistedHexSSIDs) {
  InitializeStandardProfiles();
  std::vector<std::string> blacklist = {"476F6F676C65477565737450534B"};

  // Check transfer to NetworkStateHandler
  EXPECT_CALL(*network_state_handler_,
              UpdateBlockedWifiNetworks(false, false, blacklist))
      .Times(1);

  // Set 'BlacklistedHexSSIDs' policy and a random user policy.
  SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
            "policy/policy_blacklisted_hex_ssids.onc");
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToConnect());
  EXPECT_FALSE(
      managed_handler()->AllowOnlyPolicyNetworksToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_EQ(blacklist, managed_handler()->GetBlacklistedHexSSIDs());
}

}  // namespace chromeos
