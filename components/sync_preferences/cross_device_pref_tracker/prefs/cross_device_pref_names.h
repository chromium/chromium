// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_PREFS_CROSS_DEVICE_PREF_NAMES_H_
#define COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_PREFS_CROSS_DEVICE_PREF_NAMES_H_

namespace prefs {

// Alphabetical list of preferences used by the cross device pref tracker.
// Keep alphabetized, and document each.

// go/keep-sorted start newline_separated=yes skip_lines=1

// Dictionary that stores the value of the 16th active day for cross-platform
// promos on iOS across a user's syncing devices.
inline constexpr char kCrossDeviceCrossPlatformPromosIOS16thActiveDay[] =
    "cross_device.cross_platform_promos.ios_16th_active_day";

// Dictionary that stores if the Magic Stack Home Module is enabled across a
// user's syncing devices.
inline constexpr char kCrossDeviceMagicStackHomeModuleEnabled[] =
    "cross_device.home.module.magic_stack.enabled";

// Dictionary that stores if the Most Visited Tiles Home Module is enabled.
inline constexpr char kCrossDeviceMostVisitedHomeModuleEnabled[] =
    "cross_device.home.module.most_visited.enabled";

// Dictionary that stores the value of the omnibox position (bottom/top) across
// a user's syncing devices.
inline constexpr char kCrossDeviceOmniboxIsInBottomPosition[] =
    "cross_device.omnibox.is_in_bottom_position";

// Dictionary that stores if the Price Tracking Home Module is enabled across a
// user's syncing devices.
inline constexpr char kCrossDevicePriceTrackingHomeModuleEnabled[] =
    "cross_device.home.module.price_tracking.enabled";

// Dictionary that stores if the Safety Check Home Module is enabled across a
// user's syncing devices.
inline constexpr char kCrossDeviceSafetyCheckHomeModuleEnabled[] =
    "cross_device.home.module.safety_check.enabled";

// Dictionary that stores if the Tab Resumption Home Module is enabled across a
// user's syncing devices.
inline constexpr char kCrossDeviceTabResumptionHomeModuleEnabled[] =
    "cross_device.home.module.tab_resumption.enabled";

// Dictionary that stores if the Tips Home Module is enabled.
inline constexpr char kCrossDeviceTipsHomeModuleEnabled[] =
    "cross_device.home.module.tips.enabled";

// go/keep-sorted end

}  // namespace prefs

#endif  // COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_PREFS_CROSS_DEVICE_PREF_NAMES_H_
