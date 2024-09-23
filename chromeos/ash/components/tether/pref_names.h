// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_PREF_NAMES_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_PREF_NAMES_H_

namespace ash::tether::prefs {

// Preference name for the preference which stores IDs corresponding to devices
// which have most recently replied to a TetherAvailabilityRequest with a
// response code indicating that tethering is available. The value stored is a
// ListValue, with the most recent response residing at the start of the list.
inline constexpr char kMostRecentTetherAvailablilityResponderIds[] =
    "tether.most_recent_tether_availability_responder_ids";

// Preference name for the preference which stores IDs corresponding to devices
// which have most recently replied to a ConnectTetheringResponse with a
// response code indicating that tethering is available. The value stored is a
// ListValue, with the most recent response residing at the start of the list.
inline constexpr char kMostRecentConnectTetheringResponderIds[] =
    "tether.most_recent_connect_tethering_responder_ids";

// The status of the active host. The value stored for this key is the integer
// version of an ActiveHost::ActiveHostStatus enumeration value.
inline constexpr char kActiveHostStatus[] = "tether.active_host_status";

// The device ID of the active host. If there is no active host, the value at
// this key is "".
inline constexpr char kActiveHostDeviceId[] = "tether.active_host_device_id";

// The tether network GUID of the active host. If there is no active host, the
// value at this key is "".
inline constexpr char kTetherNetworkGuid[] = "tether.tether_network_id";

// The Wi-Fi network GUID of the active host. If there is no active host, the
// value at this key is "".
inline constexpr char kWifiNetworkGuid[] = "tether.wifi_network_id";

// The Wi-Fi network path that is currently being disconnected. When
// disconnecting under normal circumstances, this value is set when a
// disconnection is initiated and is cleared when a disconnection completes.
// However, when a disconnection is triggered by the user logging out, the
// disconnection flow cannot complete before Chrome shuts down (due to the
// asynchronous nature of the network stack), so this path remains in prefs.
// When the Tether component starts up again (the next time the user logs in),
// this path is fetched, the associated network configuration is removed, and
// the path is cleared from prefs.
inline constexpr char kDisconnectingWifiNetworkPath[] =
    "tether.disconnecting_wifi_network_path";

// Scanned Tether host results. The value stored is a ListValue containing
// DictionaryValues containing the scan results. See PersistentHostScanCache for
// more details.
inline constexpr char kHostScanCache[] = "tether.host_scan_cache";

// Whether the user has chosen to allow Instant Hotspot to present
// notifications. Enabled by default.
inline constexpr char kNotificationsEnabled[] = "tether.notifications_enabled";

}  // namespace ash::tether::prefs

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_PREF_NAMES_H_
