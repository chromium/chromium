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
#include "base/values.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "chromeos/components/onc/variable_expander.h"
#include "chromeos/test/chromeos_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::onc {

namespace test_utils = ::chromeos::onc::test_utils;

TEST(ONCUtils, ProxySettingsToProxyConfig) {
  base::Value list_of_tests = test_utils::ReadTestJson("proxy_config.json");
  ASSERT_TRUE(list_of_tests.is_list());

  // Additional ONC -> ProxyConfig test cases to test fixup.
  base::Value additional_tests =
      test_utils::ReadTestJson("proxy_config_from_onc.json");
  ASSERT_TRUE(additional_tests.is_list());
  for (const base::Value& value : additional_tests.GetList())
    list_of_tests.GetList().Append(value.Clone());

  int index = 0;
  for (const base::Value& test_case : list_of_tests.GetList()) {
    SCOPED_TRACE("Test case #" + base::NumberToString(index++));

    ASSERT_TRUE(test_case.is_dict());

    const base::Value* expected_proxy_config = test_case.FindKey("ProxyConfig");
    ASSERT_TRUE(expected_proxy_config);

    const base::Value* onc_proxy_settings =
        test_case.FindKey("ONC_ProxySettings");
    ASSERT_TRUE(onc_proxy_settings);

    base::Value::Dict actual_proxy_config =
        ConvertOncProxySettingsToProxyConfig(*onc_proxy_settings);
    EXPECT_EQ(*expected_proxy_config, actual_proxy_config);
  }
}

TEST(ONCUtils, ProxyConfigToOncProxySettings) {
  base::Value list_of_tests = test_utils::ReadTestJson("proxy_config.json");
  ASSERT_TRUE(list_of_tests.is_list());

  int index = 0;
  for (const base::Value& test_case : list_of_tests.GetList()) {
    SCOPED_TRACE("Test case #" + base::NumberToString(index++));

    const base::Value* shill_proxy_config = test_case.FindKey("ProxyConfig");
    ASSERT_TRUE(shill_proxy_config);

    const base::Value* onc_proxy_settings =
        test_case.FindKey("ONC_ProxySettings");
    ASSERT_TRUE(onc_proxy_settings);

    base::Value actual_proxy_settings =
        ConvertProxyConfigToOncProxySettings(*shill_proxy_config);
    EXPECT_TRUE(test_utils::Equals(onc_proxy_settings, &actual_proxy_settings));
  }
}

TEST(ONCPasswordVariable, PasswordAvailable) {
  const auto wifi_onc = test_utils::ReadTestDictionaryValue(
      "wifi_eap_ttls_with_password_variable.onc");

  EXPECT_TRUE(HasUserPasswordSubsitutionVariable(
      chromeos::onc::kNetworkConfigurationSignature, &wifi_onc));
}

TEST(ONCPasswordVariable, PasswordNotAvailable) {
  const auto wifi_onc =
      test_utils::ReadTestDictionaryValue("wifi_eap_ttls.onc");

  EXPECT_FALSE(HasUserPasswordSubsitutionVariable(
      chromeos::onc::kNetworkConfigurationSignature, &wifi_onc));
}

TEST(ONCPasswordVariable, PasswordHarcdoded) {
  const auto wifi_onc = test_utils::ReadTestDictionaryValue(
      "wifi_eap_ttls_with_hardcoded_password.onc");

  EXPECT_FALSE(HasUserPasswordSubsitutionVariable(
      chromeos::onc::kNetworkConfigurationSignature, &wifi_onc));
}

TEST(ONCPasswordVariable, MultipleNetworksPasswordAvailable) {
  const auto network_dictionary = test_utils::ReadTestDictionaryValue(
      "managed_toplevel_with_password_variable.onc");
  const base::Value* network_list =
      network_dictionary.FindListKey("NetworkConfigurations");
  ASSERT_TRUE(network_list);

  EXPECT_TRUE(HasUserPasswordSubsitutionVariable(network_list));
}

TEST(ONCPasswordVariable, MultipleNetworksPasswordNotAvailable) {
  const auto network_dictionary = test_utils::ReadTestDictionaryValue(
      "managed_toplevel_with_no_password_variable.onc");

  const base::Value* network_list =
      network_dictionary.FindListKey("NetworkConfigurations");
  ASSERT_TRUE(network_list);

  EXPECT_FALSE(HasUserPasswordSubsitutionVariable(network_list));
}

TEST(ONCPasswordVariable, MultipleNetworksPasswordAvailableForL2tpVpn) {
  const auto network_dictionary = test_utils::ReadTestDictionaryValue(
      "managed_toplevel_with_password_variable_in_l2tp_vpn.onc");
  const base::Value* network_list =
      network_dictionary.FindListKey("NetworkConfigurations");
  ASSERT_TRUE(network_list);

  EXPECT_TRUE(HasUserPasswordSubsitutionVariable(network_list));
}

}  // namespace ash::onc
