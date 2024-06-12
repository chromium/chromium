// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_config.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/values.h"
#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace {

TEST(NetworkConfigTest, ParseValue) {
  base::Value::Dict properties;
  properties.Set(shill::kNetworkConfigIPv4AddressProperty, "1.2.3.4/24");
  properties.Set(shill::kNetworkConfigIPv4GatewayProperty, "1.2.3.5");
  properties.Set(shill::kNetworkConfigIPv6AddressesProperty,
                 base::Value::List().Append("fd00::1/64").Append("fd00::2/64"));
  properties.Set(shill::kNetworkConfigIPv6GatewayProperty, "fd01::2");
  properties.Set(shill::kNetworkConfigNameServersProperty,
                 base::Value::List().Append("4.3.2.1").Append("fdfd::1").Append(
                     "0.0.0.0"));
  properties.Set(shill::kNetworkConfigSearchDomainsProperty,
                 base::Value::List().Append("domain1").Append("domain2"));
  properties.Set(shill::kNetworkConfigMTUProperty, 1400);
  properties.Set(
      shill::kNetworkConfigIncludedRoutesProperty,
      base::Value::List().Append("10.10.10.0/24").Append("fd01::/64"));
  properties.Set(
      shill::kNetworkConfigExcludedRoutesProperty,
      base::Value::List().Append("10.20.30.0/24").Append("fd02::/64"));

  std::unique_ptr<NetworkConfig> config =
      NetworkConfig::ParseFromServicePropertyValue(
          base::Value(std::move(properties)));

  ASSERT_TRUE(config);
  EXPECT_EQ(config->ipv4_address->addr.ToString(), "1.2.3.4");
  EXPECT_EQ(config->ipv4_address->prefix_len, 24);
  EXPECT_EQ(config->ipv4_gateway->ToString(), "1.2.3.5");
  ASSERT_EQ(config->ipv6_addresses.size(), 2u);
  EXPECT_EQ(config->ipv6_addresses[0].addr.ToString(), "fd00::1");
  EXPECT_EQ(config->ipv6_addresses[0].prefix_len, 64);
  EXPECT_EQ(config->ipv6_addresses[1].addr.ToString(), "fd00::2");
  EXPECT_EQ(config->ipv6_addresses[1].prefix_len, 64);
  EXPECT_EQ(config->ipv6_gateway->ToString(), "fd01::2");
  ASSERT_EQ(config->dns_servers.size(), 2u);
  EXPECT_EQ(config->dns_servers[0].ToString(), "4.3.2.1");
  EXPECT_EQ(config->dns_servers[1].ToString(), "fdfd::1");
  ASSERT_EQ(config->search_domains.size(), 2u);
  EXPECT_EQ(config->search_domains[0], "domain1");
  EXPECT_EQ(config->search_domains[1], "domain2");
  EXPECT_EQ(config->mtu, 1400);
  ASSERT_EQ(config->included_routes.size(), 2u);
  EXPECT_EQ(config->included_routes[0].addr.ToString(), "10.10.10.0");
  EXPECT_EQ(config->included_routes[0].prefix_len, 24);
  EXPECT_EQ(config->included_routes[1].addr.ToString(), "fd01::");
  EXPECT_EQ(config->included_routes[1].prefix_len, 64);
  ASSERT_EQ(config->excluded_routes.size(), 2u);
  EXPECT_EQ(config->excluded_routes[0].addr.ToString(), "10.20.30.0");
  EXPECT_EQ(config->excluded_routes[0].prefix_len, 24);
  EXPECT_EQ(config->excluded_routes[1].addr.ToString(), "fd02::");
  EXPECT_EQ(config->excluded_routes[1].prefix_len, 64);
}

TEST(NetworkConfigTest, ParseEmptyValue) {
  std::unique_ptr<NetworkConfig> config =
      NetworkConfig::ParseFromServicePropertyValue(
          base::Value(base::Value::Dict()));
  EXPECT_FALSE(config);
}

}  // namespace
}  // namespace ash
