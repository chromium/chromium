// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_validator.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/onc/onc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {

class ONCValidatorTest : public ::testing::Test {
 public:
  // Validate |onc_object| with the given |signature|. The object is considered
  // to be managed if |managed_onc| is true. A strict validator is used if
  // |strict| is true. |onc_object| and the resulting repaired object of the
  // validation is stored, so that expectations can be checked afterwards using
  // one of the Expect* functions below.
  void Validate(bool strict,
                std::unique_ptr<base::Value> onc_object,
                const OncValueSignature* signature,
                bool managed_onc,
                ::onc::ONCSource onc_source) {
    std::unique_ptr<Validator> validator;
    if (strict) {
      // Create a strict validator that complains about every error.
      validator =
          std::make_unique<Validator>(true,  // error_on_unknown_field
                                      true,  // error_on_wrong_recommended
                                      true,  // error_on_missing_field,
                                      managed_onc,  // managed_onc
                                      true);        // log_warnings
    } else {
      // Create a liberal validator that ignores or repairs non-critical errors.
      validator =
          std::make_unique<Validator>(false,  // error_on_unknown_field
                                      false,  // error_on_wrong_recommended
                                      false,  // error_on_missing_field,
                                      managed_onc,  // managed_onc
                                      true);        // log_warnings
    }
    validator->SetOncSource(onc_source);
    original_object_ = base::DictionaryValue::From(std::move(onc_object));
    repaired_object_ = validator->ValidateAndRepairObject(signature,
                                                          *original_object_,
                                                          &validation_result_);
  }

  void ExpectValid() {
    EXPECT_EQ(Validator::VALID, validation_result_);
    EXPECT_TRUE(test_utils::Equals(original_object_.get(),
                                   repaired_object_.get()));
  }

  void ExpectRepairWithWarnings(
      const base::DictionaryValue& expected_repaired) {
    EXPECT_EQ(Validator::VALID_WITH_WARNINGS, validation_result_);
    EXPECT_TRUE(test_utils::Equals(&expected_repaired, repaired_object_.get()));
  }

  void ExpectInvalid() {
    EXPECT_EQ(Validator::INVALID, validation_result_);
    EXPECT_EQ(NULL, repaired_object_.get());
  }

 private:
  Validator::Result validation_result_;
  std::unique_ptr<const base::DictionaryValue> original_object_;
  std::unique_ptr<const base::DictionaryValue> repaired_object_;
};

namespace {

struct OncParams {
  // |location_of_object| is a string to identify the object to be tested. It
  // may be used as a filename or as a dictionary key.
  OncParams(const std::string& location_of_object,
            const OncValueSignature* onc_signature,
            bool is_managed_onc,
            ::onc::ONCSource onc_source = ::onc::ONC_SOURCE_NONE)
      : location(location_of_object),
        signature(onc_signature),
        is_managed(is_managed_onc),
        onc_source(onc_source) {
  }

  std::string location;
  const OncValueSignature* signature;
  bool is_managed;
  ::onc::ONCSource onc_source;
};

::std::ostream& operator<<(::std::ostream& os, const OncParams& onc) {
  return os << "(" << onc.location << ", " << onc.signature << ", "
            << (onc.is_managed ? "managed" : "unmanaged") << ", "
            << GetSourceAsString(onc.onc_source) << ")";
}

}  // namespace

// Ensure that the constant |kEmptyUnencryptedConfiguration| describes a valid
// ONC toplevel object.
TEST_F(ONCValidatorTest, EmptyUnencryptedConfiguration) {
  Validate(true, ReadDictionaryFromJson(kEmptyUnencryptedConfiguration),
           &kToplevelConfigurationSignature, false, ::onc::ONC_SOURCE_NONE);
  ExpectValid();
}

// This test case is about validating valid ONC objects without any errors. Both
// the strict and the liberal validator accept the object.
class ONCValidatorValidTest : public ONCValidatorTest,
                              public ::testing::WithParamInterface<OncParams> {
};

TEST_P(ONCValidatorValidTest, StrictValidationValid) {
  OncParams onc = GetParam();
  Validate(true, test_utils::ReadTestDictionary(onc.location), onc.signature,
           onc.is_managed, onc.onc_source);
  ExpectValid();
}

TEST_P(ONCValidatorValidTest, LiberalValidationValid) {
  OncParams onc = GetParam();
  Validate(false, test_utils::ReadTestDictionary(onc.location), onc.signature,
           onc.is_managed, onc.onc_source);
  ExpectValid();
}

// The parameters are:
// OncParams(string: Filename of a ONC file that is to be validated,
//           OncValueSignature: signature of that ONC,
//           bool: true if the ONC is managed).
INSTANTIATE_TEST_SUITE_P(
    ONCValidatorValidTest,
    ONCValidatorValidTest,
    ::testing::Values(
        OncParams("managed_toplevel1.onc",
                  &kToplevelConfigurationSignature,
                  true),
        OncParams("managed_toplevel2.onc",
                  &kToplevelConfigurationSignature,
                  true),
        OncParams("managed_toplevel_with_global_config.onc",
                  &kToplevelConfigurationSignature,
                  true),
        // Check that at least one configuration is accepted for
        // device policies.
        OncParams("managed_toplevel_wifi_peap.onc",
                  &kToplevelConfigurationSignature,
                  true,
                  ::onc::ONC_SOURCE_DEVICE_POLICY),
        // Disabled technologies are only allowed for device policies.
        OncParams("managed_toplevel_with_disabled_technologies.onc",
                  &kToplevelConfigurationSignature,
                  true,
                  ::onc::ONC_SOURCE_DEVICE_POLICY),
        // AllowOnlyPolicyNetworksToConnect is only allowed for device policies.
        OncParams("managed_toplevel_with_only_managed.onc",
                  &kToplevelConfigurationSignature,
                  true,
                  ::onc::ONC_SOURCE_DEVICE_POLICY),
        OncParams("managed_toplevel_l2tpipsec.onc",
                  &kToplevelConfigurationSignature,
                  true),
        OncParams("managed_toplevel_with_server_and_ca_cert.onc",
                  &kToplevelConfigurationSignature,
                  true),
        OncParams("toplevel_wifi_hexssid.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_wifi_ssid_and_hexssid.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_wifi_wpa_psk.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_wifi_wep_proxy.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_wifi_leap.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_wifi_eap_clientcert_with_cert_pems.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_wifi_remove.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_wifi_open.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_openvpn_clientcert_with_cert_pems.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_empty.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_only_global_config.onc",
                  &kToplevelConfigurationSignature,
                  true),
        OncParams("encrypted.onc", &kToplevelConfigurationSignature, true),
        OncParams("managed_vpn.onc", &kNetworkConfigurationSignature, true),
        OncParams("ethernet.onc", &kNetworkConfigurationSignature, true),
        OncParams("ethernet_with_eap.onc",
                  &kNetworkConfigurationSignature,
                  true),
        OncParams("translation_of_shill_ethernet_with_ipconfig.onc",
                  &kNetworkWithStateSignature,
                  true),
        OncParams("translation_of_shill_wifi_with_state.onc",
                  &kNetworkWithStateSignature,
                  false),
        OncParams("translation_of_shill_cellular_with_state.onc",
                  &kNetworkWithStateSignature,
                  false),
        OncParams("valid_openvpn_with_cert_pems.onc",
                  &kNetworkConfigurationSignature,
                  false),
        OncParams("openvpn_with_password.onc",
                  &kNetworkConfigurationSignature,
                  false),
        OncParams("third_party_vpn.onc",
                  &kNetworkConfigurationSignature,
                  false),
        OncParams("arc_vpn.onc", &kNetworkConfigurationSignature, false),
        OncParams("tether.onc", &kNetworkWithStateSignature, false),
        OncParams("cert_with_valid_scope.onc", &kCertificateSignature, false),
        OncParams("cert_with_explicit_default_scope.onc",
                  &kCertificateSignature,
                  false)));

namespace {

struct RepairParams {
  RepairParams(const std::string& strict_repaired,
               const std::string& liberal_repaired,
               bool liberal_valid)
      : location_of_strict_repaired(strict_repaired),
        location_of_liberal_repaired(liberal_repaired),
        expect_liberal_valid(liberal_valid) {}

  std::string location_of_strict_repaired;
  std::string location_of_liberal_repaired;
  bool expect_liberal_valid;
};

// Both |strict_repaired| and |liberal_repaired| are strings to identify the
// object that is expected as the validation result. They may either be used
// as filenames or as dictionary keys.
RepairParams ExpectBothNotValid(const std::string& strict_repaired,
                                const std::string& liberal_repaired) {
  return RepairParams(strict_repaired, liberal_repaired, false);
}

::std::ostream& operator<<(::std::ostream& os, const RepairParams& rp) {
  if (rp.expect_liberal_valid) {
    os << "(" << rp.location_of_strict_repaired << ", liberal is valid)";
  } else {
    os << "(" << rp.location_of_strict_repaired << ", "
       << rp.location_of_liberal_repaired << ")";
  }
  return os;
}

}  // namespace

// This test case is about validating ONC objects that contain errors which can
// be repaired (then the errors count as warnings). If a location of the
// expected repaired object is given, then it is checked that the validator
// (either strict or liberal) returns this repaired object and the result is
// VALID_WITH_WARNINGS. If the location is the empty string, then it is expected
// that the validator returns NULL and the result INVALID.
class ONCValidatorTestRepairable
    : public ONCValidatorTest,
      public ::testing::WithParamInterface<std::pair<OncParams, RepairParams>> {
 public:
  // Load the common test data and return the dictionary at the field with
  // name |name|.
  std::unique_ptr<base::DictionaryValue> GetDictionaryFromTestFile(
      const std::string& name) {
    std::unique_ptr<const base::DictionaryValue> dict(
        test_utils::ReadTestDictionary("invalid_settings_with_repairs.json"));
    const base::DictionaryValue* onc_object = NULL;
    CHECK(dict->GetDictionary(name, &onc_object));
    return base::WrapUnique(onc_object->DeepCopy());
  }
};

TEST_P(ONCValidatorTestRepairable, StrictValidation) {
  OncParams onc = GetParam().first;
  Validate(true, GetDictionaryFromTestFile(onc.location), onc.signature,
           onc.is_managed, onc.onc_source);
  std::string location_of_repaired =
      GetParam().second.location_of_strict_repaired;
  if (location_of_repaired.empty())
    ExpectInvalid();
  else
    ExpectRepairWithWarnings(*GetDictionaryFromTestFile(location_of_repaired));
}

TEST_P(ONCValidatorTestRepairable, LiberalValidation) {
  OncParams onc = GetParam().first;
  Validate(false, GetDictionaryFromTestFile(onc.location), onc.signature,
           onc.is_managed, onc.onc_source);
  if (GetParam().second.expect_liberal_valid) {
    ExpectValid();
  } else {
    std::string location_of_repaired =
        GetParam().second.location_of_liberal_repaired;
    if (location_of_repaired.empty())
      ExpectInvalid();
    else
      ExpectRepairWithWarnings(
          *GetDictionaryFromTestFile(location_of_repaired));
  }
}

// The parameters for all test case instantations below are:
// OncParams(string: A fieldname in the dictionary from the file
//                   "invalid_settings_with_repairs.json". That nested
//                   dictionary will be tested.
//           OncValueSignature: signature of that ONC,
//           bool: true if the ONC is managed).
//
// If both strict and liberal validation are expected to be not valid:
//  ExpectBothNotValid(string: A fieldname in the dictionary from the file
//                      "invalid_settings_with_repairs.json". That nested
//                      dictionary is the expected result from strict
//                      validation,
//                     string: A fieldname in the dictionary from the file
//                      "invalid_settings_with_repairs.json". That nested
//                      dictionary is the expected result from liberal
//                      validation).

// Strict validator returns INVALID. Liberal validator returns
// VALID_WITH_WARNINGS (unrepaired).
INSTANTIATE_TEST_SUITE_P(
    StrictInvalidLiberalValidWithWarnings,
    ONCValidatorTestRepairable,
    ::testing::Values(
        std::make_pair(OncParams("network-missing-required",
                                 &kNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("", "network-missing-required")),
        std::make_pair(OncParams("network-missing-required-type",
                                 &kNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("", "network-missing-required-type")),
        std::make_pair(OncParams("managed-network-missing-required",
                                 &kNetworkConfigurationSignature,
                                 true),
                       ExpectBothNotValid("",
                                          "managed-network-missing-required")),
        std::make_pair(OncParams("openvpn-missing-verify-x509-name",
                                 &kNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("",
                                          "openvpn-missing-verify-x509-name")),
        std::make_pair(
            OncParams("third-party-vpn-missing-extension-id",
                      &kNetworkConfigurationSignature,
                      false),
            ExpectBothNotValid("", "third-party-vpn-missing-extension-id")),
        std::make_pair(OncParams("tether-missing-battery-percentage",
                                 &kNetworkWithStateSignature,
                                 true),
                       ExpectBothNotValid("",
                                          "tether-missing-battery-percentage")),
        std::make_pair(OncParams("tether-missing-carrier",
                                 &kNetworkWithStateSignature,
                                 true),
                       ExpectBothNotValid("", "tether-missing-carrier")),
        std::make_pair(
            OncParams("tether-missing-has-connected-to-host",
                      &kNetworkWithStateSignature,
                      true),
            ExpectBothNotValid("", "tether-missing-has-connected-to-host")),
        std::make_pair(OncParams("tether-missing-signal-strength",
                                 &kNetworkWithStateSignature,
                                 true),
                       ExpectBothNotValid("",
                                          "tether-missing-signal-strength"))));

// Strict validator returns INVALID. Liberal validator repairs.
INSTANTIATE_TEST_SUITE_P(
    StrictInvalidLiberalRepair,
    ONCValidatorTestRepairable,
    ::testing::Values(
        std::make_pair(OncParams("network-unknown-fieldname",
                                 &kNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("", "network-repaired")),
        std::make_pair(OncParams("managed-network-unknown-fieldname",
                                 &kNetworkConfigurationSignature,
                                 true),
                       ExpectBothNotValid("", "managed-network-repaired")),
        std::make_pair(OncParams("managed-network-unknown-recommended",
                                 &kNetworkConfigurationSignature,
                                 true),
                       ExpectBothNotValid("", "managed-network-repaired")),
        std::make_pair(OncParams("managed-network-dict-recommended",
                                 &kNetworkConfigurationSignature,
                                 true),
                       ExpectBothNotValid("", "managed-network-repaired")),
        // Ensure that state values from Shill aren't accepted as
        // configuration.
        std::make_pair(OncParams("network-state-field",
                                 &kNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("", "network-repaired")),
        std::make_pair(
            OncParams("network-nested-state-field",
                      &kNetworkConfigurationSignature,
                      false),
            ExpectBothNotValid("", "network-nested-state-field-repaired")),
        std::make_pair(OncParams("network-with-ipconfigs",
                                 &kNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("", "network-repaired")),
        std::make_pair(
            OncParams("ipsec-with-client-cert-missing-cacert",
                      &kIPsecSignature,
                      false),
            ExpectBothNotValid("", "ipsec-with-client-cert-missing-cacert")),
        std::make_pair(OncParams("toplevel-with-repairable-networks",
                                 &kToplevelConfigurationSignature,
                                 false,
                                 ::onc::ONC_SOURCE_DEVICE_POLICY),
                       ExpectBothNotValid("",
                                          "toplevel-with-repaired-networks"))));

// Strict and liberal validator repair identically.
INSTANTIATE_TEST_SUITE_P(
    StrictAndLiberalRepairIdentically,
    ONCValidatorTestRepairable,
    ::testing::Values(
        std::make_pair(OncParams("toplevel-invalid-network",
                                 &kToplevelConfigurationSignature,
                                 false),
                       ExpectBothNotValid("toplevel-repaired",
                                          "toplevel-repaired")),
        std::make_pair(OncParams("duplicate-network-guid",
                                 &kToplevelConfigurationSignature,
                                 false),
                       ExpectBothNotValid("repaired-duplicate-network-guid",
                                          "repaired-duplicate-network-guid")),
        std::make_pair(OncParams("duplicate-cert-guid",
                                 &kToplevelConfigurationSignature,
                                 false),
                       ExpectBothNotValid("repaired-duplicate-cert-guid",
                                          "repaired-duplicate-cert-guid")),
        std::make_pair(OncParams("toplevel-invalid-network",
                                 &kToplevelConfigurationSignature,
                                 true),
                       ExpectBothNotValid("toplevel-repaired",
                                          "toplevel-repaired")),
        // Ignore recommended arrays in unmanaged ONC.
        std::make_pair(OncParams("network-with-illegal-recommended",
                                 &kNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("network-repaired",
                                          "network-repaired")),
        std::make_pair(OncParams("toplevel-with-vpn",
                                 &kToplevelConfigurationSignature,
                                 false,
                                 ::onc::ONC_SOURCE_DEVICE_POLICY),
                       ExpectBothNotValid("toplevel-empty", "toplevel-empty")),
        std::make_pair(OncParams("wifi-ssid-and-hexssid-inconsistent",
                                 &kNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("wifi-ssid-and-hexssid-repaired",
                                          "wifi-ssid-and-hexssid-repaired")),
        std::make_pair(OncParams("wifi-ssid-and-hexssid-partially-invalid",
                                 &kNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("wifi-ssid-and-hexssid-repaired",
                                          "wifi-ssid-and-hexssid-repaired"))));

// Strict and liberal validator both repair, but with different results.
INSTANTIATE_TEST_SUITE_P(
    StrictAndLiberalRepairDifferently,
    ONCValidatorTestRepairable,
    ::testing::Values(std::make_pair(OncParams("toplevel-with-nested-warning",
                                               &kToplevelConfigurationSignature,
                                               false),
                                     ExpectBothNotValid("toplevel-empty",
                                                        "toplevel-repaired"))));

// Strict and liberal validator return both INVALID.
INSTANTIATE_TEST_SUITE_P(
    StrictAndLiberalInvalid,
    ONCValidatorTestRepairable,
    ::testing::Values(
        std::make_pair(OncParams("global-disabled-technologies",
                                 &kGlobalNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("network-unknown-value",
                                 &kNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("wifi-hexssid-invalid-length",
                                 &kNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("wifi-ssid-invalid-length",
                                 &kNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("wifi-invalid-hexssid",
                                 &kNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("managed-network-unknown-value",
                                 &kNetworkConfigurationSignature,
                                 true),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("network-value-out-of-range",
                                 &kNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("", "")),
        std::make_pair(
            OncParams("ipsec-with-psk-and-cacert", &kIPsecSignature, false),
            ExpectBothNotValid("", "")),
        std::make_pair(
            OncParams("ipsec-with-empty-cacertrefs", &kIPsecSignature, false),
            ExpectBothNotValid("", "")),
        std::make_pair(OncParams("ipsec-with-servercaref-and-servercarefs",
                                 &kIPsecSignature,
                                 false),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("openvpn-with-servercaref-and-servercarefs",
                                 &kOpenVPNSignature,
                                 false),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("eap-with-servercaref-and-servercarefs",
                                 &kEAPSignature,
                                 false),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("managed-network-value-out-of-range",
                                 &kNetworkConfigurationSignature,
                                 true),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("network-wrong-type",
                                 &kNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("managed-network-wrong-type",
                                 &kNetworkConfigurationSignature,
                                 true),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("openvpn-invalid-verify-x509-type",
                                 &kNetworkConfigurationSignature,
                                 false),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("tether-negative-battery",
                                 &kNetworkWithStateSignature,
                                 true),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("tether-battery-over-100",
                                 &kNetworkWithStateSignature,
                                 true),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("tether-negative-signal-strength",
                                 &kNetworkWithStateSignature,
                                 true),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("tether-signal-strength-over-100",
                                 &kNetworkWithStateSignature,
                                 true),
                       ExpectBothNotValid("", "")),
        std::make_pair(
            OncParams("invalid-scope-due-to-type", &kScopeSignature, true),
            ExpectBothNotValid("", "")),
        std::make_pair(OncParams("invalid-scope-due-to-missing-id",
                                 &kScopeSignature,
                                 true),
                       ExpectBothNotValid("",
                                          "invalid-scope-due-to-missing-id")),
        std::make_pair(OncParams("invalid-scope-due-to-invalid-id-length",
                                 &kScopeSignature,
                                 true),
                       ExpectBothNotValid("", "")),
        std::make_pair(OncParams("invalid-scope-due-to-invalid-id-character",
                                 &kScopeSignature,
                                 true),
                       ExpectBothNotValid("", "")),
        std::make_pair(
            OncParams("invalid-scope-due-to-missing-type",
                      &kScopeSignature,
                      true),
            ExpectBothNotValid("", "invalid-scope-due-to-missing-type"))));

}  // namespace onc
}  // namespace chromeos
