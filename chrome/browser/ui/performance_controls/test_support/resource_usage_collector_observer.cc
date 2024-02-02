// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/test_support/resource_usage_collector_observer.h"

ResourceUsageCollectorObserver::ResourceUsageCollectorObserver(
    base::OnceClosure metrics_refresh_callback) {
  resource_collector_observeration_.Observe(TabResourceUsageCollector::Get());
  metrics_refresh_callback_ = std::move(metrics_refresh_callback);
}

ResourceUsageCollectorObserver::~ResourceUsageCollectorObserver() = default;

void ResourceUsageCollectorObserver::OnTabResourceMetricsRefreshed() {
  std::move(metrics_refresh_callback_).Run();
}
