// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/shill_property_util.h"

#include <stdint.h>

#include <memory>
#include <set>

#include "base/i18n/encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/values.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace shill_property_util {

namespace {

// Replace non UTF8 characters in |str| with a replacement character.
std::string ValidateUTF8(const std::string& str) {
  std::string result;
  for (int32_t index = 0; index < static_cast<int32_t>(str.size()); ++index) {
    uint32_t code_point_out;
    bool is_unicode_char = base::ReadUnicodeCharacter(
        str.c_str(), str.size(), &index, &code_point_out);
    const uint32_t kFirstNonControlChar = 0x20;
    if (is_unicode_char && (code_point_out >= kFirstNonControlChar)) {
      base::WriteUnicodeCharacter(code_point_out, &result);
    } else {
      const uint32_t kReplacementChar = 0xFFFD;
      // Puts kReplacementChar if character is a control character [0,0x20)
      // or is not readable UTF8.
      base::WriteUnicodeCharacter(kReplacementChar, &result);
    }
  }
  return result;
}

// If existent and non-empty, copies the string at |key| from |source| to
// |dest|. Returns true if the string was copied.
bool CopyStringFromDictionary(const base::DictionaryValue& source,
                              const std::string& key,
                              base::DictionaryValue* dest) {
  std::string string_value;
  if (!source.GetStringWithoutPathExpansion(key, &string_value) ||
      string_value.empty()) {
    return false;
  }
  dest->SetKey(key, base::Value(string_value));
  return true;
}

std::string GetStringFromDictionary(const base::Value* dict, const char* key) {
  const base::Value* v = dict ? dict->FindKey(key) : nullptr;
  return v ? v->GetString() : std::string();
}

}  // namespace

void SetSSID(const std::string& ssid, base::Value* properties) {
  std::string hex_ssid = base::HexEncode(ssid.c_str(), ssid.size());
  properties->SetKey(shill::kWifiHexSsid, base::Value(hex_ssid));
}

std::string GetSSIDFromProperties(const base::Value& properties,
                                  bool verbose_logging,
                                  bool* unknown_encoding) {
  if (unknown_encoding)
    *unknown_encoding = false;

  // Get name for debugging.
  std::string name = GetStringFromDictionary(&properties, shill::kNameProperty);
  std::string hex_ssid =
      GetStringFromDictionary(&properties, shill::kWifiHexSsid);

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
    encoding = GetStringFromDictionary(&properties, shill::kCountryProperty);
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

std::string GetNetworkIdFromProperties(const base::Value& properties) {
  if (properties.DictEmpty())
    return "EmptyProperties";
  const base::Value* result =
      properties.FindKeyOfType(shill::kGuidProperty, base::Value::Type::STRING);
  if (!result) {
    result = properties.FindKeyOfType(shill::kSSIDProperty,
                                      base::Value::Type::STRING);
  }
  if (!result) {
    result = properties.FindKeyOfType(shill::kNameProperty,
                                      base::Value::Type::STRING);
  }
  if (result)
    return result->GetString();
  result =
      properties.FindKeyOfType(shill::kTypeProperty, base::Value::Type::STRING);
  return result ? "Unidentified " + result->GetString() : "UnknownType";
}

std::string GetNameFromProperties(const std::string& service_path,
                                  const base::Value& properties) {
  std::string name = GetStringFromDictionary(&properties, shill::kNameProperty);
  std::string validated_name = ValidateUTF8(name);
  if (validated_name != name) {
    NET_LOG(DEBUG) << "GetNameFromProperties: " << service_path
                   << " Validated name=" << validated_name << " name=" << name;
  }

  std::string type = GetStringFromDictionary(&properties, shill::kTypeProperty);
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
  std::string ui_data_str;
  if (!ui_data_value.GetAsString(&ui_data_str))
    return std::unique_ptr<NetworkUIData>();
  if (ui_data_str.empty())
    return std::make_unique<NetworkUIData>();
  std::unique_ptr<base::Value> ui_data_dict(
      chromeos::onc::ReadDictionaryFromJson(ui_data_str));
  if (!ui_data_dict)
    return std::unique_ptr<NetworkUIData>();
  return std::make_unique<NetworkUIData>(*ui_data_dict);
}

std::unique_ptr<NetworkUIData> GetUIDataFromProperties(
    const base::DictionaryValue& shill_dictionary) {
  const base::Value* ui_data_value = NULL;
  shill_dictionary.GetWithoutPathExpansion(shill::kUIDataProperty,
                                           &ui_data_value);
  if (!ui_data_value) {
    VLOG(2) << "Dictionary has no UIData entry.";
    return std::unique_ptr<NetworkUIData>();
  }
  std::unique_ptr<NetworkUIData> ui_data = GetUIDataFromValue(*ui_data_value);
  if (!ui_data)
    LOG(ERROR) << "UIData is not a valid JSON dictionary.";
  return ui_data;
}

void SetUIData(const NetworkUIData& ui_data,
               base::DictionaryValue* shill_dictionary) {
  shill_dictionary->SetKey(shill::kUIDataProperty,
                           base::Value(ui_data.GetAsJson()));
}

bool CopyIdentifyingProperties(const base::DictionaryValue& service_properties,
                               const bool properties_read_from_shill,
                               base::DictionaryValue* dest) {
  bool success = true;

  // GUID is optional.
  CopyStringFromDictionary(service_properties, shill::kGuidProperty, dest);

  std::string type;
  service_properties.GetStringWithoutPathExpansion(shill::kTypeProperty, &type);
  success &= !type.empty();
  dest->SetKey(shill::kTypeProperty, base::Value(type));
  if (type == shill::kTypeWifi) {
    success &=
        CopyStringFromDictionary(
            service_properties, shill::kSecurityClassProperty, dest);
    success &=
        CopyStringFromDictionary(service_properties, shill::kWifiHexSsid, dest);
    success &= CopyStringFromDictionary(
        service_properties, shill::kModeProperty, dest);
  } else if (type == shill::kTypeCellular) {
    success &= CopyStringFromDictionary(
        service_properties, shill::kNetworkTechnologyProperty, dest);
  } else if (type == shill::kTypeVPN) {
    success &= CopyStringFromDictionary(
        service_properties, shill::kNameProperty, dest);

    // VPN Provider values are read from the "Provider" dictionary, but written
    // with the keys "Provider.Type" and "Provider.Host".
    // TODO(pneubeck): Simplify this once http://crbug.com/381135 is fixed.
    std::string vpn_provider_type;
    std::string vpn_provider_host;
    if (properties_read_from_shill) {
      const base::DictionaryValue* provider_properties = NULL;
      if (!service_properties.GetDictionaryWithoutPathExpansion(
              shill::kProviderProperty, &provider_properties)) {
        NET_LOG(ERROR) << "Missing VPN provider dict: "
                       << GetNetworkIdFromProperties(service_properties);
      }
      provider_properties->GetStringWithoutPathExpansion(shill::kTypeProperty,
                                                         &vpn_provider_type);
      provider_properties->GetStringWithoutPathExpansion(shill::kHostProperty,
                                                         &vpn_provider_host);
    } else {
      service_properties.GetStringWithoutPathExpansion(
          shill::kProviderTypeProperty, &vpn_provider_type);
      service_properties.GetStringWithoutPathExpansion(
          shill::kProviderHostProperty, &vpn_provider_host);
    }
    success &= !vpn_provider_type.empty();
    dest->SetKey(shill::kProviderTypeProperty, base::Value(vpn_provider_type));

    success &= !vpn_provider_host.empty();
    dest->SetKey(shill::kProviderHostProperty, base::Value(vpn_provider_host));
  } else if (type == shill::kTypeEthernet || type == shill::kTypeEthernetEap) {
    // Ethernet and EthernetEAP don't have any additional identifying
    // properties.
  } else {
    NOTREACHED() << "Unsupported network type " << type;
    success = false;
  }
  if (!success) {
    NET_LOG(ERROR) << "Missing required properties: "
                   << GetNetworkIdFromProperties(service_properties);
  }
  return success;
}

bool DoIdentifyingPropertiesMatch(const base::DictionaryValue& new_properties,
                                  const base::DictionaryValue& old_properties) {
  base::DictionaryValue new_identifying;
  if (!CopyIdentifyingProperties(
          new_properties,
          false /* properties were not read from Shill */,
          &new_identifying)) {
    return false;
  }
  base::DictionaryValue old_identifying;
  if (!CopyIdentifyingProperties(old_properties,
                                 true /* properties were read from Shill */,
                                 &old_identifying)) {
    return false;
  }

  return new_identifying.Equals(&old_identifying);
}

bool IsLoggableShillProperty(const std::string& key) {
  static std::set<std::string>* s_skip_properties = nullptr;
  if (!s_skip_properties) {
    s_skip_properties = new std::set<std::string>;
    s_skip_properties->insert(shill::kApnPasswordProperty);
    s_skip_properties->insert(shill::kEapCaCertNssProperty);
    s_skip_properties->insert(shill::kEapCaCertPemProperty);
    s_skip_properties->insert(shill::kEapCaCertProperty);
    s_skip_properties->insert(shill::kEapClientCertNssProperty);
    s_skip_properties->insert(shill::kEapClientCertProperty);
    s_skip_properties->insert(shill::kEapPasswordProperty);
    s_skip_properties->insert(shill::kEapPinProperty);
    s_skip_properties->insert(shill::kEapPrivateKeyPasswordProperty);
    s_skip_properties->insert(shill::kEapPrivateKeyProperty);
    s_skip_properties->insert(shill::kL2tpIpsecCaCertPemProperty);
    s_skip_properties->insert(shill::kL2tpIpsecPasswordProperty);
    s_skip_properties->insert(shill::kL2tpIpsecPinProperty);
    s_skip_properties->insert(shill::kL2tpIpsecPskProperty);
    s_skip_properties->insert(shill::kOpenVPNAuthUserPassProperty);
    s_skip_properties->insert(shill::kOpenVPNCaCertNSSProperty);
    s_skip_properties->insert(shill::kOpenVPNCaCertPemProperty);
    s_skip_properties->insert(shill::kOpenVPNCaCertProperty);
    s_skip_properties->insert(shill::kOpenVPNCertProperty);
    s_skip_properties->insert(shill::kOpenVPNExtraCertPemProperty);
    s_skip_properties->insert(shill::kOpenVPNOTPProperty);
    s_skip_properties->insert(shill::kOpenVPNPasswordProperty);
    s_skip_properties->insert(shill::kOpenVPNPinProperty);
    s_skip_properties->insert(shill::kOpenVPNTLSAuthContentsProperty);
    s_skip_properties->insert(shill::kPPPoEPasswordProperty);
    s_skip_properties->insert(shill::kPassphraseProperty);
  }
  return s_skip_properties->count(key) == 0;
}

}  // namespace shill_property_util

}  // namespace chromeos
