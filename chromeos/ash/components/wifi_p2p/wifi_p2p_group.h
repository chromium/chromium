// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_WIFI_P2P_WIFI_P2P_GROUP_H_
#define CHROMEOS_ASH_COMPONENTS_WIFI_P2P_WIFI_P2P_GROUP_H_

#include <string>

#include "base/component_export.h"

namespace ash {

// Metadata representing an P2P group.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_WIFI_P2P) WifiP2PGroup {
 public:
  WifiP2PGroup(int shill_id,
               uint32_t frequency,
               int network_id,
               const std::string& ipv4_address,
               const std::string& ssid,
               const std::string& passphrase,
               bool is_owner);
  WifiP2PGroup(const WifiP2PGroup&);
  WifiP2PGroup& operator=(const WifiP2PGroup&);
  ~WifiP2PGroup();

  // Unique ID to identify the Wifi P2P group.
  int shill_id() const { return shill_id_; }

  // The operating frequency of the Wifi P2P group network.
  uint32_t frequency() const { return frequency_; }

  // Unique ID to identify the network in Patchpanel.
  int network_id() const { return network_id_; }

  // Ipv4 address for the Wifi P2P group.
  const std::string& ipv4_address() const { return ipv4_address_; }

  // SSID of the Wifi P2P group.
  const std::string& ssid() const { return ssid_; }

  // Passphrase of the Wifi P2P group
  const std::string& passphrase() const { return passphrase_; }

  // Whether the device is the group owner of the group.
  bool is_owner() const { return is_owner_; }

 private:
  int shill_id_;
  uint32_t frequency_;
  int network_id_;
  std::string ipv4_address_;
  std::string ssid_;
  std::string passphrase_;
  bool is_owner_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_WIFI_P2P_WIFI_P2P_GROUP_H_
