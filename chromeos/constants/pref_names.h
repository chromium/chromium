// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_PREF_NAMES_H_
#define CHROMEOS_CONSTANTS_PREF_NAMES_H_

namespace chromeos::prefs {

// A boolean pref that controls whether the prefs are associated with a captive
// portal signin window. Used to ignore proxies and allow extensions in an OTR
// profile when signing into a captive portal.
inline constexpr char kCaptivePortalSignin[] = "captive_portal_signin";

}  // namespace chromeos::prefs

#endif  // CHROMEOS_CONSTANTS_PREF_NAMES_H_
