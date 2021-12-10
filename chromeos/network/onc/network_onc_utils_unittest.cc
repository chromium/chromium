// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/network_onc_utils.h"

#include <string>

#include "base/check.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "chromeos/components/onc/variable_expander.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/test/chromeos_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {

TEST(ONCUtils, ProxySettingsToProxyConfig) {
  std::unique_ptr<base::Value> list_of_tests =
      test_utils::ReadTestJson("proxy_config.json");
  ASSERT_TRUE(list_of_tests->is_list());

  // Additional ONC -> ProxyConfig test cases to test fixup.
  std::unique_ptr<base::Value> additional_tests =
      test_utils::ReadTestJson("proxy_config_from_onc.json");
  ASSERT_TRUE(additional_tests->is_list());
  for (const base::Value& value : additional_tests->GetList())
    list_of_tests->Append(value.Clone());

  int index = 0;
  for (const base::Value& test_case : list_of_tests->GetList()) {
    SCOPED_TRACE("Test case #" + base::NumberToString(index++));

    ASSERT_TRUE(test_case.is_dict());

    const base::Value* expected_proxy_config = test_case.FindKey("ProxyConfig");
    ASSERT_TRUE(expected_proxy_config);

    const base::Value* onc_proxy_settings =
        test_case.FindKey("ONC_ProxySettings");
    ASSERT_TRUE(onc_proxy_settings);

    base::Value actual_proxy_config =
        ConvertOncProxySettingsToProxyConfig(*onc_proxy_settings);
    EXPECT_TRUE(
        test_utils::Equals(expected_proxy_config, &actual_proxy_config));
  }
}

TEST(ONCUtils, ProxyConfigToOncProxySettings) {
  std::unique_ptr<base::Value> list_of_tests(
      test_utils::ReadTestJson("proxy_config.json"));
  ASSERT_TRUE(list_of_tests->is_list());

  int index = 0;
  for (const base::Value& test_case : list_of_tests->GetList()) {
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
  const auto wifi_onc = test_utils::ReadTestDictionary(
      "wifi_eap_ttls_with_password_variable.onc");

  EXPECT_TRUE(HasUserPasswordSubsitutionVariable(kNetworkConfigurationSignature,
                                                 wifi_onc.get()));
}

TEST(ONCPasswordVariable, PasswordNotAvailable) {
  const auto wifi_onc = test_utils::ReadTestDictionary("wifi_eap_ttls.onc");

  EXPECT_FALSE(HasUserPasswordSubsitutionVariable(
      kNetworkConfigurationSignature, wifi_onc.get()));
}

TEST(ONCPasswordVariable, PasswordHarcdoded) {
  const auto wifi_onc = test_utils::ReadTestDictionary(
      "wifi_eap_ttls_with_hardcoded_password.onc");

  EXPECT_FALSE(HasUserPasswordSubsitutionVariable(
      kNetworkConfigurationSignature, wifi_onc.get()));
}

TEST(ONCPasswordVariable, MultipleNetworksPasswordAvailable) {
  const auto network_dictionary = test_utils::ReadTestDictionary(
      "managed_toplevel_with_password_variable.onc");

  const auto network_list = std::make_unique<base::ListValue>(base::ListValue(
      network_dictionary->FindKey("NetworkConfigurations")->GetList()));

  EXPECT_TRUE(HasUserPasswordSubsitutionVariable(network_list.get()));
}

TEST(ONCPasswordVariable, MultipleNetworksPasswordNotAvailable) {
  const auto network_dictionary = test_utils::ReadTestDictionary(
      "managed_toplevel_with_no_password_variable.onc");

  const auto network_list = std::make_unique<base::ListValue>(base::ListValue(
      network_dictionary->FindKey("NetworkConfigurations")->GetList()));

  EXPECT_FALSE(HasUserPasswordSubsitutionVariable(network_list.get()));
}

}  // namespace onc
}  // namespace chromeos
