// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_event_log.h"

#include "base/strings/string_util.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/tether_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {
constexpr char kServicePrefix[] = "/service/";
constexpr char kUnknownId[] = "<none>";

ash::NetworkStateHandler* GetNetworkStateHandler() {
  if (!ash::NetworkHandler::IsInitialized())
    return nullptr;
  return ash::NetworkHandler::Get()->network_state_handler();
}

}  // namespace

namespace ash {

// Returns a descriptive unique id for |network|
// e.g.: ethernet_0, wifi_psk_1, cellular_lte_2, vpn_openvpn_3.
std::string NetworkId(const NetworkState* network) {
  if (!network)
    return kUnknownId;

  const std::string& type = network->type();
  if (type == kTypeTether) {
    // Tether uses a GUID for its service path. Use the first 8 digits.
    return type + "_" + network->path().substr(0, 8);
  }

  // Test networks may not follow the Shill pattern, just use the path.
  if (!base::StartsWith(network->path(), kServicePrefix,
                        base::CompareCase::SENSITIVE)) {
    return network->path();
  }

  std::string id = network->path().substr(strlen(kServicePrefix));
  if (type.empty())
    return "service_" + id;

  std::string result = type + "_";
  if (type == shill::kTypeWifi) {
    result += network->security_class() + "_";
  } else if (type == shill::kTypeCellular) {
    result += network->network_technology() + "_";
  } else if (type == shill::kTypeVPN) {
    result += network->GetVpnProviderType() + "_";
  }
  return result + id;
}

// Calls NetworkId() if a NetworkState for |service_path| exists, otherwise
// returns service_{id}. If |service_path| does not represent a valid service
// path (e.g. in tests), returns |service_path|.
std::string NetworkPathId(const std::string& service_path) {
  NetworkStateHandler* handler = GetNetworkStateHandler();
  if (handler) {
    const NetworkState* network = handler->GetNetworkState(service_path);
    if (network)
      return NetworkId(network);
  }
  if (!base::StartsWith(service_path, kServicePrefix,
                        base::CompareCase::SENSITIVE)) {
    return service_path;
  }
  std::string id = service_path.substr(strlen(kServicePrefix));
  return "service_" + id;
}

// Calls NetworkId() if a NetworkState exists for |guid|, otherwise
// returns |guid|.
std::string NetworkGuidId(const std::string& guid) {
  if (guid.empty())
    return kUnknownId;
  NetworkStateHandler* handler = GetNetworkStateHandler();
  if (handler) {
    const NetworkState* network = handler->GetNetworkStateFromGuid(guid);
    if (network)
      return NetworkId(network);
  }
  return guid;
}

}  // namespace ash
