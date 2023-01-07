// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/pref_names.h"

namespace ash {

namespace tether {

namespace prefs {

const char kMostRecentTetherAvailablilityResponderIds[] =
    "tether.most_recent_tether_availability_responder_ids";

const char kMostRecentConnectTetheringResponderIds[] =
    "tether.most_recent_connect_tethering_responder_ids";

const char kActiveHostStatus[] = "tether.active_host_status";

const char kActiveHostDeviceId[] = "tether.active_host_device_id";

const char kTetherNetworkGuid[] = "tether.tether_network_id";

const char kWifiNetworkGuid[] = "tether.wifi_network_id";

const char kDisconnectingWifiNetworkPath[] =
    "tether.disconnecting_wifi_network_path";

const char kHostScanCache[] = "tether.host_scan_cache";

}  // namespace prefs

}  // namespace tether

}  // namespace ash
