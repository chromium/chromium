// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/onc/onc_utils.h"

#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "chromeos/components/onc/variable_expander.h"
#include "components/onc/onc_constants.h"
#include "onc_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::onc {

TEST(ONCDecrypterTest, BrokenEncryptionIterations) {
  base::Value::Dict encrypted_onc =
      test_utils::ReadTestDictionary("broken-encrypted-iterations.onc");

  std::optional<base::Value::Dict> decrypted_onc =
      Decrypt("test0000", encrypted_onc);

  EXPECT_FALSE(decrypted_onc.has_value());
}

TEST(ONCDecrypterTest, BrokenEncryptionZeroIterations) {
  base::Value::Dict encrypted_onc =
      test_utils::ReadTestDictionary("broken-encrypted-zero-iterations.onc");

  std::optional<base::Value::Dict> decrypted_onc =
      Decrypt("test0000", encrypted_onc);

  EXPECT_FALSE(decrypted_onc.has_value());
}

TEST(ONCDecrypterTest, LoadEncryptedOnc) {
  base::Value::Dict encrypted_onc =
      test_utils::ReadTestDictionary("encrypted.onc");
  base::Value::Dict expected_decrypted_onc =
      test_utils::ReadTestDictionary("decrypted.onc");

  std::string error;
  std::optional<base::Value::Dict> actual_decrypted_onc =
      Decrypt("test0000", encrypted_onc);

  EXPECT_TRUE(test_utils::Equals(&expected_decrypted_onc,
                                 &actual_decrypted_onc.value()));
}

namespace {

const char* kLoginId = "hans";
const char* kLoginEmail = "hans@my.domain.com";

const std::vector<std::string> kValidApnTypes = {
    ::onc::cellular_apn::kIpTypeAutomatic,
    ::onc::cellular_apn::kIpTypeIpv4,
    ::onc::cellular_apn::kIpTypeIpv4Ipv6,
    ::onc::cellular_apn::kIpTypeIpv6,
};

const std::vector<std::string>& kTestAdminApnListAllIds = {"id-1", "id-2",
                                                           "id-3"};
const std::vector<std::string>& kTestAdminApnListSubsetIds = {"id-1", "id-3"};
const std::vector<std::string>& kTestNonAdminApnListIds = {"id-x", "id-y"};

base::flat_map<std::string, std::string> GetTestStringSubstitutions() {
  base::flat_map<std::string, std::string> substitutions;
  substitutions[::onc::substitutes::kLoginID] = kLoginId;
  substitutions[::onc::substitutes::kLoginEmail] = kLoginEmail;
  return substitutions;
}

}  // namespace

TEST(ONCStringExpansion, OpenVPN) {
  base::Value::Dict vpn_onc =
      test_utils::ReadTestDictionary("valid_openvpn.onc");

  VariableExpander variable_expander(GetTestStringSubstitutions());
  ExpandStringsInOncObject(kNetworkConfigurationSignature, variable_expander,
                           &vpn_onc);

  std::string* actual_expanded =
      vpn_onc.FindStringByDottedPath("VPN.OpenVPN.Username");
  ASSERT_TRUE(actual_expanded);
  EXPECT_EQ(*actual_expanded, std::string("abc ") + kLoginEmail + " def");
}

TEST(ONCStringExpansion, WiFi_EAP) {
  base::Value::Dict wifi_onc =
      test_utils::ReadTestDictionary("wifi_clientcert_with_cert_pems.onc");

  VariableExpander variable_expander(GetTestStringSubstitutions());
  ExpandStringsInOncObject(kNetworkConfigurationSignature, variable_expander,
                           &wifi_onc);

  std::string* actual_expanded =
      wifi_onc.FindStringByDottedPath("WiFi.EAP.Identity");
  ASSERT_TRUE(actual_expanded);
  EXPECT_EQ(*actual_expanded,
            std::string("abc ") + kLoginId + "@my.domain.com");
}

TEST(ONCResolveServerCertRefs, ResolveServerCertRefs) {
  base::Value::Dict test_cases = test_utils::ReadTestDictionary(
      "network_configs_with_resolved_certs.json");

  CertPEMsByGUIDMap certs;
  certs["cert_google"] = "pem_google";
  certs["cert_webkit"] = "pem_webkit";

  for (auto iter : test_cases) {
    SCOPED_TRACE("Test case: " + iter.first);

    const base::Value::Dict* test_case_dict = iter.second.GetIfDict();
    ASSERT_TRUE(test_case_dict);

    const base::Value::List* networks_with_cert_refs =
        test_case_dict->FindList("WithCertRefs");
    ASSERT_TRUE(networks_with_cert_refs);
    const base::Value::List* expected_resolved_onc =
        test_case_dict->FindList("WithResolvedRefs");
    ASSERT_TRUE(expected_resolved_onc);

    bool expected_success =
        (networks_with_cert_refs->size() == expected_resolved_onc->size());

    base::Value::List actual_resolved_onc(networks_with_cert_refs->Clone());
    bool success = ResolveServerCertRefsInNetworks(certs, actual_resolved_onc);
    EXPECT_EQ(expected_success, success);
    EXPECT_EQ(*expected_resolved_onc, actual_resolved_onc);
  }
}

TEST(ONCUtils, SetHiddenSSIDField_WithNoValueSet) {
  // WiFi configuration that doesn't have HiddenSSID field set.
  base::Value::Dict wifi_onc =
      test_utils::ReadTestDictionary("wifi_clientcert_with_cert_pems.onc");
  base::Value::Dict* wifi_fields = wifi_onc.FindDict("WiFi");
  ASSERT_TRUE(wifi_fields);

  ASSERT_FALSE(wifi_fields->Find(::onc::wifi::kHiddenSSID));
  SetHiddenSSIDField(*wifi_fields);
  base::Value* hidden_ssid_field = wifi_fields->Find(::onc::wifi::kHiddenSSID);
  ASSERT_TRUE(hidden_ssid_field);
  EXPECT_FALSE(hidden_ssid_field->GetBool());
}

TEST(ONCUtils, SetHiddenSSIDField_WithValueSetFalse) {
  // WiFi configuration that have HiddenSSID field set to false.
  base::Value::Dict wifi_onc = test_utils::ReadTestDictionary(
      "translation_of_shill_wifi_with_state.onc");
  base::Value::Dict* wifi_fields = wifi_onc.FindDict("WiFi");
  ASSERT_TRUE(wifi_fields);

  ASSERT_TRUE(wifi_fields->Find(::onc::wifi::kHiddenSSID));
  SetHiddenSSIDField(*wifi_fields);
  EXPECT_FALSE(wifi_fields->Find(::onc::wifi::kHiddenSSID)->GetBool());
}

TEST(ONCUtils, SetHiddenSSIDField_WithValueSetTrue) {
  // WiFi configuration that have HiddenSSID field set to true.
  base::Value::Dict wifi_onc =
      test_utils::ReadTestDictionary("wifi_with_hidden_ssid.onc");
  base::Value::Dict* wifi_fields = wifi_onc.FindDict("WiFi");
  ASSERT_TRUE(wifi_fields);

  ASSERT_TRUE(wifi_fields->Find(::onc::wifi::kHiddenSSID));
  SetHiddenSSIDField(*wifi_fields);
  EXPECT_TRUE(wifi_fields->Find(::onc::wifi::kHiddenSSID)->GetBool());
}

TEST(ONCUtils, ParseAndValidateOncForImport_ApnProvided) {
  const auto onc_blob = test_utils::ReadTestData("valid_cellular_with_apn.onc");
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;

  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  base::Value::Dict expected;
  expected.Set(::onc::cellular_apn::kAccessPointName, "test-apn");
  expected.Set(::onc::cellular_apn::kAuthentication, "");
  expected.Set(::onc::cellular_apn::kUsername, "test-username");
  expected.Set(::onc::cellular_apn::kPassword, "test-password");

  const auto* cellular_apn =
      network_configs[0].GetDict().FindByDottedPath("Cellular.APN");
  EXPECT_THAT(cellular_apn->GetDict(),
              base::test::DictionaryHasValues(std::move(expected)));
}

TEST(ONCUtils, ParseAndValidateOncForImport_NoApnProvided) {
  const auto onc_blob = test_utils::ReadTestData("valid_cellular_no_apn.onc");
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;

  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  const auto* cellular_apn =
      network_configs[0].GetDict().FindByDottedPath("Cellular.APN");
  ASSERT_NE(nullptr, cellular_apn);
  ASSERT_NE(nullptr, cellular_apn->GetDict().Find(::onc::kRecommended));

  base::Value::List recommended =
      base::Value::List()
          .Append(::onc::cellular_apn::kAccessPointName)
          .Append(::onc::cellular_apn::kAttach)
          .Append(::onc::cellular_apn::kAuthentication)
          .Append(::onc::cellular_apn::kUsername)
          .Append(::onc::cellular_apn::kPassword);

  base::Value::Dict expected;
  expected.Set(::onc::kRecommended, std::move(recommended));
  EXPECT_THAT(cellular_apn->GetDict(),
              base::test::DictionaryHasValues(std::move(expected)));
}

TEST(ONCUtils, ParseAndValidateOncForImport_APNAccessPointName) {
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;
  test_utils::TestToplevelApnData apn_data;

  // Test APN with only Access Point Name
  apn_data.access_point_name = "test-access-point-name";
  std::string onc_blob =
      test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  // Failure if APN has empty Access Point Name
  apn_data.access_point_name = "";
  onc_blob = test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_FALSE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  //  Failure if APN has no AccessPointName field.
  apn_data.access_point_name = std::nullopt;
  onc_blob = test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_FALSE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));
}

TEST(ONCUtils, ParseAndValidateOncForImport_APNApnType) {
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;
  test_utils::TestToplevelApnData apn_data;

  // Test that no APN Types field is fine
  apn_data.apn_types = std::nullopt;
  std::string onc_blob =
      test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  // Test valid APN types
  apn_data.apn_types = {
      ::onc::cellular_apn::kApnTypeDefault,
      ::onc::cellular_apn::kApnTypeAttach,
      ::onc::cellular_apn::kApnTypeTether,
  };
  onc_blob = test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  // Test invalid APN types
  apn_data.apn_types = {"invalidApn", ::onc::cellular_apn::kApnTypeDefault};
  onc_blob = test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_FALSE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  // Test empty APN types array
  apn_data.apn_types->clear();
  onc_blob = test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_FALSE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));
}

TEST(ONCUtils, ParseAndValidateOncForImport_APNIpType) {
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;
  test_utils::TestToplevelApnData apn_data;

  // Test that no IpType provided is fine
  std::string onc_blob =
      test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  // Test valid IpTypes
  for (const std::string& ip_type : kValidApnTypes) {
    apn_data.ip_type = ip_type;
    onc_blob = test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);

    ASSERT_TRUE(ParseAndValidateOncForImport(
        onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
        &network_configs, &global_network_config, &certificates));
  }

  // Failure if Invalid IP type
  apn_data.ip_type = "InvalidApnType";
  onc_blob = test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_FALSE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));
}

TEST(ONCUtils, ParseAndValidateOncForImport_AdminAPNsExistForAdminAPNIds) {
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;
  test_utils::TestToplevelApnData apn_data;
  apn_data.admin_apn_list_ids = kTestAdminApnListAllIds;

  apn_data.psim_admin_assigned_apn_ids = kTestAdminApnListSubsetIds;
  std::string onc_blob =
      test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  apn_data.psim_admin_assigned_apn_ids = kTestAdminApnListSubsetIds;
  apn_data.admin_assigned_apn_ids = std::vector<std::string>();
  onc_blob = test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  apn_data.psim_admin_assigned_apn_ids = std::vector<std::string>();
  apn_data.admin_assigned_apn_ids = kTestAdminApnListSubsetIds;
  onc_blob = test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  apn_data.psim_admin_assigned_apn_ids = kTestAdminApnListSubsetIds;
  apn_data.admin_assigned_apn_ids = kTestAdminApnListSubsetIds;
  onc_blob = test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));
}

TEST(ONCUtils, ParseAndValidateOncForImport_AdminAPNsDoNotExistForAdminAPNIds) {
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;
  test_utils::TestToplevelApnData apn_data;
  apn_data.admin_apn_list_ids = kTestAdminApnListAllIds;

  apn_data.psim_admin_assigned_apn_ids = kTestNonAdminApnListIds;
  apn_data.admin_assigned_apn_ids = std::nullopt;
  std::string onc_blob =
      test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_FALSE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  apn_data.psim_admin_assigned_apn_ids = std::nullopt;
  apn_data.admin_assigned_apn_ids = kTestNonAdminApnListIds;
  onc_blob = test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_FALSE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  apn_data.psim_admin_assigned_apn_ids = kTestNonAdminApnListIds;
  apn_data.admin_assigned_apn_ids = std::vector<std::string>();
  onc_blob = test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_FALSE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  apn_data.psim_admin_assigned_apn_ids = std::vector<std::string>();
  apn_data.admin_assigned_apn_ids = kTestNonAdminApnListIds;
  onc_blob = test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_FALSE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  apn_data.psim_admin_assigned_apn_ids = kTestAdminApnListAllIds;
  apn_data.admin_assigned_apn_ids = kTestNonAdminApnListIds;
  onc_blob = test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_FALSE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  apn_data.psim_admin_assigned_apn_ids = kTestNonAdminApnListIds;
  apn_data.admin_assigned_apn_ids = kTestAdminApnListAllIds;
  onc_blob = test_utils::GenerateTopLevelWithCellularWithAPNAsJSON(apn_data);
  ASSERT_FALSE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));
}

TEST(ONCUtils, ParseAndValidateOncForImport_CustomApnListRecommendedByDefault) {
  const auto onc_blob =
      test_utils::ReadTestData("valid_cellular_no_recommended.onc");
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;

  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  const auto* recommended =
      network_configs[0].GetDict().FindByDottedPath("Cellular.Recommended");
  ASSERT_NE(nullptr, recommended);

  base::Value::List expected_recommended =
      base::Value::List().Append(::onc::cellular::kCustomAPNList);

  EXPECT_EQ(expected_recommended, *recommended);
}

TEST(
    ONCUtils,
    ParseAndValidateOncForImport_CustomApnListRecommendedWhenApnModificationNotProvided) {
  const auto onc_blob = test_utils::ReadTestData(
      "managed_cellular_no_recommended_allow_apn_modification_not_provided."
      "onc");
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;

  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  const auto* recommended =
      network_configs[0].GetDict().FindByDottedPath("Cellular.Recommended");
  ASSERT_NE(nullptr, recommended);

  base::Value::List expected_recommended =
      base::Value::List().Append(::onc::cellular::kCustomAPNList);

  EXPECT_EQ(expected_recommended, *recommended);
}

TEST(
    ONCUtils,
    ParseAndValidateOncForImport_CustomApnListRecommendedWhenApnModificationAllowed) {
  const auto onc_blob = test_utils::ReadTestData(
      "managed_cellular_no_recommended_allow_apn_modification_true.onc");
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;

  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  const auto* recommended =
      network_configs[0].GetDict().FindByDottedPath("Cellular.Recommended");
  ASSERT_NE(nullptr, recommended);

  base::Value::List expected_recommended =
      base::Value::List().Append(::onc::cellular::kCustomAPNList);

  EXPECT_EQ(expected_recommended, *recommended);
}

TEST(
    ONCUtils,
    ParseAndValidateOncForImport_CustomApnListNotRecommendeWhenApnModificationProhibited) {
  const auto onc_blob = test_utils::ReadTestData(
      "managed_cellular_no_recommended_allow_apn_modification_false.onc");
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;

  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  const auto* recommended =
      network_configs[0].GetDict().FindByDottedPath("Cellular.Recommended");
  EXPECT_EQ(nullptr, recommended);
}

TEST(ONCUtils, ParseAndValidateOncForImport_AdminApnProvided) {
  const auto onc_blob = test_utils::ReadTestData(
      "managed_toplevel_with_multiple_cellular_and_admin_apns.onc");
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;

  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  // Expected custom APN list for the first network configuration
  base::Value::List expected_custom_apns;

  // First expected custom APN details for the first network configuration
  base::Value::Dict first_custom_apn_details;
  first_custom_apn_details.Set(::onc::cellular_apn::kId, "admin-apn-id-y");
  first_custom_apn_details.Set(::onc::cellular_apn::kAccessPointName,
                               "test-apn-admin-y");
  first_custom_apn_details.Set(::onc::cellular_apn::kAuthentication, "");
  first_custom_apn_details.Set(::onc::cellular_apn::kUsername,
                               "test-username-y");
  first_custom_apn_details.Set(::onc::cellular_apn::kPassword,
                               "test-password-y");
  first_custom_apn_details.Set(::onc::cellular_apn::kSource,
                               ::onc::cellular_apn::kSourceAdmin);

  // Second expected custom APN details for the first network configuration
  base::Value::Dict second_custom_apn_details;
  second_custom_apn_details.Set(::onc::cellular_apn::kId, "admin-apn-id-x");
  second_custom_apn_details.Set(::onc::cellular_apn::kAccessPointName,
                                "test-apn-admin-x");
  second_custom_apn_details.Set(::onc::cellular_apn::kAuthentication, "");
  second_custom_apn_details.Set(::onc::cellular_apn::kUsername,
                                "test-username-x");
  second_custom_apn_details.Set(::onc::cellular_apn::kPassword,
                                "test-password-x");
  second_custom_apn_details.Set(::onc::cellular_apn::kSource,
                                ::onc::cellular_apn::kSourceAdmin);

  // Add the APN details to the expected custom APN list for the first network
  // configuration
  expected_custom_apns.Append(std::move(first_custom_apn_details));
  expected_custom_apns.Append(std::move(second_custom_apn_details));

  // Get the actual APN list from the first network configuration
  const auto* actual_custom_apns =
      network_configs[0].GetDict().FindByDottedPath("Cellular.CustomAPNList");

  // Verify that the actual APN list for the first network configuration matches
  // the expected custom APN list
  EXPECT_TRUE(actual_custom_apns->GetList() == expected_custom_apns);

  // Expected custom APN list for the second network configuration
  base::Value::List expected_custom_apns_2;

  // First expected custom APN details for the second network configuration
  base::Value::Dict first_custom_apn_details_2;
  first_custom_apn_details_2.Set(::onc::cellular_apn::kId, "admin-apn-id-z");
  first_custom_apn_details_2.Set(::onc::cellular_apn::kAccessPointName,
                                 "test-apn-admin-z");
  first_custom_apn_details_2.Set(::onc::cellular_apn::kAuthentication, "");
  first_custom_apn_details_2.Set(::onc::cellular_apn::kUsername,
                                 "test-username-z");
  first_custom_apn_details_2.Set(::onc::cellular_apn::kPassword,
                                 "test-password-z");
  first_custom_apn_details_2.Set(::onc::cellular_apn::kSource,
                                 ::onc::cellular_apn::kSourceAdmin);

  // Second expected custom APN details for the second network configuration
  base::Value::Dict second_custom_apn_details_2;
  second_custom_apn_details_2.Set(::onc::cellular_apn::kId, "admin-apn-id-x");
  second_custom_apn_details_2.Set(::onc::cellular_apn::kAccessPointName,
                                  "test-apn-admin-x");
  second_custom_apn_details_2.Set(::onc::cellular_apn::kAuthentication, "");
  second_custom_apn_details_2.Set(::onc::cellular_apn::kUsername,
                                  "test-username-x");
  second_custom_apn_details_2.Set(::onc::cellular_apn::kPassword,
                                  "test-password-x");
  second_custom_apn_details_2.Set(::onc::cellular_apn::kSource,
                                  ::onc::cellular_apn::kSourceAdmin);

  // Add the APN details to the expected custom APN list for the second network
  // configuration
  expected_custom_apns_2.Append(std::move(first_custom_apn_details_2));
  expected_custom_apns_2.Append(std::move(second_custom_apn_details_2));

  // Get the actual APN list from the second network configuration
  const auto* actual_custom_apns_2 =
      network_configs[1].GetDict().FindByDottedPath("Cellular.CustomAPNList");

  // Verify that the actual APN list for the second network configuration
  // matches the expected custom APN list
  EXPECT_TRUE(actual_custom_apns_2->GetList() == expected_custom_apns_2);

  // Expected custom APN list for the third network configuration, which does
  // not have a admin provided custom APN.
  base::Value::List expected_custom_apns_3;

  // The APN should remain unchanged since the admin assigned APN Ids field was
  // not provided.
  base::Value::Dict only_custom_apn_details_3;
  only_custom_apn_details_3.Set(::onc::cellular_apn::kAccessPointName,
                                "test-apn-3");
  only_custom_apn_details_3.Set(::onc::cellular_apn::kAuthentication, "");
  only_custom_apn_details_3.Set(::onc::cellular_apn::kUsername,
                                "test-username-3");
  only_custom_apn_details_3.Set(::onc::cellular_apn::kPassword,
                                "test-password-3");

  // Add the APN details to the expected custom APN list for the third network
  // configuration
  expected_custom_apns_3.Append(std::move(only_custom_apn_details_3));

  // Get the actual APN list from the third network configuration
  const auto* actual_custom_apns_3 =
      network_configs[2].GetDict().FindByDottedPath("Cellular.CustomAPNList");

  // Verify that the actual APN list for the third network configuration
  // matches the expected custom APN list
  EXPECT_TRUE(actual_custom_apns_3->GetList() == expected_custom_apns_3);

  // Get the actual APN list from the fourth network configuration
  const auto* actual_custom_apns_4 =
      network_configs[3].GetDict().FindByDottedPath("Cellular.CustomAPNList");

  // Verify that the custom APN list is empty if the admin provides an empty
  // list of APN IDs.
  EXPECT_TRUE(actual_custom_apns_4->GetList() == base::Value::List());
}

TEST(ONCUtils,
     ParseAndValidateOncForImport_InvalidPSIMAdminAssignedApnIdsProvided) {
  const auto onc_blob = test_utils::ReadTestData(
      "managed_toplevel_with_invalid_psim_admin_assigned_apn_id_list.onc");
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;

  ASSERT_FALSE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));
}

TEST(ONCUtils, ParseAndValidateOncForImport_PSIMAdminAssignedApnIdsProvided) {
  const auto onc_blob = test_utils::ReadTestData(
      "managed_toplevel_with_psim_admin_assigned_apn_id_list.onc");
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;

  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));

  // Expected PSIM Admin APN list
  base::Value::List expected_psim_admin_assigned_apns;

  // First expected admin assigned APN details.
  base::Value::Dict first_psim_admin_assigned_apn;
  first_psim_admin_assigned_apn.Set(::onc::cellular_apn::kId, "admin-apn-id-y");
  first_psim_admin_assigned_apn.Set(::onc::cellular_apn::kAccessPointName,
                                    "test-apn-admin-y");
  first_psim_admin_assigned_apn.Set(::onc::cellular_apn::kAuthentication, "");
  first_psim_admin_assigned_apn.Set(::onc::cellular_apn::kUsername,
                                    "test-username-y");
  first_psim_admin_assigned_apn.Set(::onc::cellular_apn::kPassword,
                                    "test-password-y");
  first_psim_admin_assigned_apn.Set(::onc::cellular_apn::kSource,
                                    ::onc::cellular_apn::kSourceAdmin);

  // Second expected admin assigned APN details.
  base::Value::Dict second_psim_admin_assigned_apn;
  second_psim_admin_assigned_apn.Set(::onc::cellular_apn::kId,
                                     "admin-apn-id-x");
  second_psim_admin_assigned_apn.Set(::onc::cellular_apn::kAccessPointName,
                                     "test-apn-admin-x");
  second_psim_admin_assigned_apn.Set(::onc::cellular_apn::kAuthentication, "");
  second_psim_admin_assigned_apn.Set(::onc::cellular_apn::kUsername,
                                     "test-username-x");
  second_psim_admin_assigned_apn.Set(::onc::cellular_apn::kPassword,
                                     "test-password-x");
  second_psim_admin_assigned_apn.Set(::onc::cellular_apn::kSource,
                                     ::onc::cellular_apn::kSourceAdmin);

  // Add the APN details to the expected PSIM admin assigned APN list.
  expected_psim_admin_assigned_apns.Append(
      std::move(first_psim_admin_assigned_apn));
  expected_psim_admin_assigned_apns.Append(
      std::move(second_psim_admin_assigned_apn));

  // Get the constructed PSIM admin assigned APN list from the global network
  // configuration
  const auto* actual_psim_admin_assigned_apns =
      global_network_config.FindByDottedPath("PSIMAdminAssignedAPNs");

  // Verify that the constructed PSIM admin assigned APN list matches the
  // expected
  EXPECT_TRUE(actual_psim_admin_assigned_apns->GetList() ==
              expected_psim_admin_assigned_apns);
}

TEST(ONCUtils, ParseAndValidateOncForImport_AdminApnProvidedWithDuplicateIds) {
  const auto onc_blob = test_utils::ReadTestData("duplicate_admin_apn_ids.onc");
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;

  ASSERT_FALSE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY, std::string(),
      &network_configs, &global_network_config, &certificates));
}

TEST(ONCUtils, ParseAndValidateOncForImport_WithAdvancedOpenVPNSettings) {
  constexpr auto* auth_key =
      "-----BEGIN OpenVPN Static key V1-----\n"
      "83f8e7ccd99be189b4663e18615f9166\n"
      "d885cdea6c8accb0ebf5be304f0b8081\n"
      "5404f2a6574e029815d7a2fb65b83d0c\n"
      "676850714c6a56b23415a78e06aad6b1\n"
      "34900dd512049598382039e4816cb5ff\n"
      "1848532b71af47578c9b4a14b5bca49f\n"
      "99e0ae4dae2f4e5eadfea374aeb8fb1e\n"
      "a6fdf02adc73ea778dfd43d64bf7bc75\n"
      "7779d629498f8c2fbfd32812bfdf6df7\n"
      "8cebafafef3e5496cb13202274f2768a\n"
      "1959bc53d67a70945c4c8c6f34b63327\n"
      "fb60dc84990ffec1243461e0b6310f61\n"
      "e90aee1f11fb6292d6f5fcd7cd508aab\n"
      "50d80f9963589c148cb4b933ec86128d\n"
      "ed77d3fad6005b62f36369e2319f52bd\n"
      "09c6d2e52cce2362a05009dc29b6b39a\n"
      "-----END OpenVPN Static key V1-----\n";
  const auto onc_blob = test_utils::ReadTestData("valid_openvpn_full.onc");
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;

  ASSERT_TRUE(ParseAndValidateOncForImport(
      onc_blob, ::onc::ONCSource::ONC_SOURCE_USER_POLICY, "", &network_configs,
      &global_network_config, &certificates));

  const auto* open_vpn =
      network_configs[0].GetDict().FindByDottedPath("VPN.OpenVPN");
  ASSERT_NE(open_vpn, nullptr);
  base::Value::Dict expected;
  expected.Set(::onc::openvpn::kAuth, "MD5");
  expected.Set(::onc::openvpn::kCipher, "AES-192-CBC");
  expected.Set(::onc::openvpn::kCompressionAlgorithm,
               ::onc::openvpn_compression_algorithm::kLzo);
  expected.Set(::onc::openvpn::kTLSAuthContents, auth_key);
  expected.Set(::onc::openvpn::kKeyDirection, "1");
  EXPECT_THAT(open_vpn->GetDict(),
              base::test::DictionaryHasValues(std::move(expected)));
}

struct MaskCredentialsTestCase {
  // RAW_PTR_EXCLUSION: #global-scope
  RAW_PTR_EXCLUSION const OncValueSignature* onc_signature;
  const char* onc;
  const char* expected_after_masking;
};

using ONCUtilsMaskCredentialsTest =
    testing::TestWithParam<MaskCredentialsTestCase>;

TEST_P(ONCUtilsMaskCredentialsTest, Test) {
  std::optional<base::Value> onc_value = base::JSONReader::Read(GetParam().onc);
  ASSERT_TRUE(onc_value) << "Could not parse " << GetParam().onc;
  std::optional<base::Value> expected_after_masking_value =
      base::JSONReader::Read(GetParam().expected_after_masking);
  ASSERT_TRUE(expected_after_masking_value)
      << "Could not parse " << GetParam().expected_after_masking;

  base::Value::Dict masked = MaskCredentialsInOncObject(
      *(GetParam().onc_signature), onc_value->GetDict(), "******");

  EXPECT_EQ(masked, expected_after_masking_value->GetDict());
}

constexpr MaskCredentialsTestCase kMaskCredentialsTestCases[] = {
    // Actual passwords in the L2TP Password field in NetworkConfiguration are
    // masked.
    {&kNetworkConfigurationSignature,
     R"({ "GUID": "guid",
        "VPN": {
          "L2TP": {
            "Username": "some username",
            "Password": "secret_pwd"
          }
        }
      }
   )",
     R"({ "GUID": "guid",
        "VPN": {
          "L2TP": {
            "Username": "some username",
            "Password": "******"
          }
        }
      }
   )"},
    // The ${PASSWORD} variable in the L2TP Password field in
    // NetworkConfiguration is not masked.
    {&kNetworkConfigurationSignature,
     R"({ "GUID": "guid",
        "VPN": {
          "L2TP": {
            "Username": "some username",
            "Password": "${PASSWORD}"
          }
        }
      }
   )",
     R"({ "GUID": "guid",
        "VPN": {
          "L2TP": {
            "Username": "some username",
            "Password": "${PASSWORD}"
          }
        }
      }
   )"},
    // Actual passwords in the EAP Password field in NetworkConfiguration are
    // masked.
    {&kNetworkConfigurationSignature,
     R"({ "GUID": "guid",
        "WiFi": {
          "EAP": {
            "Identity": "some username",
            "Password": "secret_pwd"
          }
        }
      }
   )",
     R"({ "GUID": "guid",
        "WiFi": {
          "EAP": {
            "Identity": "some username",
            "Password": "******"
          }
        }
      }
   )"},
    // The ${PASSWORD} variable in the EAP Password field in
    // NetworkConfiguration is not masked.
    {&kNetworkConfigurationSignature,
     R"({ "GUID": "guid",
        "WiFi": {
          "EAP": {
            "Identity": "some username",
            "Password": "${PASSWORD}"
          }
        }
      }
   )",
     R"({ "GUID": "guid",
        "WiFi": {
          "EAP": {
            "Identity": "some username",
            "Password": "${PASSWORD}"
          }
        }
      }
   )"},
    // The PSK Passphrase is masked no matter if it contains ${PASSWORD} or not.
    {&kNetworkConfigurationSignature,
     R"({ "GUID": "guid",
        "WiFi": {
          "Passphrase": "${PASSWORD}"
        }
      }
   )",
     R"({ "GUID": "guid",
        "WiFi": {
          "Passphrase": "******"
        }
      }
   )"},
};

INSTANTIATE_TEST_SUITE_P(ONCUtilsMaskCredentialsTest,
                         ONCUtilsMaskCredentialsTest,
                         ::testing::ValuesIn(kMaskCredentialsTestCases));

}  // namespace chromeos::onc
