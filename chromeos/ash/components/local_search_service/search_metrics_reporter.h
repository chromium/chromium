// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_SEARCH_METRICS_REPORTER_H_
#define CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_SEARCH_METRICS_REPORTER_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/local_search_service/public/mojom/local_search_service.mojom.h"
#include "chromeos/ash/components/local_search_service/shared_structs.h"
#include "components/metrics/daily_event.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

class PrefRegistrySimple;
class PrefService;

namespace ash::local_search_service {

// SearchMetricsReporter logs daily search requests to UMA.
class SearchMetricsReporter : public mojom::SearchMetricsReporter {
 public:
  static constexpr int kNumberIndexIds =
      static_cast<int>(IndexId::kMaxValue) + 1;

  // A histogram recorded in UMA, showing reasons why daily metrics are
  // reported.
  static const char kDailyEventIntervalName[];

  // Histogram names of daily counts, one for each IndexId.
  static const char kCrosSettingsName[];
  static const char kHelpAppName[];
  static const char kHelpAppLauncherName[];
  static const char kPersonalizationName[];
  static const char kShortcutsAppName[];

  // Registers prefs used by SearchMetricsReporter in |registry|.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // RegisterLocalStatePrefs() must be called before instantiating this class.
  explicit SearchMetricsReporter(PrefService* local_state_pref_service);
  ~SearchMetricsReporter() override;

  SearchMetricsReporter(const SearchMetricsReporter&) = delete;
  SearchMetricsReporter& operator=(const SearchMetricsReporter&) = delete;

  // mojom::SearchMetricReporter:
  void OnSearchPerformed(IndexId index_id,
                         OnSearchPerformedCallback callback) override;

  // Calls ReportDailyMetrics directly.
  void ReportDailyMetricsForTesting(metrics::DailyEvent::IntervalType type);

  mojo::PendingRemote<mojom::SearchMetricsReporter> BindNewPipeAndPassRemote();

 private:
  class DailyEventObserver;

  // Called by DailyEventObserver whenever a day has elapsed according to
  // |daily_event_|.
  void ReportDailyMetrics(metrics::DailyEvent::IntervalType type);

  raw_ptr<PrefService> pref_service_;  // Not owned.

  std::unique_ptr<metrics::DailyEvent> daily_event_;

  // Instructs |daily_event_| to check if a day has passed.
  base::RepeatingTimer timer_;

  // Daily count for each index id. Ordered by IndexId values.
  // Initial values will be loaded from prefs service.
  std::array<int, kNumberIndexIds> daily_counts_;

  std::vector<std::unique_ptr<mojo::Receiver<mojom::SearchMetricsReporter>>>
      receivers_;
};

}  // namespace ash::local_search_service

#endif  // CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_SEARCH_METRICS_REPORTER_H_
