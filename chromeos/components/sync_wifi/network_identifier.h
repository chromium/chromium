// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_NETWORK_IDENTIFIER_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_NETWORK_IDENTIFIER_H_

#include <string>

#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"

namespace sync_pb {
class WifiConfigurationSpecificsData;
}

namespace chromeos {

namespace sync_wifi {

// A unique identifier for synced networks which contains the properties
// necessary to differentiate a synced network from another with the same name.
class NetworkIdentifier {
 public:
  static NetworkIdentifier FromProto(
      const sync_pb::WifiConfigurationSpecificsData& specifics);
  static NetworkIdentifier FromMojoNetwork(
      const network_config::mojom::NetworkStatePropertiesPtr& network);
  // |serialized_string| is in the format of hex_ssid and security_type
  // concatenated with an underscore.  security_type is the shill constant
  // returned from NetworkState::security_class(). For example, it would be
  // "2F_psk" if the hex_ssid is 2F and the security_type is psk.
  static NetworkIdentifier DeserializeFromString(
      const std::string& serialized_string);

  // |hex_ssid| hex encoded representation of an ssid.  For example, ssid
  // "network" could be provided as "6E6574776F726B" or "0x6e6574776f726b".
  // |security_type| is the shill constant that would be returned from
  // NetworkState::security_class().
  NetworkIdentifier(const std::string& hex_ssid,
                    const std::string& security_type);
  virtual ~NetworkIdentifier();

  bool operator==(const NetworkIdentifier& o) const;
  bool operator!=(const NetworkIdentifier& o) const;
  bool operator<(const NetworkIdentifier& o) const;
  bool operator>(const NetworkIdentifier& o) const;

  std::string SerializeToString() const;

  // This will always be returned in a format with upper case letters and no
  // 0x prefix.
  const std::string& hex_ssid() const { return hex_ssid_; }
  const std::string& security_type() const { return security_type_; }

 private:
  std::string hex_ssid_;
  std::string security_type_;
};

}  // namespace sync_wifi

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_NETWORK_IDENTIFIER_H_
