// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBFRAME_NAVIGATION_TEST_UTILS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBFRAME_NAVIGATION_TEST_UTILS_H_

#include "content/public/browser/navigation_throttle.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace content {
class NavigationSimulator;
}

namespace subresource_filter {

content::NavigationThrottle::ThrottleCheckResult SimulateStartAndGetResult(
    content::NavigationSimulator* navigation_simulator);

content::NavigationThrottle::ThrottleCheckResult SimulateRedirectAndGetResult(
    content::NavigationSimulator* navigation_simulator,
    const GURL& new_url);

content::NavigationThrottle::ThrottleCheckResult SimulateCommitAndGetResult(
    content::NavigationSimulator* navigation_simulator);

void SimulateFailedNavigation(
    content::NavigationSimulator* navigation_simulator,
    net::Error error);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBFRAME_NAVIGATION_TEST_UTILS_H_
