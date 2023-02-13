// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"

#include "components/onc/onc_constants.h"

namespace chromeos::network_config {

namespace {

absl::optional<std::string> GetString(const base::Value::Dict& onc_apn,
                                      const char* key) {
  const std::string* v = onc_apn.FindString(key);
  return v ? absl::make_optional<std::string>(*v) : absl::nullopt;
}

std::string GetRequiredString(const base::Value::Dict& onc_apn,
                              const char* key) {
  const std::string* v = onc_apn.FindString(key);
  if (!v) {
    NOTREACHED() << "Required key missing: " << key;
    return std::string();
  }
  return *v;
}

std::vector<std::string> GetRequiredStringList(const base::Value::Dict& dict,
                                               const char* key) {
  const base::Value::List* v = dict.FindList(key);
  if (!v) {
    NOTREACHED() << "Required key missing: " << key;
    return {};
  }
  std::vector<std::string> result;
  result.reserve(v->size());
  for (const base::Value& e : *v) {
    if (!e.is_string()) {
      NOTREACHED() << "Expected string, found: " << e;
      break;
    }
    result.push_back(e.GetString());
  }
  return result;
}

mojom::ApnAuthenticationType OncApnAuthenticationTypeToMojo(
    const absl::optional<std::string>& authentication_type) {
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

  NOTREACHED() << "Unexpected ONC APN Authentication type: "
               << authentication_type.value();
  return mojom::ApnAuthenticationType::kAutomatic;
}

mojom::ApnIpType OncApnIpTypeToMojo(const std::string& ip_type) {
  if (ip_type.empty() || ip_type == ::onc::cellular_apn::kIpTypeAutomatic) {
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

  NOTREACHED() << "Unexpected ONC APN IP type: " << ip_type;
  return mojom::ApnIpType::kAutomatic;
}

}  // namespace

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
  NOTREACHED();
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
  NOTREACHED();
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
  NOTREACHED();
  return 0;
}

bool IsInhibited(const mojom::DeviceStateProperties* device) {
  return device->inhibit_reason != mojom::InhibitReason::kNotInhibited;
}

base::Value::Dict UserApnListToOnc(const std::string& network_guid,
                                   const base::Value::List* user_apn_list) {
  base::Value::Dict onc;
  onc.Set(::onc::network_config::kGUID, network_guid);
  onc.Set(::onc::network_config::kType, ::onc::network_type::kCellular);
  base::Value::Dict type_dict;
  // If |user_apn_list| is a nullptr, set the value as Value::Type::NONE
  if (user_apn_list) {
    type_dict.Set(::onc::cellular::kUserAPNList, user_apn_list->Clone());
  } else {
    type_dict.Set(::onc::cellular::kUserAPNList, base::Value());
  }
  onc.Set(::onc::network_type::kCellular, std::move(type_dict));
  return onc;
}

std::vector<mojom::ApnType> OncApnTypesToMojo(
    const std::vector<std::string>& apn_types) {
  DCHECK(!apn_types.empty());
  std::vector<mojom::ApnType> apn_types_result;
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

    NOTREACHED() << "Unexpected ONC APN Type: " << apn_type;
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
    apn->ip_type = OncApnIpTypeToMojo(
        GetRequiredString(onc_apn, ::onc::cellular_apn::kIpType));
    apn->apn_types = OncApnTypesToMojo(
        GetRequiredStringList(onc_apn, ::onc::cellular_apn::kApnTypes));
  }

  return apn;
}

}  // namespace chromeos::network_config
