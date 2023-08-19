// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/child_frame_navigation_filtering_throttle.h"

#include <memory>
#include <sstream>
#include <string>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter_test_utils.h"
#include "components/subresource_filter/content/browser/child_frame_navigation_test_utils.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

class ChildFrameNavigationFilteringThrottleTest
    : public content::RenderViewHostTestHarness,
      public content::WebContentsObserver {
 public:
  ChildFrameNavigationFilteringThrottleTest() {}

  ChildFrameNavigationFilteringThrottleTest(
      const ChildFrameNavigationFilteringThrottleTest&) = delete;
  ChildFrameNavigationFilteringThrottleTest& operator=(
      const ChildFrameNavigationFilteringThrottleTest&) = delete;

  ~ChildFrameNavigationFilteringThrottleTest() override {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://example.test"));
    Observe(RenderViewHostTestHarness::web_contents());
  }

  void TearDown() override {
    dealer_handle_.reset();
    ruleset_handle_.reset();
    parent_filter_.reset();
    RunUntilIdle();
    content::RenderViewHostTestHarness::TearDown();
  }

  content::NavigationSimulator* navigation_simulator() {
    return navigation_simulator_.get();
  }

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    ASSERT_FALSE(navigation_handle->IsInMainFrame());
    // The |parent_filter_| is the parent frame's filter. Do not register a
    // throttle if the parent is not activated with a valid filter.
    if (parent_filter_) {
      auto throttle = std::make_unique<ChildFrameNavigationFilteringThrottle>(
          navigation_handle, parent_filter_.get());
      ASSERT_NE(nullptr, throttle->GetNameForLogging());
      navigation_handle->RegisterThrottleForTesting(std::move(throttle));
    }
  }

  void InitializeDocumentSubresourceFilter(
      const GURL& document_url,
      mojom::ActivationLevel parent_level = mojom::ActivationLevel::kEnabled) {
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            "disallowed.html", &test_ruleset_pair_));

    FinishInitializingDocumentSubresourceFilter(document_url, parent_level);
  }

  void InitializeDocumentSubresourceFilterWithSubstringRules(
      const GURL& document_url,
      std::vector<base::StringPiece> urls_to_block,
      mojom::ActivationLevel parent_level = mojom::ActivationLevel::kEnabled) {
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_creator_.CreateRulesetToDisallowURLWithSubstrings(
            urls_to_block, &test_ruleset_pair_));

    FinishInitializingDocumentSubresourceFilter(document_url, parent_level);
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  void CreateTestSubframeAndInitNavigation(const GURL& first_url,
                                           content::RenderFrameHost* parent) {
    content::RenderFrameHost* render_frame =
        content::RenderFrameHostTester::For(parent)->AppendChild(
            base::StringPrintf("subframe-%s", first_url.spec().c_str()));
    navigation_simulator_ =
        content::NavigationSimulator::CreateRendererInitiated(first_url,
                                                              render_frame);
  }

  const std::vector<std::string>& GetConsoleMessages() {
    return content::RenderFrameHostTester::For(main_rfh())
        ->GetConsoleMessages();
  }

  std::string GetFilterConsoleMessage(const GURL& filtered_url) {
    return base::StringPrintf(kDisallowChildFrameConsoleMessageFormat,
                              filtered_url.possibly_invalid_spec().c_str());
  }

  void SetResponseDnsAliasesForNavigation(std::vector<std::string> aliases) {
    DCHECK(navigation_simulator_);
    navigation_simulator_->SetResponseDnsAliases(std::move(aliases));
  }

 private:
  void FinishInitializingDocumentSubresourceFilter(
      const GURL& document_url,
      mojom::ActivationLevel parent_level) {
    // Make the blocking task runner run on the current task runner for the
    // tests, to ensure that the NavigationSimulator properly runs all necessary
    // tasks while waiting for throttle checks to finish.
    dealer_handle_ = std::make_unique<VerifiedRulesetDealer::Handle>(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    dealer_handle_->TryOpenAndSetRulesetFile(test_ruleset_pair_.indexed.path,
                                             /*expected_checksum=*/0,
                                             base::DoNothing());
    ruleset_handle_ =
        std::make_unique<VerifiedRuleset::Handle>(dealer_handle_.get());

    testing::TestActivationStateCallbackReceiver activation_state;
    mojom::ActivationState parent_activation_state;
    parent_activation_state.activation_level = parent_level;
    parent_activation_state.enable_logging = true;
    parent_filter_ = std::make_unique<AsyncDocumentSubresourceFilter>(
        ruleset_handle_.get(),
        AsyncDocumentSubresourceFilter::InitializationParams(
            document_url, url::Origin::Create(document_url),
            parent_activation_state),
        activation_state.GetCallback());
    RunUntilIdle();
    activation_state.ExpectReceivedOnce(parent_activation_state);
  }

  testing::TestRulesetCreator test_ruleset_creator_;
  testing::TestRulesetPair test_ruleset_pair_;

  std::unique_ptr<VerifiedRulesetDealer::Handle> dealer_handle_;
  std::unique_ptr<VerifiedRuleset::Handle> ruleset_handle_;

  std::unique_ptr<AsyncDocumentSubresourceFilter> parent_filter_;

  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;
};

TEST_F(ChildFrameNavigationFilteringThrottleTest, FilterOnStart) {
  InitializeDocumentSubresourceFilter(GURL("https://example.test"));
  const GURL url("https://example.test/disallowed.html");
  CreateTestSubframeAndInitNavigation(url, main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_TRUE(
      base::Contains(GetConsoleMessages(), GetFilterConsoleMessage(url)));
}

TEST_F(ChildFrameNavigationFilteringThrottleTest, FilterOnRedirect) {
  InitializeDocumentSubresourceFilter(GURL("https://example.test"));
  CreateTestSubframeAndInitNavigation(GURL("https://example.test/allowed.html"),
                                      main_rfh());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateRedirectAndGetResult(
                navigation_simulator(),
                GURL("https://example.test/disallowed.html")));
}

TEST_F(ChildFrameNavigationFilteringThrottleTest, DryRunOnStart) {
  InitializeDocumentSubresourceFilter(GURL("https://example.test"),
                                      mojom::ActivationLevel::kDryRun);
  const GURL url("https://example.test/disallowed.html");
  CreateTestSubframeAndInitNavigation(url, main_rfh());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_FALSE(
      base::Contains(GetConsoleMessages(), GetFilterConsoleMessage(url)));
}

TEST_F(ChildFrameNavigationFilteringThrottleTest, DryRunOnRedirect) {
  InitializeDocumentSubresourceFilter(GURL("https://example.test"),
                                      mojom::ActivationLevel::kDryRun);
  CreateTestSubframeAndInitNavigation(GURL("https://example.test/allowed.html"),
                                      main_rfh());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateRedirectAndGetResult(
                navigation_simulator(),
                GURL("https://example.test/disallowed.html")));
}

TEST_F(ChildFrameNavigationFilteringThrottleTest, FilterOnSecondRedirect) {
  InitializeDocumentSubresourceFilter(GURL("https://example.test"));
  CreateTestSubframeAndInitNavigation(GURL("https://example.test/allowed.html"),
                                      main_rfh());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(
      content::NavigationThrottle::PROCEED,
      SimulateRedirectAndGetResult(navigation_simulator(),
                                   GURL("https://example.test/allowed2.html")));
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateRedirectAndGetResult(
                navigation_simulator(),
                GURL("https://example.test/disallowed.html")));
}

TEST_F(ChildFrameNavigationFilteringThrottleTest, NeverFilterNonMatchingRule) {
  InitializeDocumentSubresourceFilter(GURL("https://example.test"));
  CreateTestSubframeAndInitNavigation(GURL("https://example.test/allowed.html"),
                                      main_rfh());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(
      content::NavigationThrottle::PROCEED,
      SimulateRedirectAndGetResult(navigation_simulator(),
                                   GURL("https://example.test/allowed2.html")));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
}

TEST_F(ChildFrameNavigationFilteringThrottleTest, FilterSubsubframe) {
  // Fake an activation of the subframe.
  content::RenderFrameHost* parent_subframe =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChild("parent-sub");
  GURL test_url = GURL("https://example.test");
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      test_url, parent_subframe);
  navigation->Start();
  InitializeDocumentSubresourceFilter(GURL("https://example.test"));
  navigation->Commit();

  CreateTestSubframeAndInitNavigation(
      GURL("https://example.test/disallowed.html"), parent_subframe);
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));
}

TEST_F(ChildFrameNavigationFilteringThrottleTest, DelayMetrics) {
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

TEST_F(ChildFrameNavigationFilteringThrottleTest, DelayMetricsDryRun) {
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

class ChildFrameNavigationFilteringThrottleDnsAliasTest
    : public ChildFrameNavigationFilteringThrottleTest {
 public:
  ChildFrameNavigationFilteringThrottleDnsAliasTest() {
    feature_list_.InitAndEnableFeature(
        ::features::kSendCnameAliasesToSubresourceFilterFromBrowser);
  }

  ~ChildFrameNavigationFilteringThrottleDnsAliasTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

TEST_F(ChildFrameNavigationFilteringThrottleDnsAliasTest,
       FilterOnWillProcessResponse) {
  InitializeDocumentSubresourceFilterWithSubstringRules(
      GURL("https://example.test"), {"disallowed.com", ".bad-ad/some"});

  const GURL url("https://example.test/some_path.html");
  CreateTestSubframeAndInitNavigation(url, main_rfh());

  std::vector<std::string> dns_aliases({"alias1.com", "/", "example.test", "",
                                        "disallowed.com", "allowed.com",
                                        "test.bad-ad"});
  SetResponseDnsAliasesForNavigation(std::move(dns_aliases));

  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            SimulateCommitAndGetResult(navigation_simulator()));
  EXPECT_TRUE(
      base::Contains(GetConsoleMessages(), GetFilterConsoleMessage(url)));
}

TEST_F(ChildFrameNavigationFilteringThrottleDnsAliasTest,
       DryRunOnWillProcessResponse) {
  InitializeDocumentSubresourceFilterWithSubstringRules(
      GURL("https://example.test"), {"disallowed.com", "bad", "blocked"},
      mojom::ActivationLevel::kDryRun);

  const GURL url("https://example.test/some_path.html");
  CreateTestSubframeAndInitNavigation(url, main_rfh());

  std::vector<std::string> dns_aliases({"alias1.com", "", "test.disallowed.com",
                                        "allowed.com", "blocked.com",
                                        "bad.org"});
  SetResponseDnsAliasesForNavigation(std::move(dns_aliases));

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  EXPECT_FALSE(
      base::Contains(GetConsoleMessages(), GetFilterConsoleMessage(url)));
}

TEST_F(ChildFrameNavigationFilteringThrottleDnsAliasTest, EnabledNoAliases) {
  InitializeDocumentSubresourceFilterWithSubstringRules(
      GURL("https://example.test"), {"disallowed.com"},
      mojom::ActivationLevel::kEnabled);

  const GURL url("https://example.test/some_path.html");
  CreateTestSubframeAndInitNavigation(url, main_rfh());

  std::vector<std::string> dns_aliases;
  SetResponseDnsAliasesForNavigation(std::move(dns_aliases));

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  EXPECT_FALSE(
      base::Contains(GetConsoleMessages(), GetFilterConsoleMessage(url)));
}

}  // namespace subresource_filter
