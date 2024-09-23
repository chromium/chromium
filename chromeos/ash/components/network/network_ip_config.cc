// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_ip_config.h"

#include "base/notreached.h"

namespace ash {

namespace {
#define ENUM_CASE(x) case x: return std::string(#x)
std::string IPConfigTypeAsString(IPConfigType type) {
  switch (type) {
    ENUM_CASE(IPCONFIG_TYPE_UNKNOWN);
    ENUM_CASE(IPCONFIG_TYPE_IPV4);
    ENUM_CASE(IPCONFIG_TYPE_IPV6);
    ENUM_CASE(IPCONFIG_TYPE_DHCP);
    ENUM_CASE(IPCONFIG_TYPE_BOOTP);
    ENUM_CASE(IPCONFIG_TYPE_ZEROCONF);
    ENUM_CASE(IPCONFIG_TYPE_DHCP6);
    ENUM_CASE(IPCONFIG_TYPE_PPP);
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}
#undef ENUM_CASE
}  // namespace

NetworkIPConfig::NetworkIPConfig(
    const std::string& device_path, IPConfigType type,
    const std::string& address, const std::string& netmask,
    const std::string& gateway, const std::string& name_servers)
    : device_path(device_path),
      type(type),
      address(address),
      netmask(netmask),
      gateway(gateway),
      name_servers(name_servers) {
}

NetworkIPConfig::~NetworkIPConfig() = default;

std::string NetworkIPConfig::ToString() const {
  return std::string("path: ") + device_path
      + " type: " + IPConfigTypeAsString(type)
      + " address: " + address
      + " netmask: " + netmask
      + " gateway: " + gateway
      + " name_servers: " + name_servers;
}

}  // namespace ash
