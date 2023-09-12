// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_PREFS_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_PREFS_H_

class PrefRegistrySimple;

namespace prefs {
// Tracking protection Onboarding Prefs

// Unsynced pref that indicates what status the profile is at with regards to
// tracking protections (3PCD Onboarding Notice).
inline constexpr char kTrackingProtectionOnboardingStatus[] =
    "tracking_protection.tracking_protection_onboarding_status";

// Unsynced pref that indicates when the profile has been marked eligible for
// tracking protection.
inline constexpr char kTrackingProtectionEligibleSince[] =
    "tracking_protection.tracking_protection_eligible_since";

// Unsynced pref that indicates when the profile has been onboarded onto
// tracking protection.
inline constexpr char kTrackingProtectionOnboardedSince[] =
    "tracking_protection.tracking_protection_onboarded_since";

// Unsynced boolean that indicates whether or not the user has acknowledged the
// onboarding message. This is kept separate from the onboardingStatus
// intentionally.
inline constexpr char kTrackingProtectionOnboardingAcked[] =
    "tracking_protection.tracking_protection_onboarding_acked";

// Tracking Protection Settings Prefs.

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

// Whether to send the DNT header.
inline constexpr char kEnableDoNotTrack[] = "enable_do_not_track";

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
