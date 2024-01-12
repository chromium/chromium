// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/search_metrics_reporter.h"

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/ash/components/local_search_service/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::local_search_service {

namespace {

// Interval for asking metrics::DailyEvent to check whether a day has passed.
constexpr base::TimeDelta kCheckDailyEventInternal = base::Minutes(30);

// Prefs corresponding to IndexId values.
constexpr std::array<const char*, SearchMetricsReporter::kNumberIndexIds>
    kDailyCountPrefs = {
        prefs::kLocalSearchServiceMetricsCrosSettingsCount,
        prefs::kLocalSearchServiceMetricsHelpAppCount,
        prefs::kLocalSearchServiceMetricsHelpAppLauncherCount,
        prefs::kLocalSearchServiceMetricsPersonalizationCount,
        prefs::kLocalSearchServiceMetricsShortcutsAppCount,
};

// Histograms corresponding to IndexId values.
constexpr std::array<const char*, SearchMetricsReporter::kNumberIndexIds>
    kDailyCountHistograms = {
        SearchMetricsReporter::kCrosSettingsName,
        SearchMetricsReporter::kHelpAppName,
        SearchMetricsReporter::kHelpAppLauncherName,
        SearchMetricsReporter::kPersonalizationName,
        SearchMetricsReporter::kShortcutsAppName,
};

}  // namespace

const char SearchMetricsReporter::kDailyEventIntervalName[] =
    "LocalSearchService.MetricsDailyEventInterval";
const char SearchMetricsReporter::kCrosSettingsName[] =
    "LocalSearchService.CrosSettings.DailySearch";
const char SearchMetricsReporter::kHelpAppName[] =
    "LocalSearchService.HelpApp.DailySearch";
const char SearchMetricsReporter::kHelpAppLauncherName[] =
    "LocalSearchService.HelpAppLauncher.DailySearch";
const char SearchMetricsReporter::kPersonalizationName[] =
    "LocalSearchService.Personalization.DailySearch";
const char SearchMetricsReporter::kShortcutsAppName[] =
    "LocalSearchService.ShortcutsApp.DailySearch";

constexpr int SearchMetricsReporter::kNumberIndexIds;

// This class is needed since metrics::DailyEvent requires taking ownership
// of its observers. It just forwards events to SearchMetricsReporter.
class SearchMetricsReporter::DailyEventObserver
    : public metrics::DailyEvent::Observer {
 public:
  explicit DailyEventObserver(SearchMetricsReporter* reporter)
      : reporter_(reporter) {
    DCHECK(reporter_);
  }

  ~DailyEventObserver() override = default;
  DailyEventObserver(const DailyEventObserver&) = delete;
  DailyEventObserver& operator=(const DailyEventObserver&) = delete;

  // metrics::DailyEvent::Observer:
  void OnDailyEvent(metrics::DailyEvent::IntervalType type) override {
    reporter_->ReportDailyMetrics(type);
  }

 private:
  raw_ptr<SearchMetricsReporter> reporter_;  // Not owned.
};

// static:
void SearchMetricsReporter::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  metrics::DailyEvent::RegisterPref(
      registry, prefs::kLocalSearchServiceMetricsDailySample);
  for (const char* daily_count_pref : kDailyCountPrefs) {
    registry->RegisterIntegerPref(daily_count_pref, 0);
  }
}

SearchMetricsReporter::SearchMetricsReporter(
    PrefService* local_state_pref_service)
    : pref_service_(local_state_pref_service),
      daily_event_(std::make_unique<metrics::DailyEvent>(
          pref_service_,
          prefs::kLocalSearchServiceMetricsDailySample,
          kDailyEventIntervalName)) {
  for (size_t i = 0; i < kDailyCountPrefs.size(); ++i) {
    daily_counts_[i] = pref_service_->GetInteger(kDailyCountPrefs[i]);
  }

  daily_event_->AddObserver(std::make_unique<DailyEventObserver>(this));
  daily_event_->CheckInterval();
  timer_.Start(FROM_HERE, kCheckDailyEventInternal, daily_event_.get(),
               &metrics::DailyEvent::CheckInterval);
}

SearchMetricsReporter::~SearchMetricsReporter() = default;

void SearchMetricsReporter::OnSearchPerformed(
    IndexId index_id,
    OnSearchPerformedCallback callback) {
  const size_t index = static_cast<size_t>(index_id);
  const char* daily_count_pref = kDailyCountPrefs[index];
  ++daily_counts_[index];
  pref_service_->SetInteger(daily_count_pref, daily_counts_[index]);
  std::move(callback).Run();
}

void SearchMetricsReporter::ReportDailyMetricsForTesting(
    metrics::DailyEvent::IntervalType type) {
  ReportDailyMetrics(type);
}

mojo::PendingRemote<mojom::SearchMetricsReporter>
SearchMetricsReporter::BindNewPipeAndPassRemote() {
  receivers_.push_back(
      std::make_unique<mojo::Receiver<mojom::SearchMetricsReporter>>(this));
  return receivers_.back()->BindNewPipeAndPassRemote();
}

void SearchMetricsReporter::ReportDailyMetrics(
    metrics::DailyEvent::IntervalType type) {
  // Do nothing on the first run.
  if (type == metrics::DailyEvent::IntervalType::FIRST_RUN)
    return;

  // Only send metrics for DAY_ELAPSED event.
  if (type == metrics::DailyEvent::IntervalType::DAY_ELAPSED) {
    for (size_t index = 0; index < kDailyCountPrefs.size(); ++index) {
      base::UmaHistogramCounts1000(kDailyCountHistograms[index],
                                   daily_counts_[index]);
    }
  }

  for (size_t i = 0; i < kDailyCountPrefs.size(); ++i) {
    daily_counts_[i] = 0;
    pref_service_->SetInteger(kDailyCountPrefs[i], 0);
  }
}

}  // namespace ash::local_search_service
