// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_TYPE_PATTERN_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_TYPE_PATTERN_H_

#include <string>

#include "base/component_export.h"

namespace ash {

// Class to convert Shill network type names to explicit types and do pattern
// matching for grouped types (e.g. Wireless). Grouped type matching is also
// implemented for mojo types in cros_network_config_util.cc.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkTypePattern {
 public:
  // Matches any network.
  static NetworkTypePattern Default();

  // Matches wireless (WiFi, Cellular, etc.) networks
  static NetworkTypePattern Wireless();

  // Matches Cellular or Tether networks.
  static NetworkTypePattern Mobile();

  // Matches Physical networks (i.e. excludes Tether and VPN).
  static NetworkTypePattern Physical();

  // Excludes virtual networks and EthernetEAP.
  static NetworkTypePattern NonVirtual();

  // Matches ethernet networks.
  static NetworkTypePattern Ethernet();

  // Matches ethernet or ethernet EAP networks.
  static NetworkTypePattern EthernetOrEthernetEAP();

  static NetworkTypePattern WiFi();
  static NetworkTypePattern Cellular();
  static NetworkTypePattern VPN();

  static NetworkTypePattern Tether();

  // Matches only networks of exactly the type |shill_network_type|, which must
  // be one of the types defined in service_constants.h (e.g.
  // shill::kTypeWifi).
  // Note: Shill distinguishes Ethernet without EAP from Ethernet with EAP. If
  // unsure, better use one of the matchers above.
  static NetworkTypePattern Primitive(const std::string& shill_network_type);

  NetworkTypePattern(const NetworkTypePattern&) = default;
  NetworkTypePattern& operator=(const NetworkTypePattern&) = default;

  bool Equals(const NetworkTypePattern& other) const;
  bool MatchesType(const std::string& shill_network_type) const;

  // Returns true if this pattern matches at least one network type that
  // |other_pattern| matches (according to MatchesType). Thus MatchesPattern is
  // symmetric and reflexive but not transitive.
  // See the unit test for examples.
  bool MatchesPattern(const NetworkTypePattern& other_pattern) const;

  NetworkTypePattern operator|(const NetworkTypePattern& other) const;

  std::string ToDebugString() const;

 private:
  explicit NetworkTypePattern(int pattern);

  // The bit array of the matching network types.
  int pattern_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_TYPE_PATTERN_H_
