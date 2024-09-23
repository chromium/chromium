// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_METRICS_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_METRICS_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"
#include "components/metrics/daily_event.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

// Enums for histograms:
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BatterySaverBubbleActionType {
  kTurnOffNow = 0,
  kDismiss = 1,
  kMaxValue = kDismiss
};

enum class InterventionMessageTriggerResult {
  kShown = 0,
  kRateLimited = 1,
  kMixedProfile = 2,
  kMaxValue = kMixedProfile
};

enum class InterventionBubbleActionType {
  kUnknown = 0,
  kIgnore = 1,
  kAccept = 2,
  kDismiss = 3,
  kClose = 4,
  kMaxValue = kClose
};

enum class MemorySaverBubbleActionType {
  kOpenSettings = 0,
  kDismiss = 1,
  kAddException = 2,
  kMaxValue = kAddException
};

enum class MemorySaverChipState {
  kCollapsed = 0,
  kExpandedEducation = 1,
  kExpandedWithSavings = 2,
  kMaxValue = kExpandedWithSavings
};
// End of enums for histograms.

class PerformanceInterventionMetricsReporter {
 public:
  explicit PerformanceInterventionMetricsReporter(PrefService* pref_service);
  ~PerformanceInterventionMetricsReporter();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

 private:
  std::unique_ptr<metrics::DailyEvent> daily_event_;

  // The timer used to periodically check if the daily event should be
  // triggered.
  base::RepeatingTimer daily_event_timer_;
};

void RecordBatterySaverBubbleAction(BatterySaverBubbleActionType type);
void RecordBatterySaverIPHOpenSettings(bool success);
void RecordMemorySaverBubbleAction(MemorySaverBubbleActionType type);
void RecordMemorySaverIPHEnableMode(bool success);
void RecordMemorySaverChipState(MemorySaverChipState type);

void RecordInterventionMessageCount(
    performance_manager::user_tuning::PerformanceDetectionManager::ResourceType
        resource_type,
    PrefService* pref_service);
void RecordInterventionRateLimitedCount(
    performance_manager::user_tuning::PerformanceDetectionManager::ResourceType
        resource_type,
    PrefService* pref_service);
void RecordInterventionTriggerResult(
    performance_manager::user_tuning::PerformanceDetectionManager::ResourceType
        resource_type,
    InterventionMessageTriggerResult reason);
void RecordInterventionToolbarButtonClicked();
void RecordInterventionBubbleClosedReason(
    performance_manager::user_tuning::PerformanceDetectionManager::ResourceType
        resource_type,
    InterventionBubbleActionType type);
void RecordCpuHealthStatusAfterDiscard(
    base::TimeDelta time_after_discard,
    performance_manager::user_tuning::PerformanceDetectionManager::HealthLevel
        health_level);
void RecordCpuUsageBeforeDiscard(int cpu_usage);
void RecordSuggestedTabShownCount(int count);
void RecordTabRemovedFromTabList(int count_after_removal);
void RecordNumberOfDiscardedTabs(int count);

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_METRICS_H_
