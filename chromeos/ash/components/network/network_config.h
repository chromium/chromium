// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONFIG_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONFIG_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "net/base/ip_address.h"

namespace ash {

// This reflects the NetworkConfig dict on Service and contains information
// about the network layer configuration on this Service. All fields are
// optional so they can be empty. Also see platform2/shill/doc/service-api.txt
// in CrOS repo.
struct COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkConfig {
  // Represents an IP address with a prefix.
  struct IPCIDR {
    net::IPAddress addr;
    int prefix_len = 0;
  };

  // Parse the NetworkConfig from the D-Bus |value| for the "NetworkConfig"
  // property on Service. Returns nullptr if |value| is an empty dict, otherwise
  // parses |value| in a best-effort way and returns a valid object.
  static std::unique_ptr<NetworkConfig> ParseFromServicePropertyValue(
      const base::Value& value);

  NetworkConfig();
  ~NetworkConfig();

  // NetworkConfig is neither copyable nor movable.
  NetworkConfig(const NetworkConfig&) = delete;
  NetworkConfig& operator=(const NetworkConfig&) = delete;

  // Assigned IPv4 address on the device for this network.
  std::optional<IPCIDR> ipv4_address;
  // IPv4 gateway on the network.
  std::optional<net::IPAddress> ipv4_gateway;

  // Assigned IPv6 addresses on the device for this network.
  std::vector<IPCIDR> ipv6_addresses;
  // IPv6 gateway on the network.
  std::optional<net::IPAddress> ipv6_gateway;

  // Name servers (both IPv4 and IPv6) and search domains.
  std::vector<net::IPAddress> dns_servers;
  std::vector<std::string> search_domains;

  // Maximum transmission unit. 0 means the default 1500 value is used by the
  // platform.
  int mtu = 0;

  // The prefixes for the destinations which should not or should be routed
  // via this network.These two properties are only meaningful for a VPN
  // network.
  std::vector<IPCIDR> included_routes;
  std::vector<IPCIDR> excluded_routes;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONFIG_H_
