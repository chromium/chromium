// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/onc/onc_validator.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/components/onc/onc_signature.h"
#include "components/crx_file/id_util.h"
#include "components/device_event_log/device_event_log.h"
#include "components/onc/onc_constants.h"
#include "onc_signature.h"

namespace chromeos::onc {

namespace {

// According to the IEEE 802.11 standard the SSID is a series of 0 to 32 octets.
const int kMaximumSSIDLengthInBytes = 32;

// Valid top-level configuration types
const std::vector<const char*>& GetValidToplevelConfigurationTypes() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::toplevel_config::kUnencryptedConfiguration,
       ::onc::toplevel_config::kEncryptedConfiguration});
  return *valid_values;
}

// Valid network types
const std::vector<const char*>& GetValidNetworkTypes() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::network_type::kEthernet, ::onc::network_type::kVPN,
       ::onc::network_type::kWiFi, ::onc::network_type::kCellular,
       ::onc::network_type::kTether});
  return *valid_values;
}

// Valid cellular IP configuration types
const std::vector<const char*>& GetValidIPConfigTypes() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::network_config::kIPConfigTypeDHCP,
       ::onc::network_config::kIPConfigTypeStatic});
  return *valid_values;
}

// Valid check captive portal values
const std::vector<const char*>& GetValidCheckCaptivePortalValues() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::check_captive_portal::kTrue, ::onc::check_captive_portal::kFalse,
       ::onc::check_captive_portal::kHTTPOnly});
  return *valid_values;
}

// Valid cellular APN IP types
const std::vector<const char*>& GetValidAPNIpTypes() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::cellular_apn::kIpTypeAutomatic, ::onc::cellular_apn::kIpTypeIpv4,
       ::onc::cellular_apn::kIpTypeIpv6, ::onc::cellular_apn::kIpTypeIpv4Ipv6});
  return *valid_values;
}

// Valid APN types
const std::vector<const char*>& GetValidApnTypes() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::cellular_apn::kApnTypeDefault,
       ::onc::cellular_apn::kApnTypeAttach,
       ::onc::cellular_apn::kApnTypeTether});
  return *valid_values;
}

// Valid ethernet authentications
const std::vector<const char*>& GetValidEthernetAuthentications() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::ethernet::kAuthenticationNone, ::onc::ethernet::k8021X});
  return *valid_values;
}

// Valid network IP config types
const std::vector<const char*>& GetValidNetworkIPConfigTypes() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::ipconfig::kIPv4, ::onc::ipconfig::kIPv6});
  return *valid_values;
}

// Valid Wi-Fi securities
const std::vector<const char*>& GetValidWiFiSecurities() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::wifi::kSecurityNone, ::onc::wifi::kWEP_PSK,
       ::onc::wifi::kWEP_8021X, ::onc::wifi::kWPA_PSK, ::onc::wifi::kWPA_EAP});
  return *valid_values;
}

// Valid IPSec authentications
const std::vector<const char*>& GetValidIPsecAuthentications() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::ipsec::kPSK, ::onc::ipsec::kCert, ::onc::ipsec::kEAP});
  return *valid_values;
}

// Valid OpenVPN auth retry values
const std::vector<const char*>& GetValidVPNAuthRetryValues() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::openvpn::kNone, ::onc::openvpn::kInteract,
       ::onc::openvpn::kNoInteract});
  return *valid_values;
}

// Valid OpenVPN cert TLS values
const std::vector<const char*>& GetValidVPNCertTlsValues() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::openvpn::kNone, ::onc::openvpn::kServer});
  return *valid_values;
}

// Valid OpenVPN compression algorithm values
const std::vector<const char*>& GetValidVPNCompressionAlgorithmValues() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::openvpn_compression_algorithm::kFramingOnly,
       ::onc::openvpn_compression_algorithm::kLz4,
       ::onc::openvpn_compression_algorithm::kLz4V2,
       ::onc::openvpn_compression_algorithm::kLzo,
       ::onc::openvpn_compression_algorithm::kNone});
  return *valid_values;
}

// Valid OpenVPN user auth types
const std::vector<const char*>& GetValidVPNUserAuthTypes() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::openvpn_user_auth_type::kNone,
       ::onc::openvpn_user_auth_type::kOTP,
       ::onc::openvpn_user_auth_type::kPassword,
       ::onc::openvpn_user_auth_type::kPasswordAndOTP});
  return *valid_values;
}

// Valid X.509 types
const std::vector<const char*>& GetValidX509Types() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::verify_x509::types::kName, ::onc::verify_x509::types::kNamePrefix,
       ::onc::verify_x509::types::kSubject});
  return *valid_values;
}

// Valid allow text messages types
const std::vector<const char*>& GetValidAllowTextMessagesTypes() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::cellular::kTextMessagesAllow,
       ::onc::cellular::kTextMessagesSuppress,
       ::onc::cellular::kTextMessagesUnset});
  return *valid_values;
}

// Valid proxy settings types
const std::vector<const char*>& GetValidProxySettingsTypes() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::proxy::kDirect, ::onc::proxy::kManual, ::onc::proxy::kPAC,
       ::onc::proxy::kWPAD});
  return *valid_values;
}

// Valid EAP inner values
const std::vector<const char*>& GetValidEAPInnerValues() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::eap::kAutomatic, ::onc::eap::kGTC, ::onc::eap::kMD5,
       ::onc::eap::kMSCHAPv2, ::onc::eap::kPAP});
  return *valid_values;
}

// Valid EAP outer values
const std::vector<const char*>& GetValidEAPOuterValues() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::eap::kPEAP, ::onc::eap::kEAP_TLS, ::onc::eap::kEAP_TTLS,
       ::onc::eap::kLEAP, ::onc::eap::kEAP_SIM, ::onc::eap::kEAP_FAST,
       ::onc::eap::kEAP_AKA});
  return *valid_values;
}

// Valid EAP Subject Alternative Name match types
const std::vector<const char*>& GetValidEAPSubjectAlternativeNameMatchTypes() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::eap_subject_alternative_name_match::kEMAIL,
       ::onc::eap_subject_alternative_name_match::kDNS,
       ::onc::eap_subject_alternative_name_match::kURI});
  return *valid_values;
}

// Valid certificate types
const std::vector<const char*>& GetValidCertificateTypes() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::certificate::kClient, ::onc::certificate::kServer,
       ::onc::certificate::kAuthority});
  return *valid_values;
}

// Valid scope types
const std::vector<const char*>& GetValidScopeTypes() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::scope::kDefault, ::onc::scope::kExtension});
  return *valid_values;
}

// All valid EAP types
const std::vector<const char*>& GetAllValidVPNTypes() {
  static const base::NoDestructor<std::vector<const char*>> valid_values(
      {::onc::vpn::kIPsec, ::onc::vpn::kTypeL2TP_IPsec, ::onc::vpn::kOpenVPN,
       ::onc::vpn::kWireGuard, ::onc::vpn::kThirdPartyVpn, ::onc::vpn::kArcVpn

      });
  return *valid_values;
}

// Valid managed EAP types
const std::vector<const char*>& GetValidManagedVPNTypes() {
  static const base::NoDestructor<std::vector<const char*>> valid_values({
      ::onc::vpn::kIPsec,
      ::onc::vpn::kTypeL2TP_IPsec,
      ::onc::vpn::kOpenVPN,
      ::onc::vpn::kWireGuard,
  });
  return *valid_values;
}

void AddKeyToList(const char* key, base::Value::List* list) {
  base::Value key_value(key);
  if (!base::Contains(*list, key_value)) {
    list->Append(std::move(key_value));
  }
}

std::string GetStringFromDict(const base::Value::Dict& dict, const char* key) {
  const std::string* value = dict.FindString(key);
  return value ? *value : std::string();
}

base::flat_set<std::string> GetStringsFromDicts(const base::Value::List& dicts,
                                                const char* key) {
  base::flat_set<std::string> values;
  for (const base::Value& dict : dicts) {
    if (!dict.is_dict()) {
      continue;
    }

    const std::string* value = dict.GetDict().FindString(key);
    if (!value) {
      continue;
    }

    values.emplace(*value);
  }
  return values;
}

bool FieldIsRecommended(const base::Value::Dict& object,
                        const std::string& field_name) {
  const base::Value::List* recommended = object.FindList(::onc::kRecommended);
  return recommended && base::Contains(*recommended, base::Value(field_name));
}

bool FieldIsSetToValueOrRecommended(const base::Value::Dict& object,
                                    const std::string& field_name,
                                    const base::Value& expected_value) {
  const base::Value* actual_value = object.Find(field_name);
  if (actual_value && expected_value == *actual_value)
    return true;

  return FieldIsRecommended(object, field_name);
}

// Determines whether the values associated with a specific key within a list of
// dictionaries are all unique.
bool HasUniqueValuesForKeyInDicts(const base::Value::List& dicts,
                                  const std::string& key) {
  base::flat_set<base::Value> seen_values;
  for (const base::Value& dict : dicts) {
    if (!dict.is_dict()) {
      return false;
    }

    const base::Value* value = dict.GetDict().Find(key);
    if (!value || seen_values.count(*value) > 0) {
      return false;
    }

    seen_values.insert(value->Clone());
  }

  return true;
}

}  // namespace

Validator::Validator(bool error_on_unknown_field,
                     bool error_on_wrong_recommended,
                     bool error_on_missing_field,
                     bool managed_onc,
                     bool log_warnings)
    : error_on_unknown_field_(error_on_unknown_field),
      error_on_wrong_recommended_(error_on_wrong_recommended),
      error_on_missing_field_(error_on_missing_field),
      managed_onc_(managed_onc),
      log_warnings_(log_warnings) {}

Validator::~Validator() = default;

std::optional<base::Value::Dict> Validator::ValidateAndRepairObject(
    const OncValueSignature* object_signature,
    const base::Value::Dict& onc_object,
    Result* result) {
  CHECK(object_signature);
  *result = VALID;
  bool error = false;
  base::Value::Dict result_value =
      MapObject(*object_signature, onc_object, &error);
  if (error) {
    *result = INVALID;
    return std::nullopt;
  }
  if (!validation_issues_.empty()) {
    *result = VALID_WITH_WARNINGS;
  }
  return result_value;
}

base::Value Validator::MapValue(const OncValueSignature& signature,
                                const base::Value& onc_value,
                                bool* error) {
  if (onc_value.type() != signature.onc_type) {
    *error = true;
    std::ostringstream msg;
    msg << "Found value of type '" << base::Value::GetTypeName(onc_value.type())
        << "', but type '" << base::Value::GetTypeName(signature.onc_type)
        << "' is required.";
    AddValidationIssue(true /* is_error */, msg.str());
    return {};
  }

  base::Value repaired = Mapper::MapValue(signature, onc_value, error);
  CHECK(repaired.is_none() || repaired.type() == signature.onc_type);
  return repaired;
}

base::Value::Dict Validator::MapObject(const OncValueSignature& signature,
                                       const base::Value::Dict& onc_object,
                                       bool* error) {
  base::Value::Dict repaired;
  bool valid = ValidateObjectDefault(signature, onc_object, &repaired);

  if (valid) {
    if (&signature == &kToplevelConfigurationSignature) {
      valid = ValidateToplevelConfiguration(&repaired);
    } else if (&signature == &kNetworkConfigurationSignature) {
      valid = ValidateNetworkConfiguration(&repaired);
    } else if (&signature == &kCellularSignature) {
      valid = ValidateCellular(&repaired);
    } else if (&signature == &kCellularApnSignature) {
      valid = ValidateAPN(&repaired);
    } else if (&signature == &kEthernetSignature) {
      valid = ValidateEthernet(&repaired);
    } else if (&signature == &kIPConfigSignature ||
               &signature == &kSavedIPConfigSignature) {
      valid = ValidateIPConfig(&repaired);
    } else if (&signature == &kWiFiSignature) {
      valid = ValidateWiFi(&repaired);
    } else if (&signature == &kVPNSignature) {
      valid = ValidateVPN(&repaired);
    } else if (&signature == &kIPsecSignature) {
      valid = ValidateIPsec(&repaired);
    } else if (&signature == &kOpenVPNSignature) {
      valid = ValidateOpenVPN(&repaired);
    } else if (&signature == &kWireGuardSignature) {
      valid = ValidateWireGuard(&repaired);
    } else if (&signature == &kThirdPartyVPNSignature) {
      valid = ValidateThirdPartyVPN(&repaired);
    } else if (&signature == &kARCVPNSignature) {
      valid = ValidateARCVPN(&repaired);
    } else if (&signature == &kVerifyX509Signature) {
      valid = ValidateVerifyX509(&repaired);
    } else if (&signature == &kCertificatePatternSignature) {
      valid = ValidateCertificatePattern(&repaired);
    } else if (&signature == &kGlobalNetworkConfigurationSignature) {
      valid = ValidateGlobalNetworkConfiguration(&repaired);
    } else if (&signature == &kProxySettingsSignature) {
      valid = ValidateProxySettings(&repaired);
    } else if (&signature == &kProxyLocationSignature) {
      valid = ValidateProxyLocation(&repaired);
    } else if (&signature == &kEAPSignature) {
      valid = ValidateEAP(&repaired);
    } else if (&signature == &kEAPSubjectAlternativeNameMatchSignature) {
      valid = ValidateSubjectAlternativeNameMatch(&repaired);
    } else if (&signature == &kCertificateSignature) {
      valid = ValidateCertificate(&repaired);
    } else if (&signature == &kScopeSignature) {
      valid = ValidateScope(&repaired);
    } else if (&signature == &kTetherWithStateSignature) {
      valid = ValidateTether(&repaired);
    }

    // StaticIPConfig is not validated here, because its correctness depends
    // on NetworkConfiguration's 'IPAddressConfigType', 'NameServersConfigType'
    // and 'Recommended' fields. It's validated in
    // ValidateNetworkConfiguration() instead.
  }

  if (valid)
    return repaired;

  DCHECK(!validation_issues_.empty());
  *error = true;
  return base::Value::Dict();
}

base::Value Validator::MapField(const std::string& field_name,
                                const OncValueSignature& object_signature,
                                const base::Value& onc_value,
                                bool* found_unknown_field,
                                bool* error) {
  path_.push_back(field_name);
  bool current_field_unknown = false;
  base::Value result = Mapper::MapField(field_name, object_signature, onc_value,
                                        &current_field_unknown, error);

  DCHECK_EQ(field_name, path_.back());
  path_.pop_back();

  if (current_field_unknown) {
    *found_unknown_field = true;
    std::ostringstream msg;
    msg << "Field name '" << field_name << "' is unknown.";
    AddValidationIssue(error_on_unknown_field_, msg.str());
  }

  return result;
}

base::Value::List Validator::MapArray(const OncValueSignature& array_signature,
                                      const base::Value::List& onc_array,
                                      bool* nested_error) {
  bool nested_error_in_current_array = false;
  base::Value::List result = Mapper::MapArray(array_signature, onc_array,
                                              &nested_error_in_current_array);

  if (&array_signature == &kNetworkConfigurationListSignature) {
    ValidateEthernetConfigs(&result);
  }

  // Drop individual networks and certificates instead of rejecting all of
  // the configuration.
  if (nested_error_in_current_array &&
      &array_signature != &kNetworkConfigurationListSignature &&
      &array_signature != &kCertificateListSignature &&
      &array_signature != &kAdminApnListSignature) {
    *nested_error = nested_error_in_current_array;
  }
  return result;
}

base::Value Validator::MapEntry(int index,
                                const OncValueSignature& signature,
                                const base::Value& onc_value,
                                bool* error) {
  std::string index_as_string = base::NumberToString(index);
  path_.push_back(index_as_string);
  base::Value result = Mapper::MapEntry(index, signature, onc_value, error);
  DCHECK_EQ(index_as_string, path_.back());
  path_.pop_back();
  if (result.is_none() && (&signature == &kNetworkConfigurationSignature ||
                           &signature == &kCertificateSignature)) {
    std::ostringstream msg;
    msg << "Entry at index '" << index_as_string
        << "' has been removed because it contained errors.";
    AddValidationIssue(false /* is_error */, msg.str());
  }
  return result;
}

bool Validator::ValidateObjectDefault(const OncValueSignature& signature,
                                      const base::Value::Dict& onc_object,
                                      base::Value::Dict* result) {
  bool found_unknown_field = false;
  bool nested_error_occurred = false;
  MapFields(signature, onc_object, &found_unknown_field, &nested_error_occurred,
            result);

  if (found_unknown_field && error_on_unknown_field_) {
    DVLOG(1) << "Unknown field names are errors: Aborting.";
    return false;
  }

  if (nested_error_occurred) {
    return false;
  }

  return ValidateRecommendedField(signature, result);
}

bool Validator::ValidateRecommendedField(
    const OncValueSignature& object_signature,
    base::Value::Dict* result) {
  CHECK(result);

  std::optional<base::Value> recommended_value =
      result->Extract(::onc::kRecommended);
  // This remove passes ownership to |recommended_value|.
  if (!recommended_value) {
    return true;
  }

  // The types of field values are already verified.
  DCHECK(recommended_value->is_list());

  if (!managed_onc_) {
    std::ostringstream msg;
    msg << "Found the field '" << ::onc::kRecommended
        << "' in an unmanaged ONC";
    AddValidationIssue(false /* is_error */, msg.str());
    return true;
  }

  base::Value::List repaired_recommended;
  for (const auto& entry : recommended_value->GetList()) {
    const std::string* field_name = entry.GetIfString();
    if (!field_name) {
      NOTREACHED_IN_MIGRATION();  // The types of field values are already
                                  // verified.
      continue;
    }

    const OncFieldSignature* field_signature =
        GetFieldSignature(object_signature, *field_name);

    bool found_error = false;
    std::string error_cause;
    if (!field_signature) {
      found_error = true;
      error_cause = "unknown";
    } else if (field_signature->value_signature->onc_type ==
               base::Value::Type::DICT) {
      found_error = true;
      error_cause = "dictionary-typed";
    }

    if (found_error) {
      path_.push_back(::onc::kRecommended);
      std::ostringstream msg;
      msg << "The " << error_cause << " field '" << *field_name
          << "' cannot be recommended.";
      AddValidationIssue(error_on_wrong_recommended_, msg.str());
      path_.pop_back();
      if (error_on_wrong_recommended_)
        return false;
      continue;
    }

    repaired_recommended.Append(*field_name);
  }

  result->Set(::onc::kRecommended, std::move(repaired_recommended));
  return true;
}

bool Validator::ValidateClientCertFields(bool allow_cert_type_none,
                                         base::Value::Dict* result) {
  std::vector<const char*> valid_cert_types = {
      ::onc::client_cert::kRef, ::onc::client_cert::kPattern,
      ::onc::client_cert::kProvisioningProfileId,
      ::onc::client_cert::kPKCS11Id};
  if (allow_cert_type_none)
    valid_cert_types.push_back(::onc::client_cert::kClientCertTypeNone);

  std::string cert_type =
      GetStringFromDict(*result, ::onc::client_cert::kClientCertType);

  // TODO(crbug.com/40117885): Remove the client certificate type empty
  // check. Ignored fields should be removed by normalizer before validating.
  if (cert_type.empty())
    return true;

  if (!IsValidValue(cert_type, valid_cert_types))
    return false;

  bool all_required_exist = true;

  if (cert_type == ::onc::client_cert::kProvisioningProfileId)
    all_required_exist &= RequireField(
        *result, ::onc::client_cert::kClientCertProvisioningProfileId);
  else if (cert_type == ::onc::client_cert::kPattern)
    all_required_exist &=
        RequireField(*result, ::onc::client_cert::kClientCertPattern);
  else if (cert_type == ::onc::client_cert::kRef)
    all_required_exist &=
        RequireField(*result, ::onc::client_cert::kClientCertRef);
  else if (cert_type == ::onc::client_cert::kPKCS11Id)
    all_required_exist &=
        RequireField(*result, ::onc::client_cert::kClientCertPKCS11Id);

  return !error_on_missing_field_ || all_required_exist;
}

namespace {

std::string JoinStringRange(const std::vector<const char*>& strings,
                            const std::string& separator) {
  std::vector<std::string_view> string_vector(strings.begin(), strings.end());
  return base::JoinString(string_vector, separator);
}

}  // namespace

bool Validator::IsInDevicePolicy(base::Value::Dict* result,
                                 std::string_view field_name) {
  if (result->contains(field_name)) {
    if (onc_source_ != ::onc::ONC_SOURCE_DEVICE_POLICY) {
      std::ostringstream msg;
      msg << "Field '" << field_name << "' is only allowed in a device policy.";
      AddValidationIssue(true /* is_error */, msg.str());
      return false;
    }
  }
  return true;
}

bool Validator::IsValidValue(const std::string& field_value,
                             const std::vector<const char*>& valid_values) {
  for (const char* it : valid_values) {
    if (field_value == it)
      return true;
  }

  std::ostringstream msg;
  msg << "Found value '" << field_value << "', but expected one of the values ["
      << JoinStringRange(valid_values, ", ") << "]";
  AddValidationIssue(true /* is_error */, msg.str());
  return false;
}

bool Validator::FieldExistsAndHasNoValidValue(
    const base::Value::Dict& object,
    const std::string& field_name,
    const std::vector<const char*>& valid_values) {
  const std::string* actual_value = object.FindString(field_name);
  if (!actual_value)
    return false;

  path_.push_back(field_name);
  const bool valid = IsValidValue(*actual_value, valid_values);
  path_.pop_back();
  return !valid;
}

bool Validator::FieldExistsAndIsNotInRange(const base::Value::Dict& object,
                                           const std::string& field_name,
                                           int lower_bound,
                                           int upper_bound) {
  std::optional<int> actual_value = object.FindInt(field_name);
  if (!actual_value || (lower_bound <= actual_value.value() &&
                        actual_value.value() <= upper_bound)) {
    return false;
  }

  path_.push_back(field_name);
  std::ostringstream msg;
  msg << "Found value '" << actual_value.value()
      << "', but expected a value in the range [" << lower_bound << ", "
      << upper_bound << "] (boundaries inclusive)";
  AddValidationIssue(true /* is_error */, msg.str());
  path_.pop_back();
  return true;
}

bool Validator::FieldExistsAndIsEmpty(const base::Value::Dict& dict,
                                      const std::string& field_name) {
  if (!dict.contains(field_name)) {
    return false;
  }

  const std::string* maybe_str = dict.FindString(field_name);
  const base::Value::List* maybe_list = dict.FindList(field_name);
  if (maybe_str) {
    if (!(*maybe_str).empty()) {
      return false;
    }
  } else if (maybe_list) {
    if (!(*maybe_list).empty()) {
      return false;
    }
  } else {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  path_.push_back(field_name);
  std::ostringstream msg;
  msg << "Found an empty string, but expected a non-empty string.";
  AddValidationIssue(/*is_error=*/true, /*debug_info=*/msg.str());
  path_.pop_back();
  return true;
}

bool Validator::FieldShouldExistOrBeRecommended(const base::Value::Dict& object,
                                                const std::string& field_name) {
  if (object.contains(field_name) || FieldIsRecommended(object, field_name)) {
    return true;
  }

  std::ostringstream msg;
  msg << "Field " << field_name << " is not found, but expected either to be "
      << "set or to be recommended.";
  AddValidationIssue(error_on_missing_field_, msg.str());
  return !error_on_missing_field_;
}

bool Validator::OnlyOneFieldSet(const base::Value::Dict& object,
                                const std::string& field_name1,
                                const std::string& field_name2) {
  if (object.contains(field_name1) && object.contains(field_name2)) {
    std::ostringstream msg;
    msg << "At most one of '" << field_name1 << "' and '" << field_name2
        << "' can be set.";
    AddValidationIssue(true /* is_error */, msg.str());
    return false;
  }
  return true;
}

bool Validator::ListFieldContainsValidValues(
    const base::Value::Dict& object,
    const std::string& field_name,
    const std::vector<const char*>& valid_values) {
  const base::Value::List* list = object.FindList(field_name);
  if (!list)
    return true;
  path_.push_back(field_name);
  for (const auto& entry : *list) {
    const std::string* value = entry.GetIfString();
    if (!value) {
      NOTREACHED_IN_MIGRATION();  // The types of field values are already
                                  // verified.
      continue;
    }
    if (!IsValidValue(*value, valid_values)) {
      path_.pop_back();
      return false;
    }
  }
  path_.pop_back();
  return true;
}

bool Validator::ValidateSSIDAndHexSSID(base::Value::Dict* object) {
  const std::string kInvalidLength = "Invalid length";

  // Check SSID validity.
  std::string* ssid_string = object->FindString(::onc::wifi::kSSID);
  if (ssid_string && (ssid_string->size() <= 0 ||
                      ssid_string->size() > kMaximumSSIDLengthInBytes)) {
    path_.push_back(::onc::wifi::kSSID);
    std::ostringstream msg;
    msg << kInvalidLength;
    // If the HexSSID field is present, ignore errors in SSID because these
    // might be caused by the usage of a non-UTF-8 encoding when the SSID
    // field was automatically added (see FillInHexSSIDField).
    if (!object->contains(::onc::wifi::kHexSSID)) {
      AddValidationIssue(true /* is_error */, msg.str());
      path_.pop_back();
      return false;
    }
    AddValidationIssue(false /* is_error */, msg.str());
    path_.pop_back();
  }

  // Check HexSSID validity.
  std::string* hex_ssid_string = object->FindString(::onc::wifi::kHexSSID);
  if (!hex_ssid_string)
    return true;

  std::string decoded_ssid;
  if (!base::HexStringToString(*hex_ssid_string, &decoded_ssid)) {
    path_.push_back(::onc::wifi::kHexSSID);
    std::ostringstream msg;
    msg << "Not a valid hex representation: '" << *hex_ssid_string << "'";
    AddValidationIssue(true /* is_error */, msg.str());
    path_.pop_back();
    return false;
  }
  if (decoded_ssid.size() <= 0 ||
      decoded_ssid.size() > kMaximumSSIDLengthInBytes) {
    path_.push_back(::onc::wifi::kHexSSID);
    std::ostringstream msg;
    msg << kInvalidLength;
    AddValidationIssue(true /* is_error */, msg.str());
    path_.pop_back();
    return false;
  }

  // If both SSID and HexSSID are set, check whether they are consistent, i.e.
  // HexSSID contains the UTF-8 encoding of SSID. If not, remove the SSID
  // field.
  if (ssid_string && ssid_string->length() > 0) {
    if (*ssid_string != decoded_ssid) {
      path_.push_back(::onc::wifi::kSSID);
      std::ostringstream msg;
      msg << "Fields '" << ::onc::wifi::kSSID << "' and '"
          << ::onc::wifi::kHexSSID << "' contain inconsistent values.";
      AddValidationIssue(false /* is_error */, msg.str());
      path_.pop_back();
      object->Remove(::onc::wifi::kSSID);
    }
  }
  return true;
}

bool Validator::RequireField(const base::Value::Dict& dict,
                             const std::string& field_name) {
  if (dict.contains(field_name)) {
    return true;
  }

  std::ostringstream msg;
  msg << "The required field '" << field_name << "' is missing.";
  AddValidationIssue(error_on_missing_field_, msg.str());
  return false;
}

bool Validator::CheckAdminAssignedAPNIdsAreNonEmptyAndAddToSet(
    const base::Value::Dict& dict,
    const std::string& key_list_of_ids) {
  CHECK(key_list_of_ids == ::onc::cellular::kAdminAssignedAPNIds ||
        key_list_of_ids ==
            ::onc::global_network_config::kPSIMAdminAssignedAPNIds);

  const base::Value::List* id_list = dict.FindList(key_list_of_ids);
  if (!id_list) {
    return true;
  }

  for (const base::Value& id_value : *id_list) {
    const std::string id = id_value.GetString();
    if (id.empty()) {
      std::ostringstream msg;
      msg << key_list_of_ids << " must only include non-empty IDs";
      AddValidationIssue(true /* is_error */, msg.str());
      return false;
    }
    admin_assigned_apn_ids_.emplace(id);
  }
  return true;
}

bool Validator::CheckGuidIsUniqueAndAddToSet(const base::Value::Dict& dict,
                                             const std::string& key_guid,
                                             std::set<std::string>* guids) {
  const std::string* guid = dict.FindString(key_guid);
  if (!guid) {
    return true;
  }

  if (guids->count(*guid) != 0) {
    path_.push_back(key_guid);
    std::ostringstream msg;
    msg << "Found a duplicate GUID '" << *guid << "'.";
    AddValidationIssue(true /* is_error */, msg.str());
    path_.pop_back();
    return false;
  }
  guids->insert(*guid);
  return true;
}

bool Validator::IsGlobalNetworkConfigInUserImport(
    const base::Value::Dict& onc_object) {
  if (onc_source_ == ::onc::ONC_SOURCE_USER_IMPORT &&
      onc_object.contains(
          ::onc::toplevel_config::kGlobalNetworkConfiguration)) {
    std::ostringstream msg;
    msg << "Field '" << ::onc::toplevel_config::kGlobalNetworkConfiguration
        << "' is prohibited in ONC user imports";
    AddValidationIssue(true /* is_error */, msg.str());
    return true;
  }
  return false;
}

bool Validator::ValidateToplevelConfiguration(base::Value::Dict* result) {
  if (FieldExistsAndHasNoValidValue(*result, ::onc::toplevel_config::kType,
                                    GetValidToplevelConfigurationTypes())) {
    return false;
  }

  base::Value::List* admin_apn_list =
      result->FindList(::onc::toplevel_config::kAdminAPNList);

  // Enforces unique string identifiers for APNs within the 'AdminAPNList'. Note
  // that duplicate identifiers may still exist in other APN arrays due to
  // sources (like the modem or modb) that don't provide unique Ids.
  if (admin_apn_list) {
    if (!HasUniqueValuesForKeyInDicts(*admin_apn_list,
                                      ::onc::cellular_apn::kId)) {
      AddValidationIssue(/*is_error=*/true,
                         "APNs in the AdminAPNList do not have unique IDs");
      return false;
    }

    base::flat_set<std::string> ids_of_toplevel_apns =
        GetStringsFromDicts(*admin_apn_list, ::onc::cellular_apn::kId);
    if (!std::includes(ids_of_toplevel_apns.begin(), ids_of_toplevel_apns.end(),
                       admin_assigned_apn_ids_.begin(),
                       admin_assigned_apn_ids_.end())) {
      AddValidationIssue(/*is_error=*/true,
                         "Some cellular network configurations have admin APN "
                         "IDs that are not sourced from the admin");
      return false;
    }
  }

  if (IsGlobalNetworkConfigInUserImport(*result)) {
    return false;
  }

  return true;
}

bool Validator::ValidateNetworkConfiguration(base::Value::Dict* result) {
  const std::string* onc_type =
      result->FindString(::onc::network_config::kType);
  if (onc_type && *onc_type == ::onc::network_type::kWimaxDeprecated) {
    AddValidationIssue(/*is_error=*/false, "WiMax is deprecated");
    return true;
  }

  if (FieldExistsAndHasNoValidValue(*result, ::onc::network_config::kType,
                                    GetValidNetworkTypes()) ||
      FieldExistsAndHasNoValidValue(*result,
                                    ::onc::network_config::kIPAddressConfigType,
                                    GetValidIPConfigTypes()) ||
      FieldExistsAndHasNoValidValue(
          *result, ::onc::network_config::kNameServersConfigType,
          GetValidIPConfigTypes()) ||
      FieldExistsAndHasNoValidValue(*result,
                                    ::onc::network_config::kCheckCaptivePortal,
                                    GetValidCheckCaptivePortalValues()) ||
      FieldExistsAndIsEmpty(*result, ::onc::network_config::kGUID)) {
    return false;
  }

  if (!CheckGuidIsUniqueAndAddToSet(*result, ::onc::network_config::kGUID,
                                    &network_guids_))
    return false;

  bool all_required_exist = RequireField(*result, ::onc::network_config::kGUID);

  bool remove = result->FindBool(::onc::kRemove).value_or(false);
  if (!remove) {
    all_required_exist &= RequireField(*result, ::onc::network_config::kName) &&
                          RequireField(*result, ::onc::network_config::kType);

    if (!NetworkHasCorrectStaticIPConfig(result))
      return false;

    std::string type = GetStringFromDict(*result, ::onc::network_config::kType);

    // Prohibit anything but WiFi, Ethernet, VPN and Cellular for device-level
    // policy (which corresponds to shared networks). See also
    // http://crosbug.com/28741.
    if (onc_source_ == ::onc::ONC_SOURCE_DEVICE_POLICY && !type.empty() &&
        type != ::onc::network_type::kVPN &&
        type != ::onc::network_type::kWiFi &&
        type != ::onc::network_type::kEthernet &&
        type != ::onc::network_type::kCellular) {
      std::ostringstream msg;
      msg << "Networks of type '" << type
          << "' are prohibited in ONC device policies.";
      AddValidationIssue(true /* is_error */, msg.str());
      return false;
    }
    if (type == ::onc::network_type::kWiFi) {
      all_required_exist &= RequireField(*result, ::onc::network_config::kWiFi);
    } else if (type == ::onc::network_type::kEthernet) {
      all_required_exist &=
          RequireField(*result, ::onc::network_config::kEthernet);
    } else if (type == ::onc::network_type::kCellular) {
      all_required_exist &=
          RequireField(*result, ::onc::network_config::kCellular);
    } else if (type == ::onc::network_type::kWimaxDeprecated) {
      all_required_exist &=
          RequireField(*result, ::onc::network_config::kWimaxDeprecated);
    } else if (type == ::onc::network_type::kVPN) {
      all_required_exist &= RequireField(*result, ::onc::network_config::kVPN);
    } else if (type == ::onc::network_type::kTether) {
      all_required_exist &=
          RequireField(*result, ::onc::network_config::kTether);
    } else if (type == ::onc::network_type::kCellular) {
      all_required_exist &=
          RequireField(*result, ::onc::network_config::kCellular);
    }
  }

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateCellular(base::Value::Dict* result) {
  if (result->contains(::onc::cellular::kSMDPAddress) &&
      result->contains(::onc::cellular::kSMDSAddress)) {
    AddValidationIssue(
        /*is_error=*/true,
        R"(The "SMDPAddress" and "SMDSAddress" fields are mutually exclusive.)");
    return false;
  }

  if (!CheckAdminAssignedAPNIdsAreNonEmptyAndAddToSet(
          *result, ::onc::cellular::kAdminAssignedAPNIds)) {
    return false;
  }

  return true;
}

bool Validator::ValidateAPN(base::Value::Dict* result) {
  if (!RequireField(*result, ::onc::cellular_apn::kAccessPointName) ||
      FieldExistsAndIsEmpty(*result, ::onc::cellular_apn::kAccessPointName)) {
    return false;
  }

  if (FieldExistsAndHasNoValidValue(*result, ::onc::cellular_apn::kIpType,
                                    GetValidAPNIpTypes())) {
    return false;
  }

  if (FieldExistsAndIsEmpty(*result, ::onc::cellular_apn::kApnTypes) ||
      !ListFieldContainsValidValues(*result, ::onc::cellular_apn::kApnTypes,
                                    GetValidApnTypes())) {
    return false;
  }

  // TODO(b/333100319): Validate that all APNs with ::onc::cellular_apn::kSource
  // that are ::onc::cellular_apn::kAdmin or ::onc::cellular_apn::kUi have a
  // non-empty string ::onc::cellular_apn::kId. This should be done after
  // kApnPolicies flag is moved to chromeos as it is in ash currently.

  return true;
}

bool Validator::ValidateEthernet(base::Value::Dict* result) {
  if (FieldExistsAndHasNoValidValue(*result, ::onc::ethernet::kAuthentication,
                                    GetValidEthernetAuthentications())) {
    return false;
  }

  bool all_required_exist = true;
  std::string auth =
      GetStringFromDict(*result, ::onc::ethernet::kAuthentication);
  if (auth == ::onc::ethernet::k8021X)
    all_required_exist &= RequireField(*result, ::onc::ethernet::kEAP);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateIPConfig(base::Value::Dict* result,
                                 bool require_fields) {
  if (FieldExistsAndHasNoValidValue(*result, ::onc::ipconfig::kType,
                                    GetValidNetworkIPConfigTypes())) {
    return false;
  }

  std::string type = GetStringFromDict(*result, ::onc::ipconfig::kType);
  int lower_bound = 1;
  // In case of missing type, choose higher upper_bound.
  int upper_bound = (type == ::onc::ipconfig::kIPv4) ? 32 : 128;
  if (FieldExistsAndIsNotInRange(*result, ::onc::ipconfig::kRoutingPrefix,
                                 lower_bound, upper_bound)) {
    return false;
  }

  if (FieldExistsAndIsNotInRange(*result, ::onc::ipconfig::kMTU, 0,
                                 std::numeric_limits<int>::max())) {
    return false;
  }

  bool all_required_exist = true;
  if (require_fields) {
    all_required_exist &= RequireField(*result, ::onc::ipconfig::kIPAddress);
    all_required_exist &=
        RequireField(*result, ::onc::ipconfig::kRoutingPrefix);
    all_required_exist &= RequireField(*result, ::onc::ipconfig::kGateway);
  } else {
    all_required_exist &=
        FieldShouldExistOrBeRecommended(*result, ::onc::ipconfig::kIPAddress);
    all_required_exist &= FieldShouldExistOrBeRecommended(
        *result, ::onc::ipconfig::kRoutingPrefix);
    all_required_exist &=
        FieldShouldExistOrBeRecommended(*result, ::onc::ipconfig::kGateway);
  }

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::NetworkHasCorrectStaticIPConfig(base::Value::Dict* network) {
  bool must_have_ip_config = FieldIsSetToValueOrRecommended(
      *network, ::onc::network_config::kIPAddressConfigType,
      base::Value(::onc::network_config::kIPConfigTypeStatic));
  bool must_have_nameservers = FieldIsSetToValueOrRecommended(
      *network, ::onc::network_config::kNameServersConfigType,
      base::Value(::onc::network_config::kIPConfigTypeStatic));

  if (!must_have_ip_config && !must_have_nameservers)
    return true;

  if (!RequireField(*network, ::onc::network_config::kStaticIPConfig))
    return false;

  base::Value::Dict* static_ip_config =
      network->FindDict(::onc::network_config::kStaticIPConfig);
  bool valid = true;
  // StaticIPConfig should have all fields required by the corresponding
  // IPAddressConfigType and NameServersConfigType values.
  if (must_have_ip_config)
    valid &= ValidateIPConfig(static_ip_config, false /* require_fields */);
  if (must_have_nameservers)
    valid &= FieldShouldExistOrBeRecommended(*static_ip_config,
                                             ::onc::ipconfig::kNameServers);
  return valid;
}

bool Validator::ValidateWiFi(base::Value::Dict* result) {
  if (FieldExistsAndHasNoValidValue(*result, ::onc::wifi::kSecurity,
                                    GetValidWiFiSecurities())) {
    return false;
  }

  if (!ValidateSSIDAndHexSSID(result))
    return false;

  bool all_required_exist = RequireField(*result, ::onc::wifi::kSecurity);

  // One of {kSSID, kHexSSID} must be present.
  if (!result->contains(::onc::wifi::kSSID)) {
    all_required_exist &= RequireField(*result, ::onc::wifi::kHexSSID);
  }
  if (!result->contains(::onc::wifi::kHexSSID)) {
    all_required_exist &= RequireField(*result, ::onc::wifi::kSSID);
  }

  std::string security = GetStringFromDict(*result, ::onc::wifi::kSecurity);
  if (security == ::onc::wifi::kWEP_8021X || security == ::onc::wifi::kWPA_EAP)
    all_required_exist &= RequireField(*result, ::onc::wifi::kEAP);
  else if (security == ::onc::wifi::kWEP_PSK ||
           security == ::onc::wifi::kWPA_PSK)
    all_required_exist &= RequireField(*result, ::onc::wifi::kPassphrase);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateVPN(base::Value::Dict* result) {
  if (FieldExistsAndHasNoValidValue(
          *result, ::onc::vpn::kType,
          managed_onc_ ? GetValidManagedVPNTypes() : GetAllValidVPNTypes())) {
    return false;
  }

  bool all_required_exist = RequireField(*result, ::onc::vpn::kType);
  std::string type = GetStringFromDict(*result, ::onc::vpn::kType);
  if (type == ::onc::vpn::kOpenVPN) {
    all_required_exist &= RequireField(*result, ::onc::vpn::kOpenVPN);
  } else if (type == ::onc::vpn::kIPsec) {
    all_required_exist &= RequireField(*result, ::onc::vpn::kIPsec);
  } else if (type == ::onc::vpn::kTypeL2TP_IPsec) {
    all_required_exist &= RequireField(*result, ::onc::vpn::kIPsec) &&
                          RequireField(*result, ::onc::vpn::kL2TP);
  } else if (type == ::onc::vpn::kWireGuard) {
    all_required_exist &= RequireField(*result, ::onc::vpn::kWireGuard);
  } else if (type == ::onc::vpn::kThirdPartyVpn) {
    all_required_exist &= RequireField(*result, ::onc::vpn::kThirdPartyVpn);
  } else if (type == ::onc::vpn::kArcVpn) {
    all_required_exist &= RequireField(*result, ::onc::vpn::kArcVpn);
  }

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateIPsec(base::Value::Dict* result) {
  if (FieldExistsAndHasNoValidValue(*result, ::onc::ipsec::kAuthenticationType,
                                    GetValidIPsecAuthentications()) ||
      FieldExistsAndIsEmpty(*result, ::onc::ipsec::kServerCARefs)) {
    return false;
  }

  if (!OnlyOneFieldSet(*result, ::onc::ipsec::kServerCARefs,
                       ::onc::ipsec::kServerCARef))
    return false;

  if (!ValidateClientCertFields(/*allow_cert_type_none=*/false, result))
    return false;

  bool all_required_exist =
      RequireField(*result, ::onc::ipsec::kAuthenticationType) &&
      RequireField(*result, ::onc::ipsec::kIKEVersion);
  std::string auth =
      GetStringFromDict(*result, ::onc::ipsec::kAuthenticationType);
  if (auth == ::onc::ipsec::kCert) {
    all_required_exist &=
        RequireField(*result, ::onc::client_cert::kClientCertType);
  }

  // For cert-based or EAP-based authentication, server CA must exist.
  // For PSK-based authentication, server CA must not exist.
  bool has_server_ca_cert = result->contains(::onc::ipsec::kServerCARefs) ||
                            result->contains(::onc::ipsec::kServerCARef) ||
                            result->contains(::onc::ipsec::kServerCAPEMs);
  if ((auth == ::onc::ipsec::kCert || auth == ::onc::ipsec::kEAP) &&
      !has_server_ca_cert) {
    all_required_exist = false;
    std::ostringstream msg;
    msg << "Server CA config is missing (one of the fields "
        << ::onc::ipsec::kServerCARefs << " or " << ::onc::ipsec::kServerCAPEMs
        << ").";
    AddValidationIssue(error_on_missing_field_, msg.str());
  }
  if (auth == ::onc::ipsec::kPSK && has_server_ca_cert) {
    std::ostringstream msg;
    msg << "Field '" << ::onc::ipsec::kServerCARefs << "' (or '"
        << ::onc::ipsec::kServerCARef << "') can only be set if '"
        << ::onc::ipsec::kAuthenticationType << "' is set to '"
        << ::onc::ipsec::kCert << "'.";
    AddValidationIssue(true /* is_error */, msg.str());
    return false;
  }

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateOpenVPN(base::Value::Dict* result) {
  if (FieldExistsAndHasNoValidValue(*result, ::onc::openvpn::kAuthRetry,
                                    GetValidVPNAuthRetryValues()) ||
      FieldExistsAndHasNoValidValue(*result, ::onc::openvpn::kRemoteCertTLS,
                                    GetValidVPNCertTlsValues()) ||
      FieldExistsAndHasNoValidValue(*result,
                                    ::onc::openvpn::kCompressionAlgorithm,
                                    GetValidVPNCompressionAlgorithmValues()) ||
      FieldExistsAndHasNoValidValue(*result,
                                    ::onc::openvpn::kUserAuthenticationType,
                                    GetValidVPNUserAuthTypes()) ||
      FieldExistsAndIsEmpty(*result, ::onc::openvpn::kServerCARefs)) {
    return false;
  }

  // ONC policy prevents the UI from setting properties that are not explicitly
  // listed as 'recommended' (i.e. the default is 'enforced'). Historically
  // the configuration UI ignored this restriction. In order to support legacy
  // ONC configurations, add recommended entries for user authentication
  // properties where appropriate.
  if ((onc_source_ == ::onc::ONC_SOURCE_DEVICE_POLICY ||
       onc_source_ == ::onc::ONC_SOURCE_USER_POLICY)) {
    base::Value::List* recommended = result->FindList(::onc::kRecommended);
    if (!recommended) {
      recommended =
          &result->Set(::onc::kRecommended, base::Value::List())->GetList();
    }

    // If kUserAuthenticationType is unspecified, allow Password and OTP.
    if (!result->FindString(::onc::openvpn::kUserAuthenticationType)) {
      AddKeyToList(::onc::openvpn::kPassword, recommended);
      AddKeyToList(::onc::openvpn::kOTP, recommended);
    }

    // If client cert type is not provided, empty, or 'None', allow client cert
    // properties.
    std::string client_cert_type =
        GetStringFromDict(*result, ::onc::client_cert::kClientCertType);
    if (client_cert_type.empty() ||
        client_cert_type == ::onc::client_cert::kClientCertTypeNone) {
      AddKeyToList(::onc::client_cert::kClientCertType, recommended);
      AddKeyToList(::onc::client_cert::kClientCertPKCS11Id, recommended);
    }
  }

  if (!OnlyOneFieldSet(*result, ::onc::openvpn::kServerCARefs,
                       ::onc::openvpn::kServerCARef))
    return false;

  if (!ValidateClientCertFields(true /* allow ClientCertType None */, result))
    return false;

  bool all_required_exist =
      RequireField(*result, ::onc::client_cert::kClientCertType);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateWireGuard(base::Value::Dict* result) {
  const base::Value::List* peers = result->FindList(::onc::wireguard::kPeers);
  std::ostringstream msg;
  if (!peers) {
    msg << "A " << ::onc::wireguard::kPeers
        << " list is required but not present.";
    AddValidationIssue(true /* is_error */, msg.str());
    return false;
  }
  for (const base::Value& p : *peers) {
    if (!p.GetDict().contains(::onc::wireguard::kPublicKey)) {
      msg << ::onc::wireguard::kPublicKey
          << " field is required for each peer.";
      AddValidationIssue(true /* is_error */, msg.str());
      return false;
    }
  }

  const bool all_required_exist =
      RequireField(*result, ::onc::wireguard::kEndpoint);
  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateThirdPartyVPN(base::Value::Dict* result) {
  const bool all_required_exist =
      RequireField(*result, ::onc::third_party_vpn::kExtensionID);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateARCVPN(base::Value::Dict* result) {
  return true;
}

bool Validator::ValidateVerifyX509(base::Value::Dict* result) {
  if (FieldExistsAndHasNoValidValue(*result, ::onc::verify_x509::kType,
                                    GetValidX509Types())) {
    return false;
  }

  bool all_required_exist = RequireField(*result, ::onc::verify_x509::kName);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateCertificatePattern(base::Value::Dict* result) {
  bool all_required_exist = true;
  if (!result->contains(::onc::client_cert::kSubject) &&
      !result->contains(::onc::client_cert::kIssuer) &&
      !result->contains(::onc::client_cert::kIssuerCARef)) {
    all_required_exist = false;
    std::ostringstream msg;
    msg << "None of the fields '" << ::onc::client_cert::kSubject << "', '"
        << ::onc::client_cert::kIssuer << "', and '"
        << ::onc::client_cert::kIssuerCARef
        << "' is present, but at least one is required.";
    AddValidationIssue(error_on_missing_field_, msg.str());
  }

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateGlobalNetworkConfiguration(base::Value::Dict* result) {
  // Replace the deprecated kBlacklistedHexSSIDs with kBlockedHexSSIDs.
  if (!result->contains(::onc::global_network_config::kBlockedHexSSIDs)) {
    std::optional<base::Value> blocked =
        result->Extract(::onc::global_network_config::kBlacklistedHexSSIDs);
    if (blocked) {
      result->Set(::onc::global_network_config::kBlockedHexSSIDs,
                  std::move(*blocked));
    }
  }

  // Validate that these are only allowed in device policy.
  const std::string_view kDevicePolicyOnlyKeys[] = {
      ::onc::global_network_config::kAllowTextMessages,
      ::onc::global_network_config::kAllowCellularSimLock,
      ::onc::global_network_config::kAllowCellularHotspot,
      ::onc::global_network_config::kAllowAPNModification,
      ::onc::global_network_config::kDisableNetworkTypes,
      ::onc::global_network_config::kAllowOnlyPolicyCellularNetworks,
      ::onc::global_network_config::kAllowOnlyPolicyWiFiToConnect,
      ::onc::global_network_config::kAllowOnlyPolicyWiFiToConnectIfAvailable,
      ::onc::global_network_config::kBlockedHexSSIDs,
      ::onc::global_network_config::kRecommendedValuesAreEphemeral,
      ::onc::global_network_config::
          kUserCreatedNetworkConfigurationsAreEphemeral,
      ::onc::global_network_config::kDisconnectWiFiOnEthernet};
  for (std::string_view key : kDevicePolicyOnlyKeys) {
    if (!IsInDevicePolicy(result, key)) {
      return false;
    }
  }

  std::vector<const char*> valid_network_types = GetValidNetworkTypes();
  valid_network_types.push_back(::onc::network_config::kWimaxDeprecated);

  // Ensure the list contains only legitimate network type identifiers.
  if (!ListFieldContainsValidValues(
          *result, ::onc::global_network_config::kDisableNetworkTypes,
          valid_network_types)) {
    return false;
  }

  // Ensure that AllowTextMessages contains valid types
  if (FieldExistsAndHasNoValidValue(
          *result, ::onc::global_network_config::kAllowTextMessages,
          GetValidAllowTextMessagesTypes())) {
    return false;
  }

  if (!CheckAdminAssignedAPNIdsAreNonEmptyAndAddToSet(
          *result, ::onc::global_network_config::kPSIMAdminAssignedAPNIds)) {
    return false;
  }

  return true;
}

bool Validator::ValidateProxySettings(base::Value::Dict* result) {
  if (FieldExistsAndHasNoValidValue(*result, ::onc::proxy::kType,
                                    GetValidProxySettingsTypes())) {
    return false;
  }

  bool all_required_exist = RequireField(*result, ::onc::proxy::kType);
  std::string type = GetStringFromDict(*result, ::onc::proxy::kType);
  if (type == ::onc::proxy::kManual)
    all_required_exist &= RequireField(*result, ::onc::proxy::kManual);
  else if (type == ::onc::proxy::kPAC)
    all_required_exist &= RequireField(*result, ::onc::proxy::kPAC);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateProxyLocation(base::Value::Dict* result) {
  bool all_required_exist = RequireField(*result, ::onc::proxy::kHost) &&
                            RequireField(*result, ::onc::proxy::kPort);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateEAP(base::Value::Dict* result) {
  // If this EAP dict is in a IPsec dict (i.e., IPsec is the second-to-last
  // element in its path), the only valid method is MSCHAPv2.
  std::vector<const char*> valid_outer_values = GetValidEAPOuterValues();
  if (path_.size() >= 2) {
    auto it = std::next(path_.rbegin());
    if (*it == ::onc::vpn::kIPsec)
      valid_outer_values = {::onc::eap::kMSCHAPv2};
  }

  if (FieldExistsAndHasNoValidValue(*result, ::onc::eap::kInner,
                                    GetValidEAPInnerValues()) ||
      FieldExistsAndHasNoValidValue(*result, ::onc::eap::kOuter,
                                    valid_outer_values) ||
      FieldExistsAndIsEmpty(*result, ::onc::eap::kServerCARefs)) {
    return false;
  }

  if (!OnlyOneFieldSet(*result, ::onc::eap::kServerCARefs,
                       ::onc::eap::kServerCARef))
    return false;

  if (!ValidateClientCertFields(true /* allow ClientCertType None */, result))
    return false;

  bool all_required_exist = RequireField(*result, ::onc::eap::kOuter);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateSubjectAlternativeNameMatch(base::Value::Dict* result) {
  if (FieldExistsAndHasNoValidValue(
          *result, ::onc::eap_subject_alternative_name_match::kType,
          GetValidEAPSubjectAlternativeNameMatchTypes())) {
    return false;
  }

  bool all_required_exist =
      RequireField(*result, ::onc::eap_subject_alternative_name_match::kType) &&
      RequireField(*result, ::onc::eap_subject_alternative_name_match::kValue);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateCertificate(base::Value::Dict* result) {
  if (FieldExistsAndHasNoValidValue(*result, ::onc::certificate::kType,
                                    GetValidCertificateTypes()) ||
      FieldExistsAndIsEmpty(*result, ::onc::certificate::kGUID)) {
    return false;
  }

  std::string type = GetStringFromDict(*result, ::onc::certificate::kType);

  if (!CheckGuidIsUniqueAndAddToSet(*result, ::onc::certificate::kGUID,
                                    &certificate_guids_))
    return false;

  bool all_required_exist = RequireField(*result, ::onc::certificate::kGUID);

  bool remove = result->FindBool(::onc::kRemove).value_or(false);
  if (remove) {
    path_.push_back(::onc::kRemove);
    std::ostringstream msg;
    msg << "Removal of certificates is not supported.";
    AddValidationIssue(true /* is_error */, msg.str());
    path_.pop_back();
    return false;
  }

  all_required_exist &= RequireField(*result, ::onc::certificate::kType);

  if (type == ::onc::certificate::kClient)
    all_required_exist &= RequireField(*result, ::onc::certificate::kPKCS12);
  else if (type == ::onc::certificate::kServer ||
           type == ::onc::certificate::kAuthority)
    all_required_exist &= RequireField(*result, ::onc::certificate::kX509);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateScope(base::Value::Dict* result) {
  if (FieldExistsAndHasNoValidValue(*result, ::onc::scope::kType,
                                    GetValidScopeTypes()) ||
      FieldExistsAndIsEmpty(*result, ::onc::scope::kId)) {
    return false;
  }

  bool all_required_exist = RequireField(*result, ::onc::scope::kType);
  const std::string* type_string = result->FindString(::onc::scope::kType);
  if (type_string && *type_string == ::onc::scope::kExtension) {
    all_required_exist &= RequireField(*result, ::onc::scope::kId);
    // Check Id validity for type 'Extension'.
    const std::string* id_string = result->FindString(::onc::scope::kId);
    if (id_string && !crx_file::id_util::IdIsValid(*id_string)) {
      std::ostringstream msg;
      msg << "Field '" << ::onc::scope::kId << "' is not a valid extension id.";
      AddValidationIssue(false /* is_error */, msg.str());
      return false;
    }
  }

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateTether(base::Value::Dict* result) {
  if (FieldExistsAndIsNotInRange(*result, ::onc::tether::kBatteryPercentage, 0,
                                 100) ||
      FieldExistsAndIsNotInRange(*result, ::onc::tether::kSignalStrength, 0,
                                 100) ||
      FieldExistsAndIsEmpty(*result, ::onc::tether::kCarrier)) {
    return false;
  }

  bool all_required_exist =
      RequireField(*result, ::onc::tether::kHasConnectedToHost);
  all_required_exist &=
      RequireField(*result, ::onc::tether::kBatteryPercentage);
  all_required_exist &= RequireField(*result, ::onc::tether::kSignalStrength);
  all_required_exist &= RequireField(*result, ::onc::tether::kCarrier);

  return !error_on_missing_field_ || all_required_exist;
}

void Validator::ValidateEthernetConfigs(
    base::Value::List* network_configurations_list) {
  // Ensures that at most one NetworkConfiguration is effective within these
  // categories:
  // - "Type": "Ethernet" and "Authentication": "None"
  // - "Type": "Ethernet" and "Authentication": "8021X"
  // This is currently necessary because shill only persists one configuration
  // per such category and the UI only supports one Ethernet configuration.
  // TODO(b/159725895): Design better Ethernet configuration + policy
  // management.
  std::vector<std::string> ethernet_auth_none_guids;
  std::vector<std::string> ethernet_auth_8021x_guids;

  for (const base::Value& network_configuration :
       *network_configurations_list) {
    const std::string* guid = network_configuration.GetDict().FindString(
        ::onc::network_config::kGUID);
    const base::Value::Dict* ethernet =
        network_configuration.GetDict().FindDict(
            ::onc::network_config::kEthernet);
    if (!guid || !ethernet)
      continue;

    const std::string* auth =
        ethernet->FindString(::onc::ethernet::kAuthentication);
    if (!auth)
      continue;
    if (*auth == ::onc::ethernet::kAuthenticationNone)
      ethernet_auth_none_guids.push_back(*guid);
    if (*auth == ::onc::ethernet::k8021X)
      ethernet_auth_8021x_guids.push_back(*guid);
  }

  // If there were multiple NetworkConfigurations in such a bucket, keep the
  // last one because that's the one which would be effective, as it would be
  // applies last in shill.
  OnlyKeepLast(network_configurations_list, ethernet_auth_none_guids,
               /*type=*/"Ethernet");
  OnlyKeepLast(network_configurations_list, ethernet_auth_8021x_guids,
               /*type=*/"Ethernet 802.1x");
}

void Validator::OnlyKeepLast(base::Value::List* network_configurations_list,
                             const std::vector<std::string>& guids,
                             const char* type_for_messages) {
  if (guids.size() < 2)
    return;
  for (size_t i = 0; i < guids.size() - 1; ++i) {
    RemoveNetworkConfigurationWithGuid(network_configurations_list, guids[i]);

    std::ostringstream msg;
    msg << "NetworkConfiguration '" << guids[i] << "' ignored - only one "
        << type_for_messages << " configuration can be processed";
    AddValidationIssue(/*is_error=*/false, msg.str());
  }
}

void Validator::RemoveNetworkConfigurationWithGuid(
    base::Value::List* network_configurations_list,
    const std::string& guid_to_remove) {
  base::Value::List& list = *network_configurations_list;
  for (auto it = list.begin(); it != list.end(); ++it) {
    const std::string* guid =
        it->GetDict().FindString(::onc::network_config::kGUID);
    if (!guid)
      continue;
    if (*guid == guid_to_remove) {
      list.erase(it);
      return;
    }
  }
}

void Validator::AddValidationIssue(bool is_error,
                                   const std::string& debug_info) {
  std::ostringstream msg;
  msg << (is_error ? "ERROR: " : "WARNING: ") << debug_info << " (at "
      << (path_.empty() ? "toplevel" : base::JoinString(path_, ".")) << ")";
  std::string message = msg.str();

  if (is_error)
    NET_LOG(ERROR) << message;
  else if (log_warnings_)
    NET_LOG(DEBUG) << message;

  validation_issues_.push_back({is_error, message});
}

}  // namespace chromeos::onc
