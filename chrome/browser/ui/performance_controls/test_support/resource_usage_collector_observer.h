// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_RESOURCE_USAGE_COLLECTOR_OBSERVER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_RESOURCE_USAGE_COLLECTOR_OBSERVER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_collector.h"

// Automatically observes the TabResourceUsageCollector and runs the given
// OnceClosure when OnTabResourcemetricsRefreshed() is called
class ResourceUsageCollectorObserver
    : public TabResourceUsageCollector::Observer {
 public:
  explicit ResourceUsageCollectorObserver(
      base::OnceClosure metrics_refresh_callback);
  ~ResourceUsageCollectorObserver() override;

  // TabResourceUsageCollector::Observer:
  void OnTabResourceMetricsRefreshed() override;

 protected:
  base::OnceClosure metrics_refresh_callback_;
  base::ScopedObservation<TabResourceUsageCollector,
                          ResourceUsageCollectorObserver>
      resource_collector_observeration_{this};
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_RESOURCE_USAGE_COLLECTOR_OBSERVER_H_
