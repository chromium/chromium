// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/onc/onc_utils.h"

#include <string>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "chromeos/components/onc/variable_expander.h"
#include "components/onc/onc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace onc {

TEST(ONCDecrypterTest, BrokenEncryptionIterations) {
  base::Value encrypted_onc =
      test_utils::ReadTestDictionaryValue("broken-encrypted-iterations.onc");

  base::Value decrypted_onc = Decrypt("test0000", encrypted_onc);

  EXPECT_TRUE(decrypted_onc.is_none());
}

TEST(ONCDecrypterTest, BrokenEncryptionZeroIterations) {
  base::Value encrypted_onc = test_utils::ReadTestDictionaryValue(
      "broken-encrypted-zero-iterations.onc");

  base::Value decrypted_onc = Decrypt("test0000", encrypted_onc);

  EXPECT_TRUE(decrypted_onc.is_none());
}

TEST(ONCDecrypterTest, LoadEncryptedOnc) {
  base::Value encrypted_onc =
      test_utils::ReadTestDictionaryValue("encrypted.onc");
  base::Value expected_decrypted_onc =
      test_utils::ReadTestDictionaryValue("decrypted.onc");

  std::string error;
  base::Value actual_decrypted_onc = Decrypt("test0000", encrypted_onc);

  base::Value emptyDict;
  EXPECT_TRUE(
      test_utils::Equals(&expected_decrypted_onc, &actual_decrypted_onc));
}

namespace {

const char* kLoginId = "hans";
const char* kLoginEmail = "hans@my.domain.com";

base::flat_map<std::string, std::string> GetTestStringSubstutions() {
  base::flat_map<std::string, std::string> substitutions;
  substitutions[::onc::substitutes::kLoginID] = kLoginId;
  substitutions[::onc::substitutes::kLoginEmail] = kLoginEmail;
  return substitutions;
}

}  // namespace

TEST(ONCStringExpansion, OpenVPN) {
  base::Value vpn_onc =
      test_utils::ReadTestDictionaryValue("valid_openvpn.onc");

  VariableExpander variable_expander(GetTestStringSubstutions());
  ExpandStringsInOncObject(kNetworkConfigurationSignature, variable_expander,
                           &vpn_onc);

  std::string* actual_expanded = vpn_onc.FindStringPath("VPN.OpenVPN.Username");
  ASSERT_TRUE(actual_expanded);
  EXPECT_EQ(*actual_expanded, std::string("abc ") + kLoginEmail + " def");
}

TEST(ONCStringExpansion, WiFi_EAP) {
  base::Value wifi_onc =
      test_utils::ReadTestDictionaryValue("wifi_clientcert_with_cert_pems.onc");

  VariableExpander variable_expander(GetTestStringSubstutions());
  ExpandStringsInOncObject(kNetworkConfigurationSignature, variable_expander,
                           &wifi_onc);

  std::string* actual_expanded = wifi_onc.FindStringPath("WiFi.EAP.Identity");
  ASSERT_TRUE(actual_expanded);
  EXPECT_EQ(*actual_expanded,
            std::string("abc ") + kLoginId + "@my.domain.com");
}

TEST(ONCResolveServerCertRefs, ResolveServerCertRefs) {
  base::Value::Dict test_cases = test_utils::ReadTestDictionaryValue(
                                     "network_configs_with_resolved_certs.json")
                                     .TakeDict();

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
      test_utils::ReadTestDictionaryValue("wifi_clientcert_with_cert_pems.onc")
          .TakeDict();
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
  base::Value::Dict wifi_onc = test_utils::ReadTestDictionaryValue(
                                   "translation_of_shill_wifi_with_state.onc")
                                   .TakeDict();
  base::Value::Dict* wifi_fields = wifi_onc.FindDict("WiFi");
  ASSERT_TRUE(wifi_fields);

  ASSERT_TRUE(wifi_fields->Find(::onc::wifi::kHiddenSSID));
  SetHiddenSSIDField(*wifi_fields);
  EXPECT_FALSE(wifi_fields->Find(::onc::wifi::kHiddenSSID)->GetBool());
}

TEST(ONCUtils, SetHiddenSSIDField_WithValueSetTrue) {
  // WiFi configuration that have HiddenSSID field set to true.
  base::Value::Dict wifi_onc =
      std::move(test_utils::ReadTestDictionaryValue("wifi_with_hidden_ssid.onc")
                    .GetDict());
  base::Value::Dict* wifi_fields = wifi_onc.FindDict("WiFi");
  ASSERT_TRUE(wifi_fields);

  ASSERT_TRUE(wifi_fields->Find(::onc::wifi::kHiddenSSID));
  SetHiddenSSIDField(*wifi_fields);
  EXPECT_TRUE(wifi_fields->Find(::onc::wifi::kHiddenSSID)->GetBool());
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
  base::Value::Dict expected{};
  expected.Set(::onc::openvpn::kAuth, "MD5");
  expected.Set(::onc::openvpn::kCipher, "AES-192-CBC");
  expected.Set(::onc::openvpn::kCompressionAlgorithm,
               ::onc::openvpn_compression_algorithm::kLzo);
  expected.Set(::onc::openvpn::kTLSAuthContents, auth_key);
  expected.Set(::onc::openvpn::kKeyDirection, "1");
  EXPECT_THAT(*open_vpn, base::test::DictionaryHasValues(
                             base::Value{std::move(expected)}));
}

struct MaskCredentialsTestCase {
  const OncValueSignature* onc_signature;
  const char* onc;
  const char* expected_after_masking;
};

using ONCUtilsMaskCredentialsTest =
    testing::TestWithParam<MaskCredentialsTestCase>;

TEST_P(ONCUtilsMaskCredentialsTest, Test) {
  absl::optional<base::Value> onc_value =
      base::JSONReader::Read(GetParam().onc);
  ASSERT_TRUE(onc_value) << "Could not parse " << GetParam().onc;
  absl::optional<base::Value> expected_after_masking_value =
      base::JSONReader::Read(GetParam().expected_after_masking);
  ASSERT_TRUE(expected_after_masking_value)
      << "Could not parse " << GetParam().expected_after_masking;

  base::Value masked = MaskCredentialsInOncObject(*(GetParam().onc_signature),
                                                  *onc_value, "******");

  EXPECT_EQ(masked, *expected_after_masking_value);
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

}  // namespace onc
}  // namespace chromeos
