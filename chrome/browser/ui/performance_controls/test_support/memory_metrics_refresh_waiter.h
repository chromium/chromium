// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_MEMORY_METRICS_REFRESH_WAITER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_MEMORY_METRICS_REFRESH_WAITER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/performance_controls/test_support/resource_usage_collector_observer.h"

// Requests and waits for memory usage data to update so tabs can report memory
// savings through discard
class MemoryMetricsRefreshWaiter {
 public:
  MemoryMetricsRefreshWaiter() = default;
  ~MemoryMetricsRefreshWaiter() = default;

  // Forces and waits for the memory metrics to refresh
  void Wait();
};

// Requests and waits for memory usage data to updates resource usage data for
// active tabs
class TabResourceUsageRefreshWaiter : public ResourceUsageCollectorObserver {
 public:
  TabResourceUsageRefreshWaiter();
  ~TabResourceUsageRefreshWaiter() override;

  // Forces and waits for the memory metrics to refresh
  void Wait();
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_MEMORY_METRICS_REFRESH_WAITER_H_
