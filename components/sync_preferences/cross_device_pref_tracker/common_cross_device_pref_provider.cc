// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/cross_device_pref_tracker/common_cross_device_pref_provider.h"

#include "base/no_destructor.h"
#include "components/commerce/core/pref_names.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/safety_check/safety_check_pref_names.h"

namespace sync_preferences {

CommonCrossDevicePrefProvider::CommonCrossDevicePrefProvider() = default;
CommonCrossDevicePrefProvider::~CommonCrossDevicePrefProvider() = default;

const base::flat_set<std::string_view>&
CommonCrossDevicePrefProvider::GetProfilePrefs() const {
  static const base::NoDestructor<base::flat_set<std::string_view>>
      kProfilePrefs({
          // go/keep-sorted start
          commerce::kPriceTrackingHomeModuleEnabled,
          ntp_tiles::prefs::kMagicStackHomeModuleEnabled,
          ntp_tiles::prefs::kMostVisitedHomeModuleEnabled,
          ntp_tiles::prefs::kTabResumptionHomeModuleEnabled,
          ntp_tiles::prefs::kTipsHomeModuleEnabled,
          safety_check::prefs::kSafetyCheckHomeModuleEnabled,
          // go/keep-sorted end
      });
  return *kProfilePrefs;
}

// These prefs should be the tracked prefs, not the ones prefixed with
// `cross_device.`
const base::flat_set<std::string_view>&
CommonCrossDevicePrefProvider::GetLocalStatePrefs() const {
  static const base::NoDestructor<base::flat_set<std::string_view>>
      kLocalStatePrefs({omnibox::kIsOmniboxInBottomPosition});
  return *kLocalStatePrefs;
}

}  // namespace sync_preferences
