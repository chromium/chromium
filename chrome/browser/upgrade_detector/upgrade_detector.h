// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_UPGRADE_DETECTOR_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_UPGRADE_DETECTOR_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class UpgradeObserver;
namespace base {
class Clock;
class TickClock;
}

///////////////////////////////////////////////////////////////////////////////
// UpgradeDetector
//
// This class is a singleton class that monitors when an upgrade happens in the
// background. We basically ask Omaha what it thinks the latest version is and
// if our version is lower we send out a notification upon:
//   a) Detecting an upgrade and...
//   b) When we think the user should be notified about the upgrade.
// The latter happens much later, since we don't want to be too annoying.
// This class is not thread safe -- all access must take place on the UI thread.
//
class UpgradeDetector {
 public:
  // The Homeland Security Upgrade Advisory System.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum UpgradeNotificationAnnoyanceLevel {
    UPGRADE_ANNOYANCE_NONE = 0,      // What? Me worry?
    UPGRADE_ANNOYANCE_LOW = 1,       // Green.
    UPGRADE_ANNOYANCE_ELEVATED = 2,  // Yellow.
    UPGRADE_ANNOYANCE_HIGH = 3,      // Red.
    // UPGRADE_ANNOYANCE_SEVERE = 4,  // Removed in 2018-03 for lack of use.
    UPGRADE_ANNOYANCE_CRITICAL = 5,  // Red exclamation mark.
    UPGRADE_ANNOYANCE_VERY_LOW = 6,  // Green early warning for canary and dev.
    UPGRADE_ANNOYANCE_MAX_VALUE = UPGRADE_ANNOYANCE_VERY_LOW
  };

  // Returns the singleton implementation instance.
  static UpgradeDetector* GetInstance();

  virtual ~UpgradeDetector();

  // Returns the default delta from upgrade detection until high annoyance is
  // reached.
  static base::TimeDelta GetDefaultHighAnnoyanceThreshold();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  virtual void Init();
  virtual void Shutdown();

  // Returns the time at which an available upgrade was detected.
  base::Time upgrade_detected_time() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return upgrade_detected_time_;
  }

  // Whether the user should be notified about an upgrade.
  bool notify_upgrade() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return notify_upgrade_;
  }

  // Whether the upgrade recommendation is due to Chrome being outdated.
  bool is_outdated_install() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return upgrade_available_ == UPGRADE_NEEDED_OUTDATED_INSTALL;
  }

  // Whether the upgrade recommendation is due to Chrome being outdated AND
  // auto-update is turned off.
  bool is_outdated_install_no_au() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return upgrade_available_ == UPGRADE_NEEDED_OUTDATED_INSTALL_NO_AU;
  }

  // Returns true if the detector has found that a newer version of Chrome is
  // installed and a relaunch would complete the update.
  bool is_upgrade_available() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return upgrade_available_ == UPGRADE_AVAILABLE_REGULAR ||
           upgrade_available_ == UPGRADE_AVAILABLE_CRITICAL;
  }

  // Notify this object that the user has acknowledged the critical update so we
  // don't need to complain about it for now.
  void acknowledge_critical_update() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    critical_update_acknowledged_ = true;
  }

  // Whether the user has acknowledged the critical update.
  bool critical_update_acknowledged() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return critical_update_acknowledged_;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool is_factory_reset_required() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_factory_reset_required_;
  }

  bool is_rollback() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_rollback_;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  UpgradeNotificationAnnoyanceLevel upgrade_notification_stage() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return upgrade_notification_stage_;
  }

  // Returns the delta between "elevated" and "high" annoyance levels.
  virtual base::TimeDelta GetHighAnnoyanceLevelDelta() = 0;

  // Returns the tick count at which "high" annoyance level will be (or was)
  // reached, or a null tick count if an upgrade has not yet been detected.
  virtual base::Time GetHighAnnoyanceDeadline() = 0;

  // Overrides the "high" annoyance deadline, setting it to |deadline|. On
  // Chrome OS, this also sets the "elevated" annoyance deadline to the time at
  // which the available update was detected. This has no effect on desktop
  // Chrome browsers.
  virtual void OverrideHighAnnoyanceDeadline(base::Time deadine) {}

  // Resets the overridden deadlines and recalculates them according to the
  // thresholds from the Local State. This has no effect on desktop Chrome
  // browsers.
  virtual void ResetOverriddenDeadline() {}

  // Overrides the relaunch notification style to required if |override|; else
  // resets the override so that the policy settings take effect.
  void OverrideRelaunchNotificationToRequired(bool override);

  void AddObserver(UpgradeObserver* observer);

  void RemoveObserver(UpgradeObserver* observer);

  // Notifies that the current install is outdated. No details are expected.
  void NotifyOutdatedInstall();

  // Notifies that the current install is outdated and auto-update (AU) is
  // disabled. No details are expected.
  void NotifyOutdatedInstallNoAutoUpdate();

 protected:
  enum UpgradeAvailable {
    // If no update is available and current install is recent enough.
    UPGRADE_AVAILABLE_NONE,
    // If a regular update is available.
    UPGRADE_AVAILABLE_REGULAR,
    // If a critical update to Chrome has been installed, such as a zero-day
    // fix.
    UPGRADE_AVAILABLE_CRITICAL,
    // If no update to Chrome has been installed for more than the recommended
    // time.
    UPGRADE_NEEDED_OUTDATED_INSTALL,
    // If no update to Chrome has been installed for more than the recommended
    // time AND auto-update is turned off.
    UPGRADE_NEEDED_OUTDATED_INSTALL_NO_AU,
  };

  UpgradeDetector(const base::Clock* clock, const base::TickClock* tick_clock);

  // Returns the notification period specified via the
  // RelaunchNotificationPeriod policy setting, or a zero delta if unset or out
  // of range.
  static base::TimeDelta GetRelaunchNotificationPeriod();
  static bool IsRelaunchNotificationPolicyEnabled();

  const base::Clock* clock() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return clock_;
  }

  const base::TickClock* tick_clock() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return tick_clock_;
  }

  // Notifies that update is recommended and triggers different actions based
  // on the update availability.
  void NotifyUpgrade();

  // Notifies that update is recommended.
  void NotifyUpgradeRecommended();

  // Notifies that a critical update has been installed. No details are
  // expected.
  void NotifyCriticalUpgradeInstalled();

  // The function that sends out a notification that lets the rest of the UI
  // know we should notify the user that a new update is available to download
  // over cellular connection.
  void NotifyUpdateOverCellularAvailable();

  // Notifies that the user's one time permission on update over cellular
  // connection has been granted.
  void NotifyUpdateOverCellularOneTimePermissionGranted();

  // Notifies about a request to override the relaunch notification style to
  // required or reset the overridden style.
  void NotifyRelaunchOverriddenToRequired(bool override);

  // Triggers a critical update, which starts a timer that checks the machine
  // idle state. Protected and virtual so that it could be overridden by tests.
  virtual void TriggerCriticalUpdate();

  UpgradeAvailable upgrade_available() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return upgrade_available_;
  }
  void set_upgrade_available(UpgradeAvailable available) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    upgrade_available_ = available;
  }

  void set_upgrade_detected_time(base::Time upgrade_detected_time) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    upgrade_detected_time_ = upgrade_detected_time;
  }

  void set_best_effort_experiment_updates_available(bool available) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    best_effort_experiment_updates_available_ = available;
  }

  bool critical_experiment_updates_available() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return critical_experiment_updates_available_;
  }
  void set_critical_experiment_updates_available(bool available) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    critical_experiment_updates_available_ = available;
  }

  void set_critical_update_acknowledged(bool acknowledged) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    critical_update_acknowledged_ = acknowledged;
  }

  void set_upgrade_notification_stage(UpgradeNotificationAnnoyanceLevel stage) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    upgrade_notification_stage_ = stage;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void set_is_factory_reset_required(bool is_factory_reset_required) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    is_factory_reset_required_ = is_factory_reset_required;
  }

  void set_is_rollback(bool is_rollback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    is_rollback_ = is_rollback;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  FRIEND_TEST_ALL_PREFIXES(AppMenuModelTest, Basics);
  FRIEND_TEST_ALL_PREFIXES(SystemTrayClientTest, UpdateTrayIcon);
  friend class UpgradeMetricsProviderTest;

  // Handles a change to the browser.relaunch_notification_period Local State
  // preference. Subclasses should call NotifyUpgrade if observers are to be
  // notified of the change (generally speaking, if an upgrade is available).
  virtual void OnRelaunchNotificationPeriodPrefChanged() = 0;

  // Initiates an Idle check. Tells us whether Chrome has received any
  // input events since the specified time.
  void CheckIdle();

  // A provider of Time to the detector.
  const base::Clock* const clock_;

  // A provider of TimeTicks to the detectors' timers.
  const base::TickClock* const tick_clock_;

  // Observes changes to the browser.relaunch_notification_period Local State
  // preference.
  PrefChangeRegistrar pref_change_registrar_;

  // Whether any software updates are available (experiment updates are tracked
  // separately via additional member variables below).
  UpgradeAvailable upgrade_available_;

  // The time at which an available upgrade was detected.
  base::Time upgrade_detected_time_;

  // Whether "best effort" experiment updates are available.
  bool best_effort_experiment_updates_available_;

  // Whether "critical" experiment updates are available.
  bool critical_experiment_updates_available_;

  // Whether the user has acknowledged the critical update.
  bool critical_update_acknowledged_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Whether a factory reset is needed to complete an update.
  bool is_factory_reset_required_ = false;

  // Whether the update is actually an admin-initiated rollback of the device
  // to an earlier version of Chrome OS, which results in the device being
  // wiped when it's rebooted.
  bool is_rollback_ = false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // A timer to check to see if we've been idle for long enough to show the
  // critical warning. Should only be set if |upgrade_available_| is
  // UPGRADE_AVAILABLE_CRITICAL.
  base::RepeatingTimer idle_check_timer_;

  // The stage at which the annoyance level for upgrade notifications is at.
  UpgradeNotificationAnnoyanceLevel upgrade_notification_stage_;

  // Whether we have waited long enough after detecting an upgrade (to see
  // is we should start nagging about upgrading).
  bool notify_upgrade_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::ObserverList<UpgradeObserver>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(UpgradeDetector);
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_UPGRADE_DETECTOR_H_
