// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/services/metrics/per_session_settings_user_action_tracker.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash::settings {

namespace {

// The maximum amount of time that the settings window can be blurred to be
// considered short enough for the "first change" metric.
constexpr base::TimeDelta kShortBlurTimeLimit = base::Minutes(1);

// The minimum amount of time between a setting change and a subsequent setting
// change. If two changes occur les than this amount of time from each other,
// they are ignored by metrics. See https://crbug.com/1073714 for details.
constexpr base::TimeDelta kMinSubsequentChange = base::Milliseconds(200);

// Min/max values for the duration metrics. Note that these values are tied to
// the metrics defined below; if these ever change, the metric names must also
// be updated.
constexpr base::TimeDelta kMinDurationMetric = base::Milliseconds(100);
constexpr base::TimeDelta kMaxDurationMetric = base::Minutes(10);

// Used to check whether it has been one week since the user has finished OOBE
// onboarding.
constexpr base::TimeDelta kOneWeek = base::Days(7);

void LogDurationMetric(const char* metric_name, base::TimeDelta duration) {
  base::UmaHistogramCustomTimes(metric_name, duration, kMinDurationMetric,
                                kMaxDurationMetric, /*buckets=*/50);
}

}  // namespace

PerSessionSettingsUserActionTracker::PerSessionSettingsUserActionTracker(
    PrefService* profile_pref_service)
    : metric_start_time_(base::TimeTicks::Now()),
      window_last_active_timestamp_(base::TimeTicks::Now()),
      profile_pref_service_(profile_pref_service) {
  DCHECK(profile_pref_service_);
}

PerSessionSettingsUserActionTracker::~PerSessionSettingsUserActionTracker() {
  RecordPageActiveTime();
  LogDurationMetric("ChromeOS.Settings.WindowTotalActiveDuration",
                    total_time_session_active_);

  base::UmaHistogramCounts1000(
      "ChromeOS.Settings.NumUniqueSettingsChanged.PerSession",
      changed_settings_.size());
}

bool PerSessionSettingsUserActionTracker::HasUserMetricsConsent() {
  DCHECK(g_browser_process);

  metrics::MetricsService* metrics_service_ =
      g_browser_process->metrics_service();

  // Metrics consent can be either per-device, ie. the profile that is the owner
  // of the device, or per-user. If no per-user consent is provided, we will use
  // the device settings.
  if (metrics_service_->GetCurrentUserMetricsConsent().has_value()) {
    return metrics_service_->GetCurrentUserMetricsConsent().value();
  }

  // Return the device setting.
  return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
}

void PerSessionSettingsUserActionTracker::
    SetHasUserEverRevokedMetricsConsent() {
  bool metric_consent_pref_value_ = HasUserMetricsConsent();

  // We will not keep a record of the user's total number of unique Settings
  // changed if they have turned off UMA. We will not transmit the old data when
  // the user revokes their consent for UMA. The pref gets cleared and will
  // never record the Device Lifetime metric again.
  if (profile_pref_service_
          ->FindPreference(::prefs::kHasEverRevokedMetricsConsent)
          ->IsDefaultValue()) {
    profile_pref_service_->SetBoolean(::prefs::kHasEverRevokedMetricsConsent,
                                      !metric_consent_pref_value_);
  } else if (!metric_consent_pref_value_) {
    profile_pref_service_->SetBoolean(::prefs::kHasEverRevokedMetricsConsent,
                                      true);
  }
}

void PerSessionSettingsUserActionTracker::RecordLifetimeMetricToPref() {
  SetHasUserEverRevokedMetricsConsent();

  // The pref kHasResetFirst7DaysSettingsUsedCount indicates whether the pref
  // kTotalUniqueOsSettingsChanged has been cleared once after 1 week has passed
  // since OOBE. If the pref kHasResetFirst7DaysSettingsUsedCount is False and
  // it has been over 7 days since the user has taken OOBE, it means that this
  // is the first time since one week after OOBE that the user has opened and
  // changed Settings. In this case, clear the pref
  // kTotalUniqueOsSettingsChanged to prepare it for the
  // ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.SubsequentWeeks
  // histogram.
  // NOTE: prefs::kOobeOnboardingTime does not exist for users in guest mode.
  if (!profile_pref_service_->GetBoolean(
          ::prefs::kHasResetFirst7DaysSettingsUsedCount) &&
      profile_pref_service_->HasPrefPath(prefs::kOobeOnboardingTime) &&
      !IsTodayInFirst7Days()) {
    profile_pref_service_->SetBoolean(
        ::prefs::kHasResetFirst7DaysSettingsUsedCount, true);
    ClearTotalUniqueSettingsChangedPref();
  }

  UpdateSettingsPrefTotalUniqueChanged();
}

bool PerSessionSettingsUserActionTracker::IsTodayInFirst7Days() {
  // The pref kOobeOnboardingTime does not get set for users in guest mode.
  // Because we are accessing the value of the pref, we must ensure that it does
  // exist.
  DCHECK(profile_pref_service_->HasPrefPath(prefs::kOobeOnboardingTime));
  return base::Time::Now() - profile_pref_service_->GetTime(
                                 ::ash::prefs::kOobeOnboardingTime) <=
         kOneWeek;
}

void PerSessionSettingsUserActionTracker::
    ClearTotalUniqueSettingsChangedPref() {
  profile_pref_service_->ClearPref(::prefs::kTotalUniqueOsSettingsChanged);
}

void PerSessionSettingsUserActionTracker::RecordPageFocus() {
  const base::TimeTicks now = base::TimeTicks::Now();
  window_last_active_timestamp_ = now;

  if (last_blur_timestamp_.is_null()) {
    return;
  }

  // Log the duration of being blurred.
  const base::TimeDelta blurred_duration = now - last_blur_timestamp_;
  LogDurationMetric("ChromeOS.Settings.BlurredWindowDuration",
                    blurred_duration);

  // If the window was blurred for more than |kShortBlurTimeLimit|,
  // the user was away from the window for long enough that we consider the
  // user coming back to the window a new session for the purpose of metrics.
  if (blurred_duration >= kShortBlurTimeLimit) {
    ResetMetricsCountersAndTimestamp();
    last_record_setting_changed_timestamp_ = base::TimeTicks();
  }
}

void PerSessionSettingsUserActionTracker::RecordPageActiveTime() {
  if (window_last_active_timestamp_ != base::TimeTicks()) {
    total_time_session_active_ +=
        base::TimeTicks::Now() - window_last_active_timestamp_;
  }
  window_last_active_timestamp_ = base::TimeTicks();
}

void PerSessionSettingsUserActionTracker::RecordPageBlur() {
  last_blur_timestamp_ = base::TimeTicks::Now();
  RecordPageActiveTime();
}

void PerSessionSettingsUserActionTracker::RecordClick() {
  ++num_clicks_since_start_time_;
}

void PerSessionSettingsUserActionTracker::RecordNavigation() {
  ++num_navigations_since_start_time_;
}

void PerSessionSettingsUserActionTracker::RecordSearch() {
  ++num_searches_since_start_time_;
}

void PerSessionSettingsUserActionTracker::RecordSettingChange(
    std::optional<chromeos::settings::mojom::Setting> setting) {
  if (setting.has_value()) {
    changed_settings_.insert(
        base::NumberToString(static_cast<int>(setting.value())));

    // Record the total unique Settings changed to the histogram
    // ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.{Time}.
    RecordLifetimeMetricToPref();
  }
  base::TimeTicks now = base::TimeTicks::Now();

  if (!last_record_setting_changed_timestamp_.is_null()) {
    // If it has been less than |kMinSubsequentChange| since the last recorded
    // setting change, this change is discarded. See https://crbug.com/1073714
    // for details.
    if (now - last_record_setting_changed_timestamp_ < kMinSubsequentChange) {
      return;
    }

    base::UmaHistogramCounts1000(
        "ChromeOS.Settings.NumClicksUntilChange.SubsequentChange",
        num_clicks_since_start_time_);
    base::UmaHistogramCounts1000(
        "ChromeOS.Settings.NumNavigationsUntilChange.SubsequentChange",
        num_navigations_since_start_time_);
    base::UmaHistogramCounts1000(
        "ChromeOS.Settings.NumSearchesUntilChange.SubsequentChange",
        num_searches_since_start_time_);
    LogDurationMetric("ChromeOS.Settings.TimeUntilChange.SubsequentChange",
                      now - metric_start_time_);
  } else {
    base::UmaHistogramCounts1000(
        "ChromeOS.Settings.NumClicksUntilChange.FirstChange",
        num_clicks_since_start_time_);
    base::UmaHistogramCounts1000(
        "ChromeOS.Settings.NumNavigationsUntilChange.FirstChange",
        num_navigations_since_start_time_);
    base::UmaHistogramCounts1000(
        "ChromeOS.Settings.NumSearchesUntilChange.FirstChange",
        num_searches_since_start_time_);
    LogDurationMetric("ChromeOS.Settings.TimeUntilChange.FirstChange",
                      now - metric_start_time_);
  }

  ResetMetricsCountersAndTimestamp();
  last_record_setting_changed_timestamp_ = now;
}

void PerSessionSettingsUserActionTracker::ResetMetricsCountersAndTimestamp() {
  metric_start_time_ = base::TimeTicks::Now();
  num_clicks_since_start_time_ = 0u;
  num_navigations_since_start_time_ = 0u;
  num_searches_since_start_time_ = 0u;
}

void PerSessionSettingsUserActionTracker::
    UpdateSettingsPrefTotalUniqueChanged() {
  // Fetch the dictionary from the pref.
  ScopedDictPrefUpdate total_unique_settings_changed_(
      profile_pref_service_, ::prefs::kTotalUniqueOsSettingsChanged);
  base::Value::Dict& pref_data = total_unique_settings_changed_.Get();

  // Set the dictionary.
  // Value is a constant 1 since we only want to know which Setting has been
  // used, not how many times it has been used.
  //
  // The value of pref_data will automatically get stored to
  // profile_pref_service_ upon destruction.
  constexpr int value = 1;
  for (const std::string& setting_string : changed_settings_) {
    if (!pref_data.contains(setting_string)) {
      pref_data.Set(setting_string, value);
    }
  }
}

}  // namespace ash::settings
