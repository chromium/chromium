// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subframe_navigation_test_utils.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

content::NavigationThrottle::ThrottleCheckResult SimulateStartAndGetResult(
    content::NavigationSimulator* navigation_simulator) {
  navigation_simulator->Start();
  return navigation_simulator->GetLastThrottleCheckResult();
}

content::NavigationThrottle::ThrottleCheckResult SimulateRedirectAndGetResult(
    content::NavigationSimulator* navigation_simulator,
    const GURL& new_url) {
  navigation_simulator->Redirect(new_url);
  return navigation_simulator->GetLastThrottleCheckResult();
}

content::NavigationThrottle::ThrottleCheckResult SimulateCommitAndGetResult(
    content::NavigationSimulator* navigation_simulator) {
  navigation_simulator->Commit();
  return navigation_simulator->GetLastThrottleCheckResult();
}

void SimulateFailedNavigation(
    content::NavigationSimulator* navigation_simulator,
    net::Error error) {
  navigation_simulator->Fail(error);
  if (error != net::ERR_ABORTED) {
    navigation_simulator->CommitErrorPage();
  }
}

}  // namespace subresource_filter
