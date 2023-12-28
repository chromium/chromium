// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_MEMORY_METRICS_REFRESH_WAITER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_MEMORY_METRICS_REFRESH_WAITER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

class MemoryMetricsRefreshWaiter : public performance_manager::user_tuning::
                                       UserPerformanceTuningManager::Observer {
 public:
  MemoryMetricsRefreshWaiter();
  ~MemoryMetricsRefreshWaiter() override;

  void OnMemoryMetricsRefreshed() override;

  // Forces and waits for the memory metrics to refresh
  void Wait();

 private:
  base::OnceClosure quit_closure_;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_MEMORY_METRICS_REFRESH_WAITER_H_
