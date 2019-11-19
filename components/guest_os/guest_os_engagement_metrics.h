// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_OS_GUEST_OS_ENGAGEMENT_METRICS_H_
#define COMPONENTS_GUEST_OS_GUEST_OS_ENGAGEMENT_METRICS_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "ui/wm/public/activation_change_observer.h"

class PrefService;

namespace aura {
class Window;
}  // namespace aura

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace guest_os {

// A class for recording engagement metrics. Calculates and reports daily
// totals for the following metrics:
// - Foo.EngagementTime.Total: Engaged session time, i.e. excluding when the
// screen is locked or dim due to user idle. To allow comparisons with the
// other metrics, this class should only be instantiated when the relevant
// Guest OS is supported.
// - Foo.EngagementTime.Foreground: Time when the user is engaged and focused
// on a matching window.
// - Foo.EngagementTime.Background: Time when the user is engaged and not
// focused on a matching window, but the Guest OS is otherwise active in the
// background.
// - Foo.Engagement.FooTotal: Total of Foreground and Background.
class GuestOsEngagementMetrics : public wm::ActivationChangeObserver,
                                 public session_manager::SessionManagerObserver,
                                 public chromeos::PowerManagerClient::Observer {
 public:
  using WindowMatcher = base::RepeatingCallback<bool(const aura::Window*)>;

  GuestOsEngagementMetrics(PrefService* pref_service,
                           WindowMatcher window_matcher,
                           const std::string& pref_prefix,
                           const std::string& uma_name);
  ~GuestOsEngagementMetrics() override;

  // Instead of using |window_matcher_|, we let consumers define when the Guest
  // OS is active in the background. This function should be called whenever
  // changes.
  void SetBackgroundActive(bool background_active);

  void SetClocksForTesting(base::Clock* clock, base::TickClock* tick_clock);

  // wm::ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // chromeos::PowerManagerClient::Observer:
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& proto) override;

 private:
  // Restores accumulated engagement time in previous sessions from profile
  // preferences.
  void RestoreEngagementTimeFromPrefs();

  // Called periodically to save accumulated results to profile preferences.
  void SaveEngagementTimeToPrefs();

  // Called whenever engagement state is changed. Time spent in last state is
  // accumulated to corresponding metrics.
  void UpdateEngagementTime();

  // Records accumulated engagement time metrics to UMA if necessary (i.e. day
  // has changed).
  void RecordEngagementTimeToUmaIfNeeded();

  // Resets accumulated engagement times to zero, and updates both OS version
  // and day ID.
  void ResetEngagementTimePrefs();

  bool ShouldAccumulateEngagementTotalTime() const;
  bool ShouldAccumulateEngagementForegroundTime() const;
  bool ShouldAccumulateEngagementBackgroundTime() const;

  bool ShouldRecordEngagementTimeToUma() const;

  PrefService* const pref_service_;

  WindowMatcher window_matcher_;
  std::string pref_prefix_;
  std::string uma_name_;

  // |clock_| is used for determining when to log to UMA, while |tick_clock_|
  // is used to calculate elapsed time.
  const base::Clock* clock_;
  const base::TickClock* tick_clock_;
  base::RepeatingTimer update_engagement_time_timer_;
  base::RepeatingTimer save_engagement_time_to_prefs_timer_;
  base::TimeTicks last_update_ticks_;

  // States for determining which engagement metrics should we accumulate to.
  bool session_active_ = false;
  bool screen_dimmed_ = false;
  bool background_active_ = false;
  bool matched_window_active_ = false;

  // Accumulated results and associated state which are saved to profile
  // preferences at fixed interval.
  int day_id_ = 0;
  base::TimeDelta engagement_time_total_;
  base::TimeDelta engagement_time_foreground_;
  base::TimeDelta engagement_time_background_;

  DISALLOW_COPY_AND_ASSIGN(GuestOsEngagementMetrics);
};

}  // namespace guest_os

#endif  // COMPONENTS_GUEST_OS_GUEST_OS_ENGAGEMENT_METRICS_H_
