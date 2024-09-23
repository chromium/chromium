// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/upgrade_detector.h"

#include <algorithm>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/idle/idle.h"

namespace {

// How long to wait between checks for whether the user has been idle.
constexpr int kIdleRepeatingTimerWait = 10;  // Minutes (seconds if testing).

// How much idle time (since last input even was detected) must have passed
// until we notify that a critical update has occurred.
constexpr int kIdleAmount = 2;  // Hours (or seconds, if testing).

// Maximum duration for a relaunch window.
constexpr base::TimeDelta kRelaunchWindowMaxDuration = base::Hours(24);

// The default amount of time between the detector's annoyance level change
// from UPGRADE_ANNOYANCE_GRACE to UPGRADE_ANNOYANCE_HIGH.
constexpr auto kDefaultGracePeriod = base::Hours(1);

bool UseTestingIntervals() {
  // If a command line parameter specifying how long the upgrade check should
  // be, we assume it is for testing and switch to using seconds instead of
  // hours.
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  return !cmd_line.GetSwitchValueASCII(switches::kCheckForUpdateIntervalSec)
              .empty();
}

// Returns the start time of the relaunch window on the day of `time`.
base::Time ComputeRelaunchWindowStartForDay(
    const UpgradeDetector::RelaunchWindow& window,
    base::Time time) {
  base::Time::Exploded window_start_exploded;
  time.LocalExplode(&window_start_exploded);
  window_start_exploded.hour = window.hour;
  window_start_exploded.minute = window.minute;
  window_start_exploded.second = 0;
  window_start_exploded.millisecond = 0;
  base::Time window_start;
  if (!base::Time::FromLocalExploded(window_start_exploded, &window_start)) {
    // The start time doesn't exist on that day; likely due to a TZ change at
    // that precise moment (e.g., `window` is 02:00 for zones that observe DST
    // changes at 2am local time). Move forward/backward by one hour and try
    // again. As of this writing, Australia/Lord_Howe is the only zone that
    // doesn't change by one full hour on transitions. Meaning no disrespect to
    // its residents, it is simpler to be fuzzy for that one timezone than to be
    // absolutely accurate.
    if (window_start_exploded.hour < 23) {
      ++window_start_exploded.hour;
    } else {
      --window_start_exploded.hour;
    }

    // The adjusted time could still fail `Time::FromLocalExploded`. This
    // happens on ARM devices in ChromeOS. Once it happens, it could be sticky
    // and creates a crash loop. Return the unadjusted time in this case.
    // See http://crbug/1307913
    if (!base::Time::FromLocalExploded(window_start_exploded, &window_start)) {
      LOG(ERROR) << "FromLocalExploded failed with time=" << time
                 << ", now=" << base::Time::Now()
                 << ", year=" << window_start_exploded.year
                 << ", month=" << window_start_exploded.month
                 << ", day=" << window_start_exploded.day_of_month;

      base::debug::Alias(&window_start_exploded);

      // Dump once per chrome run.
      static bool dumped = false;
      if (!dumped) {
        dumped = base::debug::DumpWithoutCrashing();
      }

      return time;
    }
  }
  return window_start;
}

}  // namespace

// static
void UpgradeDetector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kAttemptedToEnableAutoupdate, false);
}

void UpgradeDetector::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Not all tests provide a PrefService for local_state().
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    pref_change_registrar_.Init(local_state);
    MonitorPrefChanges(prefs::kRelaunchNotificationPeriod);
    MonitorPrefChanges(prefs::kRelaunchWindow);
  }
}

void UpgradeDetector::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  pref_change_task_pending_ = false;
  idle_check_timer_.Stop();
  pref_change_registrar_.Reset();
}

void UpgradeDetector::OverrideRelaunchNotificationToRequired(bool overridden) {
  NotifyRelaunchOverriddenToRequired(overridden);
}

void UpgradeDetector::AddObserver(UpgradeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void UpgradeDetector::RemoveObserver(UpgradeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

void UpgradeDetector::NotifyOutdatedInstall() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observer_list_.empty())
    return;

  for (auto& observer : observer_list_)
    observer.OnOutdatedInstall();
}

void UpgradeDetector::NotifyOutdatedInstallNoAutoUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observer_list_.empty())
    return;

  for (auto& observer : observer_list_)
    observer.OnOutdatedInstallNoAutoUpdate();
}

void UpgradeDetector::NotifyUpgradeForTesting() {
  NotifyUpgrade();
}

UpgradeDetector::UpgradeDetector(const base::Clock* clock,
                                 const base::TickClock* tick_clock)
    : clock_(clock),
      tick_clock_(tick_clock),
      upgrade_available_(UPGRADE_AVAILABLE_NONE),
      best_effort_experiment_updates_available_(false),
      critical_experiment_updates_available_(false),
      critical_update_acknowledged_(false),
      idle_check_timer_(tick_clock_),
      upgrade_notification_stage_(UPGRADE_ANNOYANCE_NONE),
      notify_upgrade_(false) {}

UpgradeDetector::~UpgradeDetector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ensure that Shutdown() was called.
  DCHECK(pref_change_registrar_.IsEmpty());
}

void UpgradeDetector::MonitorPrefChanges(const std::string& pref) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Not all tests provide a PrefService to be monitored.
  if (pref_change_registrar_.prefs()) {
    // base::Unretained is safe here because |this| outlives the registrar.
    pref_change_registrar_.Add(
        pref, base::BindRepeating(&UpgradeDetector::OnRelaunchPrefChanged,
                                  base::Unretained(this)));
  }
}

// static
base::TimeDelta UpgradeDetector::GetRelaunchNotificationPeriod() {
  // Not all tests provide a PrefService for local_state().
  auto* local_state = g_browser_process->local_state();
  if (!local_state)
    return base::TimeDelta();
  const auto* preference =
      local_state->FindPreference(prefs::kRelaunchNotificationPeriod);
  const int value = preference->GetValue()->GetInt();
  // Enforce the preference's documented minimum value.
  constexpr base::TimeDelta kMinValue = base::Hours(1);
  if (preference->IsDefaultValue() || value < kMinValue.InMilliseconds())
    return base::TimeDelta();
  return base::Milliseconds(value);
}

// static
bool UpgradeDetector::IsRelaunchNotificationPolicyEnabled() {
  // Not all tests provide a PrefService for local_state().
  auto* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;

  // "Chrome menu only" means that the policy is disabled.
  constexpr int kChromeMenuOnly = 0;
  return local_state->GetInteger(prefs::kRelaunchNotification) !=
         kChromeMenuOnly;
}

// static
base::Time UpgradeDetector::AdjustDeadline(base::Time deadline,
                                           const RelaunchWindow& window) {
  DCHECK(window.IsValid());
  const base::TimeDelta duration = window.duration;

  // Window duration greater than equal to 24 hours means window covers the
  // whole day, so no need to adjust.
  if (duration >= kRelaunchWindowMaxDuration)
    return deadline;

  // Compute the window on the day of the deadline.
  const base::Time window_start =
      ComputeRelaunchWindowStartForDay(window, deadline);

  if (deadline >= window_start + duration) {
    // Push the deadline forward into a random interval in the next day's
    // window. The next day may be 25, 24 or 23 hours in the future. Take a stab
    // at 24 hours (the norm) and retry once if needed.
    base::Time next_window_start =
        ComputeRelaunchWindowStartForDay(window, deadline + base::Hours(24));
    if (next_window_start == window_start) {
      // The clocks must be set back, yielding a longer day. For example, 24
      // hours after a deadline of 00:30 could be at 23:30 on the same day due
      // to a DST change in the interim that sets clocks backward by one hour.
      // Try again. Use 26 rather than 25 in case some jurisdiction decides to
      // implement a shift of greater than 1 hour.
      next_window_start =
          ComputeRelaunchWindowStartForDay(window, deadline + base::Hours(26));
    } else if (next_window_start - window_start >= base::Hours(26)) {
      // The clocks must be set forward, yielding a shorter day, and we jumped
      // two days rather than one. For example, 24 hours after a deadline of
      // 23:30 could be at 00:30 two days later due to a DST change in the
      // interim that sets clocks forward by one hour". Try again.
      next_window_start =
          ComputeRelaunchWindowStartForDay(window, deadline + base::Hours(23));
    }
    return next_window_start + base::RandTimeDeltaUpTo(duration);
  }

  // Is the deadline within this day's window?
  if (deadline >= window_start)
    return deadline;

  // Compute the relaunch window starting on the day prior to the deadline for
  // cases where the relaunch window straddles midnight.
  base::Time prev_window_start =
      ComputeRelaunchWindowStartForDay(window, deadline - base::Hours(24));
  // The above cases do not apply here:
  // a) Previous day window jumped two days rather than one - This could arise
  // if, for example, 24 hours before the deadline of 00:30 is 23:30 two days
  // ago due to a DST change in the interim that set clocks forward by one hour.
  // But then 00:30 would actually mean 01:30 this day which would mean 00:30 on
  // the previous day. b) Previous day window on the same day - This could arise
  // if, for example, 24 hours before the deadline of 23:30 is 00:30 on the same
  // day due to clocks set back at by one hour. This is already covered in the
  // above condition `deadline >= window_start`.
  if (deadline < prev_window_start + duration)
    return deadline;

  // The deadline is after previous day's window. Push the deadline forward into
  // a random interval in the day's window.
  return window_start + base::RandTimeDeltaUpTo(duration);
}

// static
std::optional<UpgradeDetector::RelaunchWindow>
UpgradeDetector::GetRelaunchWindowPolicyValue() {
  // Not all tests provide a PrefService for local_state().
  auto* local_state = g_browser_process->local_state();
  if (!local_state)
    return std::nullopt;

  const auto* preference = local_state->FindPreference(prefs::kRelaunchWindow);
  DCHECK(preference);
  if (preference->IsDefaultValue())
    return std::nullopt;

  const base::Value* policy_value = preference->GetValue();
  DCHECK(policy_value->is_dict());

  const base::Value::List* entries =
      policy_value->GetDict().FindList("entries");
  if (!entries || entries->empty()) {
    return std::nullopt;
  }

  // Currently only single daily window is supported.
  const auto& window = entries->front().GetDict();
  const std::optional<int> hour = window.FindIntByDottedPath("start.hour");
  const std::optional<int> minute = window.FindIntByDottedPath("start.minute");
  const std::optional<int> duration_mins = window.FindInt("duration_mins");

  if (!hour || !minute || !duration_mins)
    return std::nullopt;

  return RelaunchWindow(hour.value(), minute.value(),
                        base::Minutes(duration_mins.value()));
}

// static
base::TimeDelta UpgradeDetector::GetGracePeriod(
    base::TimeDelta elevated_to_high_delta) {
  return std::min(kDefaultGracePeriod, elevated_to_high_delta / 2);
}

void UpgradeDetector::NotifyUpgrade() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // An implementation will request that a notification be sent after dropping
  // back to the "none" annoyance level if the RelaunchNotificationPeriod
  // setting changes to a large enough value such that none of the revised
  // thresholds have been hit. In this case, consumers should not perceive that
  // an upgrade is available when checking notify_upgrade(). In practice, this
  // is only the case on desktop Chrome and not Chrome OS, where the lowest
  // threshold is hit the moment the upgrade is detected.
  notify_upgrade_ = upgrade_notification_stage_ != UPGRADE_ANNOYANCE_NONE;

  NotifyUpgradeRecommended();
  if (upgrade_available_ == UPGRADE_NEEDED_OUTDATED_INSTALL) {
    NotifyOutdatedInstall();
  } else if (upgrade_available_ == UPGRADE_NEEDED_OUTDATED_INSTALL_NO_AU) {
    NotifyOutdatedInstallNoAutoUpdate();
  } else if (upgrade_available_ == UPGRADE_AVAILABLE_CRITICAL ||
             critical_experiment_updates_available_) {
    TriggerCriticalUpdate();
  }
}

void UpgradeDetector::NotifyUpgradeRecommended() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observer_list_.empty())
    return;

  for (auto& observer : observer_list_)
    observer.OnUpgradeRecommended();
}

void UpgradeDetector::NotifyCriticalUpgradeInstalled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observer_list_.empty())
    return;

  for (auto& observer : observer_list_)
    observer.OnCriticalUpgradeInstalled();
}

void UpgradeDetector::NotifyUpdateDeferred(bool use_notification) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observer_list_.empty())
    return;

  for (auto& observer : observer_list_)
    observer.OnUpdateDeferred(use_notification);
}

void UpgradeDetector::NotifyUpdateOverCellularAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observer_list_.empty())
    return;

  for (auto& observer : observer_list_)
    observer.OnUpdateOverCellularAvailable();
}

void UpgradeDetector::NotifyUpdateOverCellularOneTimePermissionGranted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observer_list_.empty())
    return;

  for (auto& observer : observer_list_)
    observer.OnUpdateOverCellularOneTimePermissionGranted();
}

void UpgradeDetector::NotifyRelaunchOverriddenToRequired(bool overridden) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observer_list_.empty())
    return;

  for (auto& observer : observer_list_)
    observer.OnRelaunchOverriddenToRequired(overridden);
}

void UpgradeDetector::TriggerCriticalUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::TimeDelta idle_timer =
      UseTestingIntervals() ? base::Seconds(kIdleRepeatingTimerWait)
                            : base::Minutes(kIdleRepeatingTimerWait);
  idle_check_timer_.Start(FROM_HERE, idle_timer, this,
                          &UpgradeDetector::CheckIdle);
}

void UpgradeDetector::CheckIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't proceed while an off-the-record or Guest window is open. The timer
  // will still keep firing, so this function will get a chance to re-evaluate
  // this.
  if (chrome::IsOffTheRecordSessionActive() ||
      BrowserList::GetGuestBrowserCount()) {
    return;
  }

  // CalculateIdleState expects an interval in seconds.
  int idle_time_allowed =
      UseTestingIntervals() ? kIdleAmount : kIdleAmount * 60 * 60;

  ui::IdleState state = ui::CalculateIdleState(idle_time_allowed);

  switch (state) {
    case ui::IDLE_STATE_LOCKED:
      // Computer is locked, auto-restart.
      idle_check_timer_.Stop();
      chrome::AttemptRestart();
      break;
    case ui::IDLE_STATE_IDLE:
      // Computer has been idle for long enough, show warning.
      idle_check_timer_.Stop();
      NotifyCriticalUpgradeInstalled();
      break;
    case ui::IDLE_STATE_ACTIVE:
    case ui::IDLE_STATE_UNKNOWN:
      break;
  }
}

void UpgradeDetector::OnRelaunchPrefChanged() {
  // Coalesce simultaneous changes to multiple prefs into a single call to the
  // implementation's OnMonitoredPrefsChanged method by making the call in a
  // task that will run after processing returns to the main event loop.
  if (pref_change_task_pending_)
    return;

  pref_change_task_pending_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<UpgradeDetector> weak_this) {
                       if (weak_this) {
                         weak_this->pref_change_task_pending_ = false;
                         weak_this->OnMonitoredPrefsChanged();
                       }
                     },
                     weak_factory_.GetWeakPtr()));
}
