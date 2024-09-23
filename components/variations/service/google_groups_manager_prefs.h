// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_GOOGLE_GROUPS_MANAGER_PREFS_H_
#define COMPONENTS_VARIATIONS_SERVICE_GOOGLE_GROUPS_MANAGER_PREFS_H_

namespace variations {
// Per-profile preference for the sync data containing the list of dogfood group
// gaia IDs for a given syncing user.
// The variables below are the pref name, and the key for the gaia ID within
// the dictionary value.
#if BUILDFLAG(IS_CHROMEOS_ASH)
inline constexpr char kOsDogfoodGroupsSyncPrefName[] = "sync.os_dogfood_groups";
#else
inline constexpr char kDogfoodGroupsSyncPrefName[] = "sync.dogfood_groups";
#endif

inline constexpr char kDogfoodGroupsSyncPrefGaiaIdKey[] = "gaia_id";
}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_GOOGLE_GROUPS_MANAGER_PREFS_H_
