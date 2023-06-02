// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/search/per_session_settings_user_action_tracker.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/pref_names.h"

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

void LogDurationMetric(const char* metric_name, base::TimeDelta duration) {
  base::UmaHistogramCustomTimes(metric_name, duration, kMinDurationMetric,
                                kMaxDurationMetric, /*buckets=*/50);
}

}  // namespace

PerSessionSettingsUserActionTracker::PerSessionSettingsUserActionTracker(
    PrefService* pref_service)
    : metric_start_time_(base::TimeTicks::Now()),
      window_last_active_timestamp_(base::TimeTicks::Now()),
      pref_service_(pref_service) {}

PerSessionSettingsUserActionTracker::~PerSessionSettingsUserActionTracker() {
  RecordPageActiveTime();
  LogDurationMetric("ChromeOS.Settings.WindowTotalActiveDuration",
                    total_time_session_active_);

  base::UmaHistogramCounts1000(
      "ChromeOS.Settings.NumUniqueSettingsChanged.PerSession",
      changed_settings_.size());

  // Record number of unique settings changed in this session.
  absl::optional<int> total_unique_settings_changed_count =
      UpdateSettingsPrefTotalUniqueChanged();

  // If the number of total unique setting used increased, flagged by the
  // optional variable total_unique_settings_changed_count having a value, add
  // the datapoint to the histogram.
  if (total_unique_settings_changed_count.has_value()) {
    base::UmaHistogramCounts1000(
        "ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime",
        total_unique_settings_changed_count.value());
  }
}

void PerSessionSettingsUserActionTracker::RecordPageFocus() {
  const base::TimeTicks now = base::TimeTicks::Now();
  window_last_active_timestamp_ = now;

  if (last_blur_timestamp_.is_null())
    return;

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
    absl::optional<chromeos::settings::mojom::Setting> setting) {
  if (setting.has_value()) {
    changed_settings_.insert(
        base::NumberToString(static_cast<int>(setting.value())));
  }
  base::TimeTicks now = base::TimeTicks::Now();

  if (!last_record_setting_changed_timestamp_.is_null()) {
    // If it has been less than |kMinSubsequentChange| since the last recorded
    // setting change, this change is discarded. See https://crbug.com/1073714
    // for details.
    if (now - last_record_setting_changed_timestamp_ < kMinSubsequentChange)
      return;

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

absl::optional<int>
PerSessionSettingsUserActionTracker::UpdateSettingsPrefTotalUniqueChanged() {
  // Fetch the dictionary from the pref.
  base::Value::Dict writeable_dict =
      pref_service_->GetDict(prefs::kTotalUniqueOsSettingsChanged).Clone();
  int current_count = writeable_dict.size();

  // Set the dictionary.
  // Value is a constant 1 since we only want to know which Setting has been
  // used, not how many times it has been used.
  constexpr int value = 1;
  for (const std::string& setting_string : changed_settings_) {
    if (!writeable_dict.contains(setting_string)) {
      writeable_dict.Set(setting_string, value);
    }
  }

  // Save to pref.
  int new_count = writeable_dict.size();
  pref_service_->SetDict(prefs::kTotalUniqueOsSettingsChanged,
                         std::move(writeable_dict));

  // If the new size of the pref dictionary is the same as before, we do not
  // want to record that in UMA so we will return a nullopt to flag not to add
  // to histogram bucket.
  return current_count == new_count ? absl::nullopt
                                    : absl::optional<int>{new_count};
}

}  // namespace ash::settings
