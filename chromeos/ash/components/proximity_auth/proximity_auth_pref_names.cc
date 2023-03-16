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

// Whether or not EasyUnlock is enabled on the ChromeOS login screen (in
// addition to the lock screen).
// TODO(b/227674947): Delete this pref now that Sign in with Smart Lock is
// deprecated.
const char kProximityAuthIsChromeOSLoginEnabled[] =
    "proximity_auth.is_chromeos_login_enabled";

// The dictionary containing remote BLE devices.
// TODO(b/227674947): Delete this pref now that Sign in with Smart Lock is
// deprecated.
const char kProximityAuthHasShownLoginDisabledMessage[] =
    "proximity_auth.has_shown_login_disabled_message";

}  // namespace prefs
}  // namespace proximity_auth
