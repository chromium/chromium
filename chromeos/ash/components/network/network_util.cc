// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_util.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/ash/components/network/onc/onc_translation_tables.h"
#include "chromeos/ash/components/network/onc/onc_translator.h"
#include "chromeos/components/onc/onc_signature.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

WifiAccessPoint::WifiAccessPoint()
    : signal_strength(0),
      signal_to_noise(0),
      channel(0) {
}

WifiAccessPoint::WifiAccessPoint(const WifiAccessPoint& other) = default;

WifiAccessPoint::~WifiAccessPoint() = default;

CellTower::CellTower() = default;

CellTower::CellTower(const CellTower& other) = default;

CellTower::~CellTower() = default;

CellularScanResult::CellularScanResult() = default;

CellularScanResult::CellularScanResult(const CellularScanResult& other) =
    default;

CellularScanResult::~CellularScanResult() = default;

CellularSIMSlotInfo::CellularSIMSlotInfo() = default;

CellularSIMSlotInfo::CellularSIMSlotInfo(const CellularSIMSlotInfo& other) =
    default;

CellularSIMSlotInfo::~CellularSIMSlotInfo() = default;

namespace network_util {

std::string PrefixLengthToNetmask(int32_t prefix_length) {
  std::string netmask;
  // Return the empty string for invalid inputs.
  if (prefix_length < 0 || prefix_length > 32)
    return netmask;
  for (int i = 0; i < 4; i++) {
    int remainder = 8;
    if (prefix_length >= 8) {
      prefix_length -= 8;
    } else {
      remainder = prefix_length;
      prefix_length = 0;
    }
    if (i > 0)
      netmask += ".";
    int value = remainder == 0 ? 0 :
        ((2L << (remainder - 1)) - 1) << (8 - remainder);
    netmask += base::NumberToString(value);
  }
  return netmask;
}

int32_t NetmaskToPrefixLength(const std::string& netmask) {
  int count = 0;
  int prefix_length = 0;
  base::StringTokenizer t(netmask, ".");
  while (t.GetNext()) {
    // If there are more than 4 numbers, then it's invalid.
    if (count == 4)
      return -1;

    std::string token = t.token();
    // If we already found the last mask and the current one is not
    // "0" then the netmask is invalid. For example, 255.224.255.0
    if (prefix_length / 8 != count) {
      if (token != "0")
        return -1;
    } else if (token == "255") {
      prefix_length += 8;
    } else if (token == "254") {
      prefix_length += 7;
    } else if (token == "252") {
      prefix_length += 6;
    } else if (token == "248") {
      prefix_length += 5;
    } else if (token == "240") {
      prefix_length += 4;
    } else if (token == "224") {
      prefix_length += 3;
    } else if (token == "192") {
      prefix_length += 2;
    } else if (token == "128") {
      prefix_length += 1;
    } else if (token == "0") {
      prefix_length += 0;
    } else {
      // mask is not a valid number.
      return -1;
    }
    count++;
  }
  if (count < 4)
    return -1;
  return prefix_length;
}

std::string FormattedMacAddress(const std::string& shill_mac_address) {
  if (shill_mac_address.size() % 2 != 0)
    return shill_mac_address;
  std::string result;
  for (size_t i = 0; i < shill_mac_address.size(); ++i) {
    if ((i != 0) && (i % 2 == 0))
      result.push_back(':');
    result.push_back(base::ToUpperASCII(shill_mac_address[i]));
  }
  return result;
}

bool ParseCellularScanResults(const base::Value::List& list,
                              std::vector<CellularScanResult>* scan_results) {
  scan_results->clear();
  scan_results->reserve(list.size());
  for (const auto& value : list) {
    const base::Value::Dict* value_dict = value.GetIfDict();
    if (!value_dict) {
      return false;
    }

    CellularScanResult scan_result;
    // If the network id property is not present then this network cannot be
    // connected to so don't include it in the results.
    const std::string* network_id =
        value_dict->FindString(shill::kNetworkIdProperty);
    if (!network_id) {
      continue;
    }
    scan_result.network_id = *network_id;
    const std::string* status = value_dict->FindString(shill::kStatusProperty);
    if (status) {
      scan_result.status = *status;
    }
    const std::string* long_name =
        value_dict->FindString(shill::kLongNameProperty);
    if (long_name) {
      scan_result.long_name = *long_name;
    }
    const std::string* short_name =
        value_dict->FindString(shill::kShortNameProperty);
    if (short_name) {
      scan_result.short_name = *short_name;
    }
    const std::string* technology =
        value_dict->FindString(shill::kTechnologyProperty);
    if (technology) {
      scan_result.technology = *technology;
    }
    scan_results->push_back(scan_result);
  }
  return true;
}

bool ParseCellularSIMSlotInfo(
    const base::Value::List& list,
    std::vector<CellularSIMSlotInfo>* sim_slot_infos) {
  sim_slot_infos->clear();
  sim_slot_infos->reserve(list.size());
  for (size_t i = 0; i < list.size(); i++) {
    const base::Value::Dict* value = list[i].GetIfDict();
    if (!value) {
      return false;
    }

    CellularSIMSlotInfo sim_slot_info;
    // The |slot_id| should start with 1.
    sim_slot_info.slot_id = i + 1;

    const std::string* eid = value->FindString(shill::kSIMSlotInfoEID);
    if (eid) {
      sim_slot_info.eid = *eid;
    }

    const std::string* iccid = value->FindString(shill::kSIMSlotInfoICCID);
    if (iccid) {
      sim_slot_info.iccid = *iccid;
    }

    std::optional<bool> primary = value->FindBool(shill::kSIMSlotInfoPrimary);
    sim_slot_info.primary = primary.has_value() ? *primary : false;

    sim_slot_infos->push_back(sim_slot_info);
  }
  return true;
}

base::Value::Dict TranslateNetworkStateToONC(const NetworkState* network) {
  // Get the properties from the NetworkState.
  base::Value::Dict shill_dictionary;
  network->GetStateProperties(&shill_dictionary);

  // Get any Device properties required to translate state.
  if (NetworkTypePattern::Cellular().MatchesType(network->type())) {
    const DeviceState* device =
        NetworkHandler::Get()->network_state_handler()->GetDeviceState(
            network->device_path());
    if (device) {
      shill_dictionary.Set(
          shill::kDeviceProperty,
          base::Value::Dict()
              // We need to set Device.Cellular.ProviderRequiresRoaming so that
              // Cellular.RoamingState can be set correctly for badging network
              // icons.
              .Set(shill::kProviderRequiresRoamingProperty,
                   device->provider_requires_roaming())
              // Scanning is also used in the UI when displaying a list of
              // networks.
              .Set(shill::kScanningProperty, device->scanning()));
    }
  }

  // NetworkState is always associated with the primary user profile, regardless
  // of what profile is associated with the page that calls this method. We do
  // not expose any sensitive properties in the resulting dictionary, it is
  // only used to show connection state and icons.
  std::string user_id_hash = LoginState::Get()->primary_user_hash();
  ::onc::ONCSource onc_source = ::onc::ONC_SOURCE_NONE;
  NetworkHandler::Get()
      ->managed_network_configuration_handler()
      ->FindPolicyByGUID(user_id_hash, network->guid(), &onc_source);

  base::Value::Dict onc_dictionary = onc::TranslateShillServiceToONCPart(
      shill_dictionary, onc_source, &chromeos::onc::kNetworkWithStateSignature,
      network);

  // Remove IPAddressConfigType/NameServersConfigType as these were
  // historically not provided by TranslateNetworkStateToONC.
  // The source shill properties for those ONC properties are not provided by
  // NetworkState::GetStateProperties, however since CL:2530330 these are
  // assumed to have defaults that are always enforced during ONC translation.
  onc_dictionary.Remove(::onc::network_config::kIPAddressConfigType);
  onc_dictionary.Remove(::onc::network_config::kNameServersConfigType);

  return onc_dictionary;
}

base::Value::List TranslateNetworkListToONC(NetworkTypePattern pattern,
                                            bool configured_only,
                                            bool visible_only,
                                            int limit) {
  NetworkStateHandler::NetworkStateList network_states;
  NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
      pattern, configured_only, visible_only, limit, &network_states);

  base::Value::List network_properties_list;
  for (const NetworkState* state : network_states) {
    network_properties_list.Append(TranslateNetworkStateToONC(state));
  }
  return network_properties_list;
}

std::string TranslateONCTypeToShill(const std::string& onc_type) {
  if (onc_type == ::onc::network_type::kEthernet)
    return shill::kTypeEthernet;
  std::string shill_type;
  onc::TranslateStringToShill(onc::kNetworkTypeTable, onc_type, &shill_type);
  return shill_type;
}

std::string TranslateONCSecurityToShill(const std::string& onc_security) {
  std::string shill_security;
  onc::TranslateStringToShill(onc::kWiFiSecurityTable, onc_security,
                              &shill_security);
  return shill_security;
}

std::string TranslateShillTypeToONC(const std::string& shill_type) {
  if (shill_type == shill::kTypeEthernet)
    return ::onc::network_type::kEthernet;
  std::string onc_type;
  onc::TranslateStringToONC(onc::kNetworkTypeTable, shill_type, &onc_type);
  return onc_type;
}

}  // namespace network_util
}  // namespace ash
