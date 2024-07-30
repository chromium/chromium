// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/onc/network_onc_utils.h"

#include <string>

#include "base/check.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "chromeos/components/onc/variable_expander.h"
#include "chromeos/test/chromeos_test_utils.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::onc {

namespace test_utils = ::chromeos::onc::test_utils;

class ONCUtilsTest : public testing::Test {
 public:
  ONCUtilsTest() = default;
  ONCUtilsTest(const ONCUtilsTest&) = delete;
  ONCUtilsTest& operator=(const ONCUtilsTest&) = delete;
  ~ONCUtilsTest() override = default;

  void SetUp() override {
    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    auto account_id = AccountId::FromUserEmail("account@test.com");
    const user_manager::User* user = fake_user_manager->AddUser(account_id);
    fake_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);
    fake_user_manager->SwitchActiveUser(account_id);

    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    network_handler_test_helper_ =
        std::make_unique<ash::NetworkHandlerTestHelper>();
    network_handler_test_helper_->AddDefaultProfiles();
    network_handler_test_helper_->profile_test()->AddProfile(
        "/profile/1", user->username_hash());
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_handler_test_helper_.reset();
    scoped_user_manager_.reset();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<ash::NetworkHandlerTestHelper> network_handler_test_helper_;
};

TEST_F(ONCUtilsTest, ImportNetworksForUser) {
  const char kPolicyGuid[] = "policy_guid";
  const char kWifiSSID[] = "wifi_ssid";
  const char kWifiPassphrase[] = "test_phassphrase";
  const char kWifiOncName[] = "wifi_onc_name";

  base::Value::Dict wifi_config =
      base::Value::Dict()
          .Set(::onc::network_config::kGUID, kPolicyGuid)
          .Set(::onc::network_config::kName, kWifiOncName)
          .Set(::onc::network_config::kType, ::onc::network_config::kWiFi)
          .Set(::onc::network_config::kWiFi,
               base::Value::Dict()
                   .Set(::onc::wifi::kSSID, kWifiSSID)
                   .Set(::onc::wifi::kPassphrase, kWifiPassphrase)
                   .Set(::onc::wifi::kSecurity, ::onc::wifi::kWEP_PSK));
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();

  // Set user policy
  NetworkHandler::Get()->managed_network_configuration_handler()->SetPolicy(
      ::onc::ONC_SOURCE_USER_POLICY, user->username_hash(),
      base::Value::List().Append(wifi_config.Clone()), base::Value::Dict());

  // Set shared policy
  NetworkHandler::Get()->managed_network_configuration_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY, std::string(), base::Value::List(),
      base::Value::Dict());
  base::RunLoop().RunUntilIdle();

  // Modify the wifi config to "None" security and attempt to import it for the
  // user.
  wifi_config.Set(::onc::network_config::kWiFi,
                  base::Value::Dict()
                      .Set(::onc::wifi::kSSID, kWifiSSID)
                      .Set(::onc::wifi::kSecurity, ::onc::wifi::kSecurityNone));
  std::string error;
  ImportNetworksForUser(user, base::Value::List().Append(wifi_config.Clone()),
                        &error);

  // Verify the import network should not override the existing policy
  // configured network.
  std::string service_path =
      network_handler_test_helper_->service_test()->FindServiceMatchingGUID(
          kPolicyGuid);
  ASSERT_FALSE(service_path.empty());

  const base::Value::Dict* properties =
      network_handler_test_helper_->service_test()->GetServiceProperties(
          service_path);
  ASSERT_TRUE(properties);
  const std::string* security =
      properties->FindString(shill::kSecurityClassProperty);
  ASSERT_TRUE(security);
  EXPECT_EQ(*security, shill::kSecurityClassWep);
}

TEST_F(ONCUtilsTest, ProxySettingsToProxyConfig) {
  base::Value::List list_of_tests =
      test_utils::ReadTestList("proxy_config.json");

  // Additional ONC -> ProxyConfig test cases to test fixup.
  base::Value::List additional_tests =
      test_utils::ReadTestList("proxy_config_from_onc.json");
  for (const base::Value& value : additional_tests) {
    list_of_tests.Append(value.Clone());
  }

  int index = 0;
  for (const base::Value& test_case : list_of_tests) {
    SCOPED_TRACE("Test case #" + base::NumberToString(index++));
    const base::Value::Dict& test_case_dict = test_case.GetDict();

    const base::Value* expected_proxy_config =
        test_case_dict.Find("ProxyConfig");
    ASSERT_TRUE(expected_proxy_config);

    const base::Value::Dict* onc_proxy_settings =
        test_case_dict.FindDict("ONC_ProxySettings");
    ASSERT_TRUE(onc_proxy_settings);

    std::optional<base::Value::Dict> actual_proxy_config =
        ConvertOncProxySettingsToProxyConfig(*onc_proxy_settings);
    ASSERT_TRUE(actual_proxy_config.has_value());
    EXPECT_EQ(*expected_proxy_config, actual_proxy_config);
  }
}

TEST_F(ONCUtilsTest, ProxyConfigToOncProxySettings) {
  base::Value::List list_of_tests =
      test_utils::ReadTestList("proxy_config.json");

  int index = 0;
  for (const base::Value& test_case : list_of_tests) {
    SCOPED_TRACE("Test case #" + base::NumberToString(index++));
    const base::Value::Dict& test_case_dict = test_case.GetDict();

    const base::Value::Dict* shill_proxy_config =
        test_case_dict.FindDict("ProxyConfig");
    ASSERT_TRUE(shill_proxy_config);

    const base::Value::Dict* onc_proxy_settings =
        test_case_dict.FindDict("ONC_ProxySettings");
    ASSERT_TRUE(onc_proxy_settings);

    std::optional<base::Value::Dict> actual_proxy_settings =
        ConvertProxyConfigToOncProxySettings(*shill_proxy_config);
    ASSERT_TRUE(actual_proxy_settings.has_value());
    EXPECT_TRUE(
        test_utils::Equals(onc_proxy_settings, &actual_proxy_settings.value()));
  }
}

TEST(ONCPasswordVariable, PasswordAvailable) {
  const auto wifi_onc = test_utils::ReadTestDictionary(
      "wifi_eap_ttls_with_password_variable.onc");

  EXPECT_TRUE(HasUserPasswordSubstitutionVariable(
      chromeos::onc::kNetworkConfigurationSignature, wifi_onc));
}

TEST(ONCPasswordVariable, PasswordNotAvailable) {
  const auto wifi_onc = test_utils::ReadTestDictionary("wifi_eap_ttls.onc");

  EXPECT_FALSE(HasUserPasswordSubstitutionVariable(
      chromeos::onc::kNetworkConfigurationSignature, wifi_onc));
}

TEST(ONCPasswordVariable, PasswordHardcoded) {
  const auto wifi_onc = test_utils::ReadTestDictionary(
      "wifi_eap_ttls_with_hardcoded_password.onc");

  EXPECT_FALSE(HasUserPasswordSubstitutionVariable(
      chromeos::onc::kNetworkConfigurationSignature, wifi_onc));
}

TEST(ONCPasswordVariable, MultipleNetworksPasswordAvailable) {
  const auto network_dictionary = test_utils::ReadTestDictionary(
      "managed_toplevel_with_password_variable.onc");
  const base::Value::List* network_list =
      network_dictionary.FindList("NetworkConfigurations");
  ASSERT_TRUE(network_list);

  EXPECT_TRUE(HasUserPasswordSubstitutionVariable(*network_list));
}

TEST(ONCPasswordVariable, MultipleNetworksPasswordNotAvailable) {
  const auto network_dictionary = test_utils::ReadTestDictionary(
      "managed_toplevel_with_no_password_variable.onc");

  const base::Value::List* network_list =
      network_dictionary.FindList("NetworkConfigurations");
  ASSERT_TRUE(network_list);

  EXPECT_FALSE(HasUserPasswordSubstitutionVariable(*network_list));
}

TEST(ONCPasswordVariable, MultipleNetworksPasswordAvailableForL2tpVpn) {
  const auto network_dictionary = test_utils::ReadTestDictionary(
      "managed_toplevel_with_password_variable_in_l2tp_vpn.onc");
  const base::Value::List* network_list =
      network_dictionary.FindList("NetworkConfigurations");
  ASSERT_TRUE(network_list);

  EXPECT_TRUE(HasUserPasswordSubstitutionVariable(*network_list));
}

}  // namespace ash::onc
