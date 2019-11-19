// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_controller.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/background_mode_manager.h"
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

namespace {

// A type represending the possible RelaunchNotification policy setting values.
enum class RelaunchNotificationSetting {
  // Indications are via the Chrome menu only -- no work for the controller.
  kChromeMenuOnly,

  // Present the relaunch recommended bubble in the last active browser window
  // on Chrome desktop, or the relaunch recommended notification in the unified
  // system tray on Chrome OS.
  kRecommendedBubble,

  // Present the relaunch required dialog in the last active browser window on
  // Chrome desktop, or the relaunch required notification in the unified system
  // tray on Chrome OS.
  kRequiredDialog,
};

// Returns the policy setting, mapping out-of-range values to kChromeMenuOnly.
RelaunchNotificationSetting ReadPreference() {
  switch (g_browser_process->local_state()->GetInteger(
      prefs::kRelaunchNotification)) {
    case 1:
      return RelaunchNotificationSetting::kRecommendedBubble;
    case 2:
      return RelaunchNotificationSetting::kRequiredDialog;
  }
  return RelaunchNotificationSetting::kChromeMenuOnly;
}

}  // namespace

RelaunchNotificationController::RelaunchNotificationController(
    UpgradeDetector* upgrade_detector)
    : RelaunchNotificationController(upgrade_detector,
                                     base::DefaultClock::GetInstance(),
                                     base::DefaultTickClock::GetInstance()) {}

RelaunchNotificationController::~RelaunchNotificationController() {
  if (last_notification_style_ != NotificationStyle::kNone)
    StopObservingUpgrades();
}

// static
constexpr base::TimeDelta RelaunchNotificationController::kRelaunchGracePeriod;

RelaunchNotificationController::RelaunchNotificationController(
    UpgradeDetector* upgrade_detector,
    const base::Clock* clock,
    const base::TickClock* tick_clock)
    : upgrade_detector_(upgrade_detector),
      clock_(clock),
      last_notification_style_(NotificationStyle::kNone),
      last_level_(UpgradeDetector::UPGRADE_ANNOYANCE_NONE),
      timer_(clock_, tick_clock) {
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    pref_change_registrar_.Init(local_state);
    // base::Unretained is safe here because |this| outlives the registrar.
    pref_change_registrar_.Add(
        prefs::kRelaunchNotification,
        base::BindRepeating(&RelaunchNotificationController::HandleCurrentStyle,
                            base::Unretained(this)));
    // Synchronize the instance with the current state of the preference.
    HandleCurrentStyle();
  }
}

void RelaunchNotificationController::OnUpgradeRecommended() {
  DCHECK_NE(last_notification_style_, NotificationStyle::kNone);
  UpgradeDetector::UpgradeNotificationAnnoyanceLevel current_level =
      upgrade_detector_->upgrade_notification_stage();
  const base::Time current_high_deadline =
      upgrade_detector_->GetHighAnnoyanceDeadline();

  // Nothing to do if there has been no change in the level and deadline. If
  // appropriate, a notification for this level has already been shown.
  if (current_level == last_level_ &&
      current_high_deadline == last_high_deadline_) {
    return;
  }

  switch (current_level) {
    case UpgradeDetector::UPGRADE_ANNOYANCE_NONE:
    case UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW:
      // While it's unexpected that the level could move back down, it's not a
      // challenge to do the right thing.
      CloseRelaunchNotification();
      break;
    case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
    case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
    case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
      ShowRelaunchNotification(current_level, current_high_deadline);
      break;
    case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
      // Critical notifications are handled by ToolbarView.
      // TODO(grt): Reconsider this when implementing the relaunch required
      // dialog. Obeying the administrator's wish to force a relaunch when
      // the annoyance level reaches HIGH is more important than showing the
      // critical update dialog. Perhaps handling of "critical" events should
      // be decoupled from the "relaunch to update" events.
      CloseRelaunchNotification();
      break;
  }

  last_level_ = current_level;
  last_high_deadline_ = current_high_deadline;
}

void RelaunchNotificationController::HandleCurrentStyle() {
  NotificationStyle notification_style = NotificationStyle::kNone;

  switch (ReadPreference()) {
    case RelaunchNotificationSetting::kChromeMenuOnly:
      DCHECK_EQ(notification_style, NotificationStyle::kNone);
      break;
    case RelaunchNotificationSetting::kRecommendedBubble:
      notification_style = NotificationStyle::kRecommended;
      break;
    case RelaunchNotificationSetting::kRequiredDialog:
      notification_style = NotificationStyle::kRequired;
      break;
  }

  // Nothing to do if there has been no change in the preference.
  if (notification_style == last_notification_style_)
    return;

  // Close the bubble or dialog if either is open.
  if (last_notification_style_ != NotificationStyle::kNone)
    CloseRelaunchNotification();

  // Reset state so that a notifications is shown anew in a new style if needed.
  last_level_ = UpgradeDetector::UPGRADE_ANNOYANCE_NONE;

  if (notification_style == NotificationStyle::kNone) {
    // Transition away from monitoring for upgrade events back to being dormant:
    // there is no need since AppMenuIconController takes care of updating the
    // Chrome menu as needed.
    StopObservingUpgrades();
    last_notification_style_ = notification_style;
    return;
  }

  // Transitioning away from being dormant: observe the UpgradeDetector.
  if (last_notification_style_ == NotificationStyle::kNone)
    StartObservingUpgrades();

  last_notification_style_ = notification_style;

  // Synchronize the instance with the current state of detection.
  OnUpgradeRecommended();
}

void RelaunchNotificationController::StartObservingUpgrades() {
  upgrade_detector_->AddObserver(this);
}

void RelaunchNotificationController::StopObservingUpgrades() {
  upgrade_detector_->RemoveObserver(this);
}

void RelaunchNotificationController::ShowRelaunchNotification(
    UpgradeDetector::UpgradeNotificationAnnoyanceLevel level,
    base::Time high_deadline) {
  DCHECK_NE(last_notification_style_, NotificationStyle::kNone);

  if (last_notification_style_ == NotificationStyle::kRecommended) {
    // Show the dialog if there has been a level change.
    if (level != last_level_) {
      NotifyRelaunchRecommended(level ==
                                UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
    }

    // If this is the final showing (the one at the "high" level), start the
    // timer to reshow the bubble at each "elevated to high" interval.
    if (level == UpgradeDetector::UPGRADE_ANNOYANCE_HIGH) {
      StartReshowTimer();
    } else {
      // Make sure the timer isn't running following a drop down from HIGH to a
      // lower level.
      timer_.Stop();
    }
  } else {
    HandleRelaunchRequiredState(level, high_deadline);
  }
}

void RelaunchNotificationController::CloseRelaunchNotification() {
  DCHECK_NE(last_notification_style_, NotificationStyle::kNone);

  // Nothing needs to be closed if the annoyance level is none, very low, or
  // critical.
  if (last_level_ == UpgradeDetector::UPGRADE_ANNOYANCE_NONE ||
      last_level_ == UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW ||
      last_level_ == UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL) {
    return;
  }

  // Explicit closure cancels either repeatedly reshowing the bubble or the
  // forced relaunch.
  timer_.Stop();

  Close();
}

void RelaunchNotificationController::HandleRelaunchRequiredState(
    UpgradeDetector::UpgradeNotificationAnnoyanceLevel level,
    base::Time high_deadline) {
  DCHECK_EQ(last_notification_style_, NotificationStyle::kRequired);

  // Make no changes if the new deadline is not in the future and the browser is
  // within the grace period of the previous deadline. The user has already been
  // given the fifteen-minutes countdown so just let it go.
  const base::Time now = clock_->Now();
  if (timer_.IsRunning()) {
    const base::Time& desired_run_time = timer_.desired_run_time();
    DCHECK(!desired_run_time.is_null());
    if (high_deadline <= now && desired_run_time - now <= kRelaunchGracePeriod)
      return;
  }

  // Compute the new deadline (minimally fifteen minutes into the future).
  const base::Time deadline =
      std::max(high_deadline, now) + kRelaunchGracePeriod;

  // (re)Start the timer to perform the relaunch when the deadline is reached.
  timer_.Start(FROM_HERE, deadline, this,
               &RelaunchNotificationController::OnRelaunchDeadlineExpired);

  if (platform_impl_.IsRequiredNotificationShown()) {
    // Tell the notification to update its title if it is showing.
    platform_impl_.SetDeadline(deadline);
  } else {
    // Otherwise, show the dialog if there has been a level change or if the
    // deadline is in the past.
    if (level != last_level_ || high_deadline <= now)
      NotifyRelaunchRequired();
  }
}
void RelaunchNotificationController::StartReshowTimer() {
  DCHECK_EQ(last_notification_style_, NotificationStyle::kRecommended);
  DCHECK(!last_relaunch_notification_time_.is_null());
  const auto high_annoyance_delta =
      upgrade_detector_->GetHighAnnoyanceLevelDelta();
  // Compute the next time to show the notification.
  const auto desired_run_time =
      last_relaunch_notification_time_ + high_annoyance_delta;
  timer_.Start(FROM_HERE, desired_run_time, this,
               &RelaunchNotificationController::OnReshowRelaunchRecommended);
}

void RelaunchNotificationController::OnReshowRelaunchRecommended() {
  DCHECK_EQ(last_notification_style_, NotificationStyle::kRecommended);
  NotifyRelaunchRecommended(true);
  StartReshowTimer();
}

void RelaunchNotificationController::NotifyRelaunchRecommended(
    bool past_deadline) {
  last_relaunch_notification_time_ = clock_->Now();
  DoNotifyRelaunchRecommended(past_deadline);
}

void RelaunchNotificationController::DoNotifyRelaunchRecommended(
    bool past_deadline) {
  platform_impl_.NotifyRelaunchRecommended(
      upgrade_detector_->upgrade_detected_time(), past_deadline);
}

void RelaunchNotificationController::NotifyRelaunchRequired() {
  DCHECK(timer_.IsRunning());
  DCHECK(!timer_.desired_run_time().is_null());
  DoNotifyRelaunchRequired(timer_.desired_run_time());
}

void RelaunchNotificationController::DoNotifyRelaunchRequired(
    base::Time deadline) {
  platform_impl_.NotifyRelaunchRequired(deadline);
}

void RelaunchNotificationController::Close() {
  platform_impl_.CloseRelaunchNotification();
}

void RelaunchNotificationController::OnRelaunchDeadlineExpired() {
  chrome::AttemptRelaunch();
}
