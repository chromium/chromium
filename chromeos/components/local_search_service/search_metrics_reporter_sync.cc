// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/search_metrics_reporter_sync.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/components/local_search_service/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace local_search_service {
namespace {

// Interval for asking metrics::DailyEvent to check whether a day has passed.
constexpr base::TimeDelta kCheckDailyEventInternal =
    base::TimeDelta::FromMinutes(30);

// Prefs corresponding to IndexId values.
constexpr std::array<const char*, SearchMetricsReporterSync::kNumberIndexIds>
    kDailyCountPrefs = {
        prefs::kLocalSearchServiceMetricsCrosSettingsCount,
        prefs::kLocalSearchServiceMetricsHelpAppCount,
};

// Histograms corresponding to IndexId values.
constexpr std::array<const char*, SearchMetricsReporterSync::kNumberIndexIds>
    kDailyCountHistograms = {
        SearchMetricsReporterSync::kCrosSettingsName,
        SearchMetricsReporterSync::kHelpAppName,
};

}  // namespace

constexpr char SearchMetricsReporterSync::kDailyEventIntervalName[];
constexpr char SearchMetricsReporterSync::kCrosSettingsName[];
constexpr char SearchMetricsReporterSync::kHelpAppName[];

constexpr int SearchMetricsReporterSync::kNumberIndexIds;

// This class is needed since metrics::DailyEvent requires taking ownership
// of its observers. It just forwards events to SearchMetricsReporter.
class SearchMetricsReporterSync::DailyEventObserver
    : public metrics::DailyEvent::Observer {
 public:
  explicit DailyEventObserver(SearchMetricsReporterSync* reporter)
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
  SearchMetricsReporterSync* reporter_;  // Not owned.
};

// static:
void SearchMetricsReporterSync::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  metrics::DailyEvent::RegisterPref(
      registry, prefs::kLocalSearchServiceMetricsDailySample);
  for (const char* daily_count_pref : kDailyCountPrefs) {
    registry->RegisterIntegerPref(daily_count_pref, 0);
  }
}

SearchMetricsReporterSync::SearchMetricsReporterSync(
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

SearchMetricsReporterSync::~SearchMetricsReporterSync() = default;

void SearchMetricsReporterSync::SetIndexId(IndexId index_id) {
  DCHECK(!index_id_);
  index_id_ = index_id;
  DCHECK_LT(static_cast<size_t>(index_id), kDailyCountPrefs.size());
}

void SearchMetricsReporterSync::OnSearchPerformed() {
  DCHECK(index_id_);
  const size_t index = static_cast<size_t>(*index_id_);
  const char* daily_count_pref = kDailyCountPrefs[index];
  ++daily_counts_[index];
  pref_service_->SetInteger(daily_count_pref, daily_counts_[index]);
}

void SearchMetricsReporterSync::ReportDailyMetricsForTesting(
    metrics::DailyEvent::IntervalType type) {
  ReportDailyMetrics(type);
}

void SearchMetricsReporterSync::ReportDailyMetrics(
    metrics::DailyEvent::IntervalType type) {
  if (!index_id_)
    return;

  // Don't send metrics on first run or if the clock is changed.
  if (type == metrics::DailyEvent::IntervalType::DAY_ELAPSED) {
    const size_t index = static_cast<size_t>(*index_id_);
    base::UmaHistogramCounts1000(kDailyCountHistograms[index],
                                 daily_counts_[index]);
  }

  for (size_t i = 0; i < kDailyCountPrefs.size(); ++i) {
    daily_counts_[i] = 0;
    pref_service_->SetInteger(kDailyCountPrefs[i], 0);
  }
}

}  // namespace local_search_service
}  // namespace chromeos
