// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/proximity_auth_pref_names.h"

namespace proximity_auth {
namespace prefs {

// Whether or not the state of EasyUnlock has actively been changed, e.g.,
// explicitly enabled by the user (through setup) or disabled via Settings.
const char kEasyUnlockEnabledStateSet[] = "easy_unlock.enabled_state_set";

// The timestamp of the last promotion check in milliseconds.
const char kProximityAuthLastPromotionCheckTimestampMs[] =
    "proximity_auth.last_promotion_check_timestamp_ms";

// The number of times the promotion was shown to the user.
const char kProximityAuthPromotionShownCount[] =
    "proximity_auth.promotion_shown_count";

// The dictionary containing remote BLE devices.
const char kProximityAuthRemoteBleDevices[] =
    "proximity_auth.remote_ble_devices";

}  // namespace prefs
}  // namespace proximity_auth
