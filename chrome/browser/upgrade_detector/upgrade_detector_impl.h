// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_UPGRADE_DETECTOR_IMPL_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_UPGRADE_DETECTOR_IMPL_H_

#include <array>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "components/variations/service/variations_service.h"

namespace base {
class Clock;
template <typename T>
class NoDestructor;
class SequencedTaskRunner;
class TaskRunner;
class TickClock;
}  // namespace base

// This class contains the non-CrOS desktop implementation of the detector.
class UpgradeDetectorImpl : public UpgradeDetector,
                            public variations::VariationsService::Observer {
 public:
  ~UpgradeDetectorImpl() override;

  // Returns the currently installed Chrome version, which may be newer than the
  // one currently running. Not supported on Android, iOS or ChromeOS. Must be
  // run on a thread where I/O operations are allowed.
  static base::Version GetCurrentlyInstalledVersion();

  // Returns the global instance.
  static UpgradeDetectorImpl* GetInstance();

  // UpgradeDetector:
  base::TimeDelta GetHighAnnoyanceLevelDelta() override;
  base::Time GetHighAnnoyanceDeadline() override;

 protected:
  UpgradeDetectorImpl(const base::Clock* clock,
                      const base::TickClock* tick_clock);

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
  // The index of a level in stages_.
  enum LevelIndex {
    kStagesIndexHigh = 0,
    kStagesIndexElevated = 1,
    kStagesIndexLow = 2,
    kStagesIndexVeryLow = 3,
    kNumStages
  };

  friend class base::NoDestructor<UpgradeDetectorImpl>;

  // A callback that receives the results of |DetectUpgradeTask|.
  using UpgradeDetectedCallback = base::OnceCallback<void(UpgradeAvailable)>;

  // Returns the index of |level| in |stages_|.
  static LevelIndex AnnoyanceLevelToStagesIndex(
      UpgradeNotificationAnnoyanceLevel level);

  // Returns the annoyance level of |index| in |stages_|.
  static UpgradeNotificationAnnoyanceLevel StageIndexToAnnoyanceLevel(
      size_t index);

  // UpgradeDetector:
  void OnRelaunchNotificationPeriodPrefChanged() override;

#if defined(OS_WIN)
  // Receives the results of AreAutoupdatesEnabled and starts the upgrade check
  // timer.
  void OnAutoupdatesEnabledResult(bool auto_updates_enabled);
#endif

  // Start the timer that will call |CheckForUpgrade()|.
  void StartTimerForUpgradeCheck();

  // Launches a background task to check if we have the latest version.
  void CheckForUpgrade();

  // Starts the upgrade notification timer that will check periodically whether
  // enough time has elapsed to update the severity (which maps to visual
  // badging) of the notification.
  void StartUpgradeNotificationTimer();

  // Lazy-initialization for the various threshold deltas (idempotent).
  void InitializeThresholds();
  void DoInitializeThresholds();

  // Returns true after calling UpgradeDetected if current install is outdated.
  bool DetectOutdatedInstall();

  // The function that sends out a notification (after a certain time has
  // elapsed) that lets the rest of the UI know we should start notifying the
  // user that a new version is available.
  void NotifyOnUpgrade();

  // Determines whether or not an update is available, posting |callback| with
  // the result to |callback_task_runner| if so.
  static void DetectUpgradeTask(
      scoped_refptr<base::TaskRunner> callback_task_runner,
      UpgradeDetectedCallback callback);

  SEQUENCE_CHECKER(sequence_checker_);

  // A sequenced task runner on which blocking tasks run.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // We periodically check to see if Chrome has been upgraded.
  base::RepeatingTimer detect_upgrade_timer_;

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

  // The various deltas from detection time to the different annoyance levels;
  // lazy-initialized by InitializeThresholds.
  std::array<base::TimeDelta, kNumStages> stages_;

  // The date the binaries were built.
  base::Time build_date_;

  // We use this factory to create callback tasks for UpgradeDetected. We pass
  // the task to the actual upgrade detection code, which is in
  // DetectUpgradeTask.
  base::WeakPtrFactory<UpgradeDetectorImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UpgradeDetectorImpl);
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_UPGRADE_DETECTOR_IMPL_H_
