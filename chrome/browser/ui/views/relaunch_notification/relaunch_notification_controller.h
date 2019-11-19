// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/relaunch_notification/wall_clock_timer.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "components/prefs/pref_change_registrar.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_controller_platform_impl_chromeos.h"
#else
#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_controller_platform_impl_desktop.h"
#endif  // defined(OS_CHROMEOS)

namespace base {
class Clock;
class TickClock;
}

// A class that observes changes to the browser.relaunch_notification
// preference (which is backed by the RelaunchNotification policy
// setting) and upgrade notifications from the UpgradeDetector. The two
// values for the RelaunchNotification policy setting are handled as follows:
//
// On Chrome desktop:
// - Recommended (1): The controller displays the relaunch recommended bubble on
//   each change to the UpgradeDetector's upgrade_notification_stage (an
//   "annoyance level" of low, elevated, or high). Once the high annoyance level
//   is reached, the controller continually reshows a the bubble on a timer with
//   a period equal to the time delta between the "elevated" and "high"
//   showings.
//
// - Required (2): The controller displays the relaunch required dialog on each
//   change to the UpgradeDetector's upgrade_notification_stage (described
//   above). The browser is relaunched fifteen minutes after the third and final
//   showing of the dialog (which takes place when the UpgradeDetector reaches
//   the high annoyance level).
//
// On Chrome OS both notifications (recommended and required, described above)
// are shown in the unified system tray, overwriting the default "update
// available" notification. It cannot be deferred, so it persists until reboot.
class RelaunchNotificationController : public UpgradeObserver {
 public:
  // |upgrade_detector| is expected to be the process-wide detector, and must
  // outlive the controller.
  explicit RelaunchNotificationController(UpgradeDetector* upgrade_detector);
  ~RelaunchNotificationController() override;

 protected:
  // The length of the final countdown given to the user before the browser is
  // summarily relaunched on Chrome desktop, or the device is rebooted on
  // Chrome OS.
  static constexpr base::TimeDelta kRelaunchGracePeriod =
      base::TimeDelta::FromMinutes(60);

  RelaunchNotificationController(UpgradeDetector* upgrade_detector,
                                 const base::Clock* clock,
                                 const base::TickClock* tick_clock);

  // UpgradeObserver:
  void OnUpgradeRecommended() override;

 private:
  enum class NotificationStyle {
    kNone,         // No notifications are shown.
    kRecommended,  // Relaunches are recommended.
    kRequired,     // Relaunches are required.
  };

  // The platform-specific implementation.
  RelaunchNotificationControllerPlatformImpl platform_impl_;

  // Adjusts to the current notification style as indicated by the
  // browser.relaunch_notification Local State preference.
  void HandleCurrentStyle();

  // Bring the instance out of or back to dormant mode.
  void StartObservingUpgrades();
  void StopObservingUpgrades();

  // Shows the proper notification based on the preference setting and starts
  // the timer to either reshow the bubble or restart the browser/device as
  // appropriate. |level| is the current annoyance level reported by the
  // UpgradeDetector, and |high_deadline| is the time at which the
  // UpgradeDetector will reach the high annoyance level; see the class comment
  // for further details.
  void ShowRelaunchNotification(
      UpgradeDetector::UpgradeNotificationAnnoyanceLevel level,
      base::Time high_deadline);

  // Closes any previously-shown notifications. This is safe to call if no
  // notifications have been shown. Notifications may be closed by other means
  // (e.g., by the user), so there is no expectation that a previously-shown
  // notification is still open when this is invoked. The timer to either
  // repeatedly show the relaunch recommended notification or to force a
  // relaunch once the deadline is reached is also stopped.
  void CloseRelaunchNotification();

  // Starts or reschedules a timer to periodically re-show the relaunch
  // recommended bubble.
  void StartReshowTimer();

  // Run on a timer once high annoyance has been reached to re-show the relaunch
  // recommended bubble.
  void OnReshowRelaunchRecommended();

  // Handles a new |level| and/or |high_deadline| by adjusting the runtime of
  // the relaunch timer, updating the deadline displayed in the title of the
  // relaunch required notification (if shown), and showing it if needed.
  void HandleRelaunchRequiredState(
      UpgradeDetector::UpgradeNotificationAnnoyanceLevel level,
      base::Time high_deadline);

  // Update |last_relaunch_notification_time_| before calling
  // DoNotifyRelaunchRecommended. |past_deadline| reflects whether the
  // Recommended deadline was already passed or not.
  void NotifyRelaunchRecommended(bool past_deadline);

  // Provide deadline to DoNotifyRelaunchRequired.
  virtual void NotifyRelaunchRequired();

  // The following methods, which are invoked by the controller to show or close
  // notifications, are virtual for the sake of testing.

  // Shows the relaunch recommended notification if it is not already open.
  // |past_deadline| reflects whether the Recommended deadline was already
  // passed or not.
  virtual void DoNotifyRelaunchRecommended(bool past_deadline);

  // Shows the relaunch required notification if it is not already open.
  virtual void DoNotifyRelaunchRequired(base::Time deadline);

  // Closes bubble or dialog if either is still open on desktop, or sets the
  // default notification on Chrome OS.
  virtual void Close();

  // Run to restart the browser/device once the relaunch deadline is reached
  // when relaunches are required by policy.
  virtual void OnRelaunchDeadlineExpired();

  // The process-wide upgrade detector.
  UpgradeDetector* const upgrade_detector_;

  // A provider of Time to the controller and its timer for the sake of
  // testability.
  const base::Clock* const clock_;

  // Observes changes to the browser.relaunch_notification Local State pref.
  PrefChangeRegistrar pref_change_registrar_;

  // The last observed notification style. When kNone, the controller is
  // said to be "dormant" as there is no work for it to do aside from watch for
  // changes to browser.relaunch_notification. When any other value, the
  // controller is observing the UpgradeDetector to detect when to show a
  // notification.
  NotificationStyle last_notification_style_;

  // The last observed annoyance level. This member is unconditionally
  // UPGRADE_ANNOYANCE_NONE when the controller is dormant
  // (browser.relaunch_notification is 0).
  UpgradeDetector::UpgradeNotificationAnnoyanceLevel last_level_;

  // The last observed high annoyance deadline.
  base::Time last_high_deadline_;

  // The last time recommended relaunch notification triggered
  base::Time last_relaunch_notification_time_;

  // A timer used either to repeatedly reshow the relaunch recommended bubble
  // once the high annoyance level has been reached, or to trigger browser
  // relaunch once the relaunch required dialog's deadline is reached.
  WallClockTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(RelaunchNotificationController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_H_
