// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_PREF_NAMES_H_
#define CHROMEOS_CONSTANTS_PREF_NAMES_H_

namespace chromeos::prefs {

// This is the policy CaptivePortalAuthenticationIgnoresProxy that allows to
// open captive portal authentication pages in a separate window under
// a temporary incognito profile ("signin profile" is used for this purpose),
// which allows to bypass the user's proxy for captive portal authentication.
inline constexpr char kCaptivePortalAuthenticationIgnoresProxy[] =
    "proxy.captive_portal_ignores_proxy";

// A boolean pref that controls whether the prefs are associated with a captive
// portal signin window. Used to ignore proxies and allow extensions in an OTR
// profile when signing into a captive portal.
inline constexpr char kCaptivePortalSignin[] = "captive_portal_signin";

// A list of weekly time intervals when login to the device is blocked.
// Example content:
// [
//   {
//     "start": {
//         "day_of_week": "WEDNESDAY",
//         "milliseconds_since_midnight": 43200000
//     },
//     "end": {
//         "day_of_week": "WEDNESDAY",
//         "milliseconds_since_midnight": 75600000
//     }
//   },
//   {
//     "start": {
//         "day_of_week": "FRIDAY",
//         "milliseconds_since_midnight": 64800000
//     },
//     "end": {
//         "day_of_week": "MONDAY",
//         "milliseconds_since_midnight": 21600000
//     }
//   }
// ]
inline constexpr char kDeviceRestrictionSchedule[] =
    "device_restriction_schedule";

// A boolean pref that signals whether a post-logout notification should be
// shown on the login screen after entering the restriction schedule.
inline constexpr char kDeviceRestrictionScheduleShowPostLogoutNotification[] =
    "device_restriction_schedule_show_post_logout_notification";

// Boolean value for the FloatingWorkspaceV2Enabled policy
inline constexpr char kFloatingWorkspaceV2Enabled[] =
    "ash.floating_workspace_v2_enabled";

// Boolean pref specifying if the the Floating SSO Service is enabled. The
// service restores the user's web service authentication state by moving
// cookies from the previous device onto another, on ChromeOS.
inline constexpr char kFloatingSsoEnabled[] = "floating_sso_enabled";

// Boolean pref that determines whether signing in on a new ChromeOS device
// automatically signs the user out of their previous session. If the pref is
// set to false, the automatic sign-out functionality could still be enabled if
// either `kFloatingSsoEnabled` or `kFloatingWorkspaceV2Enabled` pref is enabled.
inline constexpr char kAutoSignOutEnabled[] = "auto_sign_out_enabled";

}  // namespace chromeos::prefs

#endif  // CHROMEOS_CONSTANTS_PREF_NAMES_H_
