// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_TEST_UTILS_H_
#define CHROME_CHROME_CLEANER_LOGGING_TEST_UTILS_H_

// This must be added before process_metrics.h for base::IoCounters to be
// available.
#include <windows.h>

#include "base/process/process_metrics_iocounters.h"
#include "base/time/time.h"
#include "chrome/chrome_cleaner/logging/network_checker.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "url/gurl.h"

namespace chrome_cleaner {

// Provides a mock NetworkChecker.
class MockNetworkChecker : public NetworkChecker {
 public:
  MockNetworkChecker();
  ~MockNetworkChecker() override;

  void SetIsSafeBrowsingReachableResult(bool result) {
    is_safe_browsing_reachable_ = result;
  }
  void SetWaitForSafeBrowsingResult(bool result) {
    wait_for_safe_browsing_ = result;
  }

  // NetworkChecker:
  bool IsSafeBrowsingReachable(const GURL& upload_url) const override;
  bool WaitForSafeBrowsing(const GURL& upload_url,
                           const base::TimeDelta&) override;
  void CancelWaitForShutdown() override;

 private:
  bool is_safe_browsing_reachable_{true};
  bool wait_for_safe_browsing_{true};
};

// Returns true if IO counters in |resource_usage| are equal to |io_stats|.
bool IoCountersEqual(
    const base::IoCounters& io_counters,
    const ProcessInformation::SystemResourceUsage& resource_usage);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_TEST_UTILS_H_
