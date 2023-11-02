// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_OFFLINE_METRICS_COLLECTOR_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_OFFLINE_METRICS_COLLECTOR_H_

#include "components/offline_pages/core/prefetch/offline_metrics_collector.h"

class PrefService;

namespace offline_pages {

// Testing metrics collector that does nothing.
class TestOfflineMetricsCollector : public OfflineMetricsCollector {
 public:
  explicit TestOfflineMetricsCollector(PrefService*) {}
  ~TestOfflineMetricsCollector() override = default;

  void OnAppStartupOrResume() override {}
  void OnSuccessfulNavigationOnline() override {}
  void OnSuccessfulNavigationOffline() override {}
  void OnPrefetchEnabled() override {}
  void OnSuccessfulPagePrefetch() override {}
  void OnPrefetchedPageOpened() override {}
  void ReportAccumulatedStats() override {}
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_OFFLINE_METRICS_COLLECTOR_H_
