// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "components/onc/onc_constants.h"

namespace chromeos::network_config {

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

}  // namespace chromeos::network_config
