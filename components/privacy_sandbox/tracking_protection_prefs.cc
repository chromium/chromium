// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_prefs.h"

#include "base/time/time.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace privacy_sandbox::tracking_protection {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // Tracking protection Onboarding Prefs
  registry->RegisterIntegerPref(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));

  registry->RegisterTimePref(prefs::kTrackingProtectionEligibleSince,
                             base::Time());

  registry->RegisterTimePref(prefs::kTrackingProtectionOnboardedSince,
                             base::Time());

  registry->RegisterTimePref(prefs::kTrackingProtectionNoticeLastShown,
                             base::Time());

  registry->RegisterTimePref(prefs::kTrackingProtectionOnboardingAckedSince,
                             base::Time());

  registry->RegisterBooleanPref(prefs::kTrackingProtectionOnboardingAcked,
                                false);

  registry->RegisterIntegerPref(
      prefs::kTrackingProtectionOnboardingAckAction,
      static_cast<int>(TrackingProtectionOnboardingAckAction::kNotSet));

  // Silent onboarding
  registry->RegisterIntegerPref(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));

  registry->RegisterTimePref(prefs::kTrackingProtectionSilentEligibleSince,
                             base::Time());

  registry->RegisterTimePref(prefs::kTrackingProtectionSilentOnboardedSince,
                             base::Time());

  // Full 3PCD prefs.
  registry->RegisterTimePref(prefs::kTrackingProtectionFull3PCDOnboarded,
                             base::Time());

  // Tracking Protection Reminder Prefs
  registry->RegisterIntegerPref(
      prefs::kTrackingProtectionReminderStatus,
      static_cast<int>(TrackingProtectionReminderStatus::kUnset));

  // Tracking Protection Survey Prefs
  registry->RegisterTimePref(prefs::kTrackingProtectionSurveyWindowStartTime,
                             base::Time());

  // Tracking Protection Settings Prefs
  registry->RegisterBooleanPref(
      prefs::kBlockAll3pcToggleEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kAllowAll3pcToggleEnabled, false);
  registry->RegisterIntegerPref(
      prefs::kTrackingProtectionLevel,
      static_cast<int>(TrackingProtectionLevel::kStandard),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kTrackingProtection3pcdEnabled, false);
  registry->RegisterBooleanPref(
      prefs::kIpProtectionEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kIpProtectionInitializedByDogfood,
                                false);
  registry->RegisterBooleanPref(
      prefs::kFingerprintingProtectionEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kEnableDoNotTrack, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kUserBypass3pcExceptionsMigrated, false);
}

}  // namespace privacy_sandbox::tracking_protection
