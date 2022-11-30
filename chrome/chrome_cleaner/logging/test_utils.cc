// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/test_utils.h"

namespace chrome_cleaner {

MockNetworkChecker::MockNetworkChecker() = default;

MockNetworkChecker::~MockNetworkChecker() = default;

bool MockNetworkChecker::IsSafeBrowsingReachable(const GURL& upload_url) const {
  return is_safe_browsing_reachable_;
}

bool MockNetworkChecker::WaitForSafeBrowsing(const GURL& upload_url,
                                             const base::TimeDelta&) {
  return wait_for_safe_browsing_;
}

void MockNetworkChecker::CancelWaitForShutdown() {}

bool IoCountersEqual(
    const base::IoCounters& io_stats,
    const ProcessInformation::SystemResourceUsage& resource_usage) {
  return resource_usage.read_operation_count() == io_stats.ReadOperationCount &&
         resource_usage.write_operation_count() ==
             io_stats.WriteOperationCount &&
         resource_usage.other_operation_count() ==
             io_stats.OtherOperationCount &&
         resource_usage.read_transfer_count() == io_stats.ReadTransferCount &&
         resource_usage.write_transfer_count() == io_stats.WriteTransferCount &&
         resource_usage.other_transfer_count() == io_stats.OtherTransferCount;
}

}  // namespace chrome_cleaner
