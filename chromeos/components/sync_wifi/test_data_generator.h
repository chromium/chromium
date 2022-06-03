// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_TEST_DATA_GENERATOR_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_TEST_DATA_GENERATOR_H_

#include "components/sync/protocol/wifi_configuration_specifics.pb.h"

namespace chromeos {

namespace sync_wifi {

class NetworkIdentifier;

//  Creates a NetworkIdentifier with PSK security for the given |ssid|.
NetworkIdentifier GeneratePskNetworkId(const std::string& ssid);

// Creates a proto with default values and sets the hex_ssid and security_type
// based on the input |id|.
sync_pb::WifiConfigurationSpecifics GenerateTestWifiSpecifics(
    const NetworkIdentifier& id,
    const std::string& passphrase = "passphrase",
    double timestamp = 1);

}  // namespace sync_wifi

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_TEST_DATA_GENERATOR_H_
