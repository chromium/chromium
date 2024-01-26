// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_UPGRADE_DETECTOR_CHROMEOS_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_UPGRADE_DETECTOR_CHROMEOS_H_

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/upgrade_detector/build_state_observer.h"
#include "chrome/browser/upgrade_detector/installed_version_updater_chromeos.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"

class PrefRegistrySimple;
namespace base {
class Clock;
class TickClock;
}  // namespace base

class UpgradeDetectorChromeos : public UpgradeDetector,
                                public BuildStateObserver,
                                public ash::UpdateEngineClient::Observer {
 public:
  UpgradeDetectorChromeos(const UpgradeDetectorChromeos&) = delete;
  UpgradeDetectorChromeos& operator=(const UpgradeDetectorChromeos&) = delete;

  ~UpgradeDetectorChromeos() override;

  // Register ChromeOS specific Prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  static UpgradeDetectorChromeos* GetInstance();

  // UpgradeDetector:
  void Init() override;
  void Shutdown() override;
  base::Time GetAnnoyanceLevelDeadline(
      UpgradeNotificationAnnoyanceLevel level) override;
  void OverrideHighAnnoyanceDeadline(base::Time deadline) override;
  void ResetOverriddenDeadline() override;

  // BuildStateObserver:
  void OnUpdate(const BuildState* build_state) override;

 protected:
  UpgradeDetectorChromeos(const base::Clock* clock,
                          const base::TickClock* tick_clock);

 private:
  friend class base::NoDestructor<UpgradeDetectorChromeos>;

  // Returns the period between first notification and Recommended / Required
  // deadline specified via the RelaunchHeadsUpPeriod policy setting, or a
  // zero delta if unset or out of range.
  static base::TimeDelta GetRelaunchHeadsUpPeriod();

  // Calculates |elevated_deadline_| and |high_deadline_| using either
  // |high_deadline_override_| if it is not null or the threshold values
  // computed based on the RelaunchNotificationPeriod, RelaunchHeadsUpPeriod and
  // RelaunchWindow policy settings.
  void CalculateDeadlines();

  // UpgradeDetector:
  void OnMonitoredPrefsChanged() override;

  // ash::UpdateEngineClient::Observer implementation.
  void UpdateStatusChanged(const update_engine::StatusResult& status) override;
  void OnUpdateOverCellularOneTimePermissionGranted() override;

  // The function that sends out a notification (after a certain time has
  // elapsed) that lets the rest of the UI know we should start notifying the
  // user that a new version is available.
  void NotifyOnUpgrade();

  // The function that sends out a notification (after a certain time has
  // elapsed) that lets the rest of the UI know we should start notifying the
  // user that an update is available but deferred.
  void NotifyOnDeferredUpgrade();

  std::optional<InstalledVersionUpdater> installed_version_updater_;

  // The time when elevated annoyance deadline is reached.
  base::Time elevated_deadline_;

  // The time when high annoyance deadline is reached.
  base::Time high_deadline_;

  // The time when grace annoyance deadline is reached.
  base::Time grace_deadline_;

  // The overridden high annoyance deadline which takes priority over
  // |high_deadline_| for showing relaunch notifications.
  base::Time high_deadline_override_;

  // Observes changes to the browser.relaunch_heads_up_period Local State
  // preference.
  PrefChangeRegistrar pref_change_registrar_;

  // A timer used to move through the various upgrade notification stages and
  // call UpgradeDetector::NotifyUpgrade.
  base::OneShotTimer upgrade_notification_timer_;
  bool initialized_;

  // Indicates whether the flag status has been sent to update engine.
  bool toggled_update_flag_;

  // Indicates whether there is an update in progress.
  bool update_in_progress_;
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_UPGRADE_DETECTOR_CHROMEOS_H_
