// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PREF_NAMES_H_
#define CHROMEOS_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PREF_NAMES_H_

namespace proximity_auth {
namespace prefs {

extern const char kEasyUnlockEnabledStateSet[];
extern const char kEasyUnlockLocalStateUserPrefs[];
extern const char kProximityAuthLastPromotionCheckTimestampMs[];
extern const char kProximityAuthPromotionShownCount[];
extern const char kProximityAuthRemoteBleDevices[];
extern const char kProximityAuthIsChromeOSLoginEnabled[];
extern const char kProximityAuthHasShownLoginDisabledMessage[];

}  // namespace prefs
}  // namespace proximity_auth

#endif  // CHROMEOS_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PREF_NAMES_H_
