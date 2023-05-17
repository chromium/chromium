// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_SEARCH_PER_SESSION_SETTINGS_USER_ACTION_TRACKER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_SEARCH_PER_SESSION_SETTINGS_USER_ACTION_TRACKER_H_

#include <set>

#include "base/time/time.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/setting.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::settings {

// Records user actions which measure the effort required to change a setting.
// This class is only meant to track actions from an individual settings
// session; if the settings window is closed and reopened again, a new instance
// should be created for that new session.
class PerSessionSettingsUserActionTracker {
 public:
  PerSessionSettingsUserActionTracker();
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
  void RecordSettingChange(absl::optional<chromeos::settings::mojom::Setting>
                               setting = absl::nullopt);

  const std::set<chromeos::settings::mojom::Setting>&
  GetChangedSettingsForTesting() {
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

  void ResetMetricsCountersAndTimestamp();

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
  std::set<chromeos::settings::mojom::Setting> changed_settings_;

  // Total time the Settings page has been active and in focus from the opening
  // of the page to closing. Blur events pause the timer.
  base::TimeDelta total_time_session_active_;

  // The point in time which the Settings page was last active and in focus.
  base::TimeTicks window_last_active_timestamp_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_SEARCH_PER_SESSION_SETTINGS_USER_ACTION_TRACKER_H_
