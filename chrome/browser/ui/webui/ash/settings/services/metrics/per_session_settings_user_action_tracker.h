// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_METRICS_PER_SESSION_SETTINGS_USER_ACTION_TRACKER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_METRICS_PER_SESSION_SETTINGS_USER_ACTION_TRACKER_H_

#include <optional>
#include <set>

#include "ash/webui/settings/public/constants/setting.mojom.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"

namespace ash::settings {

// Records user actions which measure the effort required to change a setting.
// This class is only meant to track actions from an individual settings
// session; if the settings window is closed and reopened again, a new instance
// should be created for that new session.
class PerSessionSettingsUserActionTracker {
 public:
  // The parameter must be specifically a per-user profile pref, as we will be
  // retrieving and setting profile-specific prefs in this class which do not
  // exist in local prefs.
  explicit PerSessionSettingsUserActionTracker(
      PrefService* profile_pref_service);
  PerSessionSettingsUserActionTracker(
      const PerSessionSettingsUserActionTracker& other) = delete;
  PerSessionSettingsUserActionTracker& operator=(
      const PerSessionSettingsUserActionTracker& other) = delete;
  ~PerSessionSettingsUserActionTracker();

  void RecordPageActiveTime();
  void RecordPageFocus();
  void RecordPageBlur();
  void RecordClick();
  void RecordNavigation();
  void RecordSearch();
  // TODO (b/282233232): make 'setting' a required parameter once the
  // corresponding function 'RecordSettingChange()' in ts files have been
  // backfilled with the information on what specific Setting has been changed.
  // In the meantime, this parameter is optional, and if it is not provided, it
  // will be set to nullopt to indicate that it has not been initialized.
  void RecordSettingChange(
      std::optional<chromeos::settings::mojom::Setting> setting = std::nullopt);

  const std::set<std::string>& GetChangedSettingsForTesting() {
    return changed_settings_;
  }
  const base::TimeDelta& GetTotalTimeSessionActiveForTesting() {
    return total_time_session_active_;
  }
  const base::TimeTicks& GetWindowLastActiveTimeStampForTesting() {
    return window_last_active_timestamp_;
  }

 private:
  friend class PerSessionSettingsUserActionTrackerTest;

  // Clears the pref kTotalUniqueOsSettingsChanged after 7 days have passed
  // since the user finished OOBE. We will track the changes made within the
  // first week in
  // ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.FirstWeek, and
  // ChromeOS.Settings.NumUniqueSettingsChanged.DeviceLifetime2.SubsequentWeeks
  // for the weeks following the first week.
  void ClearTotalUniqueSettingsChangedPref();

  // Checks whether it has been 7 days since the user has completed
  // OOBE. It utilized the currently existing pref called kOobeOnboardingTime,
  // which is set once the user finished the OOBE.
  bool IsTodayInFirst7Days();

  void RecordLifetimeMetricToPref();
  void ResetMetricsCountersAndTimestamp();
  void UpdateSettingsPrefTotalUniqueChanged();
  void SetHasUserEverRevokedMetricsConsent();
  bool HasUserMetricsConsent();

  // Time at which the last setting change metric was recorded since the window
  // has been focused, or null if no setting change has been recorded since the
  // window has been focused. Note that if the user blurs the window then
  // refocuses it in less than a minute, this value remains non-null; i.e., it
  // flips back to null only when the user has blurred the window for over a
  // minute.
  base::TimeTicks last_record_setting_changed_timestamp_;

  // Time at which recording the current metric has started. If
  // |has_changed_setting_| is true, we're currently measuring the "subsequent
  // setting change" metric; otherwise, we're measuring the "first setting
  // change" metric.
  base::TimeTicks metric_start_time_;

  // Counters associated with the current metric.
  size_t num_clicks_since_start_time_ = 0u;
  size_t num_navigations_since_start_time_ = 0u;
  size_t num_searches_since_start_time_ = 0u;

  // The last time at which a page blur event was received; if no blur events
  // have been received, this field is_null().
  base::TimeTicks last_blur_timestamp_;

  // Tracks which settings have been changed in this user session
  std::set<std::string> changed_settings_;

  // Total time the Settings page has been active and in focus from the opening
  // of the page to closing. Blur events pause the timer.
  base::TimeDelta total_time_session_active_;

  // The point in time which the Settings page was last active and in focus.
  base::TimeTicks window_last_active_timestamp_;

  raw_ptr<PrefService> profile_pref_service_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_METRICS_PER_SESSION_SETTINGS_USER_ACTION_TRACKER_H_
