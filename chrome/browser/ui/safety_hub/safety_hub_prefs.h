// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_PREFS_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_PREFS_H_

#include "build/build_config.h"

class PrefRegistrySimple;

namespace safety_hub_prefs {

#if !BUILDFLAG(IS_ANDROID)
// Dictionary that determines the next time SafetyHub will trigger a background
// password check.
inline constexpr char kBackgroundPasswordCheckTimeAndInterval[] =
    "profile.background_password_check";

// Keys used inside the `kBackgroundPasswordCheckTimeAndInterval` pref dict.
inline constexpr char kNextPasswordCheckTimeKey[] = "next_check_time";
inline constexpr char kPasswordCheckIntervalKey[] = "check_interval";
inline constexpr char kPasswordCheckMonWeight[] = "check_mon_weight";
inline constexpr char kPasswordCheckTueWeight[] = "check_tue_weight";
inline constexpr char kPasswordCheckWedWeight[] = "check_wed_weight";
inline constexpr char kPasswordCheckThuWeight[] = "check_thu_weight";
inline constexpr char kPasswordCheckFriWeight[] = "check_fri_weight";
inline constexpr char kPasswordCheckSatWeight[] = "check_sat_weight";
inline constexpr char kPasswordCheckSunWeight[] = "check_sun_weight";
#endif  // !BUILDFLAG(IS_ANDROID)

// Dictionary that holds the notifications in the three-dot menu and their
// associated results.
inline const char kMenuNotificationsPrefsKey[] =
    "profile.safety_hub_menu_notifications";

// Boolean that specifies whether or not unused site permissions should be
// revoked by Safety Hub. It is used only when kSafetyHub flag is on.
// Conditioned because currently Safety Hub is available only on desktop and
// Android.
inline const char kUnusedSitePermissionsRevocationEnabled[] =
    "safety_hub.unused_site_permissions_revocation.enabled";

// Boolean that indicates whether the revoked permissions have successfully
// migrated to use string key values instead of integer key values.
inline const char kUnusedSitePermissionsRevocationMigrationCompleted[] =
    "safety_hub.unused_site_permissions_revocation.migration_completed";

}  // namespace safety_hub_prefs

void RegisterSafetyHubProfilePrefs(PrefRegistrySimple* registry);

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_PREFS_H_
