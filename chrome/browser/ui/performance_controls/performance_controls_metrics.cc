// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"

#include <memory>

#include "base/check_op.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/daily_event.h"
#include "components/prefs/pref_service.h"

namespace {

using performance_manager::user_tuning::PerformanceDetectionManager;

// The interval at which the DailyEvent::CheckInterval function should be
// called.
constexpr base::TimeDelta kDailyEventIntervalTimeDelta = base::Minutes(30);

std::string GetDetectionResourceTypeString(
    PerformanceDetectionManager::ResourceType resource_type) {
  switch (resource_type) {
    case PerformanceDetectionManager::ResourceType::kCpu:
      return "Cpu";
    case PerformanceDetectionManager::ResourceType::kMemory:
      return "Memory";
    case PerformanceDetectionManager::ResourceType::kNetwork:
      return "Network";
  }
}

class DailyEventObserver : public metrics::DailyEvent::Observer {
 public:
  explicit DailyEventObserver(PrefService* pref_service)
      : pref_service_(pref_service) {}

  ~DailyEventObserver() override = default;

  void OnDailyEvent(metrics::DailyEvent::IntervalType type) override {
    base::UmaHistogramCounts100(
        base::StrCat({"PerformanceControls.Intervention.BackgroundTab.",
                      GetDetectionResourceTypeString(
                          PerformanceDetectionManager::ResourceType::kCpu),
                      ".MessageShownCount"}),
        GetAndResetPref(
            prefs::kPerformanceInterventionBackgroundCpuMessageCount));

    base::UmaHistogramCounts100(
        base::StrCat({"PerformanceControls.Intervention.BackgroundTab.",
                      GetDetectionResourceTypeString(
                          PerformanceDetectionManager::ResourceType::kCpu),
                      ".RateLimitedCount"}),
        GetAndResetPref(
            prefs::kPerformanceInterventionBackgroundCpuRateLimitedCount));
  }

  int GetAndResetPref(std::string pref_name) {
    const int previous_count = pref_service_->GetInteger(pref_name);
    pref_service_->ClearPref(pref_name);
    return previous_count;
  }

 private:
  raw_ptr<PrefService> pref_service_ = nullptr;
};

}  // namespace

PerformanceInterventionMetricsReporter::PerformanceInterventionMetricsReporter(
    PrefService* pref_service)
    : daily_event_(std::make_unique<metrics::DailyEvent>(
          pref_service,
          prefs::kPerformanceInterventionDailySample,
          std::string())) {
  daily_event_->AddObserver(std::make_unique<DailyEventObserver>(pref_service));
  daily_event_->CheckInterval();
  daily_event_timer_.Start(FROM_HERE, kDailyEventIntervalTimeDelta,
                           daily_event_.get(),
                           &metrics::DailyEvent::CheckInterval);
}

PerformanceInterventionMetricsReporter::
    ~PerformanceInterventionMetricsReporter() = default;

void PerformanceInterventionMetricsReporter::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  metrics::DailyEvent::RegisterPref(registry,
                                    prefs::kPerformanceInterventionDailySample);
  registry->RegisterIntegerPref(
      prefs::kPerformanceInterventionBackgroundCpuMessageCount, 0);
  registry->RegisterIntegerPref(
      prefs::kPerformanceInterventionBackgroundCpuRateLimitedCount, 0);
}

void RecordBatterySaverBubbleAction(BatterySaverBubbleActionType type) {
  base::UmaHistogramEnumeration("PerformanceControls.BatterySaver.BubbleAction",
                                type);
}

void RecordBatterySaverIPHOpenSettings(bool success) {
  base::UmaHistogramBoolean("PerformanceControls.BatterySaver.IPHOpenSettings",
                            success);
}

void RecordMemorySaverBubbleAction(MemorySaverBubbleActionType type) {
  base::UmaHistogramEnumeration("PerformanceControls.MemorySaver.BubbleAction",
                                type);
}

void RecordMemorySaverIPHEnableMode(bool success) {
  base::UmaHistogramBoolean("PerformanceControls.MemorySaver.IPHEnableMode",
                            success);
}

void RecordMemorySaverChipState(MemorySaverChipState state) {
  base::UmaHistogramEnumeration("PerformanceControls.MemorySaver.ChipState",
                                state);
}

void RecordInterventionMessageCount(
    PerformanceDetectionManager::ResourceType resource_type,
    PrefService* pref_service) {
  const int previous_count = pref_service->GetInteger(
      prefs::kPerformanceInterventionBackgroundCpuMessageCount);
  pref_service->SetInteger(
      prefs::kPerformanceInterventionBackgroundCpuMessageCount,
      previous_count + 1);
}

void RecordInterventionRateLimitedCount(
    PerformanceDetectionManager::ResourceType resource_type,
    PrefService* pref_service) {
  const int previous_count = pref_service->GetInteger(
      prefs::kPerformanceInterventionBackgroundCpuRateLimitedCount);
  pref_service->SetInteger(
      prefs::kPerformanceInterventionBackgroundCpuRateLimitedCount,
      previous_count + 1);
}

void RecordInterventionTriggerResult(
    PerformanceDetectionManager::ResourceType resource_type,
    InterventionMessageTriggerResult reason) {
  base::UmaHistogramEnumeration(
      base::StrCat({"PerformanceControls.Intervention.BackgroundTab.",
                    GetDetectionResourceTypeString(resource_type),
                    ".MessageTriggerResult"}),
      reason);
}

void RecordInterventionToolbarButtonClicked() {
  base::RecordComputedAction("PerformanceIntervention.Toolbar.OpenDialog");
}

void RecordInterventionBubbleClosedReason(
    PerformanceDetectionManager::ResourceType resource_type,
    InterventionBubbleActionType type) {
  base::UmaHistogramEnumeration(
      base::StrCat({"PerformanceControls.Intervention.BackgroundTab.",
                    GetDetectionResourceTypeString(resource_type),
                    ".BubbleAction"}),
      type);
}

void RecordCpuHealthStatusAfterDiscard(
    base::TimeDelta time_after_discard,
    PerformanceDetectionManager::HealthLevel health_level) {
  std::string time = std::string();
  if (time_after_discard == base::Minutes(1)) {
    time = "1Min";
  } else if (time_after_discard == base::Minutes(2)) {
    time = "2Min";
  } else {
    CHECK_EQ(time_after_discard, base::Minutes(4));
    time = "4Min";
  }

  base::UmaHistogramEnumeration(
      base::StrCat({"PerformanceControls.Intervention.BackgroundTab.",
                    GetDetectionResourceTypeString(
                        PerformanceDetectionManager::ResourceType::kCpu),
                    ".HealthStatusAfterDiscard.", time}),
      health_level);
}

void RecordCpuUsageBeforeDiscard(int cpu_usage) {
  base::UmaHistogramPercentage(
      "PerformanceControls.Intervention.DiscardedTabsPercentageUsage",
      cpu_usage);
}

void RecordSuggestedTabShownCount(int count) {
  base::UmaHistogramCustomCounts(
      "PerformanceControls.Intervention.SuggestedTabShownCount", count, 0, 10,
      10);
}

void RecordTabRemovedFromTabList(int count_after_removal) {
  base::RecordComputedAction(
      "PerformanceIntervention.Dialog.RemovedSuggestion");
  base::UmaHistogramCustomCounts(
      "PerformanceControls.Intervention.Dialog.TabsAfterRemovalCount",
      count_after_removal, 0, 10, 10);
}

void RecordNumberOfDiscardedTabs(int count) {
  base::UmaHistogramCustomCounts(
      "PerformanceControls.Intervention.DiscardedTabCount", count, 0, 10, 10);
}
