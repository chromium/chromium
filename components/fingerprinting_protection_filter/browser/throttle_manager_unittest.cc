// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/throttle_manager.h"

#include <map>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_filter_features.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/fingerprinting_protection_filter/browser/test_support.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/subresource_filter/content/shared/browser/child_frame_navigation_test_utils.h"
#include "components/subresource_filter/core/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {

namespace {

using ::subresource_filter::SimulateCommitAndGetResult;
using ::subresource_filter::SimulateFailedNavigation;
using ::subresource_filter::SimulateRedirectAndGetResult;
using ::subresource_filter::SimulateStartAndGetResult;
using ::subresource_filter::VerifiedRulesetDealer;

}  // namespace

namespace proto = url_pattern_index::proto;

const char kTestURLWithActivation[] = "https://www.page-with-activation.com/";
const char kTestURLWithActivation2[] =
    "https://www.page-with-activation-2.com/";
const char kTestURLWithDryRun[] = "https://www.page-with-dryrun.com/";
const char kTestURLWithNoActivation[] =
    "https://www.page-without-activation.com/";

// Enum determining when the mock page state throttle notifies the throttle
// manager of page level activation state.
enum PageActivationNotificationTiming {
  WILL_START_REQUEST,
  WILL_PROCESS_RESPONSE,
};

// Simple throttle that sends page-level activation to the manager for a
// specific set of URLs.
class MockPageActivationThrottle : public content::NavigationThrottle {
 public:
  MockPageActivationThrottle(
      content::NavigationHandle* navigation_handle,
      PageActivationNotificationTiming activation_throttle_state)
      : content::NavigationThrottle(navigation_handle),
        activation_throttle_state_(activation_throttle_state) {
    // Add some default activations.
    subresource_filter::mojom::ActivationState enabled_state;
    enabled_state.activation_level =
        subresource_filter::mojom::ActivationLevel::kEnabled;

    subresource_filter::mojom::ActivationState dry_run_state;
    dry_run_state.activation_level =
        subresource_filter::mojom::ActivationLevel::kDryRun;

    mock_page_activations_[GURL(kTestURLWithActivation)] = enabled_state;
    mock_page_activations_[GURL(kTestURLWithActivation2)] = enabled_state;
    mock_page_activations_[GURL(kTestURLWithDryRun)] = dry_run_state;
    mock_page_activations_[GURL(kTestURLWithNoActivation)] =
        subresource_filter::mojom::ActivationState();
  }

  MockPageActivationThrottle(const MockPageActivationThrottle&) = delete;
  MockPageActivationThrottle& operator=(const MockPageActivationThrottle&) =
      delete;

  ~MockPageActivationThrottle() override = default;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    return MaybeNotifyActivation(WILL_START_REQUEST);
  }

  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override {
    return MaybeNotifyActivation(WILL_PROCESS_RESPONSE);
  }
  const char* GetNameForLogging() override {
    return "MockPageActivationThrottle";
  }

 private:
  content::NavigationThrottle::ThrottleCheckResult MaybeNotifyActivation(
      PageActivationNotificationTiming throttle_state) {
    if (throttle_state == activation_throttle_state_) {
      auto it = mock_page_activations_.find(navigation_handle()->GetURL());
      if (it != mock_page_activations_.end()) {
        auto* web_contents_helper =
            FingerprintingProtectionWebContentsHelper::FromWebContents(
                navigation_handle()->GetWebContents());
        if (web_contents_helper) {
          web_contents_helper->NotifyPageActivationComputed(navigation_handle(),
                                                            it->second);
        }
      }
    }
    return content::NavigationThrottle::PROCEED;
  }

  std::map<GURL, subresource_filter::mojom::ActivationState>
      mock_page_activations_;
  PageActivationNotificationTiming activation_throttle_state_;
};

class ThrottleManagerTest
    : public content::RenderViewHostTestHarness,
      public content::WebContentsObserver,
      public ::testing::WithParamInterface<PageActivationNotificationTiming> {
 public:
  ThrottleManagerTest()
      // We need the task environment to use a separate IO thread so that the
      // ChildProcessSecurityPolicy checks which perform different logic
      // based on whether they are called on the UI thread or the IO thread do
      // the right thing.
      : content::RenderViewHostTestHarness(
            content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

  ThrottleManagerTest(const ThrottleManagerTest&) = delete;
  ThrottleManagerTest& operator=(const ThrottleManagerTest&) = delete;

  ~ThrottleManagerTest() override = default;

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    content::WebContents* web_contents =
        RenderViewHostTestHarness::web_contents();

    // Initialize the ruleset dealer with a blocklist suffix rule.
    std::vector<proto::UrlRule> rules;
    rules.push_back(
        subresource_filter::testing::CreateSuffixRule("disallowed.html"));
    ASSERT_NO_FATAL_FAILURE(test_ruleset_creator_.CreateRulesetWithRules(
        rules, &test_ruleset_pair_));

    // Make the blocking task runner run on the current task runner for the
    // tests, to ensure that the NavigationSimulator properly runs all necessary
    // tasks while waiting for throttle checks to finish.
    dealer_handle_ = std::make_unique<VerifiedRulesetDealer::Handle>(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    dealer_handle_->TryOpenAndSetRulesetFile(test_ruleset_pair_.indexed.path,
                                             /*expected_checksum=*/0,
                                             base::DoNothing());

    test_support_ = std::make_unique<TestSupport>();

    FingerprintingProtectionWebContentsHelper::CreateForWebContents(
        web_contents, test_support_->prefs(),
        test_support_->tracking_protection_settings(), dealer_handle_.get());

    Observe(web_contents);

    NavigateAndCommit(GURL("https://example.first"));
  }

  void TearDown() override {
    test_support_.reset();
    dealer_handle_.reset();
    base::RunLoop().RunUntilIdle();
    content::RenderViewHostTestHarness::TearDown();
  }

  // Helper methods:

  void SetFingerprintingProtectionFlags(bool is_enabled,
                                        bool is_dry_run = false) {
    if (is_enabled && !is_dry_run) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{privacy_sandbox::kFingerprintingProtectionSetting, {}},
           {features::kEnableFingerprintingProtectionFilter,
            {{"activation_level", "enabled"}}}},
          /*disabled_features=*/{});
    } else if (is_enabled && is_dry_run) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{privacy_sandbox::kFingerprintingProtectionSetting, {}},
           {features::kEnableFingerprintingProtectionFilter,
            {{"activation_level", "dry_run"}}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{{privacy_sandbox::
                                     kFingerprintingProtectionSetting,
                                 {}}},
          /*disabled_features=*/{
              features::kEnableFingerprintingProtectionFilter});
    }
  }

  void CreateTestNavigation(const GURL& url,
                            content::RenderFrameHost* render_frame_host) {
    DCHECK(render_frame_host);
    navigation_simulator_ =
        content::NavigationSimulator::CreateRendererInitiated(
            url, render_frame_host);
  }

  content::NavigationSimulator* navigation_simulator() {
    return navigation_simulator_.get();
  }

  content::RenderFrameHost* CreateSubframeWithTestNavigation(
      const GURL& url,
      content::RenderFrameHost* parent) {
    content::RenderFrameHost* subframe =
        content::RenderFrameHostTester::For(parent)->AppendChild(
            base::StringPrintf("subframe-%s", url.spec().c_str()));
    CreateTestNavigation(url, subframe);
    return subframe;
  }

  content::RenderFrameHost* CreateFencedFrameWithTestNavigation(
      const GURL& url,
      content::RenderFrameHost* parent) {
    content::RenderFrameHost* fenced_frame =
        content::RenderFrameHostTester::For(parent)->AppendFencedFrame();
    CreateTestNavigation(url, fenced_frame);
    return fenced_frame;
  }

  void NavigateAndCommitMainFrame(const GURL& url) {
    CreateTestNavigation(url, main_rfh());
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              SimulateStartAndGetResult(navigation_simulator()).action());
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              SimulateCommitAndGetResult(navigation_simulator()).action());
  }

  bool ManagerHasRulesetHandle() {
    return throttle_manager()->ruleset_handle_for_testing();
  }

 protected:
  // content::WebContentsObserver
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsSameDocument()) {
      return;
    }

    // Inject the proper throttles at this time.
    std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
    PageActivationNotificationTiming state =
        ::testing::UnitTest::GetInstance()->current_test_info()->value_param()
            ? GetParam()
            : WILL_PROCESS_RESPONSE;
    throttles.push_back(
        std::make_unique<MockPageActivationThrottle>(navigation_handle, state));

    auto* navigation_throttle_manager =
        ThrottleManager::FromNavigationHandle(*navigation_handle);
    if (navigation_throttle_manager) {
      navigation_throttle_manager->MaybeAppendNavigationThrottles(
          navigation_handle, &throttles);
    }

    created_fp_throttle_for_last_navigation_ = false;
    for (auto& it : throttles) {
      if (strcmp(it->GetNameForLogging(),
                 "FingerprintingProtectionPageActivationThrottle") == 0) {
        created_fp_throttle_for_last_navigation_ = true;
        // continue;
      }
      navigation_handle->RegisterThrottleForTesting(std::move(it));
    }
  }

  ThrottleManager* throttle_manager() {
    return ThrottleManager::FromPage(
        RenderViewHostTestHarness::web_contents()->GetPrimaryPage());
  }

  bool created_fp_throttle_for_current_navigation() const {
    return created_fp_throttle_for_last_navigation_;
  }

  VerifiedRulesetDealer::Handle* dealer_handle() {
    return dealer_handle_.get();
  }

 private:
  FingerprintingProtectionWebContentsHelper* web_contents_helper() {
    return FingerprintingProtectionWebContentsHelper::FromWebContents(
        RenderViewHostTestHarness::web_contents());
  }

  subresource_filter::testing::TestRulesetCreator test_ruleset_creator_;
  subresource_filter::testing::TestRulesetPair test_ruleset_pair_;
  std::unique_ptr<TestSupport> test_support_;

  std::unique_ptr<VerifiedRulesetDealer::Handle> dealer_handle_;

  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;

  bool created_fp_throttle_for_last_navigation_ = false;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Class for tests with fingerprinting protection completely disabled.
class ThrottleManagerDisabledTest : public ThrottleManagerTest {
 public:
  ThrottleManagerDisabledTest() { SetFingerprintingProtectionFlags(false); }

  ThrottleManagerDisabledTest(const ThrottleManagerDisabledTest&) = delete;
  ThrottleManagerDisabledTest& operator=(const ThrottleManagerDisabledTest&) =
      delete;

  ~ThrottleManagerDisabledTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ThrottleManagerDisabledTest,
                         ::testing::Values(WILL_START_REQUEST,
                                           WILL_PROCESS_RESPONSE));

// No subresource loads should be filtered with the feature disabled.
TEST_P(ThrottleManagerDisabledTest,
       ActivateMainFrameAndDoNotFilterSubframeNavigation) {
  // Commit a navigation that would trigger page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));

  // A subframe navigation that would be disallowed should not be filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()).action());
}

// ThrottleManagers should not be created with the feature disabled.
TEST_P(ThrottleManagerDisabledTest, DoNotCreateThrottleManager) {
  auto* initial_throttle_manager =
      ThrottleManager::FromPage(main_rfh()->GetPage());
  EXPECT_FALSE(initial_throttle_manager);

  CreateTestNavigation(GURL(kTestURLWithNoActivation), main_rfh());
  navigation_simulator()->Start();

  auto* throttle_manager_at_start = ThrottleManager::FromNavigationHandle(
      *navigation_simulator()->GetNavigationHandle());
  EXPECT_FALSE(throttle_manager_at_start);
}

// Class for tests with fingerprinting protection completely enabled.
class ThrottleManagerEnabledTest : public ThrottleManagerTest {
 public:
  ThrottleManagerEnabledTest() { SetFingerprintingProtectionFlags(true); }

  ThrottleManagerEnabledTest(const ThrottleManagerEnabledTest&) = delete;
  ThrottleManagerEnabledTest& operator=(const ThrottleManagerEnabledTest&) =
      delete;

  ~ThrottleManagerEnabledTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ThrottleManagerEnabledTest,
                         ::testing::Values(WILL_START_REQUEST,
                                           WILL_PROCESS_RESPONSE));

TEST_P(ThrottleManagerEnabledTest,
       ActivateMainFrameAndFilterSubframeNavigation) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));

  // A disallowed subframe navigation should be successfully filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()).action());
}

TEST_P(ThrottleManagerEnabledTest, NoPageActivation) {
  // Commit a navigation that does not trigger page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithNoActivation));
  EXPECT_TRUE(ManagerHasRulesetHandle());

  // A disallowed subframe navigation should not be filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()).action());
}

// TODO(https://crbug.com/40280666): Dry run mode is not yet implemented and
// should be treated as if activation is enabled normally.
TEST_P(ThrottleManagerEnabledTest, ActivateMainFrameAndFilterDryRun) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()).action());

  // Check that the frame is still activated once communication with blink is
  // implemented.
}

TEST_P(ThrottleManagerEnabledTest,
       ActivateMainFrameAndFilterSubframeNavigationOnRedirect) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));

  // A disallowed subframe navigation via redirect should be successfully
  // filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/before-redirect.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()).action());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateRedirectAndGetResult(
                navigation_simulator(),
                GURL("https://www.example.com/disallowed.html"))
                .action());
}

// This should fail if the throttle manager notifies the delegate twice of a
// disallowed load for the same page load.
TEST_P(ThrottleManagerEnabledTest,
       ActivateMainFrameAndFilterTwoSubframeNavigations) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/1/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()).action());

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/2/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()).action());
}

TEST_P(ThrottleManagerEnabledTest,
       ActivateTwoMainFramesAndFilterTwoSubframeNavigations) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));

  // A disallowed subframe navigation should be successfully filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/1/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()).action());

  // Commit another navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation2));

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/2/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()).action());
}

TEST_P(ThrottleManagerEnabledTest, DoNotFilterForInactiveFrame) {
  NavigateAndCommitMainFrame(GURL("https://do-not-activate.html"));

  // A subframe navigation should complete successfully.
  CreateSubframeWithTestNavigation(GURL("https://www.example.com/allowed.html"),
                                   main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()).action());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()).action());
}

TEST_P(ThrottleManagerEnabledTest, SameSiteNavigation_RulesetIsPreserved) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(
      RenderViewHostTestHarness::web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL same_site_url =
      GURL(base::StringPrintf("%sanother_page.html", kTestURLWithActivation));

  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  EXPECT_TRUE(ManagerHasRulesetHandle());

  NavigateAndCommitMainFrame(same_site_url);
  EXPECT_TRUE(ManagerHasRulesetHandle());

  // A subframe navigation should still be blocked.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()).action());
}

TEST_P(ThrottleManagerEnabledTest,
       SameSiteFailedNavigation_MaintainActivation) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  EXPECT_TRUE(ManagerHasRulesetHandle());

  GURL same_site_inactive_url =
      GURL(base::StringPrintf("%sinactive.html", kTestURLWithActivation));

  CreateTestNavigation(same_site_inactive_url, main_rfh());
  SimulateFailedNavigation(navigation_simulator(), net::ERR_ABORTED);
  EXPECT_TRUE(ManagerHasRulesetHandle());

  // A subframe navigation fails.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()).action());
}

TEST_P(ThrottleManagerEnabledTest, FailedNavigationToErrorPage_NoActivation) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  EXPECT_TRUE(ManagerHasRulesetHandle());

  GURL same_site_inactive_url =
      GURL(base::StringPrintf("%sinactive.html", kTestURLWithActivation));

  CreateTestNavigation(same_site_inactive_url, main_rfh());
  SimulateFailedNavigation(navigation_simulator(), net::ERR_FAILED);

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()).action());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()).action());
}

// Ensure activation propagates into great-grandchild frames, including cross
// process ones.
TEST_P(ThrottleManagerEnabledTest, ActivationPropagation) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));

  // Navigate a subframe to a URL that is not itself disallowed. Subresource
  // filtering for this subframe document should still be activated.
  CreateSubframeWithTestNavigation(GURL("https://www.a.com/allowed.html"),
                                   main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()).action());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()).action());

  // Navigate a sub-subframe to a URL that is not itself disallowed. Subresource
  // filtering for this subframe document should still be activated.
  content::RenderFrameHost* subframe1 =
      navigation_simulator()->GetFinalRenderFrameHost();
  CreateSubframeWithTestNavigation(GURL("https://www.b.com/allowed.html"),
                                   subframe1);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()).action());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()).action());

  content::RenderFrameHost* subframe2 =
      navigation_simulator()->GetFinalRenderFrameHost();
  CreateSubframeWithTestNavigation(GURL("https://www.c.com/disallowed.html"),
                                   subframe2);
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()).action());
}

// Same-site navigations within a single RFH should persist activation.
TEST_P(ThrottleManagerEnabledTest, SameSiteNavigationStopsActivation) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));

  // Mock a same-site navigation, in the same RFH.
  NavigateAndCommitMainFrame(
      GURL(base::StringPrintf("%s/some_path/", kTestURLWithActivation)));

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()).action());
}

TEST_P(ThrottleManagerEnabledTest, CreateHelperForWebContents) {
  auto web_contents =
      content::RenderViewHostTestHarness::CreateTestWebContents();
  ASSERT_EQ(FingerprintingProtectionWebContentsHelper::FromWebContents(
                web_contents.get()),
            nullptr);

  TestSupport test_support;
  privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings =
      test_support.tracking_protection_settings();

  {
    base::test::ScopedFeatureList scoped_feature;
    scoped_feature.InitAndDisableFeature(
        features::kEnableFingerprintingProtectionFilter);

    // CreateForWebContents() should not do anything if the fingerprinting
    // protection filter feature is not enabled.
    FingerprintingProtectionWebContentsHelper::CreateForWebContents(
        web_contents.get(), test_support.prefs(), tracking_protection_settings,
        dealer_handle());
    EXPECT_EQ(FingerprintingProtectionWebContentsHelper::FromWebContents(
                  web_contents.get()),
              nullptr);
  }

  // If the fingerprinting protection filter feature is enabled,\
  // CreateForWebContents() should create and attach an instance.
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      web_contents.get(), test_support.prefs(), tracking_protection_settings,
      dealer_handle());
  auto* helper = FingerprintingProtectionWebContentsHelper::FromWebContents(
      web_contents.get());
  EXPECT_NE(helper, nullptr);

  // A second call should not attach a different instance.
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      web_contents.get(), test_support.prefs(), tracking_protection_settings,
      dealer_handle());
  EXPECT_EQ(FingerprintingProtectionWebContentsHelper::FromWebContents(
                web_contents.get()),
            helper);
}

// Basic test of throttle manager lifetime and getter methods. Ensure a new
// page creating navigation creates a new throttle manager and it's reachable
// using FromNavigationHandle until commit time. Once committed that same
// throttle manager should now be associated with the new page.
TEST_P(ThrottleManagerEnabledTest, ThrottleManagerLifetime_Basic) {
  auto* initial_throttle_manager =
      ThrottleManager::FromPage(main_rfh()->GetPage());
  EXPECT_TRUE(initial_throttle_manager);

  CreateTestNavigation(GURL(kTestURLWithNoActivation), main_rfh());
  navigation_simulator()->Start();

  auto* throttle_manager_at_start = ThrottleManager::FromNavigationHandle(
      *navigation_simulator()->GetNavigationHandle());

  // Starting a main-frame, cross-document navigation creates a new throttle
  // manager but doesn't replace the one on the current page yet.
  EXPECT_TRUE(throttle_manager_at_start);
  EXPECT_NE(throttle_manager_at_start, initial_throttle_manager);
  EXPECT_EQ(ThrottleManager::FromPage(main_rfh()->GetPage()),
            initial_throttle_manager);

  navigation_simulator()->ReadyToCommit();

  EXPECT_EQ(ThrottleManager::FromNavigationHandle(
                *navigation_simulator()->GetNavigationHandle()),
            throttle_manager_at_start);
  EXPECT_EQ(ThrottleManager::FromPage(main_rfh()->GetPage()),
            initial_throttle_manager);

  navigation_simulator()->Commit();

  // Now that the navigation committed, it should be associated with the current
  // page.
  EXPECT_FALSE(navigation_simulator()->GetNavigationHandle());
  ASSERT_EQ(main_rfh()->GetLastCommittedURL(), kTestURLWithNoActivation);
  EXPECT_EQ(ThrottleManager::FromPage(main_rfh()->GetPage()),
            throttle_manager_at_start);

  // A new navigation creates a new throttle manager.
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  EXPECT_NE(ThrottleManager::FromPage(main_rfh()->GetPage()),
            throttle_manager_at_start);
}

// Ensure subframe navigations do not create a new throttle manager and
// FromNavigation gets the correct one.
TEST_P(ThrottleManagerEnabledTest, ThrottleManagerLifetime_Subframe) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));

  auto* throttle_manager = ThrottleManager::FromPage(main_rfh()->GetPage());
  ASSERT_TRUE(throttle_manager);

  CreateSubframeWithTestNavigation(GURL("https://www.example.com/allowed.html"),
                                   main_rfh());
  navigation_simulator()->Start();

  // Using FromNavigation on a subframe navigation should retrieve the throttle
  // manager from the current Page.
  EXPECT_EQ(ThrottleManager::FromNavigationHandle(
                *navigation_simulator()->GetNavigationHandle()),
            throttle_manager);

  navigation_simulator()->Commit();

  // Committing the subframe navigation should not change the Page's throttle
  // manager.
  EXPECT_EQ(ThrottleManager::FromPage(main_rfh()->GetPage()), throttle_manager);
}

TEST_P(ThrottleManagerEnabledTest,
       ThrottleManagerLifetime_DidFinishInFrameNavigationSucceeds) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));

  auto* throttle_manager = ThrottleManager::FromPage(main_rfh()->GetPage());
  ASSERT_TRUE(throttle_manager);

  CreateSubframeWithTestNavigation(GURL("https://www.example.com/download"),
                                   main_rfh());
  navigation_simulator()->Start();

  // Test that `DidFinishInFrameNavigation` does not crash when an uncommitted
  // navigation is not the initial navigation.
  throttle_manager->DidFinishInFrameNavigation(
      navigation_simulator()->GetNavigationHandle(),
      /*is_initial_navigation=*/false);
}

// Same document navigations are similar to subframes: do not create a new
// throttle manager and FromNavigation gets the existing one.
TEST_P(ThrottleManagerEnabledTest, ThrottleManagerLifetime_SameDocument) {
  const GURL kUrl = GURL(kTestURLWithDryRun);
  const GURL kSameDocumentUrl =
      GURL(base::StringPrintf("%s#ref", kTestURLWithDryRun));

  NavigateAndCommitMainFrame(kUrl);

  auto* throttle_manager = ThrottleManager::FromPage(main_rfh()->GetPage());
  ASSERT_TRUE(throttle_manager);

  CreateTestNavigation(kSameDocumentUrl, main_rfh());
  navigation_simulator()->CommitSameDocument();

  // Committing the same-document navigation should not change the Page's
  // throttle manager.
  EXPECT_EQ(ThrottleManager::FromPage(main_rfh()->GetPage()), throttle_manager);
}

TEST_P(ThrottleManagerEnabledTest,
       ActivateMainFrameAndFilterFencedFrameNavigation) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));

  // A disallowed fenced frame navigation should be successfully filtered.
  CreateFencedFrameWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()).action());
}

TEST_P(ThrottleManagerEnabledTest,
       ActivateMainFrameAndFilterFencedFrameNavigationOnRedirect) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));

  // A disallowed subframe navigation via redirect should be successfully
  // filtered.
  CreateFencedFrameWithTestNavigation(
      GURL("https://www.example.com/before-redirect.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()).action());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateRedirectAndGetResult(
                navigation_simulator(),
                GURL("https://www.example.com/disallowed.html"))
                .action());
}

// Ensure activation propagates into great-grandchild fenced frames, including
// cross process ones.
TEST_P(ThrottleManagerEnabledTest, ActivationPropagation_FencedFrame) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));

  // Navigate a fenced frame to a URL that is not itself disallowed. Subresource
  // filtering for this fenced frame document should still be activated.
  CreateFencedFrameWithTestNavigation(GURL("https://www.a.com/allowed.html"),
                                      main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()).action());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()).action());

  // Navigate a nested fenced frame to a URL that is not itself disallowed.
  // Subresource filtering for this fenced frame document should still be
  // activated.
  content::RenderFrameHost* fenced_frame1 =
      navigation_simulator()->GetFinalRenderFrameHost();
  CreateFencedFrameWithTestNavigation(GURL("https://www.b.com/allowed.html"),
                                      fenced_frame1);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()).action());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()).action());

  // A final, nested fenced frame navigation is filtered.
  content::RenderFrameHost* fenced_frame2 =
      navigation_simulator()->GetFinalRenderFrameHost();
  CreateFencedFrameWithTestNavigation(GURL("https://www.c.com/disallowed.html"),
                                      fenced_frame2);
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()).action());

  // A subframe navigation inside the nested fenced frame is filtered.
  CreateSubframeWithTestNavigation(GURL("https://www.c.com/disallowed.html"),
                                   fenced_frame2);
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()).action());
}

TEST_P(ThrottleManagerEnabledTest, SafeBrowsingThrottleCreation) {
  // The throttle should be created on a main frame navigation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithNoActivation));
  EXPECT_TRUE(created_fp_throttle_for_current_navigation());

  // However, it should not be created on a subframe navigation.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()).action());

  EXPECT_FALSE(created_fp_throttle_for_current_navigation());

  // It should also not be created on a fenced frame navigation.
  CreateFencedFrameWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()).action());

  EXPECT_FALSE(created_fp_throttle_for_current_navigation());
}

// Ensure fenced frame navigations do not create a new throttle manager and
// FromNavigation gets the correct one.
TEST_P(ThrottleManagerEnabledTest, ThrottleManagerLifetime_FencedFrame) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));

  auto* throttle_manager = ThrottleManager::FromPage(main_rfh()->GetPage());
  ASSERT_TRUE(throttle_manager);

  content::RenderFrameHost* fenced_frame_root =
      CreateFencedFrameWithTestNavigation(
          GURL("https://www.example.com/allowed.html"), main_rfh());
  EXPECT_TRUE(fenced_frame_root);
  navigation_simulator()->Start();

  // Using FromNavigation on a fenced frame navigation should retrieve the
  // throttle manager from the initial (outer-most) Page.
  EXPECT_EQ(ThrottleManager::FromNavigationHandle(
                *navigation_simulator()->GetNavigationHandle()),
            throttle_manager);

  navigation_simulator()->Commit();
  fenced_frame_root = navigation_simulator()->GetFinalRenderFrameHost();

  // Committing the fenced frame navigation should not change the Page's
  // throttle manager.
  EXPECT_EQ(ThrottleManager::FromPage(main_rfh()->GetPage()), throttle_manager);

  // The throttle manager on the fenced frame page should be the outer-most
  // Page's throttle manager.
  EXPECT_EQ(ThrottleManager::FromPage(fenced_frame_root->GetPage()),
            throttle_manager);
}

}  // namespace fingerprinting_protection_filter
