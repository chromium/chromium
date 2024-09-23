// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "components/device_event_log/device_event_log.h"
#include "components/onc/onc_constants.h"

namespace chromeos::network_config {

const char kMojoKeySecurity[] = "security";
const char kMojoKeySsid[] = "ssid";
const char kMojoKeyPassphrase[] = "passphrase";
const char kMojoKeyEapInner[] = "inner";
const char kMojoKeyEapOuter[] = "outer";
const char kMojoKeyEapIdentity[] = "identity";
const char kMojoKeyEapAnonymousIdentity[] = "anonymousIdentity";
const char kMojoKeyEapPassword[] = "password";
const char kMojoKeyEap[] = "eap";
const char kMojoKeyWifi[] = "wifi";
const char kMojoKeyTypeConfig[] = "typeConfig";

namespace {

std::optional<std::string> GetString(const base::Value::Dict& onc_apn,
                                     const char* key) {
  const std::string* v = onc_apn.FindString(key);
  return v ? std::make_optional<std::string>(*v) : std::nullopt;
}

std::string GetRequiredString(const base::Value::Dict& onc_apn,
                              const char* key) {
  const std::string* v = onc_apn.FindString(key);
  if (!v) {
    NOTREACHED_IN_MIGRATION() << "Required key missing: " << key;
    return std::string();
  }
  return *v;
}

std::vector<std::string> GetRequiredStringList(const base::Value::Dict& dict,
                                               const char* key) {
  const base::Value::List* v = dict.FindList(key);
  if (!v) {
    NOTREACHED_IN_MIGRATION() << "Required key missing: " << key;
    return {};
  }
  std::vector<std::string> result;
  result.reserve(v->size());
  for (const base::Value& e : *v) {
    if (!e.is_string()) {
      NOTREACHED_IN_MIGRATION() << "Expected string, found: " << e;
      break;
    }
    result.push_back(e.GetString());
  }
  return result;
}

mojom::ApnAuthenticationType OncApnAuthenticationTypeToMojo(
    const std::optional<std::string>& authentication_type) {
  if (!authentication_type.has_value() || authentication_type->empty() ||
      authentication_type == ::onc::cellular_apn::kAuthenticationAutomatic) {
    return mojom::ApnAuthenticationType::kAutomatic;
  }
  if (authentication_type == ::onc::cellular_apn::kAuthenticationPap) {
    return mojom::ApnAuthenticationType::kPap;
  }
  if (authentication_type == ::onc::cellular_apn::kAuthenticationChap) {
    return mojom::ApnAuthenticationType::kChap;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected ONC APN Authentication type: "
                            << authentication_type.value();
  return mojom::ApnAuthenticationType::kAutomatic;
}

mojom::ApnIpType OncApnIpTypeToMojo(const std::optional<std::string>& ip_type) {
  if (!ip_type.has_value() || ip_type->empty() ||
      ip_type == ::onc::cellular_apn::kIpTypeAutomatic) {
    return mojom::ApnIpType::kAutomatic;
  }
  if (ip_type == ::onc::cellular_apn::kIpTypeIpv4) {
    return mojom::ApnIpType::kIpv4;
  }
  if (ip_type == ::onc::cellular_apn::kIpTypeIpv6) {
    return mojom::ApnIpType::kIpv6;
  }
  if (ip_type == ::onc::cellular_apn::kIpTypeIpv4Ipv6) {
    return mojom::ApnIpType::kIpv4Ipv6;
  }

  NOTREACHED_IN_MIGRATION()
      << "Unexpected ONC APN IP type: " << ip_type.value();
  return mojom::ApnIpType::kAutomatic;
}

mojom::ApnSource OncApnSourceToMojo(const std::optional<std::string>& source) {
  if (!source.has_value() || source->empty() ||
      source == ::onc::cellular_apn::kSourceModem) {
    return mojom::ApnSource::kModem;
  }
  if (source == ::onc::cellular_apn::kSourceModb) {
    return mojom::ApnSource::kModb;
  }
  if (source == ::onc::cellular_apn::kSourceUi) {
    return mojom::ApnSource::kUi;
  }

  // TODO(b/5429735): Add mojom::ApnSource::kAdmin in follow up CL

  NET_LOG(DEBUG) << "Unexpected APN source: " << source.value();
  return mojom::ApnSource::kModem;
}

}  // namespace

bool GetBoolean(const base::Value::Dict* dict,
                const char* key,
                bool value_if_key_missing_from_dict) {
  const base::Value* v = dict->Find(key);
  if (v && !v->is_bool()) {
    NET_LOG(ERROR) << "Expected bool, found: " << *v;
    return false;
  }
  return v ? v->GetBool() : value_if_key_missing_from_dict;
}

std::optional<std::string> GetString(const base::Value::Dict* dict,
                                     const char* key) {
  const base::Value* v = dict->Find(key);
  if (v && !v->is_string()) {
    NET_LOG(ERROR) << "Expected string, found: " << *v;
    return std::nullopt;
  }
  return v ? std::make_optional(v->GetString()) : std::nullopt;
}

const base::Value::Dict* GetDictionary(const base::Value::Dict* dict,
                                       const char* key) {
  const base::Value* v = dict->Find(key);
  if (!v) {
    return nullptr;
  }
  if (!v->is_dict()) {
    NET_LOG(ERROR) << "Expected dictionary, found: " << *v;
    return nullptr;
  }
  return &v->GetDict();
}

ManagedDictionary GetManagedDictionary(const base::Value::Dict* onc_dict) {
  ManagedDictionary result;

  // When available, the active value (i.e. the value from Shill) is used.
  if (onc_dict->contains(::onc::kAugmentationActiveSetting)) {
    result.active_value =
        onc_dict->Find(::onc::kAugmentationActiveSetting)->Clone();
  }

  std::optional<std::string> effective =
      GetString(onc_dict, ::onc::kAugmentationEffectiveSetting);
  if (!effective) {
    return result;
  }

  // If no active value is set (e.g. the network is not visible), use the
  // effective value.
  if (result.active_value.is_none() && onc_dict->contains(effective.value())) {
    result.active_value = onc_dict->Find(effective.value())->Clone();
  }
  if (result.active_value.is_none()) {
    // No active or effective value, return a default dictionary.
    return result;
  }

  // If the effective value is set by an extension, use kActiveExtension.
  if (effective == ::onc::kAugmentationActiveExtension) {
    result.policy_source = mojom::PolicySource::kActiveExtension;
    result.policy_value = result.active_value.Clone();
    return result;
  }

  // Set policy properties based on the effective source and policies.
  // NOTE: This does not enforce valid ONC. See onc_merger.cc for details.
  const base::Value* user_policy =
      onc_dict->Find(::onc::kAugmentationUserPolicy);
  const base::Value* device_policy =
      onc_dict->Find(::onc::kAugmentationDevicePolicy);
  bool user_enforced = !GetBoolean(onc_dict, ::onc::kAugmentationUserEditable);
  bool device_enforced =
      !GetBoolean(onc_dict, ::onc::kAugmentationDeviceEditable);
  if (effective == ::onc::kAugmentationUserPolicy ||
      (user_policy && effective != ::onc::kAugmentationDevicePolicy)) {
    // Set the policy source to "User" when:
    // * The effective value is set to "UserPolicy" OR
    // * A User policy exists and the effective value is not "DevicePolicy",
    //   i.e. no enforced device policy is overriding a recommended user policy.
    result.policy_source = user_enforced
                               ? mojom::PolicySource::kUserPolicyEnforced
                               : mojom::PolicySource::kUserPolicyRecommended;
    if (user_policy) {
      result.policy_value = user_policy->Clone();
    }
  } else if (effective == ::onc::kAugmentationDevicePolicy || device_policy) {
    // Set the policy source to "Device" when:
    // * The effective value is set to "DevicePolicy" OR
    // * A Device policy exists (since we checked for a user policy first).
    result.policy_source = device_enforced
                               ? mojom::PolicySource::kDevicePolicyEnforced
                               : mojom::PolicySource::kDevicePolicyRecommended;
    if (device_policy) {
      result.policy_value = device_policy->Clone();
    }
  } else if (effective == ::onc::kAugmentationUserSetting ||
             effective == ::onc::kAugmentationSharedSetting) {
    // User or shared setting, no policy source.
  } else {
    // Unexpected ONC. No policy source or value will be set.
    NET_LOG(ERROR) << "Unexpected ONC property: " << *onc_dict;
  }

  DCHECK(result.policy_value.is_none() ||
         result.policy_value.type() == result.active_value.type());
  return result;
}

mojom::ManagedStringPtr GetManagedString(const base::Value::Dict* dict,
                                         const char* key) {
  const base::Value* v = dict->Find(key);
  if (!v) {
    return nullptr;
  }
  if (v->is_string()) {
    auto result = mojom::ManagedString::New();
    result->active_value = v->GetString();
    return result;
  }
  if (v->is_dict()) {
    ManagedDictionary managed_dict = GetManagedDictionary(&v->GetDict());
    if (!managed_dict.active_value.is_string()) {
      NET_LOG(ERROR) << "No active or effective value for: " << key;
      return nullptr;
    }
    auto result = mojom::ManagedString::New();
    result->active_value = managed_dict.active_value.GetString();
    result->policy_source = managed_dict.policy_source;
    if (!managed_dict.policy_value.is_none()) {
      result->policy_value = managed_dict.policy_value.GetString();
    }
    return result;
  }
  NET_LOG(ERROR) << "Expected string or dictionary, found: " << *v;
  return nullptr;
}

mojom::ManagedStringPtr GetRequiredManagedString(const base::Value::Dict* dict,
                                                 const char* key) {
  mojom::ManagedStringPtr result = GetManagedString(dict, key);
  if (!result) {
    // Return an empty string with no policy source.
    result = mojom::ManagedString::New();
  }
  return result;
}

mojom::ManagedApnPropertiesPtr GetManagedApnProperties(
    const base::Value::Dict* cellular_dict,
    const char* key) {
  const base::Value::Dict* apn_dict = cellular_dict->FindDict(key);
  if (!apn_dict) {
    return nullptr;
  }
  auto apn = mojom::ManagedApnProperties::New();
  apn->access_point_name =
      GetRequiredManagedString(apn_dict, ::onc::cellular_apn::kAccessPointName);
  CHECK(apn->access_point_name);
  apn->authentication =
      GetManagedString(apn_dict, ::onc::cellular_apn::kAuthentication);
  apn->language = GetManagedString(apn_dict, ::onc::cellular_apn::kLanguage);
  apn->localized_name =
      GetManagedString(apn_dict, ::onc::cellular_apn::kLocalizedName);
  apn->name = GetManagedString(apn_dict, ::onc::cellular_apn::kName);
  apn->password = GetManagedString(apn_dict, ::onc::cellular_apn::kPassword);
  apn->username = GetManagedString(apn_dict, ::onc::cellular_apn::kUsername);
  apn->attach = GetManagedString(apn_dict, ::onc::cellular_apn::kAttach);
  return apn;
}

// This matches logic in NetworkTypePattern and should be kept in sync.
bool NetworkTypeMatchesType(mojom::NetworkType network_type,
                            mojom::NetworkType match_type) {
  switch (match_type) {
    case mojom::NetworkType::kAll:
      return true;
    case mojom::NetworkType::kMobile:
      return network_type == mojom::NetworkType::kCellular ||
             network_type == mojom::NetworkType::kTether;
    case mojom::NetworkType::kWireless:
      return network_type == mojom::NetworkType::kCellular ||
             network_type == mojom::NetworkType::kTether ||
             network_type == mojom::NetworkType::kWiFi;
    case mojom::NetworkType::kCellular:
    case mojom::NetworkType::kEthernet:
    case mojom::NetworkType::kTether:
    case mojom::NetworkType::kVPN:
    case mojom::NetworkType::kWiFi:
      return network_type == match_type;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool NetworkStateMatchesType(const mojom::NetworkStateProperties* network,
                             mojom::NetworkType type) {
  return NetworkTypeMatchesType(network->type, type);
}

bool StateIsConnected(mojom::ConnectionStateType connection_state) {
  switch (connection_state) {
    case mojom::ConnectionStateType::kOnline:
    case mojom::ConnectionStateType::kConnected:
    case mojom::ConnectionStateType::kPortal:
      return true;
    case mojom::ConnectionStateType::kConnecting:
    case mojom::ConnectionStateType::kNotConnected:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

int GetWirelessSignalStrength(const mojom::NetworkStateProperties* network) {
  switch (network->type) {
    case mojom::NetworkType::kCellular:
      return network->type_state->get_cellular()->signal_strength;
    case mojom::NetworkType::kEthernet:
      return 0;
    case mojom::NetworkType::kTether:
      return network->type_state->get_tether()->signal_strength;
    case mojom::NetworkType::kVPN:
      return 0;
    case mojom::NetworkType::kWiFi:
      return network->type_state->get_wifi()->signal_strength;
    case mojom::NetworkType::kAll:
    case mojom::NetworkType::kMobile:
    case mojom::NetworkType::kWireless:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

bool IsInhibited(const mojom::DeviceStateProperties* device) {
  return device->inhibit_reason != mojom::InhibitReason::kNotInhibited;
}

base::Value::Dict CustomApnListToOnc(const std::string& network_guid,
                                     const base::Value::List* custom_apn_list) {
  CHECK(custom_apn_list);
  base::Value::Dict onc;
  onc.Set(::onc::network_config::kGUID, network_guid);
  onc.Set(::onc::network_config::kType, ::onc::network_type::kCellular);
  base::Value::Dict type_dict;
  type_dict.Set(::onc::cellular::kCustomAPNList, custom_apn_list->Clone());
  onc.Set(::onc::network_type::kCellular, std::move(type_dict));
  return onc;
}

std::vector<mojom::ApnType> OncApnTypesToMojo(
    const std::vector<std::string>& apn_types) {
  std::vector<mojom::ApnType> apn_types_result;
  if (apn_types.empty()) {
    NET_LOG(ERROR) << "APN types is empty";
    return apn_types_result;
  }

  apn_types_result.reserve(apn_types.size());
  for (const std::string& apn_type : apn_types) {
    if (apn_type == ::onc::cellular_apn::kApnTypeDefault) {
      apn_types_result.push_back(mojom::ApnType::kDefault);
      continue;
    }
    if (apn_type == ::onc::cellular_apn::kApnTypeAttach) {
      apn_types_result.push_back(mojom::ApnType::kAttach);
      continue;
    }
    if (apn_type == ::onc::cellular_apn::kApnTypeTether) {
      apn_types_result.push_back(mojom::ApnType::kTether);
      continue;
    }

    NOTREACHED_IN_MIGRATION() << "Unexpected ONC APN Type: " << apn_type;
  }

  return apn_types_result;
}

mojom::ApnPropertiesPtr GetApnProperties(const base::Value::Dict& onc_apn,
                                         bool is_apn_revamp_enabled) {
  auto apn = mojom::ApnProperties::New();
  apn->access_point_name =
      GetRequiredString(onc_apn, ::onc::cellular_apn::kAccessPointName);
  apn->authentication = OncApnAuthenticationTypeToMojo(
      GetString(onc_apn, ::onc::cellular_apn::kAuthentication));
  apn->language = GetString(onc_apn, ::onc::cellular_apn::kLanguage);
  apn->localized_name = GetString(onc_apn, ::onc::cellular_apn::kLocalizedName);
  apn->name = GetString(onc_apn, ::onc::cellular_apn::kName);
  apn->password = GetString(onc_apn, ::onc::cellular_apn::kPassword);
  apn->username = GetString(onc_apn, ::onc::cellular_apn::kUsername);
  apn->attach = GetString(onc_apn, ::onc::cellular_apn::kAttach);

  if (is_apn_revamp_enabled) {
    apn->id = GetString(onc_apn, ::onc::cellular_apn::kId);
    apn->ip_type =
        OncApnIpTypeToMojo(GetString(onc_apn, ::onc::cellular_apn::kIpType));
    apn->apn_types = OncApnTypesToMojo(
        GetRequiredStringList(onc_apn, ::onc::cellular_apn::kApnTypes));
    apn->source =
        OncApnSourceToMojo(GetString(onc_apn, ::onc::cellular_apn::kSource));
  }

  return apn;
}

mojom::ManagedApnListPtr GetManagedApnList(const base::Value* value,
                                           bool is_apn_revamp_enabled) {
  if (!value) {
    return nullptr;
  }
  if (value->is_list()) {
    auto result = mojom::ManagedApnList::New();
    std::vector<mojom::ApnPropertiesPtr> active;
    for (const base::Value& e : value->GetList()) {
      active.push_back(GetApnProperties(e.GetDict(), is_apn_revamp_enabled));
    }
    result->active_value = std::move(active);
    return result;
  } else if (value->is_dict()) {
    ManagedDictionary managed_dict = GetManagedDictionary(&value->GetDict());
    if (!managed_dict.active_value.is_list()) {
      NET_LOG(ERROR) << "No active or effective value for APNList";
      return nullptr;
    }
    auto result = mojom::ManagedApnList::New();
    for (const base::Value& e : managed_dict.active_value.GetList()) {
      result->active_value.push_back(
          GetApnProperties(e.GetDict(), is_apn_revamp_enabled));
    }
    result->policy_source = managed_dict.policy_source;
    if (!managed_dict.policy_value.is_none()) {
      result->policy_value = std::vector<mojom::ApnPropertiesPtr>();
      for (const base::Value& e : managed_dict.policy_value.GetList()) {
        result->policy_value->push_back(
            GetApnProperties(e.GetDict(), is_apn_revamp_enabled));
      }
    }
    return result;
  }
  NET_LOG(ERROR) << "Expected list or dictionary, found: " << *value;
  return nullptr;
}

base::Value::Dict WiFiConfigPropertiesToMojoJsValue(
    const mojo::StructPtr<
        chromeos::network_config::mojom::WiFiConfigProperties>& wifi_config) {
  base::Value::Dict prefilled_wifi_config;
  prefilled_wifi_config.Set(kMojoKeySecurity,
                            static_cast<int>(wifi_config->security));
  if (wifi_config->ssid.has_value()) {
    prefilled_wifi_config.Set(kMojoKeySsid, *(wifi_config->ssid));
  }
  if (wifi_config->passphrase.has_value()) {
    prefilled_wifi_config.Set(kMojoKeyPassphrase, *(wifi_config->passphrase));
  }
  if (!wifi_config->eap.is_null()) {
    auto& eap_config = wifi_config->eap;
    base::Value::Dict prefilled_eap_config;
    if (eap_config->inner.has_value()) {
      prefilled_eap_config.Set(kMojoKeyEapInner, *(eap_config->inner));
    }
    if (eap_config->outer.has_value()) {
      prefilled_eap_config.Set(kMojoKeyEapOuter, *(eap_config->outer));
    }
    if (eap_config->identity.has_value()) {
      prefilled_eap_config.Set(kMojoKeyEapIdentity, *(eap_config->identity));
    }
    if (eap_config->anonymous_identity.has_value()) {
      prefilled_eap_config.Set(kMojoKeyEapAnonymousIdentity,
                               *(eap_config->anonymous_identity));
    }
    if (eap_config->password.has_value()) {
      prefilled_eap_config.Set(kMojoKeyEapPassword, *(eap_config->password));
    }
    prefilled_wifi_config.Set(kMojoKeyEap, prefilled_eap_config.Clone());
  }
  base::Value::Dict type_config;
  type_config.Set(kMojoKeyWifi, prefilled_wifi_config.Clone());
  base::Value::Dict config;
  config.Set(kMojoKeyTypeConfig, type_config.Clone());
  return config;
}

}  // namespace chromeos::network_config
