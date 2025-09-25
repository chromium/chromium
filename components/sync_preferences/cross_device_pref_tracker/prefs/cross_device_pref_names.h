// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_PREFS_CROSS_DEVICE_PREF_NAMES_H_
#define COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_PREFS_CROSS_DEVICE_PREF_NAMES_H_

namespace prefs {

// Alphabetical list of preferences used by the cross device pref tracker.
// Keep alphabetized, and document each.

// Dictionary that stores the value of the omnibox position (bottom/top) across
// a user's syncing devices.
inline constexpr char kCrossDeviceOmniboxIsInBottomPosition[] =
    "cross_device.omnibox.is_in_bottom_position";

}  // namespace prefs

#endif  // COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_PREFS_CROSS_DEVICE_PREF_NAMES_H_
