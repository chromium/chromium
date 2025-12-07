// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_COMMON_CROSS_DEVICE_PREF_PROVIDER_H_
#define COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_COMMON_CROSS_DEVICE_PREF_PROVIDER_H_

#include <string_view>

#include "base/containers/flat_set.h"
#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_provider.h"

namespace sync_preferences {

// A `CrossDevicePrefProvider` for prefs that are common across all platforms.
class CommonCrossDevicePrefProvider : public CrossDevicePrefProvider {
 public:
  CommonCrossDevicePrefProvider();
  ~CommonCrossDevicePrefProvider() override;

  // `CrossDevicePrefProvider` overrides:
  const base::flat_set<std::string_view>& GetProfilePrefs() const override;
  const base::flat_set<std::string_view>& GetLocalStatePrefs() const override;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_COMMON_CROSS_DEVICE_PREF_PROVIDER_H_
