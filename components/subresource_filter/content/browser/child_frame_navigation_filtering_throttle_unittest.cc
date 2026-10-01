// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/child_frame_navigation_filtering_throttle.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "child_frame_navigation_filtering_throttle.h"
#include "components/subresource_filter/content/browser/child_frame_navigation_test_utils.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

class TestChildFrameNavigationFilteringThrottle
    : public ChildFrameNavigationFilteringThrottle {
 public:
  TestChildFrameNavigationFilteringThrottle(
      content::NavigationThrottleRegistry& registry,
      AsyncDocumentSubresourceFilter* parent_frame_filter,
      bool alias_check_enabled,
      base::RepeatingCallback<std::string(const GURL& url)>
          disallow_message_callback,
      base::RepeatingCallback<void(bool)> on_finished_callback =
          base::RepeatingCallback<void(bool)>())
      : ChildFrameNavigationFilteringThrottle(
            registry,
            parent_frame_filter,
            alias_check_enabled,
            std::move(disallow_message_callback)),
        on_finished_callback_(std::move(on_finished_callback)) {}

  TestChildFrameNavigationFilteringThrottle(
      const TestChildFrameNavigationFilteringThrottle&) = delete;
  TestChildFrameNavigationFilteringThrottle& operator=(
      const TestChildFrameNavigationFilteringThrottle&) = delete;

  ~TestChildFrameNavigationFilteringThrottle() override = default;

  const char* GetNameForLogging() override {
    return "TestChildFrameNavigationFilteringThrottle";
  }

  using ChildFrameNavigationFilteringThrottle::matched_subdomain_disallow_rule;

 private:
  bool ShouldDeferNavigation() const override {
    return parent_frame_filter_ &&
           parent_frame_filter_->activation_state().activation_level ==
               mojom::ActivationLevel::kEnabled;
  }

  void OnCalculatedLoadPolicyFinished() override {
    if (on_finished_callback_) {
      on_finished_callback_.Run(matched_subdomain_disallow_rule());
    }
  }

  base::RepeatingCallback<void(bool)> on_finished_callback_;

  void NotifyLoadPolicy() const override {
    // No observers to notify.
    return;
  }
};

class RedirectDeferringThrottle : public content::NavigationThrottle {
 public:
  explicit RedirectDeferringThrottle(
      content::NavigationThrottleRegistry& registry)
      : content::NavigationThrottle(registry) {}

  RedirectDeferringThrottle(const RedirectDeferringThrottle&) = delete;
  RedirectDeferringThrottle& operator=(const RedirectDeferringThrottle&) =
      delete;

  ~RedirectDeferringThrottle() override = default;

  // content::NavigationThrottle:
  ThrottleCheckResult WillRedirectRequest() override {
    return defer_redirects_ ? DEFER : PROCEED;
  }
  const char* GetNameForLogging() override {
    return "RedirectDeferringThrottle";
  }

  void set_defer_redirects(bool defer_redirects) {
    defer_redirects_ = defer_redirects;
  }

  using content::NavigationThrottle::Resume;

  base::WeakPtr<RedirectDeferringThrottle> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  bool defer_redirects_ = false;
  base::WeakPtrFactory<RedirectDeferringThrottle> weak_ptr_factory_{this};
};

class ChildFrameNavigationFilteringThrottleTest
    : public ChildFrameNavigationFilteringThrottleTestHarness {
 public:
  ChildFrameNavigationFilteringThrottleTest() = default;

  ChildFrameNavigationFilteringThrottleTest(
      const ChildFrameNavigationFilteringThrottleTest&) = delete;
  ChildFrameNavigationFilteringThrottleTest& operator=(
      const ChildFrameNavigationFilteringThrottleTest&) = delete;

  ~ChildFrameNavigationFilteringThrottleTest() override = default;

  void SetUp() override {
    ChildFrameNavigationFilteringThrottleTestHarness::SetUp();

    throttle_inserter_ =
        std::make_unique<content::TestNavigationThrottleInserter>(
            content::RenderViewHostTestHarness::web_contents(),
            base::BindLambdaForTesting([&](content::NavigationThrottleRegistry&
                                               registry) -> void {
              // The |parent_filter_| is the parent frame's filter. Do not
              // register a throttle if the parent is not activated with a valid
              // filter.
              if (parent_filter_) {
                // Add this throttle first, so that it runs before the filter
                // throttle.
                if (add_redirect_deferring_throttle_) {
                  auto redirect_deferring_throttle =
                      std::make_unique<RedirectDeferringThrottle>(registry);
                  redirect_deferring_throttle_ =
                      redirect_deferring_throttle->GetWeakPtr();
                  registry.AddThrottle(std::move(redirect_deferring_throttle));
                }
                auto throttle =
                    std::make_unique<TestChildFrameNavigationFilteringThrottle>(
                        registry, parent_filter_.get(),
                        /*alias_check_enabled=*/alias_check_enabled_,
                        base::BindRepeating([](const GURL& filtered_url) {
                          // Same as GetFilterConsoleMessage().
                          return base::StringPrintf(
                              kDisallowChildFrameConsoleMessageFormat,
                              filtered_url.possibly_invalid_spec().c_str());
                        }),
                        base::BindRepeating(
                            &ChildFrameNavigationFilteringThrottleTest::
                                OnThrottleFinished,
                            base::Unretained(this)));
                EXPECT_NE(nullptr, throttle->GetNameForLogging());
                if (destroy_parent_filter_after_throttle_creation_) {
                  parent_filter_.reset();
                }
                registry.AddThrottle(std::move(throttle));
              }
            }));
  }

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    ASSERT_FALSE(navigation_handle->IsInMainFrame());
  }

 protected:
  void OnThrottleFinished(bool val) {
    last_matched_subdomain_disallow_rule_ = val;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void WaitForThrottle() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  bool alias_check_enabled_ = false;
  bool destroy_parent_filter_after_throttle_creation_ = false;
  bool add_redirect_deferring_throttle_ = false;
  base::WeakPtr<RedirectDeferringThrottle> redirect_deferring_throttle_;
  std::optional<bool> last_matched_subdomain_disallow_rule_;
  base::OnceClosure quit_closure_;
  std::unique_ptr<content::TestNavigationThrottleInserter> throttle_inserter_;
};

TEST_F(ChildFrameNavigationFilteringThrottleTest,
       ParentFilterDestroyedBeforeStartProceeds) {
  InitializeDocumentSubresourceFilter(GURL("https://example.test"));
  destroy_parent_filter_after_throttle_creation_ = true;
  CreateTestSubframeAndInitNavigation(
      GURL("https://example.test/disallowed.html"), main_rfh());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
}

TEST_F(ChildFrameNavigationFilteringThrottleTest,
       ParentFilterDestroyedWhileStartDeferredResumes) {
  InitializeDocumentSubresourceFilter(GURL("https://example.test"));
  CreateTestSubframeAndInitNavigation(
      GURL("https://example.test/disallowed.html"), main_rfh());
  navigation_simulator()->SetAutoAdvance(false);

  navigation_simulator()->Start();
  EXPECT_TRUE(navigation_simulator()->IsDeferred());

  parent_filter_.reset();
  navigation_simulator()->Wait();

  EXPECT_FALSE(navigation_simulator()->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator()->GetLastThrottleCheckResult());
}

TEST_F(ChildFrameNavigationFilteringThrottleTest,
       ParentFilterDestroyedWhileRedirectDeferredResumes) {
  InitializeDocumentSubresourceFilter(GURL("https://example.test"));
  CreateTestSubframeAndInitNavigation(GURL("https://example.test/allowed.html"),
                                      main_rfh());
  navigation_simulator()->SetAutoAdvance(false);

  navigation_simulator()->Start();
  EXPECT_TRUE(navigation_simulator()->IsDeferred());
  navigation_simulator()->Wait();
  EXPECT_FALSE(navigation_simulator()->IsDeferred());

  navigation_simulator()->Redirect(
      GURL("https://example.test/disallowed.html"));
  EXPECT_TRUE(navigation_simulator()->IsDeferred());

  parent_filter_.reset();
  navigation_simulator()->Wait();

  EXPECT_FALSE(navigation_simulator()->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator()->GetLastThrottleCheckResult());
}

TEST_F(ChildFrameNavigationFilteringThrottleTest, FilterOnStart) {
  InitializeDocumentSubresourceFilter(GURL("https://example.test"));
  const GURL url("https://example.test/disallowed.html");
  CreateTestSubframeAndInitNavigation(url, main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_TRUE(std::ranges::contains(GetConsoleMessages(),
                                    GetFilterConsoleMessage(url)));
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

TEST_F(ChildFrameNavigationFilteringThrottleTest,
       MatchedSubdomainDisallowRule_TrueOnRedirect) {
  InitializeDocumentSubresourceFilterWithSubdomainRule(
      GURL("https://example.test"), "anchored_disallowed.com");

  CreateTestSubframeAndInitNavigation(GURL("https://example.test/allowed.html"),
                                      main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_FALSE(last_matched_subdomain_disallow_rule_.value_or(true));

  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateRedirectAndGetResult(
                navigation_simulator(),
                GURL("https://anchored_disallowed.com/foo.html")));
  EXPECT_TRUE(last_matched_subdomain_disallow_rule_.value_or(false));
}

TEST_F(ChildFrameNavigationFilteringThrottleTest,
       MatchedSubdomainDisallowRule_FalseOnRedirectToAllowed) {
  // Use kDryRun so that matched URLs are not blocked, allowing redirect to
  // continue.
  InitializeDocumentSubresourceFilterWithSubdomainRule(
      GURL("https://example.test"), "anchored_disallowed.com",
      mojom::ActivationLevel::kDryRun);

  CreateTestSubframeAndInitNavigation(GURL("https://example.test/allowed.html"),
                                      main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  WaitForThrottle();
  EXPECT_FALSE(last_matched_subdomain_disallow_rule_.value_or(true));

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateRedirectAndGetResult(
                navigation_simulator(),
                GURL("https://anchored_disallowed.com/foo.html")));
  WaitForThrottle();
  EXPECT_TRUE(last_matched_subdomain_disallow_rule_.value_or(false));

  EXPECT_EQ(
      content::NavigationThrottle::PROCEED,
      SimulateRedirectAndGetResult(navigation_simulator(),
                                   GURL("https://example.test/allowed2.html")));
  WaitForThrottle();
  EXPECT_FALSE(last_matched_subdomain_disallow_rule_.value_or(true));
}

// A different throttle can defer WillRedirectRequest() before the filter
// throttle checks the new URL. In dry-run mode, the filter throttle can receive
// the result for the earlier URL while the navigation is deferred. Make sure
// that the new URL does not get the subdomain match of the earlier URL.
TEST_F(ChildFrameNavigationFilteringThrottleTest,
       MatchedSubdomainDisallowRule_IgnoresLateResultForEarlierUrl) {
  InitializeDocumentSubresourceFilterWithSubdomainRule(
      GURL("https://example.test"), "anchored_disallowed.com",
      mojom::ActivationLevel::kDryRun);
  add_redirect_deferring_throttle_ = true;

  CreateTestSubframeAndInitNavigation(
      GURL("https://anchored_disallowed.com/foo.html"), main_rfh());
  navigation_simulator()->SetAutoAdvance(false);
  navigation_simulator()->Start();
  ASSERT_FALSE(navigation_simulator()->IsDeferred());
  // The ruleset check for the ad URL is not complete.
  ASSERT_FALSE(last_matched_subdomain_disallow_rule_.has_value());

  ASSERT_TRUE(redirect_deferring_throttle_);
  redirect_deferring_throttle_->set_defer_redirects(true);
  navigation_simulator()->Redirect(GURL("https://example.test/allowed.html"));
  ASSERT_TRUE(navigation_simulator()->IsDeferred());

  // The filter throttle receives the result for the ad URL while the
  // navigation is deferred.
  WaitForThrottle();
  EXPECT_TRUE(navigation_simulator()->IsDeferred());
  EXPECT_FALSE(last_matched_subdomain_disallow_rule_.value_or(true));

  redirect_deferring_throttle_->Resume();
  EXPECT_FALSE(navigation_simulator()->IsDeferred());
  WaitForThrottle();
  EXPECT_FALSE(last_matched_subdomain_disallow_rule_.value_or(true));
}

// The navigation goes from an ad URL to a second URL, and then to a third URL.
// The filter throttle receives the result for the second URL after the
// redirect to the third URL, and ignores it. Make sure that the third URL does
// not get the subdomain match of the ad URL.
TEST_F(ChildFrameNavigationFilteringThrottleTest,
       MatchedSubdomainDisallowRule_ClearedWhenLateResultIgnored) {
  InitializeDocumentSubresourceFilterWithSubdomainRule(
      GURL("https://example.test"), "anchored_disallowed.com",
      mojom::ActivationLevel::kDryRun);
  add_redirect_deferring_throttle_ = true;

  CreateTestSubframeAndInitNavigation(
      GURL("https://anchored_disallowed.com/foo.html"), main_rfh());
  navigation_simulator()->SetAutoAdvance(false);
  navigation_simulator()->Start();
  WaitForThrottle();
  EXPECT_TRUE(last_matched_subdomain_disallow_rule_.value_or(false));

  // The ruleset check for the second URL does not complete before the next
  // redirect.
  navigation_simulator()->Redirect(GURL("https://example.test/allowed.html"));
  ASSERT_FALSE(navigation_simulator()->IsDeferred());

  ASSERT_TRUE(redirect_deferring_throttle_);
  redirect_deferring_throttle_->set_defer_redirects(true);
  navigation_simulator()->Redirect(GURL("https://example.test/allowed2.html"));
  ASSERT_TRUE(navigation_simulator()->IsDeferred());

  // The filter throttle receives the result for the second URL while the
  // navigation is deferred.
  WaitForThrottle();
  EXPECT_TRUE(navigation_simulator()->IsDeferred());
  EXPECT_FALSE(last_matched_subdomain_disallow_rule_.value_or(true));

  redirect_deferring_throttle_->Resume();
  EXPECT_FALSE(navigation_simulator()->IsDeferred());
  WaitForThrottle();
  EXPECT_FALSE(last_matched_subdomain_disallow_rule_.value_or(true));
}

TEST_F(ChildFrameNavigationFilteringThrottleTest, DryRunOnStart) {
  InitializeDocumentSubresourceFilter(GURL("https://example.test"),
                                      mojom::ActivationLevel::kDryRun);
  const GURL url("https://example.test/disallowed.html");
  CreateTestSubframeAndInitNavigation(url, main_rfh());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_FALSE(std::ranges::contains(GetConsoleMessages(),
                                     GetFilterConsoleMessage(url)));
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

class ChildFrameNavigationFilteringThrottleDnsAliasTest
    : public ChildFrameNavigationFilteringThrottleTest {
 public:
  ChildFrameNavigationFilteringThrottleDnsAliasTest() {
    alias_check_enabled_ = true;
  }

  ~ChildFrameNavigationFilteringThrottleDnsAliasTest() override = default;

 private:
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
  EXPECT_TRUE(std::ranges::contains(GetConsoleMessages(),
                                    GetFilterConsoleMessage(url)));
}

TEST_F(ChildFrameNavigationFilteringThrottleDnsAliasTest,
       ParentFilterDestroyedWithPendingAliasChecksResumes) {
  InitializeDocumentSubresourceFilterWithSubstringRules(
      GURL("https://example.test"), {"disallowed.com"},
      mojom::ActivationLevel::kDryRun);
  CreateTestSubframeAndInitNavigation(GURL("https://example.test/allowed.html"),
                                      main_rfh());
  navigation_simulator()->SetAutoAdvance(false);

  navigation_simulator()->Start();
  EXPECT_FALSE(navigation_simulator()->IsDeferred());
  navigation_simulator()->Redirect(GURL("https://example.test/allowed2.html"));
  EXPECT_FALSE(navigation_simulator()->IsDeferred());

  SetResponseDnsAliasesForNavigation({"disallowed.com"});
  navigation_simulator()->ReadyToCommit();
  EXPECT_TRUE(navigation_simulator()->IsDeferred());

  parent_filter_.reset();
  navigation_simulator()->Wait();

  EXPECT_FALSE(navigation_simulator()->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator()->GetLastThrottleCheckResult());
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
  EXPECT_FALSE(std::ranges::contains(GetConsoleMessages(),
                                     GetFilterConsoleMessage(url)));
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
  EXPECT_FALSE(std::ranges::contains(GetConsoleMessages(),
                                     GetFilterConsoleMessage(url)));
}

TEST_F(ChildFrameNavigationFilteringThrottleDnsAliasTest,
       MatchedSubdomainDisallowRule_TrueOnAlias) {
  InitializeDocumentSubresourceFilterWithSubdomainRule(
      GURL("https://example.test"), "anchored_disallowed.com");

  const GURL url("https://example.test/some_path.html");
  CreateTestSubframeAndInitNavigation(url, main_rfh());

  std::vector<std::string> dns_aliases({"anchored_disallowed.com"});
  SetResponseDnsAliasesForNavigation(std::move(dns_aliases));

  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            SimulateCommitAndGetResult(navigation_simulator()));
  EXPECT_TRUE(last_matched_subdomain_disallow_rule_.value_or(false));
}

TEST_F(ChildFrameNavigationFilteringThrottleTest,
       AdTagReadyAtProcessSelectionMetric_Ready) {
  base::HistogramTester histogram_tester;
  InitializeDocumentSubresourceFilter(GURL("https://example.test"),
                                      mojom::ActivationLevel::kDryRun);
  CreateTestSubframeAndInitNavigation(GURL("https://example.test/allowed.html"),
                                      main_rfh());
  navigation_simulator()->Start();
  WaitForThrottle();

  navigation_simulator()->ReadyToCommit();
  histogram_tester.ExpectUniqueSample(
      "Navigation.OriginAgentCluster.AdTagReadyAtProcessSelection", true, 1);
}

TEST_F(ChildFrameNavigationFilteringThrottleDnsAliasTest,
       AdTagReadyAtProcessSelectionMetric_NotReadyDueToAliases) {
  base::HistogramTester histogram_tester;
  InitializeDocumentSubresourceFilterWithSubstringRules(
      GURL("https://example.test"), {"disallowed.com"},
      mojom::ActivationLevel::kDryRun);

  CreateTestSubframeAndInitNavigation(
      GURL("https://example.test/some_path.html"), main_rfh());
  navigation_simulator()->Start();
  WaitForThrottle();

  // The subresource filter must wait for the response to see the resolved DNS
  // aliases before judging whether it is an ad, leaving the calculation pending
  // at process selection time.
  std::vector<std::string> dns_aliases({"alias1.com", "disallowed.com"});
  SetResponseDnsAliasesForNavigation(std::move(dns_aliases));

  navigation_simulator()->ReadyToCommit();
  histogram_tester.ExpectUniqueSample(
      "Navigation.OriginAgentCluster.AdTagReadyAtProcessSelection", false, 1);
}

}  // namespace subresource_filter
