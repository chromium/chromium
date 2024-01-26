// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/shill_property_util.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/i18n/encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/components/onc/onc_utils.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash::shill_property_util {

namespace {

// Replace non UTF8 characters in |str| with a replacement character.
std::string ValidateUTF8(const std::string& str) {
  std::string result;
  for (size_t index = 0; index < str.size(); ++index) {
    base_icu::UChar32 code_point_out;
    bool is_unicode_char = base::ReadUnicodeCharacter(str.c_str(), str.size(),
                                                      &index, &code_point_out);
    constexpr base_icu::UChar32 kFirstNonControlChar = 0x20;
    if (is_unicode_char && (code_point_out >= kFirstNonControlChar)) {
      base::WriteUnicodeCharacter(code_point_out, &result);
    } else {
      constexpr base_icu::UChar32 kReplacementChar = 0xFFFD;
      // Puts kReplacementChar if character is a control character [0,0x20)
      // or is not readable UTF8.
      base::WriteUnicodeCharacter(kReplacementChar, &result);
    }
  }
  return result;
}

// If existent and non-empty, copies the string at |key| from |source| to
// |dest|. Returns true if the string was copied.
bool CopyStringFromDictionary(const base::Value::Dict& source,
                              const std::string& key,
                              base::Value::Dict* dest) {
  const std::string* string_value = source.FindString(key);
  if (!string_value || string_value->empty()) {
    return false;
  }
  dest->Set(key, *string_value);
  return true;
}

std::string GetStringFromDictionary(const base::Value::Dict& dict,
                                    const char* key) {
  const std::string* v = dict.FindString(key);
  return v ? *v : std::string();
}

}  // namespace

void SetSSID(const std::string& ssid, base::Value::Dict* properties) {
  properties->Set(shill::kWifiHexSsid, base::HexEncode(ssid));
}

std::string GetSSIDFromProperties(const base::Value::Dict& properties,
                                  bool verbose_logging,
                                  bool* unknown_encoding) {
  if (unknown_encoding)
    *unknown_encoding = false;

  // Get name for debugging.
  std::string name = GetStringFromDictionary(properties, shill::kNameProperty);
  std::string hex_ssid =
      GetStringFromDictionary(properties, shill::kWifiHexSsid);

  if (hex_ssid.empty()) {
    if (verbose_logging)
      NET_LOG(DEBUG) << "GetSSIDFromProperties: No HexSSID set: " << name;
    return std::string();
  }

  std::string ssid;
  std::string ssid_bytes;
  if (base::HexStringToString(hex_ssid, &ssid_bytes)) {
    ssid = ssid_bytes;
    VLOG(2) << "GetSSIDFromProperties: " << name << " HexSsid=" << hex_ssid
            << " SSID=" << ssid;
  } else {
    NET_LOG(ERROR) << "GetSSIDFromProperties: " << name
                   << " Error processing HexSsid: " << hex_ssid;
    return std::string();
  }

  if (base::IsStringUTF8(ssid))
    return ssid;

  // Detect encoding and convert to UTF-8.
  std::string encoding;
  if (!base::DetectEncoding(ssid, &encoding)) {
    // TODO(stevenjb): This is currently experimental. If we find a case where
    // base::DetectEncoding() fails, we need to figure out whether we can use
    // country_code with ConvertToUtf8(). crbug.com/233267.
    encoding = GetStringFromDictionary(properties, shill::kCountryProperty);
  }
  std::string utf8_ssid;
  if (!encoding.empty() &&
      base::ConvertToUtf8AndNormalize(ssid, encoding, &utf8_ssid)) {
    if (utf8_ssid != ssid) {
      if (verbose_logging) {
        NET_LOG(DEBUG) << "GetSSIDFromProperties: " << name
                       << " Encoding=" << encoding << " SSID=" << ssid
                       << " UTF8 SSID=" << utf8_ssid;
      }
    }
    return utf8_ssid;
  }

  if (unknown_encoding)
    *unknown_encoding = true;
  if (verbose_logging) {
    NET_LOG(DEBUG) << "GetSSIDFromProperties: " << name
                   << " Unrecognized Encoding=" << encoding;
  }
  return ssid;
}

std::string GetNetworkIdFromProperties(const base::Value::Dict& properties) {
  if (properties.empty()) {
    return "EmptyProperties";
  }
  std::string guid = GetStringFromDictionary(properties, shill::kGuidProperty);
  if (!guid.empty())
    return NetworkGuidId(guid);
  std::string type = GetStringFromDictionary(properties, shill::kTypeProperty);
  if (!type.empty()) {
    std::string security =
        GetStringFromDictionary(properties, shill::kSecurityClassProperty);
    if (!security.empty())
      return type + "_" + security + "_unconfigured";
  }
  return "<Unconfigured Network>";
}

std::string GetNameFromProperties(const std::string& service_path,
                                  const base::Value::Dict& properties) {
  std::string name = GetStringFromDictionary(properties, shill::kNameProperty);
  std::string validated_name = ValidateUTF8(name);
  if (validated_name != name) {
    NET_LOG(DEBUG) << "GetNameFromProperties: " << service_path
                   << " Validated name=" << validated_name << " name=" << name;
  }

  std::string type = GetStringFromDictionary(properties, shill::kTypeProperty);
  if (type.empty()) {
    NET_LOG(ERROR) << "GetNameFromProperties: " << service_path << " No type.";
    return validated_name;
  }
  if (!NetworkTypePattern::WiFi().MatchesType(type))
    return validated_name;

  bool unknown_ssid_encoding = false;
  std::string ssid = GetSSIDFromProperties(
      properties, true /* verbose_logging */, &unknown_ssid_encoding);
  if (ssid.empty()) {
    NET_LOG(ERROR) << "GetNameFromProperties: " << service_path
                   << " No SSID set";
  }

  // Use |validated_name| if |ssid| is empty.
  // And if the encoding of the SSID is unknown, use |ssid|, which contains raw
  // bytes in that case, only if |validated_name| is empty.
  if (ssid.empty() || (unknown_ssid_encoding && !validated_name.empty()))
    return validated_name;

  if (ssid != validated_name) {
    NET_LOG(DEBUG) << "GetNameFromProperties: " << service_path
                   << " SSID=" << ssid << " Validated name=" << validated_name;
  }
  return ssid;
}

std::unique_ptr<NetworkUIData> GetUIDataFromValue(
    const base::Value& ui_data_value) {
  const std::string* ui_data_str = ui_data_value.GetIfString();
  if (!ui_data_str) {
    return nullptr;
  }
  if (ui_data_str->empty()) {
    return std::make_unique<NetworkUIData>();
  }
  std::optional<base::Value::Dict> ui_data_dict =
      chromeos::onc::ReadDictionaryFromJson(*ui_data_str);
  if (!ui_data_dict.has_value()) {
    return nullptr;
  }
  return std::make_unique<NetworkUIData>(ui_data_dict.value());
}

std::unique_ptr<NetworkUIData> GetUIDataFromProperties(
    const base::Value::Dict& shill_dictionary) {
  const base::Value* ui_data_value =
      shill_dictionary.Find(shill::kUIDataProperty);
  if (!ui_data_value) {
    VLOG(2) << "Dictionary has no UIData entry.";
    return nullptr;
  }
  std::unique_ptr<NetworkUIData> ui_data = GetUIDataFromValue(*ui_data_value);
  if (!ui_data) {
    LOG(ERROR) << "UIData is not a valid JSON dictionary.";
  }
  return ui_data;
}

void SetRandomMACPolicy(::onc::ONCSource onc_source,
                        base::Value::Dict* shill_dictionary) {
  std::string* service_type =
      shill_dictionary->FindString(shill::kTypeProperty);
  DCHECK(service_type);
  if (*service_type != shill::kTypeWifi) {
    // For non-wifi types we don't set MAC policy at all.
    return;
  }

  // If the feature flag is not enabled, we set each MAC Address Policy
  // to Hardware (non-randomized).
  if (!base::FeatureList::IsEnabled(
          features::kWifiConnectMacAddressRandomization)) {
    shill_dictionary->Set(shill::kWifiRandomMACPolicy,
                          shill::kWifiRandomMacPolicyHardware);
    return;
  }

  // For enterprise policies we also set MAC Address Policy
  // to Hardware (non-randomized).
  if (onc_source == ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY ||
      onc_source == ::onc::ONCSource::ONC_SOURCE_USER_POLICY ||
      // User Import is not policy per se, but we use it to have some means
      // to force Hardware address by user in experimental mode.
      onc_source == ::onc::ONCSource::ONC_SOURCE_USER_IMPORT) {
    shill_dictionary->Set(shill::kWifiRandomMACPolicy,
                          shill::kWifiRandomMacPolicyHardware);
    return;
  }

  // In all other cases, set the MAC Address Policy
  // to Persistent-Random (Randomized per SSID, but persistent
  // once randomized).
  shill_dictionary->Set(shill::kWifiRandomMACPolicy,
                        shill::kWifiRandomMacPolicyPersistentRandom);
}

void SetUIDataAndSource(const NetworkUIData& ui_data,
                        base::Value::Dict* shill_dictionary) {
  shill_dictionary->Set(shill::kUIDataProperty, ui_data.GetAsJson());
  std::string source;
  switch (ui_data.onc_source()) {
    case ::onc::ONC_SOURCE_UNKNOWN:
      source = shill::kONCSourceUnknown;
      break;
    case ::onc::ONC_SOURCE_NONE:
      source = shill::kONCSourceNone;
      break;
    case ::onc::ONC_SOURCE_USER_IMPORT:
      source = shill::kONCSourceUserImport;
      break;
    case ::onc::ONC_SOURCE_DEVICE_POLICY:
      source = shill::kONCSourceDevicePolicy;
      break;
    case ::onc::ONC_SOURCE_USER_POLICY:
      source = shill::kONCSourceUserPolicy;
      break;
  }
  shill_dictionary->Set(shill::kONCSourceProperty, source);
}

bool CopyIdentifyingProperties(const base::Value::Dict& service_properties,
                               const bool properties_read_from_shill,
                               base::Value::Dict* dest) {
  bool success = true;

  // GUID is optional.
  CopyStringFromDictionary(service_properties, shill::kGuidProperty, dest);

  std::string type;
  const std::string* type_str =
      service_properties.FindString(shill::kTypeProperty);
  if (type_str)
    type = *type_str;
  success &= !type.empty();
  dest->Set(shill::kTypeProperty, type);
  if (type == shill::kTypeWifi) {
    success &= CopyStringFromDictionary(service_properties,
                                        shill::kSecurityClassProperty, dest);
    success &=
        CopyStringFromDictionary(service_properties, shill::kWifiHexSsid, dest);
    success &= CopyStringFromDictionary(service_properties,
                                        shill::kModeProperty, dest);
  } else if (type == shill::kTypeVPN) {
    success &= CopyStringFromDictionary(service_properties,
                                        shill::kNameProperty, dest);

    // VPN Provider values are read from the "Provider" dictionary, but written
    // with the keys "Provider.Type" and "Provider.Host".
    // TODO(pneubeck): Simplify this once http://crbug.com/381135 is fixed.
    std::string vpn_provider_type;
    std::string vpn_provider_host;
    if (properties_read_from_shill) {
      const base::Value::Dict* provider_properties =
          service_properties.FindDict(shill::kProviderProperty);
      if (!provider_properties) {
        NET_LOG(ERROR) << "Missing VPN provider dict: "
                       << GetNetworkIdFromProperties(service_properties);
        return false;
      }
      const std::string* vpn_provider_type_str =
          provider_properties->FindString(shill::kTypeProperty);
      if (vpn_provider_type_str)
        vpn_provider_type = *vpn_provider_type_str;
      const std::string* vpn_provider_host_str =
          provider_properties->FindString(shill::kHostProperty);
      if (vpn_provider_host_str)
        vpn_provider_host = *vpn_provider_host_str;
    } else {
      const std::string* vpn_provider_type_str =
          service_properties.FindString(shill::kProviderTypeProperty);
      if (vpn_provider_type_str)
        vpn_provider_type = *vpn_provider_type_str;
      const std::string* vpn_provider_host_str =
          service_properties.FindString(shill::kProviderHostProperty);
      if (vpn_provider_host_str)
        vpn_provider_host = *vpn_provider_host_str;
    }
    success &= !vpn_provider_type.empty();
    dest->Set(shill::kProviderTypeProperty, vpn_provider_type);

    success &= !vpn_provider_host.empty();
    dest->Set(shill::kProviderHostProperty, vpn_provider_host);
  } else if (type == shill::kTypeEthernet || type == shill::kTypeEthernetEap ||
             type == shill::kTypeCellular) {
    // Ethernet, EthernetEAP and Cellular don't have any additional identifying
    // properties.
  } else {
    NET_LOG(ERROR) << "Unsupported network type " << type;
    success = false;
  }
  if (!success) {
    NET_LOG(ERROR) << "Missing required properties: "
                   << GetNetworkIdFromProperties(service_properties);
  }
  return success;
}

bool DoIdentifyingPropertiesMatch(const base::Value::Dict& new_properties,
                                  const base::Value::Dict& old_properties) {
  base::Value::Dict new_identifying;
  if (!CopyIdentifyingProperties(
          new_properties, false /* properties were not read from Shill */,
          &new_identifying)) {
    return false;
  }
  base::Value::Dict old_identifying;
  if (!CopyIdentifyingProperties(old_properties,
                                 true /* properties were read from Shill */,
                                 &old_identifying)) {
    return false;
  }

  return new_identifying == old_identifying;
}

bool IsLoggableShillProperty(const std::string& key) {
  static std::set<std::string>* s_skip_properties = nullptr;
  if (!s_skip_properties) {
    s_skip_properties = new std::set<std::string>;
    s_skip_properties->insert(shill::kApnPasswordProperty);
    s_skip_properties->insert(shill::kEapCaCertPemProperty);
    s_skip_properties->insert(shill::kEapCaCertProperty);
    s_skip_properties->insert(shill::kEapPasswordProperty);
    s_skip_properties->insert(shill::kEapPinProperty);
    s_skip_properties->insert(shill::kL2TPIPsecCaCertPemProperty);
    s_skip_properties->insert(shill::kL2TPIPsecPasswordProperty);
    s_skip_properties->insert(shill::kL2TPIPsecPinProperty);
    s_skip_properties->insert(shill::kL2TPIPsecPskProperty);
    s_skip_properties->insert(shill::kOpenVPNAuthUserPassProperty);
    s_skip_properties->insert(shill::kOpenVPNCaCertPemProperty);
    s_skip_properties->insert(shill::kOpenVPNExtraCertPemProperty);
    s_skip_properties->insert(shill::kOpenVPNOTPProperty);
    s_skip_properties->insert(shill::kOpenVPNPasswordProperty);
    s_skip_properties->insert(shill::kOpenVPNPinProperty);
    s_skip_properties->insert(shill::kOpenVPNTLSAuthContentsProperty);
    s_skip_properties->insert(shill::kPassphraseProperty);
    s_skip_properties->insert(shill::kUIDataProperty);
  }
  return s_skip_properties->count(key) == 0;
}

}  // namespace ash::shill_property_util
