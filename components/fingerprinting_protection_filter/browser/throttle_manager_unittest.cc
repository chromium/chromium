// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/throttle_manager.h"

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/fingerprinting_protection_filter/browser/test_support.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/subresource_filter/content/shared/browser/child_frame_navigation_test_utils.h"
#include "components/subresource_filter/content/shared/browser/utils.h"
#include "components/subresource_filter/core/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/ukm/test_ukm_recorder.h"
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
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {
enum class ActivationDecision;
}

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

class FakeRendererAgent {
 public:
  explicit FakeRendererAgent(content::WebContents* web_contents) {
    ThrottleManager::BindReceiver(
        remote_.BindNewEndpointAndPassDedicatedReceiver(),
        &web_contents->GetPrimaryPage().GetMainDocument());
    RequestActivation();
  }

  std::optional<bool> LastActivated() {
    if (!last_activation_) {
      return std::nullopt;
    }
    bool activated = last_activation_->activation_level !=
                     subresource_filter::mojom::ActivationLevel::kDisabled;
    return activated;
  }

 private:
  void RequestActivation() {
    remote_->CheckActivation(base::BindOnce(
        &FakeRendererAgent::OnActivationComputed, base::Unretained(this)));
  }

  void OnActivationComputed(
      subresource_filter::mojom::ActivationStatePtr activation_state) {
    last_activation_ = std::move(activation_state);
  }

  mojo::AssociatedRemote<mojom::FingerprintingProtectionHost> remote_;
  subresource_filter::mojom::ActivationStatePtr last_activation_;
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

    subresource_filter::mojom::ActivationState disabled_state;
    disabled_state.activation_level =
        subresource_filter::mojom::ActivationLevel::kDisabled;

    mock_page_activations_[GURL(kTestURLWithActivation)] = enabled_state;
    mock_page_activations_[GURL(kTestURLWithActivation2)] = enabled_state;
    mock_page_activations_[GURL(kTestURLWithDryRun)] = dry_run_state;
    mock_page_activations_[GURL(kTestURLWithNoActivation)] = disabled_state;
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
      auto* web_contents_helper =
          navigation_handle()->GetWebContents()
              ? FingerprintingProtectionWebContentsHelper::FromWebContents(
                    navigation_handle()->GetWebContents())
              : nullptr;
      if (subresource_filter::IsInSubresourceFilterRoot(navigation_handle()) &&
          web_contents_helper) {
        if (it != mock_page_activations_.end()) {
          web_contents_helper->NotifyPageActivationComputed(
              navigation_handle(), it->second,
              subresource_filter::ActivationDecision::ACTIVATED);
        } else {
          web_contents_helper->NotifyPageActivationComputed(
              navigation_handle(), subresource_filter::mojom::ActivationState(),
              subresource_filter::ActivationDecision::ACTIVATED);
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
    CreateAgentForHost(web_contents->GetPrimaryMainFrame());

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
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        kFingerprintingProtectionRulesetConfig);
    dealer_handle_->TryOpenAndSetRulesetFile(test_ruleset_pair_.indexed.path,
                                             /*expected_checksum=*/0,
                                             base::DoNothing());

    test_support_ = std::make_unique<TestSupport>();

    FingerprintingProtectionWebContentsHelper::CreateForWebContents(
        web_contents, test_support_->prefs(),
        test_support_->tracking_protection_settings(), dealer_handle_.get(),
        /*is_incognito=*/false);

    Observe(web_contents);

    NavigateAndCommit(GURL("https://example.first"));
  }

  void TearDown() override {
    test_support_.reset();
    dealer_handle_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  void ExpectActivationSignalForFrame(
      content::RenderFrameHost* rfh,
      bool expect_activation,
      bool expect_activation_sent_to_agent = true) {
    FakeRendererAgent* agent = agent_map_[rfh].get();
    EXPECT_TRUE(base::test::RunUntil([expect_activation, agent]() {
      return expect_activation ==
             (agent->LastActivated() && *agent->LastActivated());
    }));
    EXPECT_TRUE(
        base::test::RunUntil([expect_activation_sent_to_agent, agent]() {
          return expect_activation_sent_to_agent ==
                 agent->LastActivated().has_value();
        }));
  }

  // Helper methods:

  void SetFingerprintingProtectionFlags(bool is_enabled,
                                        bool is_dry_run = false) {
    if (is_enabled && !is_dry_run) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{features::kEnableFingerprintingProtectionFilter,
            {{"activation_level", "enabled"}}}},
          /*disabled_features=*/{});
    } else if (is_enabled && is_dry_run) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{features::kEnableFingerprintingProtectionFilter,
            {{"activation_level", "dry_run"}}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{},
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
  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* new_host) override {
    CreateAgentForHost(new_host);
  }

  void RenderFrameDeleted(content::RenderFrameHost* host) override {
    agent_map_.erase(host);
  }

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsSameDocument()) {
      return;
    }

    // Inject the proper throttles.
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
    for (size_t i = 0; i < throttles.size(); i++) {
      if (strcmp(throttles[i]->GetNameForLogging(),
                 kPageActivationThrottleNameForLogging) == 0) {
        created_fp_throttle_for_last_navigation_ = true;
        // Delete the prod activation throttle so it doesn't interfere with
        // tests.
        throttles.erase(throttles.begin() + i);
        i--;
        continue;
      }
      navigation_handle->RegisterThrottleForTesting(std::move(throttles[i]));
    }
  }

  void CreateAgentForHost(content::RenderFrameHost* host) {
    auto new_agent = std::make_unique<FakeRendererAgent>(
        RenderViewHostTestHarness::web_contents());
    agent_map_[host] = std::move(new_agent);
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

  std::map<content::RenderFrameHost*, std::unique_ptr<FakeRendererAgent>>
      agent_map_;

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
  // Set up test ukm recorder.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  CreateAgentForHost(main_rfh());
  ExpectActivationSignalForFrame(main_rfh(), /*expect_activation=*/true);

  // A disallowed subframe navigation should be successfully filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()).action());

  // Check test ukm recorder contains event with expected metrics.
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::FingerprintingProtection::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    test_ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::FingerprintingProtection::kActivationDecisionName,
        static_cast<int64_t>(
            subresource_filter::ActivationDecision::ACTIVATED));
    EXPECT_FALSE(test_ukm_recorder.EntryHasMetric(
        entry, ukm::builders::FingerprintingProtection::kDryRunName));
  }
}

TEST_P(ThrottleManagerEnabledTest, NoPageActivation) {
  // Set up test ukm recorder.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Commit a navigation that does not trigger page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithNoActivation));
  CreateAgentForHost(main_rfh());
  ExpectActivationSignalForFrame(main_rfh(), /*expect_activation=*/false);
  EXPECT_TRUE(ManagerHasRulesetHandle());

  // A disallowed subframe navigation should not be filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()).action());

  EXPECT_EQ(0u, test_ukm_recorder
                    .GetEntriesByName(
                        ukm::builders::FingerprintingProtection::kEntryName)
                    .size());
}

TEST_P(ThrottleManagerEnabledTest, ActivateMainFrameAndDoNotFilterDryRun) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  CreateAgentForHost(main_rfh());
  ExpectActivationSignalForFrame(main_rfh(), /*expect_activation=*/true);

  // Child frames should not be filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* child =
      navigation_simulator()->GetFinalRenderFrameHost();
  // But they should still be activated.
  ExpectActivationSignalForFrame(child, /*expect_activation=*/true);
}

TEST_P(ThrottleManagerEnabledTest,
       ActivateMainFrameAndFilterSubframeNavigationOnRedirect) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  CreateAgentForHost(main_rfh());
  ExpectActivationSignalForFrame(main_rfh(), /*expect_activation=*/true);

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

TEST_P(ThrottleManagerEnabledTest,
       ActivateMainFrameAndDoNotFilterSubframeNavigation) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  CreateAgentForHost(main_rfh());
  ExpectActivationSignalForFrame(main_rfh(), /*expect_activation=*/true);

  // An allowed subframe navigation should complete successfully.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/allowed1.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateRedirectAndGetResult(
                navigation_simulator(),
                GURL("https://www.example.com/allowed2.html")));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* child =
      navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(child, /*expect_activation=*/true);
}

// This should fail if the throttle manager notifies the delegate twice of a
// disallowed load for the same page load.
TEST_P(ThrottleManagerEnabledTest,
       ActivateMainFrameAndFilterTwoSubframeNavigations) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  CreateAgentForHost(main_rfh());
  ExpectActivationSignalForFrame(main_rfh(), /*expect_activation=*/true);

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
  CreateAgentForHost(main_rfh());
  ExpectActivationSignalForFrame(main_rfh(), /*expect_activation=*/true);

  // A disallowed subframe navigation should be successfully filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/1/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()).action());

  // Commit another navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation2));
  CreateAgentForHost(main_rfh());
  ExpectActivationSignalForFrame(main_rfh(), /*expect_activation=*/true);

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/2/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()).action());
}

TEST_P(ThrottleManagerEnabledTest, DoNotFilterForInactiveFrame) {
  NavigateAndCommitMainFrame(GURL("https://do-not-activate.html"));
  CreateAgentForHost(main_rfh());
  ExpectActivationSignalForFrame(main_rfh(), /*expect_activation=*/false);

  // A subframe navigation should complete successfully.
  CreateSubframeWithTestNavigation(GURL("https://www.example.com/allowed.html"),
                                   main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()).action());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()).action());
  content::RenderFrameHost* child =
      navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(child, /*expect_activation=*/false);
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
  CreateAgentForHost(main_rfh());
  ExpectActivationSignalForFrame(main_rfh(), /*expect_activation=*/true);
  EXPECT_TRUE(ManagerHasRulesetHandle());

  NavigateAndCommitMainFrame(same_site_url);
  EXPECT_TRUE(ManagerHasRulesetHandle());

  // A subframe navigation should not be blocked because we are not activated
  // on the current main frame.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()).action());
}

TEST_P(ThrottleManagerEnabledTest,
       SameSiteFailedNavigation_MaintainActivation) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  CreateAgentForHost(main_rfh());
  ExpectActivationSignalForFrame(main_rfh(), /*expect_activation=*/true);
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

// Ensure activation propagates into great-grandchild frames, including cross
// process ones.
TEST_P(ThrottleManagerEnabledTest, ActivationPropagation) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  CreateAgentForHost(main_rfh());
  ExpectActivationSignalForFrame(main_rfh(), /*expect_activation=*/true);

  // Navigate a subframe to a URL that is not itself disallowed. Subresource
  // filtering for this subframe document should still be activated.
  CreateSubframeWithTestNavigation(GURL("https://www.a.com/allowed.html"),
                                   main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* subframe1 =
      navigation_simulator()->GetFinalRenderFrameHost();
  CreateAgentForHost(subframe1);
  ExpectActivationSignalForFrame(subframe1, /*expect_activation=*/true);

  // Navigate a sub-subframe to a URL that is not itself disallowed. Subresource
  // filtering for this subframe document should still be activated.
  CreateSubframeWithTestNavigation(GURL("https://www.b.com/allowed.html"),
                                   subframe1);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* subframe2 =
      navigation_simulator()->GetFinalRenderFrameHost();
  CreateAgentForHost(subframe2);
  ExpectActivationSignalForFrame(subframe2, /*expect_activation=*/true);

  // A final, nested subframe navigation is filtered.
  CreateSubframeWithTestNavigation(GURL("https://www.c.com/disallowed.html"),
                                   subframe2);
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));
}

// Same-site navigations within a single RFH should stop activation.
TEST_P(ThrottleManagerEnabledTest, SameSiteNavigationStopsActivation) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(
      RenderViewHostTestHarness::web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  CreateAgentForHost(main_rfh());
  ExpectActivationSignalForFrame(main_rfh(), /*expect_activation=*/true);

  NavigateAndCommitMainFrame(
      GURL(base::StringPrintf("%s/some_path/", kTestURLWithActivation)));

  // A navigation to a URL on the blocklist should be allowed to proceed - i.e.
  // no activation and the blocklist isn't checked.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
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
        dealer_handle(), /*is_incognito=*/false);
    EXPECT_EQ(FingerprintingProtectionWebContentsHelper::FromWebContents(
                  web_contents.get()),
              nullptr);
  }

  // If the fingerprinting protection filter feature is enabled,
  // CreateForWebContents() should create and attach an instance.
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      web_contents.get(), test_support.prefs(), tracking_protection_settings,
      dealer_handle(), /*is_incognito=*/false);
  auto* helper = FingerprintingProtectionWebContentsHelper::FromWebContents(
      web_contents.get());
  EXPECT_NE(helper, nullptr);

  // A second call should not attach a different instance.
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      web_contents.get(), test_support.prefs(), tracking_protection_settings,
      dealer_handle(), /*is_incognito=*/false);
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
  ASSERT_EQ(main_rfh()->GetLastCommittedURL(), GURL(kTestURLWithNoActivation));
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
  CreateAgentForHost(main_rfh());
  ExpectActivationSignalForFrame(main_rfh(), /*expect_activation=*/true);

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
  CreateAgentForHost(main_rfh());
  ExpectActivationSignalForFrame(main_rfh(), /*expect_activation=*/true);

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
  CreateAgentForHost(main_rfh());
  ExpectActivationSignalForFrame(main_rfh(), /*expect_activation=*/true);

  // Navigate a fenced frame to a URL that is not itself disallowed. Subresource
  // filtering for this fenced frame document should still be activated.
  CreateFencedFrameWithTestNavigation(GURL("https://www.a.com/allowed.html"),
                                      main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()).action());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()).action());
  content::RenderFrameHost* fenced_frame1 =
      navigation_simulator()->GetFinalRenderFrameHost();
  CreateAgentForHost(fenced_frame1);
  ExpectActivationSignalForFrame(fenced_frame1, /*expect_activation=*/true);

  // Navigate a nested fenced frame to a URL that is not itself disallowed.
  // Subresource filtering for this fenced frame document should still be
  // activated.
  CreateFencedFrameWithTestNavigation(GURL("https://www.b.com/allowed.html"),
                                      fenced_frame1);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()).action());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()).action());
  content::RenderFrameHost* fenced_frame2 =
      navigation_simulator()->GetFinalRenderFrameHost();
  CreateAgentForHost(fenced_frame2);
  ExpectActivationSignalForFrame(fenced_frame2, /*expect_activation=*/true);

  // A final, nested fenced frame navigation is filtered.
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

TEST_P(ThrottleManagerEnabledTest, ActivationThrottleCreation) {
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
