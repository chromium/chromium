// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_type_pattern.h"

#include <stddef.h>

#include "base/stl_util.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/tether_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kPatternDefault[] = "PatternDefault";
const char kPatternWireless[] = "PatternWireless";
const char kPatternMobile[] = "PatternMobile";
const char kPatternNonVirtual[] = "PatternNonVirtual";
const char kPatternPhysical[] = "PatternPhysical";

enum NetworkTypeBitFlag {
  kNetworkTypeNone = 0,
  kNetworkTypeEthernet = 1 << 0,
  kNetworkTypeWifi = 1 << 1,
  kNetworkTypeCellular = 1 << 2,
  kNetworkTypeVPN = 1 << 3,
  kNetworkTypeEthernetEap = 1 << 4,
  kNetworkTypeBluetooth = 1 << 5,
  kNetworkTypeTether = 1 << 6
};

struct ShillToBitFlagEntry {
  const char* shill_network_type;
  NetworkTypeBitFlag bit_flag;
} shill_type_to_flag[] = {{shill::kTypeEthernet, kNetworkTypeEthernet},
                          {shill::kTypeEthernetEap, kNetworkTypeEthernetEap},
                          {shill::kTypeWifi, kNetworkTypeWifi},
                          {shill::kTypeCellular, kNetworkTypeCellular},
                          {shill::kTypeVPN, kNetworkTypeVPN},
                          {shill::kTypeBluetooth, kNetworkTypeBluetooth},
                          {kTypeTether, kNetworkTypeTether}};

NetworkTypeBitFlag ShillNetworkTypeToFlag(const std::string& shill_type) {
  for (size_t i = 0; i < base::size(shill_type_to_flag); ++i) {
    if (shill_type_to_flag[i].shill_network_type == shill_type)
      return shill_type_to_flag[i].bit_flag;
  }
  NET_LOG_ERROR("ShillNetworkTypeToFlag", "Unknown type: " + shill_type);
  return kNetworkTypeNone;
}

}  // namespace

// static
NetworkTypePattern NetworkTypePattern::Default() {
  return NetworkTypePattern(~0);
}

// static
NetworkTypePattern NetworkTypePattern::Wireless() {
  return NetworkTypePattern(kNetworkTypeWifi | kNetworkTypeCellular |
                            kNetworkTypeTether);
}

// static
NetworkTypePattern NetworkTypePattern::Mobile() {
  return NetworkTypePattern(kNetworkTypeCellular | kNetworkTypeTether);
}

// static
NetworkTypePattern NetworkTypePattern::Physical() {
  return NetworkTypePattern(kNetworkTypeWifi | kNetworkTypeCellular |
                            kNetworkTypeEthernet);
}

// static
NetworkTypePattern NetworkTypePattern::NonVirtual() {
  return NetworkTypePattern(~(kNetworkTypeVPN | kNetworkTypeEthernetEap));
}

// static
NetworkTypePattern NetworkTypePattern::Ethernet() {
  return NetworkTypePattern(kNetworkTypeEthernet);
}

// static
NetworkTypePattern NetworkTypePattern::EthernetOrEthernetEAP() {
  return NetworkTypePattern(kNetworkTypeEthernet | kNetworkTypeEthernetEap);
}

// static
NetworkTypePattern NetworkTypePattern::WiFi() {
  return NetworkTypePattern(kNetworkTypeWifi);
}

// static
NetworkTypePattern NetworkTypePattern::Cellular() {
  return NetworkTypePattern(kNetworkTypeCellular);
}

// static
NetworkTypePattern NetworkTypePattern::VPN() {
  return NetworkTypePattern(kNetworkTypeVPN);
}

// static
NetworkTypePattern NetworkTypePattern::Tether() {
  return NetworkTypePattern(kNetworkTypeTether);
}

// static
NetworkTypePattern NetworkTypePattern::Primitive(
    const std::string& shill_network_type) {
  return NetworkTypePattern(ShillNetworkTypeToFlag(shill_network_type));
}

bool NetworkTypePattern::Equals(const NetworkTypePattern& other) const {
  return pattern_ == other.pattern_;
}

bool NetworkTypePattern::MatchesType(
    const std::string& shill_network_type) const {
  if (shill_network_type.empty()) {
    NOTREACHED() << "NetworkTypePattern: " << ToDebugString()
                 << ": Can not match empty type.";
    return false;
  }
  return MatchesPattern(Primitive(shill_network_type));
}

bool NetworkTypePattern::MatchesPattern(
    const NetworkTypePattern& other_pattern) const {
  if (Equals(other_pattern))
    return true;

  return pattern_ & other_pattern.pattern_;
}

NetworkTypePattern NetworkTypePattern::operator|(
    const NetworkTypePattern& other) const {
  return NetworkTypePattern(pattern_ | other.pattern_);
}

std::string NetworkTypePattern::ToDebugString() const {
  if (Equals(Default()))
    return kPatternDefault;
  if (Equals(Wireless()))
    return kPatternWireless;
  if (Equals(Mobile()))
    return kPatternMobile;
  if (Equals(Physical()))
    return kPatternPhysical;
  if (Equals(NonVirtual()))
    return kPatternNonVirtual;

  // Note: shill_type_to_flag includes kTypeTether.
  std::string str;
  for (size_t i = 0; i < base::size(shill_type_to_flag); ++i) {
    if (!(pattern_ & shill_type_to_flag[i].bit_flag))
      continue;
    if (!str.empty())
      str += "|";
    str += shill_type_to_flag[i].shill_network_type;
  }
  return str;
}

NetworkTypePattern::NetworkTypePattern(int pattern) : pattern_(pattern) {}

}  // namespace chromeos
