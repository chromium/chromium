// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_PREFS_CROSS_DEVICE_PREF_REGISTRY_H_
#define COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_PREFS_CROSS_DEVICE_PREF_REGISTRY_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace cross_device {

// Registers the cross-device preferences. All cross-device prefs must be
// Profile prefs, so there is no corresponding method for registering Local
// State prefs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace cross_device

#endif  // COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_PREFS_CROSS_DEVICE_PREF_REGISTRY_H_
