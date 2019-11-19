// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_validator.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_signature.h"
#include "components/crx_file/id_util.h"
#include "components/device_event_log/device_event_log.h"

namespace chromeos {
namespace onc {

namespace {

// According to the IEEE 802.11 standard the SSID is a series of 0 to 32 octets.
const int kMaximumSSIDLengthInBytes = 32;

void AddKeyToList(const char* key, base::Value::ListStorage& list) {
  base::Value key_value(key);
  if (!base::Contains(list, key_value))
    list.push_back(std::move(key_value));
}

std::string GetStringFromDict(const base::Value& dict, const char* key) {
  const base::Value* value = dict.FindKeyOfType(key, base::Value::Type::STRING);
  return value ? value->GetString() : std::string();
}

bool FieldIsRecommended(const base::DictionaryValue& object,
                        const std::string& field_name) {
  const base::Value* recommended =
      object.FindKeyOfType(::onc::kRecommended, base::Value::Type::LIST);
  return recommended &&
         base::Contains(recommended->GetList(), base::Value(field_name));
}

bool FieldIsSetToValueOrRecommended(const base::DictionaryValue& object,
                                    const std::string& field_name,
                                    const base::Value& expected_value) {
  const base::Value* actual_value = object.FindKey(field_name);
  if (actual_value && expected_value == *actual_value)
    return true;

  return FieldIsRecommended(object, field_name);
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
      log_warnings_(log_warnings),
      onc_source_(::onc::ONC_SOURCE_NONE) {}

Validator::~Validator() = default;

std::unique_ptr<base::DictionaryValue> Validator::ValidateAndRepairObject(
    const OncValueSignature* object_signature,
    const base::Value& onc_object,
    Result* result) {
  CHECK(object_signature);
  *result = VALID;
  bool error = false;
  std::unique_ptr<base::Value> result_value =
      MapValue(*object_signature, onc_object, &error);
  if (error) {
    *result = INVALID;
    result_value.reset();
  } else if (!validation_issues_.empty()) {
    *result = VALID_WITH_WARNINGS;
  }
  // The return value should be NULL if, and only if, |result| equals INVALID.
  DCHECK_EQ(!result_value, *result == INVALID);
  return base::DictionaryValue::From(std::move(result_value));
}

std::unique_ptr<base::Value> Validator::MapValue(
    const OncValueSignature& signature,
    const base::Value& onc_value,
    bool* error) {
  if (onc_value.type() != signature.onc_type) {
    *error = true;
    std::ostringstream msg;
    msg << "Found value of type '" << base::Value::GetTypeName(onc_value.type())
        << "', but type '" << base::Value::GetTypeName(signature.onc_type)
        << "' is required.";
    AddValidationIssue(true /* is_error */, msg.str());
    return std::unique_ptr<base::Value>();
  }

  std::unique_ptr<base::Value> repaired =
      Mapper::MapValue(signature, onc_value, error);
  if (repaired)
    CHECK_EQ(repaired->type(), signature.onc_type);
  return repaired;
}

std::unique_ptr<base::DictionaryValue> Validator::MapObject(
    const OncValueSignature& signature,
    const base::DictionaryValue& onc_object,
    bool* error) {
  std::unique_ptr<base::DictionaryValue> repaired(new base::DictionaryValue);

  bool valid = ValidateObjectDefault(signature, onc_object, repaired.get());
  if (valid) {
    if (&signature == &kToplevelConfigurationSignature) {
      valid = ValidateToplevelConfiguration(repaired.get());
    } else if (&signature == &kNetworkConfigurationSignature) {
      valid = ValidateNetworkConfiguration(repaired.get());
    } else if (&signature == &kEthernetSignature) {
      valid = ValidateEthernet(repaired.get());
    } else if (&signature == &kIPConfigSignature ||
               &signature == &kSavedIPConfigSignature) {
      valid = ValidateIPConfig(repaired.get());
    } else if (&signature == &kWiFiSignature) {
      valid = ValidateWiFi(repaired.get());
    } else if (&signature == &kVPNSignature) {
      valid = ValidateVPN(repaired.get());
    } else if (&signature == &kIPsecSignature) {
      valid = ValidateIPsec(repaired.get());
    } else if (&signature == &kOpenVPNSignature) {
      valid = ValidateOpenVPN(repaired.get());
    } else if (&signature == &kThirdPartyVPNSignature) {
      valid = ValidateThirdPartyVPN(repaired.get());
    } else if (&signature == &kARCVPNSignature) {
      valid = ValidateARCVPN(repaired.get());
    } else if (&signature == &kVerifyX509Signature) {
      valid = ValidateVerifyX509(repaired.get());
    } else if (&signature == &kCertificatePatternSignature) {
      valid = ValidateCertificatePattern(repaired.get());
    } else if (&signature == &kGlobalNetworkConfigurationSignature) {
      valid = ValidateGlobalNetworkConfiguration(repaired.get());
    } else if (&signature == &kProxySettingsSignature) {
      valid = ValidateProxySettings(repaired.get());
    } else if (&signature == &kProxyLocationSignature) {
      valid = ValidateProxyLocation(repaired.get());
    } else if (&signature == &kEAPSignature) {
      valid = ValidateEAP(repaired.get());
    } else if (&signature == &kCertificateSignature) {
      valid = ValidateCertificate(repaired.get());
    } else if (&signature == &kScopeSignature) {
      valid = ValidateScope(repaired.get());
    } else if (&signature == &kTetherWithStateSignature) {
      valid = ValidateTether(repaired.get());
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
  return std::unique_ptr<base::DictionaryValue>();
}

std::unique_ptr<base::Value> Validator::MapField(
    const std::string& field_name,
    const OncValueSignature& object_signature,
    const base::Value& onc_value,
    bool* found_unknown_field,
    bool* error) {
  path_.push_back(field_name);
  bool current_field_unknown = false;
  std::unique_ptr<base::Value> result = Mapper::MapField(
      field_name, object_signature, onc_value, &current_field_unknown, error);

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

std::unique_ptr<base::ListValue> Validator::MapArray(
    const OncValueSignature& array_signature,
    const base::ListValue& onc_array,
    bool* nested_error) {
  bool nested_error_in_current_array = false;
  std::unique_ptr<base::ListValue> result = Mapper::MapArray(
      array_signature, onc_array, &nested_error_in_current_array);

  // Drop individual networks and certificates instead of rejecting all of
  // the configuration.
  if (nested_error_in_current_array &&
      &array_signature != &kNetworkConfigurationListSignature &&
      &array_signature != &kCertificateListSignature) {
    *nested_error = nested_error_in_current_array;
  }
  return result;
}

std::unique_ptr<base::Value> Validator::MapEntry(
    int index,
    const OncValueSignature& signature,
    const base::Value& onc_value,
    bool* error) {
  std::string index_as_string = base::NumberToString(index);
  path_.push_back(index_as_string);
  std::unique_ptr<base::Value> result =
      Mapper::MapEntry(index, signature, onc_value, error);
  DCHECK_EQ(index_as_string, path_.back());
  path_.pop_back();
  if (!result.get() && (&signature == &kNetworkConfigurationSignature ||
                        &signature == &kCertificateSignature)) {
    std::ostringstream msg;
    msg << "Entry at index '" << index_as_string
        << "' has been removed because it contained errors.";
    AddValidationIssue(false /* is_error */, msg.str());
  }
  return result;
}

bool Validator::ValidateObjectDefault(const OncValueSignature& signature,
                                      const base::DictionaryValue& onc_object,
                                      base::DictionaryValue* result) {
  bool found_unknown_field = false;
  bool nested_error_occured = false;
  MapFields(signature, onc_object, &found_unknown_field, &nested_error_occured,
            result);

  if (found_unknown_field && error_on_unknown_field_) {
    DVLOG(1) << "Unknown field names are errors: Aborting.";
    return false;
  }

  if (nested_error_occured)
    return false;

  return ValidateRecommendedField(signature, result);
}

bool Validator::ValidateRecommendedField(
    const OncValueSignature& object_signature,
    base::DictionaryValue* result) {
  CHECK(result);

  std::unique_ptr<base::Value> recommended_value;
  // This remove passes ownership to |recommended_value|.
  if (!result->RemoveWithoutPathExpansion(::onc::kRecommended,
                                          &recommended_value)) {
    return true;
  }

  base::ListValue* recommended_list = nullptr;
  recommended_value->GetAsList(&recommended_list);
  DCHECK(recommended_list);  // The types of field values are already verified.

  if (!managed_onc_) {
    std::ostringstream msg;
    msg << "Found the field '" << ::onc::kRecommended
        << "' in an unmanaged ONC";
    AddValidationIssue(false /* is_error */, msg.str());
    return true;
  }

  std::unique_ptr<base::ListValue> repaired_recommended(new base::ListValue);
  for (const auto& entry : *recommended_list) {
    std::string field_name;
    if (!entry.GetAsString(&field_name)) {
      NOTREACHED();  // The types of field values are already verified.
      continue;
    }

    const OncFieldSignature* field_signature =
        GetFieldSignature(object_signature, field_name);

    bool found_error = false;
    std::string error_cause;
    if (!field_signature) {
      found_error = true;
      error_cause = "unknown";
    } else if (field_signature->value_signature->onc_type ==
               base::Value::Type::DICTIONARY) {
      found_error = true;
      error_cause = "dictionary-typed";
    }

    if (found_error) {
      path_.push_back(::onc::kRecommended);
      std::ostringstream msg;
      msg << "The " << error_cause << " field '" << field_name
          << "' cannot be recommended.";
      AddValidationIssue(error_on_wrong_recommended_, msg.str());
      path_.pop_back();
      if (error_on_wrong_recommended_)
        return false;
      continue;
    }

    repaired_recommended->AppendString(field_name);
  }

  result->Set(::onc::kRecommended, std::move(repaired_recommended));
  return true;
}

bool Validator::ValidateClientCertFields(bool allow_cert_type_none,
                                         base::DictionaryValue* result) {
  std::vector<const char*> valid_cert_types = {::onc::client_cert::kRef,
                                               ::onc::client_cert::kPattern,
                                               ::onc::client_cert::kPKCS11Id};
  if (allow_cert_type_none)
    valid_cert_types.push_back(::onc::client_cert::kClientCertTypeNone);
  if (FieldExistsAndHasNoValidValue(
          *result, ::onc::client_cert::kClientCertType, valid_cert_types))
    return false;

  std::string cert_type =
      GetStringFromDict(*result, ::onc::client_cert::kClientCertType);
  bool all_required_exist = true;

  if (cert_type == ::onc::client_cert::kPattern)
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
  std::vector<base::StringPiece> string_vector(strings.begin(), strings.end());
  return base::JoinString(string_vector, separator);
}

}  // namespace

bool Validator::IsInDevicePolicy(base::DictionaryValue* result,
                                 const std::string& field_name) {
  if (result->HasKey(field_name)) {
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
    const base::DictionaryValue& object,
    const std::string& field_name,
    const std::vector<const char*>& valid_values) {
  std::string actual_value;
  if (!object.GetStringWithoutPathExpansion(field_name, &actual_value))
    return false;

  path_.push_back(field_name);
  const bool valid = IsValidValue(actual_value, valid_values);
  path_.pop_back();
  return !valid;
}

bool Validator::FieldExistsAndIsNotInRange(const base::DictionaryValue& object,
                                           const std::string& field_name,
                                           int lower_bound,
                                           int upper_bound) {
  int actual_value;
  if (!object.GetIntegerWithoutPathExpansion(field_name, &actual_value) ||
      (lower_bound <= actual_value && actual_value <= upper_bound)) {
    return false;
  }

  path_.push_back(field_name);
  std::ostringstream msg;
  msg << "Found value '" << actual_value
      << "', but expected a value in the range [" << lower_bound << ", "
      << upper_bound << "] (boundaries inclusive)";
  AddValidationIssue(true /* is_error */, msg.str());
  path_.pop_back();
  return true;
}

bool Validator::FieldExistsAndIsEmpty(const base::DictionaryValue& object,
                                      const std::string& field_name) {
  const base::Value* value = NULL;
  if (!object.GetWithoutPathExpansion(field_name, &value))
    return false;

  std::string str;
  const base::ListValue* list = NULL;
  if (value->GetAsString(&str)) {
    if (!str.empty())
      return false;
  } else if (value->GetAsList(&list)) {
    if (!list->empty())
      return false;
  } else {
    NOTREACHED();
    return false;
  }

  path_.push_back(field_name);
  std::ostringstream msg;
  msg << "Found an empty string, but expected a non-empty string.";
  AddValidationIssue(true /* is_error */, msg.str());
  path_.pop_back();
  return true;
}

bool Validator::FieldShouldExistOrBeRecommended(
    const base::DictionaryValue& object,
    const std::string& field_name) {
  if (object.HasKey(field_name) || FieldIsRecommended(object, field_name))
    return true;

  std::ostringstream msg;
  msg << "Field " << field_name << " is not found, but expected either to be "
      << "set or to be recommended.";
  AddValidationIssue(error_on_missing_field_, msg.str());
  return !error_on_missing_field_;
}

bool Validator::OnlyOneFieldSet(const base::DictionaryValue& object,
                                const std::string& field_name1,
                                const std::string& field_name2) {
  if (object.HasKey(field_name1) && object.HasKey(field_name2)) {
    std::ostringstream msg;
    msg << "At most one of '" << field_name1 << "' and '" << field_name2
        << "' can be set.";
    AddValidationIssue(true /* is_error */, msg.str());
    return false;
  }
  return true;
}

bool Validator::ListFieldContainsValidValues(
    const base::DictionaryValue& object,
    const std::string& field_name,
    const std::vector<const char*>& valid_values) {
  const base::ListValue* list = NULL;
  if (object.GetListWithoutPathExpansion(field_name, &list)) {
    path_.push_back(field_name);
    for (const auto& entry : *list) {
      std::string value;
      if (!entry.GetAsString(&value)) {
        NOTREACHED();  // The types of field values are already verified.
        continue;
      }
      if (!IsValidValue(value, valid_values)) {
        path_.pop_back();
        return false;
      }
    }
    path_.pop_back();
  }
  return true;
}

bool Validator::ValidateSSIDAndHexSSID(base::DictionaryValue* object) {
  const std::string kInvalidLength = "Invalid length";

  // Check SSID validity.
  std::string ssid_string;
  if (object->GetStringWithoutPathExpansion(::onc::wifi::kSSID, &ssid_string) &&
      (ssid_string.size() <= 0 ||
       ssid_string.size() > kMaximumSSIDLengthInBytes)) {
    path_.push_back(::onc::wifi::kSSID);
    std::ostringstream msg;
    msg << kInvalidLength;
    // If the HexSSID field is present, ignore errors in SSID because these
    // might be caused by the usage of a non-UTF-8 encoding when the SSID
    // field was automatically added (see FillInHexSSIDField).
    if (!object->HasKey(::onc::wifi::kHexSSID)) {
      AddValidationIssue(true /* is_error */, msg.str());
      path_.pop_back();
      return false;
    }
    AddValidationIssue(false /* is_error */, msg.str());
    path_.pop_back();
  }

  // Check HexSSID validity.
  std::string hex_ssid_string;
  if (object->GetStringWithoutPathExpansion(::onc::wifi::kHexSSID,
                                            &hex_ssid_string)) {
    std::string decoded_ssid;
    if (!base::HexStringToString(hex_ssid_string, &decoded_ssid)) {
      path_.push_back(::onc::wifi::kHexSSID);
      std::ostringstream msg;
      msg << "Not a valid hex representation: '" << hex_ssid_string << "'";
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
    if (ssid_string.length() > 0) {
      if (ssid_string != decoded_ssid) {
        path_.push_back(::onc::wifi::kSSID);
        std::ostringstream msg;
        msg << "Fields '" << ::onc::wifi::kSSID << "' and '"
            << ::onc::wifi::kHexSSID << "' contain inconsistent values.";
        AddValidationIssue(false /* is_error */, msg.str());
        path_.pop_back();
        object->RemoveWithoutPathExpansion(::onc::wifi::kSSID, nullptr);
      }
    }
  }
  return true;
}

bool Validator::RequireField(const base::DictionaryValue& dict,
                             const std::string& field_name) {
  if (dict.HasKey(field_name))
    return true;

  std::ostringstream msg;
  msg << "The required field '" << field_name << "' is missing.";
  AddValidationIssue(error_on_missing_field_, msg.str());
  return false;
}

bool Validator::CheckGuidIsUniqueAndAddToSet(const base::DictionaryValue& dict,
                                             const std::string& key_guid,
                                             std::set<std::string>* guids) {
  std::string guid;
  if (dict.GetStringWithoutPathExpansion(key_guid, &guid)) {
    if (guids->count(guid) != 0) {
      path_.push_back(key_guid);
      std::ostringstream msg;
      msg << "Found a duplicate GUID '" << guid << "'.";
      AddValidationIssue(true /* is_error */, msg.str());
      path_.pop_back();
      return false;
    }
    guids->insert(guid);
  }
  return true;
}

bool Validator::IsGlobalNetworkConfigInUserImport(
    const base::DictionaryValue& onc_object) {
  if (onc_source_ == ::onc::ONC_SOURCE_USER_IMPORT &&
      onc_object.HasKey(::onc::toplevel_config::kGlobalNetworkConfiguration)) {
    std::ostringstream msg;
    msg << "Field '" << ::onc::toplevel_config::kGlobalNetworkConfiguration
        << "' is prohibited in ONC user imports";
    AddValidationIssue(true /* is_error */, msg.str());
    return true;
  }
  return false;
}

bool Validator::ValidateToplevelConfiguration(base::DictionaryValue* result) {
  const std::vector<const char*> valid_types = {
      ::onc::toplevel_config::kUnencryptedConfiguration,
      ::onc::toplevel_config::kEncryptedConfiguration};
  if (FieldExistsAndHasNoValidValue(*result, ::onc::toplevel_config::kType,
                                    valid_types))
    return false;

  if (IsGlobalNetworkConfigInUserImport(*result))
    return false;

  return true;
}

bool Validator::ValidateNetworkConfiguration(base::DictionaryValue* result) {
  const std::string* onc_type =
      result->FindStringKey(::onc::network_config::kType);
  if (onc_type && *onc_type == ::onc::network_type::kWimaxDeprecated) {
    AddValidationIssue(/*is_error=*/false, "WiMax is deprecated");
    return true;
  }

  const std::vector<const char*> valid_types = {
      ::onc::network_type::kEthernet, ::onc::network_type::kVPN,
      ::onc::network_type::kWiFi,     ::onc::network_type::kCellular,
      ::onc::network_type::kTether,
  };
  const std::vector<const char*> valid_ipconfig_types = {
      ::onc::network_config::kIPConfigTypeDHCP,
      ::onc::network_config::kIPConfigTypeStatic};
  if (FieldExistsAndHasNoValidValue(*result, ::onc::network_config::kType,
                                    valid_types) ||
      FieldExistsAndHasNoValidValue(*result,
                                    ::onc::network_config::kIPAddressConfigType,
                                    valid_ipconfig_types) ||
      FieldExistsAndHasNoValidValue(
          *result, ::onc::network_config::kNameServersConfigType,
          valid_ipconfig_types) ||
      FieldExistsAndIsEmpty(*result, ::onc::network_config::kGUID)) {
    return false;
  }

  if (!CheckGuidIsUniqueAndAddToSet(*result, ::onc::network_config::kGUID,
                                    &network_guids_))
    return false;

  bool all_required_exist = RequireField(*result, ::onc::network_config::kGUID);

  bool remove = false;
  result->GetBooleanWithoutPathExpansion(::onc::kRemove, &remove);
  if (!remove) {
    all_required_exist &= RequireField(*result, ::onc::network_config::kName) &&
                          RequireField(*result, ::onc::network_config::kType);

    if (!NetworkHasCorrectStaticIPConfig(result))
      return false;

    std::string type = GetStringFromDict(*result, ::onc::network_config::kType);

    // Prohibit anything but WiFi and Ethernet for device-level policy (which
    // corresponds to shared networks). See also http://crosbug.com/28741.
    if (onc_source_ == ::onc::ONC_SOURCE_DEVICE_POLICY && !type.empty() &&
        type != ::onc::network_type::kWiFi &&
        type != ::onc::network_type::kEthernet) {
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
    }
  }

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateEthernet(base::DictionaryValue* result) {
  const std::vector<const char*> valid_authentications = {
      ::onc::ethernet::kAuthenticationNone, ::onc::ethernet::k8021X};
  if (FieldExistsAndHasNoValidValue(*result, ::onc::ethernet::kAuthentication,
                                    valid_authentications)) {
    return false;
  }

  bool all_required_exist = true;
  std::string auth =
      GetStringFromDict(*result, ::onc::ethernet::kAuthentication);
  if (auth == ::onc::ethernet::k8021X)
    all_required_exist &= RequireField(*result, ::onc::ethernet::kEAP);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateIPConfig(base::DictionaryValue* result,
                                 bool require_fields) {
  const std::vector<const char*> valid_types = {::onc::ipconfig::kIPv4,
                                                ::onc::ipconfig::kIPv6};
  if (FieldExistsAndHasNoValidValue(
          *result, ::onc::ipconfig::kType, valid_types))
    return false;

  std::string type = GetStringFromDict(*result, ::onc::ipconfig::kType);
  int lower_bound = 1;
  // In case of missing type, choose higher upper_bound.
  int upper_bound = (type == ::onc::ipconfig::kIPv4) ? 32 : 128;
  if (FieldExistsAndIsNotInRange(*result, ::onc::ipconfig::kRoutingPrefix,
                                 lower_bound, upper_bound)) {
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

bool Validator::NetworkHasCorrectStaticIPConfig(
    base::DictionaryValue* network) {
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

  base::DictionaryValue* static_ip_config = nullptr;
  network->GetDictionary(::onc::network_config::kStaticIPConfig,
                         &static_ip_config);
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

bool Validator::ValidateWiFi(base::DictionaryValue* result) {
  const std::vector<const char*> valid_securities = {
      ::onc::wifi::kSecurityNone, ::onc::wifi::kWEP_PSK,
      ::onc::wifi::kWEP_8021X, ::onc::wifi::kWPA_PSK, ::onc::wifi::kWPA_EAP};
  if (FieldExistsAndHasNoValidValue(*result, ::onc::wifi::kSecurity,
                                    valid_securities))
    return false;

  if (!ValidateSSIDAndHexSSID(result))
    return false;

  bool all_required_exist = RequireField(*result, ::onc::wifi::kSecurity);

  // One of {kSSID, kHexSSID} must be present.
  if (!result->HasKey(::onc::wifi::kSSID))
    all_required_exist &= RequireField(*result, ::onc::wifi::kHexSSID);
  if (!result->HasKey(::onc::wifi::kHexSSID))
    all_required_exist &= RequireField(*result, ::onc::wifi::kSSID);

  std::string security = GetStringFromDict(*result, ::onc::wifi::kSecurity);
  if (security == ::onc::wifi::kWEP_8021X || security == ::onc::wifi::kWPA_EAP)
    all_required_exist &= RequireField(*result, ::onc::wifi::kEAP);
  else if (security == ::onc::wifi::kWEP_PSK ||
           security == ::onc::wifi::kWPA_PSK)
    all_required_exist &= RequireField(*result, ::onc::wifi::kPassphrase);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateVPN(base::DictionaryValue* result) {
  const std::vector<const char*> valid_types = {
      ::onc::vpn::kIPsec, ::onc::vpn::kTypeL2TP_IPsec, ::onc::vpn::kOpenVPN,
      ::onc::vpn::kThirdPartyVpn, ::onc::vpn::kArcVpn};
  if (FieldExistsAndHasNoValidValue(*result, ::onc::vpn::kType, valid_types))
    return false;

  bool all_required_exist = RequireField(*result, ::onc::vpn::kType);
  std::string type = GetStringFromDict(*result, ::onc::vpn::kType);
  if (type == ::onc::vpn::kOpenVPN) {
    all_required_exist &= RequireField(*result, ::onc::vpn::kOpenVPN);
  } else if (type == ::onc::vpn::kIPsec) {
    all_required_exist &= RequireField(*result, ::onc::vpn::kIPsec);
  } else if (type == ::onc::vpn::kTypeL2TP_IPsec) {
    all_required_exist &= RequireField(*result, ::onc::vpn::kIPsec) &&
                          RequireField(*result, ::onc::vpn::kL2TP);
  } else if (type == ::onc::vpn::kThirdPartyVpn) {
    all_required_exist &= RequireField(*result, ::onc::vpn::kThirdPartyVpn);
  } else if (type == ::onc::vpn::kArcVpn) {
    all_required_exist &= RequireField(*result, ::onc::vpn::kArcVpn);
  }

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateIPsec(base::DictionaryValue* result) {
  const std::vector<const char*> valid_authentications = {::onc::ipsec::kPSK,
                                                          ::onc::ipsec::kCert};
  if (FieldExistsAndHasNoValidValue(*result, ::onc::ipsec::kAuthenticationType,
                                    valid_authentications) ||
      FieldExistsAndIsEmpty(*result, ::onc::ipsec::kServerCARefs)) {
    return false;
  }

  if (!OnlyOneFieldSet(*result, ::onc::ipsec::kServerCARefs,
                       ::onc::ipsec::kServerCARef))
    return false;

  if (!ValidateClientCertFields(false,  // don't allow ClientCertType None
                                result)) {
    return false;
  }

  bool all_required_exist =
      RequireField(*result, ::onc::ipsec::kAuthenticationType) &&
      RequireField(*result, ::onc::ipsec::kIKEVersion);
  std::string auth =
      GetStringFromDict(*result, ::onc::ipsec::kAuthenticationType);
  bool has_server_ca_cert = result->HasKey(::onc::ipsec::kServerCARefs) ||
                            result->HasKey(::onc::ipsec::kServerCARef);
  if (auth == ::onc::ipsec::kCert) {
    all_required_exist &=
        RequireField(*result, ::onc::client_cert::kClientCertType);
    if (!has_server_ca_cert) {
      all_required_exist = false;
      std::ostringstream msg;
      msg << "The required field '" << ::onc::ipsec::kServerCARefs
          << "' is missing.";
      AddValidationIssue(error_on_missing_field_, msg.str());
    }
  } else if (has_server_ca_cert) {
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

bool Validator::ValidateOpenVPN(base::DictionaryValue* result) {
  const std::vector<const char*> valid_auth_retry_values = {
      ::onc::openvpn::kNone, ::onc::openvpn::kInteract,
      ::onc::openvpn::kNoInteract};
  const std::vector<const char*> valid_cert_tls_values = {
      ::onc::openvpn::kNone, ::onc::openvpn::kServer};
  const std::vector<const char*> valid_user_auth_types = {
      ::onc::openvpn_user_auth_type::kNone, ::onc::openvpn_user_auth_type::kOTP,
      ::onc::openvpn_user_auth_type::kPassword,
      ::onc::openvpn_user_auth_type::kPasswordAndOTP};

  if (FieldExistsAndHasNoValidValue(*result, ::onc::openvpn::kAuthRetry,
                                    valid_auth_retry_values) ||
      FieldExistsAndHasNoValidValue(*result, ::onc::openvpn::kRemoteCertTLS,
                                    valid_cert_tls_values) ||
      FieldExistsAndHasNoValidValue(*result,
                                    ::onc::openvpn::kUserAuthenticationType,
                                    valid_user_auth_types) ||
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
    base::Value* recommended =
        result->FindKeyOfType(::onc::kRecommended, base::Value::Type::LIST);
    if (!recommended)
      recommended = result->SetKey(::onc::kRecommended, base::ListValue());

    // If kUserAuthenticationType is unspecified, allow Password and OTP.
    base::Value::ListStorage& recommended_list = recommended->GetList();
    if (!result->FindKeyOfType(::onc::openvpn::kUserAuthenticationType,
                               base::Value::Type::STRING)) {
      AddKeyToList(::onc::openvpn::kPassword, recommended_list);
      AddKeyToList(::onc::openvpn::kOTP, recommended_list);
    }

    // If client cert type is not provided, empty, or 'None', allow client cert
    // properties.
    std::string client_cert_type =
        GetStringFromDict(*result, ::onc::client_cert::kClientCertType);
    if (client_cert_type.empty() ||
        client_cert_type == ::onc::client_cert::kClientCertTypeNone) {
      AddKeyToList(::onc::client_cert::kClientCertType, recommended_list);
      AddKeyToList(::onc::client_cert::kClientCertPKCS11Id, recommended_list);
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

bool Validator::ValidateThirdPartyVPN(base::DictionaryValue* result) {
  const bool all_required_exist =
      RequireField(*result, ::onc::third_party_vpn::kExtensionID);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateARCVPN(base::DictionaryValue* result) {
  const bool all_required_exist =
      RequireField(*result, ::onc::arc_vpn::kTunnelChrome);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateVerifyX509(base::DictionaryValue* result) {
  const std::vector<const char*> valid_types = {
      ::onc::verify_x509::types::kName, ::onc::verify_x509::types::kNamePrefix,
      ::onc::verify_x509::types::kSubject};

  if (FieldExistsAndHasNoValidValue(*result, ::onc::verify_x509::kType,
                                    valid_types))
    return false;

  bool all_required_exist = RequireField(*result, ::onc::verify_x509::kName);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateCertificatePattern(base::DictionaryValue* result) {
  bool all_required_exist = true;
  if (!result->HasKey(::onc::client_cert::kSubject) &&
      !result->HasKey(::onc::client_cert::kIssuer) &&
      !result->HasKey(::onc::client_cert::kIssuerCARef)) {
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

bool Validator::ValidateGlobalNetworkConfiguration(
    base::DictionaryValue* result) {
  // Validate that kDisableNetworkTypes, kAllowOnlyPolicyNetworksToConnect and
  // kBlacklistedHexSSIDs are only allowed in device policy.
  if (!IsInDevicePolicy(result,
                        ::onc::global_network_config::kDisableNetworkTypes) ||
      !IsInDevicePolicy(
          result,
          ::onc::global_network_config::kAllowOnlyPolicyNetworksToConnect) ||
      !IsInDevicePolicy(result,
                        ::onc::global_network_config::
                            kAllowOnlyPolicyNetworksToConnectIfAvailable) ||
      !IsInDevicePolicy(result,
                        ::onc::global_network_config::kBlacklistedHexSSIDs)) {
    return false;
  }

  // Ensure the list contains only legitimate network type identifiers.
  const std::vector<const char*> valid_network_type_values = {
      ::onc::network_config::kCellular, ::onc::network_config::kEthernet,
      ::onc::network_config::kWiFi, ::onc::network_config::kWimaxDeprecated,
      ::onc::network_config::kTether};
  if (!ListFieldContainsValidValues(
          *result, ::onc::global_network_config::kDisableNetworkTypes,
          valid_network_type_values)) {
    return false;
  }
  return true;
}

bool Validator::ValidateProxySettings(base::DictionaryValue* result) {
  const std::vector<const char*> valid_types = {
      ::onc::proxy::kDirect, ::onc::proxy::kManual, ::onc::proxy::kPAC,
      ::onc::proxy::kWPAD};
  if (FieldExistsAndHasNoValidValue(*result, ::onc::proxy::kType, valid_types))
    return false;

  bool all_required_exist = RequireField(*result, ::onc::proxy::kType);
  std::string type = GetStringFromDict(*result, ::onc::proxy::kType);
  if (type == ::onc::proxy::kManual)
    all_required_exist &= RequireField(*result, ::onc::proxy::kManual);
  else if (type == ::onc::proxy::kPAC)
    all_required_exist &= RequireField(*result, ::onc::proxy::kPAC);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateProxyLocation(base::DictionaryValue* result) {
  bool all_required_exist = RequireField(*result, ::onc::proxy::kHost) &&
                            RequireField(*result, ::onc::proxy::kPort);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateEAP(base::DictionaryValue* result) {
  const std::vector<const char*> valid_inner_values = {
      ::onc::eap::kAutomatic, ::onc::eap::kGTC, ::onc::eap::kMD5,
      ::onc::eap::kMSCHAPv2, ::onc::eap::kPAP};
  const std::vector<const char*> valid_outer_values = {
      ::onc::eap::kPEAP,   ::onc::eap::kEAP_TLS, ::onc::eap::kEAP_TTLS,
      ::onc::eap::kLEAP,   ::onc::eap::kEAP_SIM, ::onc::eap::kEAP_FAST,
      ::onc::eap::kEAP_AKA};

  if (FieldExistsAndHasNoValidValue(*result, ::onc::eap::kInner,
                                    valid_inner_values) ||
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

bool Validator::ValidateCertificate(base::DictionaryValue* result) {
  const std::vector<const char*> valid_types = {::onc::certificate::kClient,
                                                ::onc::certificate::kServer,
                                                ::onc::certificate::kAuthority};
  if (FieldExistsAndHasNoValidValue(*result, ::onc::certificate::kType,
                                    valid_types) ||
      FieldExistsAndIsEmpty(*result, ::onc::certificate::kGUID)) {
    return false;
  }

  std::string type = GetStringFromDict(*result, ::onc::certificate::kType);

  if (!CheckGuidIsUniqueAndAddToSet(*result, ::onc::certificate::kGUID,
                                    &certificate_guids_))
    return false;

  bool all_required_exist = RequireField(*result, ::onc::certificate::kGUID);

  bool remove = false;
  result->GetBooleanWithoutPathExpansion(::onc::kRemove, &remove);
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

bool Validator::ValidateScope(base::DictionaryValue* result) {
  const std::vector<const char*> valid_types = {::onc::scope::kDefault,
                                                ::onc::scope::kExtension};
  if (FieldExistsAndHasNoValidValue(*result, ::onc::scope::kType,
                                    valid_types) ||
      FieldExistsAndIsEmpty(*result, ::onc::scope::kId)) {
    return false;
  }

  bool all_required_exist = RequireField(*result, ::onc::scope::kType);
  const std::string* type_string = result->FindStringKey(::onc::scope::kType);
  if (type_string && *type_string == ::onc::scope::kExtension) {
    all_required_exist &= RequireField(*result, ::onc::scope::kId);
    // Check Id validity for type 'Extension'.
    const std::string* id_string = result->FindStringKey(::onc::scope::kId);
    if (id_string && !crx_file::id_util::IdIsValid(*id_string)) {
      std::ostringstream msg;
      msg << "Field '" << ::onc::scope::kId << "' is not a valid extension id.";
      AddValidationIssue(false /* is_error */, msg.str());
      return false;
    }
  }

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateTether(base::DictionaryValue* result) {
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

}  // namespace onc
}  // namespace chromeos
