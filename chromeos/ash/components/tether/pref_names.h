// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_PREF_NAMES_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_PREF_NAMES_H_

namespace ash {

namespace tether {

namespace prefs {

// Preference name for the preference which stores IDs corresponding to devices
// which have most recently replied to a TetherAvailabilityRequest with a
// response code indicating that tethering is available. The value stored is a
// ListValue, with the most recent response residing at the start of the list.
extern const char kMostRecentTetherAvailablilityResponderIds[];

// Preference name for the preference which stores IDs corresponding to devices
// which have most recently replied to a ConnectTetheringResponse with a
// response code indicating that tethering is available. The value stored is a
// ListValue, with the most recent response residing at the start of the list.
extern const char kMostRecentConnectTetheringResponderIds[];

// The status of the active host. The value stored for this key is the integer
// version of an ActiveHost::ActiveHostStatus enumeration value.
extern const char kActiveHostStatus[];

// The device ID of the active host. If there is no active host, the value at
// this key is "".
extern const char kActiveHostDeviceId[];

// The tether network GUID of the active host. If there is no active host, the
// value at this key is "".
extern const char kTetherNetworkGuid[];

// The Wi-Fi network GUID of the active host. If there is no active host, the
// value at this key is "".
extern const char kWifiNetworkGuid[];

// The Wi-Fi network path that is currently being disconnected. When
// disconnecting under normal circumstances, this value is set when a
// disconnection is initiated and is cleared when a disconnection completes.
// However, when a disconnection is triggered by the user logging out, the
// disconnection flow cannot complete before Chrome shuts down (due to the
// asynchronous nature of the network stack), so this path remains in prefs.
// When the Tether component starts up again (the next time the user logs in),
// this path is fetched, the associated network configuration is removed, and
// the path is cleared from prefs.
extern const char kDisconnectingWifiNetworkPath[];

// Scanned Tether host results. The value stored is a ListValue containing
// DictionaryValues containing the scan results. See PersistentHostScanCache for
// more details.
extern const char kHostScanCache[];

}  // namespace prefs

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_PREF_NAMES_H_
