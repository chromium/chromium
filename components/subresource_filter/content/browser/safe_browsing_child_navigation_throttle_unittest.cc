// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/safe_browsing_child_navigation_throttle.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/subresource_filter/content/shared/browser/child_frame_navigation_test_utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

class ProfileInteractionManager;

class SafeBrowsingChildNavigationThrottleTest
    : public ChildFrameNavigationFilteringThrottleTestHarness {
 public:
  SafeBrowsingChildNavigationThrottleTest() = default;

  SafeBrowsingChildNavigationThrottleTest(
      const SafeBrowsingChildNavigationThrottleTest&) = delete;
  SafeBrowsingChildNavigationThrottleTest& operator=(
      const SafeBrowsingChildNavigationThrottleTest&) = delete;

  ~SafeBrowsingChildNavigationThrottleTest() override = default;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    ASSERT_FALSE(navigation_handle->IsInMainFrame());
    // The |parent_filter_| is the parent frame's filter. Do not register a
    // throttle if the parent is not activated with a valid filter.
    if (parent_filter_) {
      auto throttle = std::make_unique<SafeBrowsingChildNavigationThrottle>(
          navigation_handle, parent_filter_.get(),
          /*profile_interaction_manager=*/
          base::WeakPtr<ProfileInteractionManager>(),
          base::BindRepeating([](const GURL& filtered_url) {
            return base::StringPrintf(
                kDisallowChildFrameConsoleMessageFormat,
                filtered_url.possibly_invalid_spec().c_str());
          }),
          /*ad_evidence=*/std::nullopt);
      ASSERT_NE(nullptr, throttle->GetNameForLogging());
      navigation_handle->RegisterThrottleForTesting(std::move(throttle));
    }
  }
};

TEST_F(SafeBrowsingChildNavigationThrottleTest, DelayMetrics) {
  base::HistogramTester histogram_tester;
  InitializeDocumentSubresourceFilter(GURL("https://example.test"));
  CreateTestSubframeAndInitNavigation(GURL("https://example.test/allowed.html"),
                                      main_rfh());
  navigation_simulator()->SetTransition(ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateRedirectAndGetResult(
                navigation_simulator(),
                GURL("https://example.test/disallowed.html")));

  navigation_simulator()->CommitErrorPage();

  const char kFilterDelayDisallowed[] =
      "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.Disallowed2";
  const char kFilterDelayWouldDisallow[] =
      "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.WouldDisallow";
  const char kFilterDelayAllowed[] =
      "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.Allowed";
  histogram_tester.ExpectTotalCount(kFilterDelayDisallowed, 1);
  histogram_tester.ExpectTotalCount(kFilterDelayWouldDisallow, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayAllowed, 0);

  CreateTestSubframeAndInitNavigation(GURL("https://example.test/allowed.html"),
                                      main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));

  histogram_tester.ExpectTotalCount(kFilterDelayDisallowed, 1);
  histogram_tester.ExpectTotalCount(kFilterDelayWouldDisallow, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayAllowed, 1);
}

TEST_F(SafeBrowsingChildNavigationThrottleTest, DelayMetricsDryRun) {
  base::HistogramTester histogram_tester;
  InitializeDocumentSubresourceFilter(GURL("https://example.test"),
                                      mojom::ActivationLevel::kDryRun);
  CreateTestSubframeAndInitNavigation(GURL("https://example.test/allowed.html"),
                                      main_rfh());
  navigation_simulator()->SetTransition(ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateRedirectAndGetResult(
                navigation_simulator(),
                GURL("https://example.test/disallowed.html")));
  navigation_simulator()->Commit();

  const char kFilterDelayDisallowed[] =
      "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.Disallowed2";
  const char kFilterDelayWouldDisallow[] =
      "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.WouldDisallow";
  const char kFilterDelayAllowed[] =
      "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.Allowed";
  histogram_tester.ExpectTotalCount(kFilterDelayDisallowed, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayWouldDisallow, 1);
  histogram_tester.ExpectTotalCount(kFilterDelayAllowed, 0);

  CreateTestSubframeAndInitNavigation(GURL("https://example.test/allowed.html"),
                                      main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));

  histogram_tester.ExpectTotalCount(kFilterDelayDisallowed, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayWouldDisallow, 1);
  histogram_tester.ExpectTotalCount(kFilterDelayAllowed, 1);
}

}  // namespace subresource_filter
