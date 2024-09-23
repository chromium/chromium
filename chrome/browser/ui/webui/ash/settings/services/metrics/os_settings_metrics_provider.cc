// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/services/metrics/os_settings_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/settings/services/metrics/settings_user_action_tracker.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/metrics/metrics_service.h"

namespace ash::settings {

namespace {
// Used to check whether it has been one week since the user has finished OOBE
// onboarding.
constexpr base::TimeDelta kOneWeek = base::Days(7);

constexpr char kOsSettingsVerifiedAccessEnabledHistogramName[] =
    "ChromeOS.Settings.Privacy.VerifiedAccessEnabled";

constexpr char kOsSettingsLifetimeNumUniqueSettingsChangedFirstWeek[] =
    "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek";
constexpr char kOsSettingsLifetimeNumUniqueSettingsChangedSubsequentWeeks[] =
    "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2."
    "SubsequentWeeks";
constexpr char kOsSettingsLifetimeNumUniqueSettingsChangedTotal[] =
    "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.Total";

}  // namespace

void OsSettingsMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  // Log verified access enabled/disabled value for this session
  LogVerifiedAccessEnabled();

  // Log total unique Settings changed over the device's lifetime if allowed.
  MaybeLogTotalUniqueSettingsChanged();
}

void OsSettingsMetricsProvider::LogVerifiedAccessEnabled() {
  bool verified_access_enabled;
  ash::CrosSettings::Get()->GetBoolean(
      ash::kAttestationForContentProtectionEnabled, &verified_access_enabled);
  base::UmaHistogramBoolean(kOsSettingsVerifiedAccessEnabledHistogramName,
                            verified_access_enabled);
}

void OsSettingsMetricsProvider::MaybeLogTotalUniqueSettingsChanged() {
  // Log total unique Settings changed over the device's lifetime if allowed.
  PrefService* profile_pref_service =
      ProfileManager::GetActiveUserProfile()->GetPrefs();
  DCHECK(profile_pref_service);

  // We do not have consent to record the metrics.
  if (!ShouldRecordMetrics(profile_pref_service)) {
    return;
  }

  int total_unique_settings_changed_count =
      profile_pref_service->GetDict(::prefs::kTotalUniqueOsSettingsChanged)
          .size();

  // prefs::kOobeOnboardingTime does not exist for users in guest mode.
  if (profile_pref_service->HasPrefPath(prefs::kOobeOnboardingTime)) {
    // We want to record the data in UMA even if no new unique Settings has
    // changed because UMA clears the data after a certain timeframe, and we
    // want to be able to still represent the user that have not used Settings
    // frequently in the histogram.
    if (IsTodayInFirst7Days(profile_pref_service)) {
      base::UmaHistogramCounts1000(
          kOsSettingsLifetimeNumUniqueSettingsChangedFirstWeek,
          total_unique_settings_changed_count);
    } else {
      base::UmaHistogramCounts1000(
          kOsSettingsLifetimeNumUniqueSettingsChangedSubsequentWeeks,
          total_unique_settings_changed_count);
    }
    // Store the total unique Settings changed in .DeviceLifetime2.Total
    // histogram.
    base::UmaHistogramCounts1000(
        kOsSettingsLifetimeNumUniqueSettingsChangedTotal,
        total_unique_settings_changed_count);
  }
}

bool OsSettingsMetricsProvider::ShouldRecordMetrics(
    PrefService* profile_pref_service) {
  if (profile_pref_service->GetBoolean(
          ::prefs::kHasEverRevokedMetricsConsent)) {
    // If the pref has been turned off at least once in the user's lifetime,
    // clear the pref kTotalUniqueOsSettingsChanged.
    profile_pref_service->ClearPref(::prefs::kTotalUniqueOsSettingsChanged);

    // We do not have consent to record the user's metrics.
    return false;
  }
  return true;
}

bool OsSettingsMetricsProvider::IsTodayInFirst7Days(
    PrefService* profile_pref_service) {
  // The pref kOobeOnboardingTime does not get set for users in guest mode.
  // Because we are accessing the value of the pref, we must ensure that it does
  // exist.
  DCHECK(profile_pref_service->HasPrefPath(prefs::kOobeOnboardingTime));
  return base::Time::Now() -
             profile_pref_service->GetTime(::ash::prefs::kOobeOnboardingTime) <=
         kOneWeek;
}

}  // namespace ash::settings
