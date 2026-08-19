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
    return parent_frame_filter_->activation_state().activation_level ==
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
  std::optional<bool> last_matched_subdomain_disallow_rule_;
  base::OnceClosure quit_closure_;
  std::unique_ptr<content::TestNavigationThrottleInserter> throttle_inserter_;
};

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

}  // namespace subresource_filter
