// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_config.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/values.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "net/base/ip_address.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

std::optional<NetworkConfig::IPCIDR> ParseCIDR(std::string_view key,
                                               const base::Value& val) {
  if (!val.is_string()) {
    NET_LOG(ERROR) << "Failed to parse " << key << " not a string";
    return std::nullopt;
  }

  const std::string& str = val.GetString();
  if (str.empty()) {
    return std::nullopt;
  }
  NetworkConfig::IPCIDR ret;
  size_t prefix_len;
  if (!net::ParseCIDRBlock(str, &ret.addr, &prefix_len)) {
    NET_LOG(ERROR) << "Failed to parse " << key << " with value " << val;
    return std::nullopt;
  }
  ret.prefix_len = static_cast<int>(prefix_len);
  return ret;
}

std::optional<net::IPAddress> ParseIPAddress(std::string_view key,
                                             const base::Value& val) {
  if (!val.is_string()) {
    NET_LOG(ERROR) << "Failed to parse " << key << " not a string";
    return std::nullopt;
  }

  const std::string& str = val.GetString();
  if (str.empty()) {
    return std::nullopt;
  }
  auto ret = net::IPAddress::FromIPLiteral(str);
  if (!ret) {
    NET_LOG(ERROR) << "Failed to parse " << key << " with value " << str;
  }
  return ret;
}

}  // namespace

std::unique_ptr<NetworkConfig> NetworkConfig::ParseFromServicePropertyValue(
    const base::Value& value) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict || dict->empty()) {
    return nullptr;
  }

  auto network_config = std::make_unique<NetworkConfig>();

  // IPv4 address and gateway.
  if (const auto* ipv4_address =
          dict->Find(shill::kNetworkConfigIPv4AddressProperty)) {
    network_config->ipv4_address =
        ParseCIDR(shill::kNetworkConfigIPv4AddressProperty, *ipv4_address);
  }
  if (const auto* ipv4_gateway =
          dict->Find(shill::kNetworkConfigIPv4GatewayProperty)) {
    network_config->ipv4_gateway =
        ParseIPAddress(shill::kNetworkConfigIPv4GatewayProperty, *ipv4_gateway);
  }

  // IPv6 addresses and gateway.
  if (const auto* ipv6_addresses =
          dict->FindList(shill::kNetworkConfigIPv6AddressesProperty)) {
    for (const base::Value& item : *ipv6_addresses) {
      std::optional<NetworkConfig::IPCIDR> cidr =
          ParseCIDR(shill::kNetworkConfigIPv6AddressesProperty, item);
      if (cidr) {
        network_config->ipv6_addresses.push_back(*cidr);
      }
    }
  }
  if (const auto* ipv6_gateway =
          dict->Find(shill::kNetworkConfigIPv6GatewayProperty)) {
    network_config->ipv6_gateway =
        ParseIPAddress(shill::kNetworkConfigIPv6GatewayProperty, *ipv6_gateway);
  }

  // DNS and domain searches.
  if (const auto* dns_list =
          dict->FindList(shill::kNetworkConfigNameServersProperty)) {
    for (const base::Value& dns_value : *dns_list) {
      std::optional<net::IPAddress> name_server =
          ParseIPAddress(shill::kNetworkConfigNameServersProperty, dns_value);
      // When manually setting DNS, up to 4 addresses can be specified in the
      // UI. Unspecified entries can show up as 0.0.0.0 and should be removed.
      if (name_server && !name_server->IsZero()) {
        network_config->dns_servers.push_back(*name_server);
      }
    }
  }
  if (const auto* domains =
          dict->FindList(shill::kNetworkConfigSearchDomainsProperty)) {
    for (const auto& domain : *domains) {
      network_config->search_domains.push_back(domain.GetString());
    }
  }

  // Included routes and excluded routes.
  if (const auto* include_routes_list =
          dict->FindList(shill::kNetworkConfigIncludedRoutesProperty)) {
    for (const base::Value& item : *include_routes_list) {
      std::optional<NetworkConfig::IPCIDR> cidr =
          ParseCIDR(shill::kNetworkConfigIncludedRoutesProperty, item);
      if (cidr) {
        network_config->included_routes.push_back(*cidr);
      }
    }
  }
  if (const auto* exclude_routes_list =
          dict->FindList(shill::kNetworkConfigExcludedRoutesProperty)) {
    for (const base::Value& item : *exclude_routes_list) {
      std::optional<NetworkConfig::IPCIDR> cidr =
          ParseCIDR(shill::kNetworkConfigExcludedRoutesProperty, item);
      if (cidr) {
        network_config->excluded_routes.push_back(*cidr);
      }
    }
  }

  // MTU.
  if (int mtu = dict->FindInt(shill::kNetworkConfigMTUProperty).value_or(0);
      mtu > 0) {
    network_config->mtu = mtu;
  }

  return network_config;
}

NetworkConfig::NetworkConfig() = default;

NetworkConfig::~NetworkConfig() = default;

}  // namespace ash
