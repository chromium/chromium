// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_PROVIDER_H_
#define COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_PROVIDER_H_

#include <string_view>

#include "base/containers/flat_set.h"

namespace sync_preferences {

// This header defines the provider interface used by `CrossDevicePrefTracker`
// to determine which prefs to track. To add a new set of platform-specific
// prefs to be tracked, a new implementation of this interface should be
// created.
//
// Implementers of this interface are responsible for creating and registering
// their own cross-device, syncable dictionary prefs in the relevant places.
// These prefs must go through the same approval process as any other syncable
// pref. This service does not handle the creation or registration of these
// prefs.
class CrossDevicePrefProvider {
 public:
  virtual ~CrossDevicePrefProvider() = default;

  // Returns a set of profile-based prefs to be tracked. These should be the
  // prefs that are tracked by the cross-device pref tracker, not the ones the
  // cross-device pref tracker updates (prefixed with `cross_device.`).
  virtual const base::flat_set<std::string_view>& GetProfilePrefs() const = 0;

  // Returns a set of local-state prefs to be tracked. These should be the prefs
  // that are tracked by the cross-device pref tracker, not the ones the
  // cross-device pref tracker updates (prefixed with `cross_device.`).
  virtual const base::flat_set<std::string_view>& GetLocalStatePrefs()
      const = 0;

 protected:
  CrossDevicePrefProvider() = default;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_PROVIDER_H_
