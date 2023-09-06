// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_PREFS_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_PREFS_H_

class PrefRegistrySimple;

namespace prefs {

// Synced boolean that indicates whether the "block all 3pc" toggle on the
// tracking protection page is enabled.
inline constexpr char kBlockAll3pcToggleEnabled[] =
    "tracking_protection.block_all_3pc_toggle_enabled";

// Synced enum that indicates the level of tracking protection the user has
// selected on the tracking protection page.
inline constexpr char kTrackingProtectionLevel[] =
    "tracking_protection.tracking_protection_level";

// Unsynced boolean that indicates whether 3PCD tracking protection (prefs + UI)
// are enabled on the current device.
inline constexpr char kTrackingProtection3pcdEnabled[] =
    "tracking_protection.tracking_protection_3pcd_enabled";

// Unsynced boolean that indicates what status the profile is at with regards to
// tracking protections (3PCD Onboarding Notice).
inline constexpr char kTrackingProtectionOnboardingStatus[] =
    "tracking_protection.tracking_protection_onboarding_status";

}  // namespace prefs

namespace privacy_sandbox::tracking_protection {

// Different levels of tracking protection available to the user.
// Values are persisted, don't renumber or reuse.
enum class TrackingProtectionLevel {
  kStandard = 0,
  kCustom = 1,
  kMaxValue = kCustom,
};

// Different tracking protection onboarding statuses stored in the pref above.
enum class TrackingProtectionOnboardingStatus {
  kIneligible = 0,
  kEligible = 1,
  kOnboarded = 2,
  kMaxValue = kOnboarded,
};

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace privacy_sandbox::tracking_protection

#endif  // COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_PREFS_H_
