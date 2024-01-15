// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_UPGRADE_DETECTOR_IMPL_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_UPGRADE_DETECTOR_IMPL_H_

#include <array>
#include <optional>

#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/version.h"
#include "chrome/browser/upgrade_detector/build_state_observer.h"
#include "chrome/browser/upgrade_detector/installed_version_poller.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "components/variations/service/variations_service.h"

namespace base {
class Clock;
class TickClock;
}  // namespace base

// This class contains the non-CrOS desktop implementation of the detector.
class UpgradeDetectorImpl : public UpgradeDetector,
                            public BuildStateObserver,
                            public variations::VariationsService::Observer {
 public:
  // Returns the global instance.
  static UpgradeDetectorImpl* GetInstance();

  UpgradeDetectorImpl(const UpgradeDetectorImpl&) = delete;
  UpgradeDetectorImpl& operator=(const UpgradeDetectorImpl&) = delete;

  // UpgradeDetector:
  void Init() override;
  void Shutdown() override;
  base::Time GetAnnoyanceLevelDeadline(
      UpgradeNotificationAnnoyanceLevel level) override;

  // BuildStateObserver:
  void OnUpdate(const BuildState* build_state) override;

 protected:
  UpgradeDetectorImpl(const base::Clock* clock,
                      const base::TickClock* tick_clock);
  ~UpgradeDetectorImpl() override;

  // Sends out a notification and starts a one shot timer to wait until
  // notifying the user.
  void UpgradeDetected(UpgradeAvailable upgrade_available);

  // variations::VariationsService::Observer:
  void OnExperimentChangesDetected(Severity severity) override;

  // Trigger an "on upgrade" notification based on the specified |time_passed|
  // interval. Exposed as protected for testing.
  void NotifyOnUpgradeWithTimePassed(base::TimeDelta time_passed);

  base::TimeDelta GetThresholdForLevel(UpgradeNotificationAnnoyanceLevel level);

 private:
  // The index of a level in `stages_`.
  enum LevelIndex {
    kStagesIndexHigh = 0,
    kStagesIndexGrace = 1,
    kStagesIndexElevated = 2,
    kStagesIndexLow = 3,
    kStagesIndexVeryLow = 4,
    kNumStages
  };

  friend class base::NoDestructor<UpgradeDetectorImpl>;

  // Returns the index of |level| in |stages_|.
  static LevelIndex AnnoyanceLevelToStagesIndex(
      UpgradeNotificationAnnoyanceLevel level);

  // Returns the annoyance level of |index| in |stages_|.
  static UpgradeNotificationAnnoyanceLevel StageIndexToAnnoyanceLevel(
      size_t index);

  // UpgradeDetector:
  void OnMonitoredPrefsChanged() override;

  // Starts the upgrade notification timer that will check periodically whether
  // enough time has elapsed to update the severity (which maps to visual
  // badging) of the notification.
  void StartUpgradeNotificationTimer();

  // Calculation for the various threshold deltas.
  void CalculateThresholds();
  void DoCalculateThresholds();

  void StartOutdatedBuildDetector();
  void DetectOutdatedInstall();

  // The function that sends out a notification (after a certain time has
  // elapsed) that lets the rest of the UI know we should start notifying the
  // user that a new version is available.
  void NotifyOnUpgrade();

  SEQUENCE_CHECKER(sequence_checker_);

  std::optional<InstalledVersionPoller> installed_version_poller_;

  // A timer used to periodically check if the build has become outdated.
  base::OneShotTimer outdated_build_timer_;

  // A timer used to move through the various upgrade notification stages and
  // schedule calls to NotifyUpgrade.
  base::OneShotTimer upgrade_notification_timer_;

  // True if auto update is turned on.
  bool is_auto_update_enabled_;

  // True if test switches that simulate an outdated install are present on the
  // command line.
  const bool simulating_outdated_;

  // True if test switches are present on the command line.
  const bool is_testing_;

  // The various deltas from upgrade detection time to the different annoyance
  // levels; only valid while `upgrade_notification_timer_` is running to
  // advance through the annoyance levels. Must be sorted in decreasing order of
  // time.
  std::array<base::TimeDelta, kNumStages> stages_;

  // The date the binaries were built.
  base::Time build_date_;

  base::WeakPtrFactory<UpgradeDetectorImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_UPGRADE_DETECTOR_IMPL_H_
