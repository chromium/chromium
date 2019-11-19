// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_translator.h"

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "components/onc/onc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {

// First parameter: Filename of source ONC.
// Second parameter: Filename of expected translated Shill json.
class ONCTranslatorOncToShillTest
    : public ::testing::TestWithParam<std::pair<std::string, std::string>> {};

// Test the translation from ONC to Shill json.
TEST_P(ONCTranslatorOncToShillTest, Translate) {
  std::string source_onc_filename = GetParam().first;
  std::unique_ptr<const base::DictionaryValue> onc_network(
      test_utils::ReadTestDictionary(source_onc_filename));
  std::string result_shill_filename = GetParam().second;
  std::unique_ptr<const base::DictionaryValue> expected_shill_network(
      test_utils::ReadTestDictionary(result_shill_filename));

  std::unique_ptr<base::DictionaryValue> translation(
      TranslateONCObjectToShill(&kNetworkConfigurationSignature, *onc_network));

  EXPECT_TRUE(test_utils::Equals(expected_shill_network.get(),
                                 translation.get()));
}

// Test different network types, such that each ONC object type is tested at
// least once.
INSTANTIATE_TEST_SUITE_P(
    ONCTranslatorOncToShillTest,
    ONCTranslatorOncToShillTest,
    ::testing::Values(
        std::make_pair("ethernet.onc", "shill_ethernet.json"),
        std::make_pair("ethernet_with_eap_and_cert_pems.onc",
                       "shill_ethernet_with_eap.json"),
        std::make_pair("valid_wifi_psk.onc", "shill_wifi_psk.json"),
        std::make_pair("wifi_clientcert_with_cert_pems.onc",
                       "shill_wifi_clientcert.json"),
        std::make_pair("wifi_wep_8021x_clientcert_with_cert_pems.onc",
                       "shill_wifi_wep_8021x_clientcert.json"),
        std::make_pair("valid_wifi_clientref.onc", "shill_wifi_clientref.json"),
        std::make_pair("valid_l2tpipsec.onc", "shill_l2tpipsec.json"),
        std::make_pair("wifi_dhcp.onc", "shill_wifi_dhcp.json"),
        std::make_pair("wifi_eap_tls.onc", "shill_wifi_eap_tls.json"),
        std::make_pair("wifi_eap_ttls.onc", "shill_wifi_eap_ttls.json"),
        std::make_pair("wifi_proxy.onc", "shill_wifi_proxy.json"),
        std::make_pair("wifi_proxy_pac.onc", "shill_wifi_proxy_pac.json"),
        std::make_pair("l2tpipsec_clientcert_with_cert_pems.onc",
                       "shill_l2tpipsec_clientcert.json"),
        std::make_pair("valid_openvpn_with_cert_pems.onc",
                       "shill_openvpn.json"),
        std::make_pair("openvpn_clientcert_with_cert_pems.onc",
                       "shill_openvpn_clientcert.json"),
        std::make_pair("openvpn_clientcert_pkcs11.onc",
                       "shill_openvpn_clientcert_pkcs11.json"),
        std::make_pair("vpn_ipsec_clientcert_pkcs11.onc",
                       "shill_vpn_ipsec_clientcert_pkcs11.json"),
        std::make_pair("cellular.onc", "shill_cellular.json"),
        // WiMAX is deprecated, but we need to ensure older ONC configurations
        // are handled gracefully.
        std::make_pair("wimax.onc", "shill_wimax.json"),
        std::make_pair("third_party_vpn.onc", "shill_third_party_vpn.json"),
        std::make_pair("arc_vpn.onc", "shill_arc_vpn.json"),
        std::make_pair("wifi_eap_ttls_with_password_variable.onc",
                       "shill_wifi_eap_ttls_with_password_variable.json"),
        std::make_pair("wifi_eap_ttls_with_hardcoded_password.onc",
                       "shill_wifi_eap_ttls_with_hardcoded_password.json")));

// First parameter: Filename of source Shill json.
// Second parameter: Filename of expected translated ONC network part.
//
// Note: This translation direction doesn't have to reconstruct all of the ONC
// fields, as Chrome doesn't need all of a Service's properties.
class ONCTranslatorShillToOncTest
    : public ::testing::TestWithParam<std::pair<std::string, std::string>> {};

TEST_P(ONCTranslatorShillToOncTest, Translate) {
  std::string source_shill_filename = GetParam().first;
  std::unique_ptr<const base::DictionaryValue> shill_network(
      test_utils::ReadTestDictionary(source_shill_filename));

  std::string result_onc_filename = GetParam().second;
  std::unique_ptr<base::DictionaryValue> expected_onc_network(
      test_utils::ReadTestDictionary(result_onc_filename));

  std::unique_ptr<base::DictionaryValue> translation(
      TranslateShillServiceToONCPart(*shill_network, ::onc::ONC_SOURCE_NONE,
                                     &kNetworkWithStateSignature,
                                     nullptr /* network_state */));

  EXPECT_TRUE(test_utils::Equals(expected_onc_network.get(),
                                 translation.get()));
}

INSTANTIATE_TEST_SUITE_P(
    ONCTranslatorShillToOncTest,
    ONCTranslatorShillToOncTest,
    ::testing::Values(
        std::make_pair("shill_ethernet.json",
                       "translation_of_shill_ethernet.onc"),
        std::make_pair("shill_ethernet_with_eap.json",
                       "translation_of_shill_ethernet_with_eap.onc"),
        std::make_pair("shill_ethernet_with_ipconfig.json",
                       "translation_of_shill_ethernet_with_ipconfig.onc"),
        std::make_pair("shill_wifi_clientcert.json",
                       "translation_of_shill_wifi_clientcert.onc"),
        std::make_pair("shill_wifi_non_utf8_ssid.json",
                       "translation_of_shill_wifi_non_utf8_ssid.onc"),
        std::make_pair("shill_wifi_wep_8021x_clientcert.json",
                       "translation_of_shill_wifi_wep_8021x_clientcert.onc"),
        std::make_pair("shill_output_l2tpipsec.json",
                       "translation_of_shill_l2tpipsec.onc"),
        std::make_pair("shill_output_openvpn.json",
                       "translation_of_shill_openvpn.onc"),
        std::make_pair("shill_output_openvpn_with_errors.json",
                       "translation_of_shill_openvpn_with_errors.onc"),
        std::make_pair("shill_output_openvpn_clientcert.json",
                       "translation_of_shill_openvpn_clientcert.onc"),
        std::make_pair("shill_output_vpn_ipsec.json",
                       "translation_of_shill_vpn_ipsec.onc"),
        std::make_pair("shill_tether.json", "tether.onc"),
        std::make_pair("shill_wifi_with_state.json",
                       "translation_of_shill_wifi_with_state.onc"),
        std::make_pair("shill_wifi_eap_tls.json",
                       "translation_of_shill_wifi_eap_tls.onc"),
        std::make_pair("shill_wifi_eap_ttls.json",
                       "translation_of_shill_wifi_eap_ttls.onc"),
        std::make_pair("shill_wifi_proxy.json",
                       "translation_of_shill_wifi_proxy.onc"),
        std::make_pair("shill_wifi_proxy_pac.json",
                       "translation_of_shill_wifi_proxy_pac.onc"),
        std::make_pair("shill_cellular_with_state.json",
                       "translation_of_shill_cellular_with_state.onc"),
        std::make_pair("shill_output_third_party_vpn.json",
                       "third_party_vpn.onc"),
        std::make_pair(
            "shill_wifi_eap_ttls_with_password_variable.json",
            "translation_of_shill_wifi_eap_ttls_with_password_variable.onc"),
        std::make_pair(
            "shill_wifi_eap_ttls_with_hardcoded_password.json",
            "translation_of_shill_wifi_eap_ttls_with_hardcoded_password.onc")));

}  // namespace onc
}  // namespace chromeos
