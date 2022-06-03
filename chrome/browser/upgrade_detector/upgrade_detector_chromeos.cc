// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/upgrade_detector_chromeos.h"

#include <stdint.h>

#include <algorithm>

#include "ash/components/settings/timezone_settings.h"
#include "ash/constants/ash_features.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine/update_engine_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

using chromeos::DBusThreadManager;
using chromeos::UpdateEngineClient;

namespace {

// How long to wait (each cycle) before checking which severity level we should
// be at. Once we reach the highest severity, the timer will stop.
constexpr base::TimeDelta kNotifyCycleDelta = base::Minutes(20);

// The default amount of time it takes for the detector's annoyance level
// (upgrade_notification_stage()) to reach UPGRADE_ANNOYANCE_HIGH once an
// upgrade is detected.
constexpr base::TimeDelta kDefaultHighThreshold = base::Days(7);

// The default amount of time it takes for the detector's annoyance level
// (upgrade_notification_stage()) to reach UPGRADE_ANNOYANCE_ELEVATED once an
// upgrade is detected.
constexpr base::TimeDelta kDefaultElevatedThreshold = base::Days(4);

// The default amount of time between the detector's annoyance level change
// from UPGRADE_ANNOYANCE_ELEVATED to UPGRADE_ANNOYANCE_HIGH.
constexpr base::TimeDelta kDefaultHeadsUpPeriod =
    kDefaultHighThreshold - kDefaultElevatedThreshold;

}  // namespace

UpgradeDetectorChromeos::UpgradeDetectorChromeos(
    const base::Clock* clock,
    const base::TickClock* tick_clock)
    : UpgradeDetector(clock, tick_clock),
      upgrade_notification_timer_(tick_clock),
      initialized_(false),
      toggled_update_flag_(false),
      update_in_progress_(false) {}

UpgradeDetectorChromeos::~UpgradeDetectorChromeos() {}

// static
void UpgradeDetectorChromeos::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kRelaunchHeadsUpPeriod,
                                kDefaultHeadsUpPeriod.InMilliseconds());
}

void UpgradeDetectorChromeos::Init() {
  UpgradeDetector::Init();
  MonitorPrefChanges(prefs::kRelaunchHeadsUpPeriod);
  MonitorPrefChanges(prefs::kRelaunchNotification);
  DBusThreadManager::Get()->GetUpdateEngineClient()->AddObserver(this);
  auto* const build_state = g_browser_process->GetBuildState();
  build_state->AddObserver(this);
  installed_version_updater_.emplace(build_state);
  initialized_ = true;
}

void UpgradeDetectorChromeos::Shutdown() {
  // Init() may not be called from tests.
  if (!initialized_)
    return;
  installed_version_updater_.reset();
  g_browser_process->GetBuildState()->RemoveObserver(this);
  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
  upgrade_notification_timer_.Stop();
  UpgradeDetector::Shutdown();
  initialized_ = false;
}

base::Time UpgradeDetectorChromeos::GetAnnoyanceLevelDeadline(
    UpgradeNotificationAnnoyanceLevel level) {
  const base::Time detected_time = upgrade_detected_time();
  if (detected_time.is_null())
    return detected_time;
  switch (level) {
    case UpgradeDetector::UPGRADE_ANNOYANCE_NONE:
    case UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW:
    case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
      return detected_time;
    case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
      return elevated_deadline_;
    case UpgradeDetector::UPGRADE_ANNOYANCE_GRACE:
      return grace_deadline_;
    case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
      return high_deadline_;
    case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
      return upgrade_notification_stage() == UPGRADE_ANNOYANCE_CRITICAL
                 ? detected_time
                 : base::Time();
  }
}

void UpgradeDetectorChromeos::OverrideHighAnnoyanceDeadline(
    base::Time deadline) {
  DCHECK(!upgrade_detected_time().is_null());
  if (deadline > upgrade_detected_time()) {
    high_deadline_override_ = deadline;
    CalculateDeadlines();
    NotifyOnUpgrade();
  }
}

void UpgradeDetectorChromeos::ResetOverriddenDeadline() {
  if (high_deadline_override_.is_null())
    return;

  DCHECK(!upgrade_detected_time().is_null());
  high_deadline_override_ = base::Time();
  CalculateDeadlines();
  NotifyOnUpgrade();
}

void UpgradeDetectorChromeos::OnUpdate(const BuildState* build_state) {
  if (build_state->update_type() == BuildState::UpdateType::kNone) {
    // If the update state changed to `kNone`, reset the state as there is no
    // longer a valid update.
    upgrade_notification_timer_.Stop();
    set_upgrade_available(UPGRADE_AVAILABLE_NONE);
    set_upgrade_detected_time(base::Time());
  } else if (upgrade_detected_time().is_null()) {
    // Only start the timer if the build state is valid.
    set_upgrade_detected_time(clock()->Now());
    CalculateDeadlines();
  }

  update_in_progress_ = false;
  set_is_rollback(build_state->update_type() ==
                  BuildState::UpdateType::kEnterpriseRollback);
  set_is_factory_reset_required(build_state->update_type() ==
                                BuildState::UpdateType::kChannelSwitchRollback);
  NotifyOnUpgrade();
}

// static
base::TimeDelta UpgradeDetectorChromeos::GetRelaunchHeadsUpPeriod() {
  // Not all tests provide a PrefService for local_state().
  auto* local_state = g_browser_process->local_state();
  if (!local_state)
    return base::TimeDelta();
  const auto* preference =
      local_state->FindPreference(prefs::kRelaunchHeadsUpPeriod);
  const int value = preference->GetValue()->GetInt();
  // Enforce the preference's documented minimum value.
  static constexpr base::TimeDelta kMinValue = base::Hours(1);
  if (preference->IsDefaultValue() || value < kMinValue.InMilliseconds())
    return base::TimeDelta();
  return base::Milliseconds(value);
}

void UpgradeDetectorChromeos::CalculateDeadlines() {
  base::TimeDelta notification_period = GetRelaunchNotificationPeriod();
  if (notification_period.is_zero())
    notification_period = kDefaultHighThreshold;

  const RelaunchWindow relaunch_window =
      GetRelaunchWindowPolicyValue().value_or(GetDefaultRelaunchWindow());
  high_deadline_ = AdjustDeadline(upgrade_detected_time() + notification_period,
                                  relaunch_window);

  base::TimeDelta heads_up_period = GetRelaunchHeadsUpPeriod();
  if (heads_up_period.is_zero())
    heads_up_period = kDefaultHeadsUpPeriod;
  elevated_deadline_ =
      std::max(high_deadline_ - heads_up_period, upgrade_detected_time());

  base::TimeDelta grace_period =
      GetGracePeriod(high_deadline_ - elevated_deadline_);
  grace_deadline_ = high_deadline_ - grace_period;

  if (!high_deadline_override_.is_null() &&
      high_deadline_ > high_deadline_override_) {
    elevated_deadline_ = upgrade_detected_time();
    high_deadline_ = std::max(elevated_deadline_, high_deadline_override_);
    grace_period = GetGracePeriod(high_deadline_ - elevated_deadline_);
    grace_deadline_ = high_deadline_ - grace_period;
  }
  DCHECK(grace_deadline_ >= elevated_deadline_);
}

void UpgradeDetectorChromeos::UpdateStatusChanged(
    const update_engine::StatusResult& status) {
  if (status.current_operation() ==
      update_engine::Operation::NEED_PERMISSION_TO_UPDATE) {
    // Update engine broadcasts this state only when update is available but
    // downloading over cellular connection requires user's agreement.
    NotifyUpdateOverCellularAvailable();
  } else if (!update_in_progress_ &&
             status.current_operation() ==
                 update_engine::Operation::DOWNLOADING) {
    update_in_progress_ = true;
    if (!upgrade_detected_time().is_null())
      NotifyOnUpgrade();
  }
  if (!toggled_update_flag_) {
    // Only send feature flag status one time.
    toggled_update_flag_ = true;
    DBusThreadManager::Get()->GetUpdateEngineClient()->ToggleFeature(
        update_engine::kFeatureRepeatedUpdates,
        base::FeatureList::IsEnabled(
            chromeos::features::kAllowRepeatedUpdates));
  }
}

void UpgradeDetectorChromeos::OnUpdateOverCellularOneTimePermissionGranted() {
  NotifyUpdateOverCellularOneTimePermissionGranted();
}

void UpgradeDetectorChromeos::OnMonitoredPrefsChanged() {
  // Check the current stage and potentially notify observers now if a change to
  // the observed policies results in changes to the thresholds.
  if (upgrade_detected_time().is_null())
    return;
  const base::Time old_elevated_deadline = elevated_deadline_;
  const base::Time old_high_deadline = high_deadline_;
  CalculateDeadlines();
  if (elevated_deadline_ != old_elevated_deadline ||
      high_deadline_ != old_high_deadline) {
    NotifyOnUpgrade();
  }
}

void UpgradeDetectorChromeos::NotifyOnUpgrade() {
  const base::Time current_time = clock()->Now();
  // The delay from now until the next highest notification stage is reached, or
  // zero if the highest notification stage has been reached.
  base::TimeDelta next_delay;

  const auto last_stage = upgrade_notification_stage();
  // These if statements must be sorted (highest interval first).
  if (update_in_progress_) {
    // Cancel any notification of a previous update (if there was one) while a
    // new update is being downloaded.
    set_upgrade_notification_stage(UPGRADE_ANNOYANCE_NONE);
  } else if (upgrade_detected_time().is_null()) {
    set_upgrade_notification_stage(UPGRADE_ANNOYANCE_NONE);
  } else if (current_time >= high_deadline_) {
    set_upgrade_notification_stage(UPGRADE_ANNOYANCE_HIGH);
  } else if (current_time >= grace_deadline_) {
    set_upgrade_notification_stage(UPGRADE_ANNOYANCE_GRACE);
    next_delay = high_deadline_ - current_time;
  } else if (current_time >= elevated_deadline_) {
    set_upgrade_notification_stage(UPGRADE_ANNOYANCE_ELEVATED);
    next_delay = grace_deadline_ - current_time;
  } else {
    // If the relaunch notification policy is enabled, the user will be notified
    // at a later time, so set the level to UPGRADE_ANNOYANCE_NONE. Otherwise,
    // the user should be notified now, so set the level to
    // UPGRADE_ANNOYANCE_LOW.
    set_upgrade_notification_stage(IsRelaunchNotificationPolicyEnabled()
                                       ? UPGRADE_ANNOYANCE_NONE
                                       : UPGRADE_ANNOYANCE_LOW);
    next_delay = elevated_deadline_ - current_time;
  }
  const auto new_stage = upgrade_notification_stage();

  if (!next_delay.is_zero()) {
    // Schedule the next wakeup in 20 minutes or when the next change to the
    // notification stage should take place.
    upgrade_notification_timer_.Start(
        FROM_HERE, std::min(next_delay, kNotifyCycleDelta), this,
        &UpgradeDetectorChromeos::NotifyOnUpgrade);
  } else if (upgrade_notification_timer_.IsRunning()) {
    // Explicitly stop the timer in case this call is due to a
    // RelaunchNotificationPeriod change that brought the instance up to the
    // "high" annoyance level.
    upgrade_notification_timer_.Stop();
  }

  // Issue a notification if the stage is above "none" or if it's dropped down
  // to "none" from something higher.
  if (new_stage != UPGRADE_ANNOYANCE_NONE ||
      last_stage != UPGRADE_ANNOYANCE_NONE) {
    NotifyUpgrade();
  }
}

// static
UpgradeDetectorChromeos* UpgradeDetectorChromeos::GetInstance() {
  static base::NoDestructor<UpgradeDetectorChromeos> instance(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance());
  return instance.get();
}

// static
UpgradeDetector* UpgradeDetector::GetInstance() {
  return UpgradeDetectorChromeos::GetInstance();
}

// static
base::TimeDelta UpgradeDetector::GetDefaultHighAnnoyanceThreshold() {
  return kDefaultHighThreshold;
}

// static
base::TimeDelta UpgradeDetector::GetDefaultElevatedAnnoyanceThreshold() {
  return kDefaultElevatedThreshold;
}

// static
UpgradeDetector::RelaunchWindow UpgradeDetector::GetDefaultRelaunchWindow() {
  // Two hours starting at 2am.
  return RelaunchWindow(/*start_hour=*/2, /*start_minute=*/0, base::Hours(2));
}
