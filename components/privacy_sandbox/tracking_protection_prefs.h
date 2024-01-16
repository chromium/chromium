// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_PREFS_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_PREFS_H_

class PrefRegistrySimple;

namespace prefs {
// Tracking protection Onboarding Prefs

// Onboarding

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

// Unsynced pref that indicates when the onboarding notice was last shown.
inline constexpr char kTrackingProtectionNoticeLastShown[] =
    "tracking_protection.tracking_protection_notice_last_shown";

// Unsynced pref that indicates when the profile acknowledged the onboarding.
inline constexpr char kTrackingProtectionOnboardingAckedSince[] =
    "tracking_protection.tracking_protection_onboarding_acked_since";

// Unsynced Pref that indicates when the notice was first requested.
inline constexpr char kTrackingProtectionOnboardingNoticeFirstRequested[] =
    "tracking_protection.tracking_protection_onboarding_notice_first_requested";

// Unsynced Pref that indicates when the notice was last requested. This is only
// being tracked until the profile is fully Onboarded.
inline constexpr char kTrackingProtectionOnboardingNoticeLastRequested[] =
    "tracking_protection.tracking_protection_onboarding_notice_last_requested";

// Unsynced boolean that indicates whether or not the user has acknowledged the
// onboarding message. This is kept separate from the onboardingStatus
// intentionally.
inline constexpr char kTrackingProtectionOnboardingAcked[] =
    "tracking_protection.tracking_protection_onboarding_acked";

// Unsynced pref that indicates the action taken to acknowledge the Onboarding
// Notice.
inline constexpr char kTrackingProtectionOnboardingAckAction[] =
    "tracking_protection.tracking_protection_onboarding_ack_action";

// Offboarding

// Unsynced boolean that indicates whether or not the user has been offboarded
// (After being onboarded previously).
inline constexpr char kTrackingProtectionOffboarded[] =
    "tracking_protection.tracking_protection_offboarded";

// Unsynced pref that indicates when the profile was offboarded.
inline constexpr char kTrackingProtectionOffboardedSince[] =
    "tracking_protection.tracking_protection_offboarded_since";

// Unsynced pref that indicates the action taken to acknowledge the Offboarding
// Notice.
inline constexpr char kTrackingProtectionOffboardingAckAction[] =
    "tracking_protection.tracking_protection_offboarding_ack_action";

// Silent onboarding

// Unsynced pref that indicates what status the profile is at with regards to
// tracking protections (Silent Onboarding Notice for control groups).
inline constexpr char kTrackingProtectionSilentOnboardingStatus[] =
    "tracking_protection.tracking_protection_silent_onboarding_status";

// Unsynced pref that indicates when the profile has been marked eligible for
// silent onboarding tracking protection control groups.
inline constexpr char kTrackingProtectionSilentEligibleSince[] =
    "tracking_protection.tracking_protection_silent_eligible_since";

// Unsynced pref that indicates when the profile has been silently onboarded
// onto tracking protection control groups.
inline constexpr char kTrackingProtectionSilentOnboardedSince[] =
    "tracking_protection.tracking_protection_silent_onboarded_since";

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

// Sentiment Survey Prefs

// Indicates which group the profile belongs to.
inline constexpr char kTrackingProtectionSentimentSurveyGroup[] =
    "tracking_protection.tracking_protection_sentiment_survey_group";

// Indicates the time the profile becomes eligible for surveying.
inline constexpr char kTrackingProtectionSentimentSurveyStartTime[] =
    "tracking_protection.tracking_protection_sentiment_survey_start_time";

// Indicates the time the profile is no longer eligible for surveying.
inline constexpr char kTrackingProtectionSentimentSurveyEndTime[] =
    "tracking_protection.tracking_protection_sentiment_survey_end_time";

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
  kRequested = 3,
  kMaxValue = kRequested,
};

// Different tracking protection onboarding ack actions stored in the pref
// above.
enum class TrackingProtectionOnboardingAckAction {
  // No Ack Action set
  kNotSet = 0,
  // Ack recorded through some other way
  kOther = 1,
  // Acked using the GotIt button
  kGotIt = 2,
  // Acked using the Settings button
  kSettings = 3,
  // Acked using the learnmore button.
  kLearnMore = 4,
  // Acked by clicking the close button/ESC/Swipe away.
  kClosed = 5,
  kMaxValue = kClosed,
};

// Different tracking protection sentiment survey groups stored in the pref
// above.
enum class TrackingProtectionSentimentSurveyGroup {
  // No Group Set.
  kNotSet = 0,
  // Profile is to be surveyed shortly after being added to the Mode B Control
  // Group.
  kControlImmediate = 1,
  // Profile is to be surveyed shortly after being added to the Mode B Treatment
  // Group.
  kTreatmentImmediate = 2,
  // Profile is to be surveyed after a delay after being added to the Mode B
  // Control Group.
  kControlDelayed = 3,
  // Profile is to be surveyed after a delay after being added to the Mode B
  // Treatment Group.
  kTreatmentDelayed = 4,
  kMaxValue = kTreatmentDelayed,
};

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace privacy_sandbox::tracking_protection

#endif  // COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_PREFS_H_
