// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_type_pattern.h"

#include "chromeos/network/tether_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

class NetworkTypePatternTest : public testing::Test {
 public:
  NetworkTypePatternTest()
      : cellular_(NetworkTypePattern::Cellular()),
        default_(NetworkTypePattern::Default()),
        ethernet_(NetworkTypePattern::Ethernet()),
        mobile_(NetworkTypePattern::Mobile()),
        physical_(NetworkTypePattern::Physical()),
        non_virtual_(NetworkTypePattern::NonVirtual()),
        wireless_(NetworkTypePattern::Wireless()),
        tether_(NetworkTypePattern::Tether()),
        vpn_(NetworkTypePattern::VPN()),
        wifi_(NetworkTypePattern::WiFi()) {}

  bool MatchesPattern(const NetworkTypePattern& a,
                      const NetworkTypePattern& b) {
    // Verify that NetworkTypePattern::MatchesPattern is symmetric.
    EXPECT_TRUE(a.MatchesPattern(b) == b.MatchesPattern(a));
    return a.MatchesPattern(b);
  }

 protected:
  const NetworkTypePattern cellular_;
  const NetworkTypePattern default_;
  const NetworkTypePattern ethernet_;
  const NetworkTypePattern mobile_;
  const NetworkTypePattern physical_;
  const NetworkTypePattern non_virtual_;
  const NetworkTypePattern wireless_;
  const NetworkTypePattern tether_;
  const NetworkTypePattern vpn_;
  const NetworkTypePattern wifi_;
};

}  // namespace

TEST_F(NetworkTypePatternTest, MatchesType) {
  // Mobile contains Cellular and Tether.
  EXPECT_TRUE(mobile_.MatchesType(shill::kTypeCellular));
  EXPECT_TRUE(mobile_.MatchesType(kTypeTether));
  EXPECT_FALSE(mobile_.MatchesType(shill::kTypeWifi));
  EXPECT_FALSE(mobile_.MatchesType(shill::kTypeEthernet));
  EXPECT_FALSE(mobile_.MatchesType(shill::kTypeVPN));

  // Wireless contains Wifi, Cellular and Tether.
  EXPECT_TRUE(wireless_.MatchesType(shill::kTypeWifi));
  EXPECT_TRUE(wireless_.MatchesType(shill::kTypeCellular));
  EXPECT_TRUE(wireless_.MatchesType(kTypeTether));
  EXPECT_FALSE(wireless_.MatchesType(shill::kTypeEthernet));
  EXPECT_FALSE(wireless_.MatchesType(shill::kTypeVPN));

  // Networks managed by Shill (excludes Tether and VPN).
  EXPECT_TRUE(physical_.MatchesType(shill::kTypeCellular));
  EXPECT_TRUE(physical_.MatchesType(shill::kTypeWifi));
  EXPECT_TRUE(physical_.MatchesType(shill::kTypeEthernet));
  EXPECT_FALSE(physical_.MatchesType(kTypeTether));
  EXPECT_FALSE(physical_.MatchesType(shill::kTypeVPN));

  // Non-virtual contains everything except VPN.
  EXPECT_TRUE(non_virtual_.MatchesType(shill::kTypeCellular));
  EXPECT_TRUE(non_virtual_.MatchesType(shill::kTypeWifi));
  EXPECT_TRUE(non_virtual_.MatchesType(shill::kTypeEthernet));
  EXPECT_TRUE(non_virtual_.MatchesType(kTypeTether));
  EXPECT_FALSE(non_virtual_.MatchesType(shill::kTypeVPN));
}

TEST_F(NetworkTypePatternTest, MatchesPattern) {
  // Each pair of {Mobile, Wireless, Cellular} is matching. Matching is
  // reflexive and symmetric (checked in MatchesPattern).
  EXPECT_TRUE(MatchesPattern(mobile_, mobile_));
  EXPECT_TRUE(MatchesPattern(wireless_, wireless_));
  EXPECT_TRUE(MatchesPattern(cellular_, cellular_));

  EXPECT_TRUE(MatchesPattern(mobile_, wireless_));
  EXPECT_TRUE(MatchesPattern(mobile_, cellular_));
  EXPECT_TRUE(MatchesPattern(wireless_, cellular_));

  // Cellular matches NonVirtual. NonVirtual matches Ethernet. But Cellular
  // doesn't match Ethernet.
  EXPECT_TRUE(MatchesPattern(cellular_, non_virtual_));
  EXPECT_TRUE(MatchesPattern(non_virtual_, ethernet_));
  EXPECT_FALSE(MatchesPattern(cellular_, ethernet_));

  EXPECT_TRUE(MatchesPattern(tether_, wireless_));
  EXPECT_TRUE(MatchesPattern(tether_, non_virtual_));

  // Default matches anything.
  EXPECT_TRUE(MatchesPattern(default_, default_));
  EXPECT_TRUE(MatchesPattern(default_, non_virtual_));
  EXPECT_TRUE(MatchesPattern(default_, cellular_));
}

TEST_F(NetworkTypePatternTest, Equals) {
  EXPECT_TRUE(mobile_.Equals(mobile_));
  EXPECT_FALSE(mobile_.Equals(cellular_));
  EXPECT_FALSE(cellular_.Equals(mobile_));
}

TEST_F(NetworkTypePatternTest, Primitive) {
  const NetworkTypePattern primitive_cellular =
      NetworkTypePattern::Primitive(shill::kTypeCellular);
  EXPECT_TRUE(cellular_.Equals(primitive_cellular));
  EXPECT_TRUE(primitive_cellular.Equals(cellular_));

  const NetworkTypePattern primitive_wifi =
      NetworkTypePattern::Primitive(shill::kTypeWifi);
  EXPECT_TRUE(wifi_.Equals(primitive_wifi));
  EXPECT_TRUE(primitive_wifi.Equals(wifi_));
}

TEST_F(NetworkTypePatternTest, Or) {
  NetworkTypePattern compound = wifi_ | cellular_;
  EXPECT_TRUE(cellular_.MatchesPattern(compound));
  EXPECT_TRUE(wifi_.MatchesPattern(compound));
  EXPECT_FALSE(ethernet_.MatchesPattern(compound));
}

TEST_F(NetworkTypePatternTest, ToDebugString) {
  EXPECT_EQ(default_.ToDebugString(), "PatternDefault");
  EXPECT_EQ(wireless_.ToDebugString(), "PatternWireless");
  EXPECT_EQ(mobile_.ToDebugString(), "PatternMobile");
  EXPECT_EQ(non_virtual_.ToDebugString(), "PatternNonVirtual");
  EXPECT_EQ(ethernet_.ToDebugString(), shill::kTypeEthernet);
  EXPECT_EQ(cellular_.ToDebugString(), shill::kTypeCellular);
  EXPECT_EQ(tether_.ToDebugString(), kTypeTether);
  EXPECT_EQ(wifi_.ToDebugString(), shill::kTypeWifi);
  EXPECT_EQ(vpn_.ToDebugString(), shill::kTypeVPN);
}

}  // namespace chromeos
