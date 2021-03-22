// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/ad_metrics/ad_intervention_browser_test_utils.h"

#include <memory>

#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// Navigate to |url| in |web_contents| and wait until we see the first
// contentful paint.
void NavigateAndWaitForFirstContentfulPaint(content::WebContents* web_contents,
                                            const GURL& url) {
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents);
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstContentfulPaint);

  EXPECT_TRUE(content::NavigateToURL(web_contents, url));
  waiter->Wait();
  waiter.reset();
}

// Create a large sticky ad in |web_contents| and trigger a series of actions
// and layout updates for the ad to be detected by the sticky ad detector.
void TriggerAndDetectLargeStickyAd(content::WebContents* web_contents) {
  // Create the large-sticky-ad.
  EXPECT_TRUE(ExecJs(
      web_contents,
      "let frame = createStickyAdIframeAtBottomOfViewport(window.innerWidth, "
      "window.innerHeight * 0.35);"));

  // Force a layout update to capture the initial state. Then scroll further
  // down.
  ASSERT_TRUE(
      EvalJsAfterLifecycleUpdate(web_contents, "", "window.scrollTo(0, 5000)")
          .error.empty());

  // Force a layout update to capture the final state. At this point the
  // detector should have detected the large-sticky-ad.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(web_contents, "", "").error.empty());
}
