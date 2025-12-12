// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_PREFS_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_PREFS_H_

class PrefRegistrySimple;

namespace prefs {

// Onboarding prefs

// Unsynced pref that indicates what status the profile is at with regards to
// tracking protections (3PCD Onboarding Notice).
inline constexpr char kTrackingProtectionOnboardingStatus[] =
    "tracking_protection.tracking_protection_onboarding_status";

// Tracking Protection prefs

// Unsynced boolean that indicates whether 3PCD tracking protection (prefs + UI)
// are enabled on the current device.
inline constexpr char kTrackingProtection3pcdEnabled[] =
    "tracking_protection.tracking_protection_3pcd_enabled";

// Synced boolean that indicates whether the "block all 3pc" toggle on the
// tracking protection page is enabled.
inline constexpr char kBlockAll3pcToggleEnabled[] =
    "tracking_protection.block_all_3pc_toggle_enabled";

}  // namespace prefs

namespace privacy_sandbox::tracking_protection {

// Different tracking protection onboarding statuses stored in the pref above.
enum class TrackingProtectionOnboardingStatus {
  kIneligible = 0,
  kEligible = 1,
  kOnboarded = 2,
  kRequested = 3,
  kMaxValue = kRequested,
};

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace privacy_sandbox::tracking_protection

#endif  // COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_PREFS_H_
