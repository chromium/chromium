// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/upgrade_detector.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/idle/idle.h"

// How long to wait between checks for whether the user has been idle.
static const int kIdleRepeatingTimerWait = 10;  // Minutes (seconds if testing).

// How much idle time (since last input even was detected) must have passed
// until we notify that a critical update has occurred.
static const int kIdleAmount = 2;  // Hours (or seconds, if testing).

bool UseTestingIntervals() {
  // If a command line parameter specifying how long the upgrade check should
  // be, we assume it is for testing and switch to using seconds instead of
  // hours.
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  return !cmd_line.GetSwitchValueASCII(switches::kCheckForUpdateIntervalSec)
              .empty();
}

// static
void UpgradeDetector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kAttemptedToEnableAutoupdate, false);
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
      notify_upgrade_(false) {
  // Not all tests provide a PrefService for local_state().
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    pref_change_registrar_.Init(local_state);
    // base::Unretained is safe here because |this| outlives the registrar.
    pref_change_registrar_.Add(
        prefs::kRelaunchNotificationPeriod,
        base::BindRepeating(
            &UpgradeDetector::OnRelaunchNotificationPeriodPrefChanged,
            base::Unretained(this)));
  }
}

UpgradeDetector::~UpgradeDetector() {}

void UpgradeDetector::NotifyOutdatedInstall() {
  for (auto& observer : observer_list_)
    observer.OnOutdatedInstall();
}

void UpgradeDetector::NotifyOutdatedInstallNoAutoUpdate() {
  for (auto& observer : observer_list_)
    observer.OnOutdatedInstallNoAutoUpdate();
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
  constexpr base::TimeDelta kMinValue = base::TimeDelta::FromHours(1);
  if (preference->IsDefaultValue() || value < kMinValue.InMilliseconds())
    return base::TimeDelta();
  return base::TimeDelta::FromMilliseconds(value);
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

void UpgradeDetector::NotifyUpgrade() {
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
  for (auto& observer : observer_list_)
    observer.OnUpgradeRecommended();
}

void UpgradeDetector::NotifyCriticalUpgradeInstalled() {
  for (auto& observer : observer_list_)
    observer.OnCriticalUpgradeInstalled();
}

void UpgradeDetector::NotifyUpdateOverCellularAvailable() {
  for (auto& observer : observer_list_)
    observer.OnUpdateOverCellularAvailable();
}

void UpgradeDetector::NotifyUpdateOverCellularOneTimePermissionGranted() {
  for (auto& observer : observer_list_)
    observer.OnUpdateOverCellularOneTimePermissionGranted();
}

void UpgradeDetector::TriggerCriticalUpdate() {
  const base::TimeDelta idle_timer =
      UseTestingIntervals()
          ? base::TimeDelta::FromSeconds(kIdleRepeatingTimerWait)
          : base::TimeDelta::FromMinutes(kIdleRepeatingTimerWait);
  idle_check_timer_.Start(FROM_HERE, idle_timer, this,
                          &UpgradeDetector::CheckIdle);
}

void UpgradeDetector::CheckIdle() {
  // Don't proceed while an incognito window is open. The timer will still
  // keep firing, so this function will get a chance to re-evaluate this.
  if (chrome::IsIncognitoSessionActive())
    return;

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

void UpgradeDetector::AddObserver(UpgradeObserver* observer) {
  observer_list_.AddObserver(observer);
}

void UpgradeDetector::RemoveObserver(UpgradeObserver* observer) {
  observer_list_.RemoveObserver(observer);
}
