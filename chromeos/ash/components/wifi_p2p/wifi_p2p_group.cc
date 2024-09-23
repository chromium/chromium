// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/wifi_p2p/wifi_p2p_group.h"

namespace ash {

WifiP2PGroup::WifiP2PGroup(int shill_id,
                           uint32_t frequency,
                           int network_id,
                           const std::string& ipv4_address,
                           const std::string& ssid,
                           const std::string& passphrase,
                           bool is_owner)
    : shill_id_(shill_id),
      frequency_(frequency),
      network_id_(network_id),
      ipv4_address_(ipv4_address),
      ssid_(ssid),
      passphrase_(passphrase),
      is_owner_(is_owner) {}

WifiP2PGroup::WifiP2PGroup(const WifiP2PGroup&) = default;

WifiP2PGroup& WifiP2PGroup::operator=(const WifiP2PGroup&) = default;

WifiP2PGroup::~WifiP2PGroup() = default;

}  // namespace ash
