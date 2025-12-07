// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_SYNCED_SET_UP_UTILS_H_
#define COMPONENTS_SYNC_PREFERENCES_SYNCED_SET_UP_UTILS_H_

#include <map>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "components/commerce/core/pref_names.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/safety_check/safety_check_pref_names.h"
#include "components/sync_preferences/cross_device_pref_tracker/prefs/cross_device_pref_names.h"

namespace base {
class Value;
}  // namespace base

namespace syncer {
class DeviceInfo;
class DeviceInfoTracker;
}  // namespace syncer

namespace sync_preferences {

class CrossDevicePrefTracker;

namespace synced_set_up {

// Map of cross-device synced prefs considered by Synced Set Up mapped to their
// corresponding tracked local-state pref.
inline constexpr auto kCrossDeviceToLocalStatePrefMap =
    base::MakeFixedFlatMap<std::string_view, std::string_view>({
        // keep-sorted start
        {prefs::kCrossDeviceOmniboxIsInBottomPosition,
         omnibox::kIsOmniboxInBottomPosition},
        // keep-sorted end
    });

// Map of cross-device synced prefs considered by Synced Set Up mapped to their
// corresponding tracked profile pref.
inline constexpr auto kCrossDeviceToProfilePrefMap =
    base::MakeFixedFlatMap<std::string_view, std::string_view>({
        // keep-sorted start
        {prefs::kCrossDeviceMagicStackHomeModuleEnabled,
         ntp_tiles::prefs::kMagicStackHomeModuleEnabled},
        {prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
         ntp_tiles::prefs::kMostVisitedHomeModuleEnabled},
        {prefs::kCrossDevicePriceTrackingHomeModuleEnabled,
         commerce::kPriceTrackingHomeModuleEnabled},
        {prefs::kCrossDeviceSafetyCheckHomeModuleEnabled,
         safety_check::prefs::kSafetyCheckHomeModuleEnabled},
        {prefs::kCrossDeviceTabResumptionHomeModuleEnabled,
         ntp_tiles::prefs::kTabResumptionHomeModuleEnabled},
        {prefs::kCrossDeviceTipsHomeModuleEnabled,
         ntp_tiles::prefs::kTipsHomeModuleEnabled},
        // keep-sorted end
    });

// Returns a map of tracked pref names and values corresponding to the "best
// match" prefs for the Synced Set Up flow to apply.
std::map<std::string_view, base::Value> GetCrossDevicePrefsFromRemoteDevice(
    const sync_preferences::CrossDevicePrefTracker* pref_tracker,
    const syncer::DeviceInfoTracker* device_info_tracker,
    const syncer::DeviceInfo* local_device);

}  // namespace synced_set_up

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_SYNCED_SET_UP_UTILS_H_
