// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"

#include <map>
#include <memory>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/mojom/subresource_filter.mojom.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/url_constants.h"

namespace subresource_filter {

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

class FakeSubresourceFilterAgent : public mojom::SubresourceFilterAgent {
 public:
  FakeSubresourceFilterAgent() : binding_(this) {}
  ~FakeSubresourceFilterAgent() override = default;

  void OnSubresourceFilterAgentRequest(
      mojo::ScopedInterfaceEndpointHandle handle) {
    binding_.Bind(
        mojo::AssociatedInterfaceRequest<mojom::SubresourceFilterAgent>(
            std::move(handle)));
  }

  // mojom::SubresourceFilterAgent:
  void ActivateForNextCommittedLoad(mojom::ActivationStatePtr activation_state,
                                    bool is_ad_subframe) override {
    last_activation_ = std::move(activation_state);
    is_ad_subframe_ = is_ad_subframe;
  }

  // These methods reset state back to default when they are called.
  bool LastAdSubframe() {
    bool is_ad_subframe = is_ad_subframe_;
    is_ad_subframe_ = false;
    return is_ad_subframe;
  }
  bool LastActivated() {
    bool activated = last_activation_ && last_activation_->activation_level !=
                                             mojom::ActivationLevel::kDisabled;
    last_activation_.reset();
    return activated;
  }

 private:
  mojom::ActivationStatePtr last_activation_;
  bool is_ad_subframe_ = false;
  mojo::AssociatedBinding<mojom::SubresourceFilterAgent> binding_;
};

// Simple throttle that sends page-level activation to the manager for a
// specific set of URLs.
class MockPageStateActivationThrottle : public content::NavigationThrottle {
 public:
  MockPageStateActivationThrottle(
      content::NavigationHandle* navigation_handle,
      PageActivationNotificationTiming activation_throttle_state)
      : content::NavigationThrottle(navigation_handle),
        activation_throttle_state_(activation_throttle_state) {
    // Add some default activations.
    mojom::ActivationState enabled_state;
    enabled_state.activation_level = mojom::ActivationLevel::kEnabled;

    mojom::ActivationState dry_run_state;
    dry_run_state.activation_level = mojom::ActivationLevel::kDryRun;

    mock_page_activations_[GURL(kTestURLWithActivation)] = enabled_state;
    mock_page_activations_[GURL(kTestURLWithActivation2)] = enabled_state;
    mock_page_activations_[GURL(kTestURLWithDryRun)] = dry_run_state;
    mock_page_activations_[GURL(kTestURLWithNoActivation)] =
        mojom::ActivationState();
  }
  ~MockPageStateActivationThrottle() override {}

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    return MaybeNotifyActivation(WILL_START_REQUEST);
  }

  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override {
    return MaybeNotifyActivation(WILL_PROCESS_RESPONSE);
  }
  const char* GetNameForLogging() override {
    return "MockPageStateActivationThrottle";
  }

 private:
  content::NavigationThrottle::ThrottleCheckResult MaybeNotifyActivation(
      PageActivationNotificationTiming throttle_state) {
    if (throttle_state == activation_throttle_state_) {
      auto it = mock_page_activations_.find(navigation_handle()->GetURL());
      if (it != mock_page_activations_.end()) {
        // The throttle manager does not use the activation decision.
        SubresourceFilterObserverManager::FromWebContents(
            navigation_handle()->GetWebContents())
            ->NotifyPageActivationComputed(navigation_handle(), it->second);
      }
    }
    return content::NavigationThrottle::PROCEED;
  }

  std::map<GURL, mojom::ActivationState> mock_page_activations_;
  PageActivationNotificationTiming activation_throttle_state_;

  DISALLOW_COPY_AND_ASSIGN(MockPageStateActivationThrottle);
};

class ContentSubresourceFilterThrottleManagerTest
    : public content::RenderViewHostTestHarness,
      public content::WebContentsObserver,
      public SubresourceFilterClient,
      public ::testing::WithParamInterface<PageActivationNotificationTiming> {
 public:
  ContentSubresourceFilterThrottleManagerTest() {}
  ~ContentSubresourceFilterThrottleManagerTest() override {}

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    content::WebContents* web_contents =
        RenderViewHostTestHarness::web_contents();
    CreateAgentForHost(web_contents->GetMainFrame());

    NavigateAndCommit(GURL("https://example.first"));

    // Initialize the ruleset dealer.
    std::vector<proto::UrlRule> rules;
    rules.push_back(testing::CreateWhitelistRuleForDocument(
        "whitelist.com", proto::ACTIVATION_TYPE_DOCUMENT,
        {"page-with-activation.com"}));
    rules.push_back(testing::CreateSuffixRule("disallowed.html"));
    ASSERT_NO_FATAL_FAILURE(test_ruleset_creator_.CreateRulesetWithRules(
        rules, &test_ruleset_pair_));

    // Make the blocking task runner run on the current task runner for the
    // tests, to ensure that the NavigationSimulator properly runs all necessary
    // tasks while waiting for throttle checks to finish.
    dealer_handle_ = std::make_unique<VerifiedRulesetDealer::Handle>(
        base::MessageLoopCurrent::Get()->task_runner());
    dealer_handle_->TryOpenAndSetRulesetFile(test_ruleset_pair_.indexed.path,
                                             /*expected_checksum=*/0,
                                             base::DoNothing());

    throttle_manager_ =
        std::make_unique<ContentSubresourceFilterThrottleManager>(
            this, dealer_handle_.get(), web_contents);
    Observe(web_contents);
  }

  void TearDown() override {
    throttle_manager_.reset();
    dealer_handle_.reset();
    base::RunLoop().RunUntilIdle();
    content::RenderViewHostTestHarness::TearDown();
  }

  void ExpectActivationSignalForFrame(content::RenderFrameHost* rfh,
                                      bool expect_activation,
                                      bool expect_is_ad_subframe = false) {
    // In some cases we need to verify that messages were _not_ sent, in which
    // case using a Wait() idiom would cause hangs. RunUntilIdle instead to
    // ensure mojo calls make it to the fake agent.
    base::RunLoop().RunUntilIdle();
    FakeSubresourceFilterAgent* agent = agent_map_[rfh].get();
    EXPECT_EQ(expect_activation, agent->LastActivated());
    EXPECT_EQ(expect_is_ad_subframe, agent->LastAdSubframe());
  }

  // Helper methods:

  void CreateTestNavigation(const GURL& url,
                            content::RenderFrameHost* render_frame_host) {
    DCHECK(!navigation_simulator_);
    DCHECK(render_frame_host);
    navigation_simulator_ =
        content::NavigationSimulator::CreateRendererInitiated(
            url, render_frame_host);
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

  void SimulateStartAndExpectResult(
      content::NavigationThrottle::ThrottleAction expect_result) {
    navigation_simulator_->Start();
    content::NavigationThrottle::ThrottleCheckResult result =
        navigation_simulator_->GetLastThrottleCheckResult();
    EXPECT_EQ(expect_result, result);
    if (result.action() != content::NavigationThrottle::PROCEED)
      navigation_simulator_.reset();
  }

  void SimulateRedirectAndExpectResult(
      const GURL& new_url,
      content::NavigationThrottle::ThrottleAction expect_result) {
    navigation_simulator_->Redirect(new_url);
    content::NavigationThrottle::ThrottleCheckResult result =
        navigation_simulator_->GetLastThrottleCheckResult();
    EXPECT_EQ(expect_result, result);
    if (result.action() != content::NavigationThrottle::PROCEED)
      navigation_simulator_.reset();
  }

  // Returns the RenderFrameHost that the navigation commit in.
  content::RenderFrameHost* SimulateCommitAndExpectResult(
      content::NavigationThrottle::ThrottleAction expect_result) {
    navigation_simulator_->Commit();
    content::NavigationThrottle::ThrottleCheckResult result =
        navigation_simulator_->GetLastThrottleCheckResult();
    EXPECT_EQ(expect_result, result);

    auto scoped_simulator = std::move(navigation_simulator_);
    if (result.action() == content::NavigationThrottle::PROCEED)
      return scoped_simulator->GetFinalRenderFrameHost();
    return nullptr;
  }

  void SimulateSameDocumentCommit() {
    navigation_simulator_->CommitSameDocument();
    navigation_simulator_.reset();
  }

  void SimulateFailedNavigation(net::Error error) {
    navigation_simulator_->Fail(error);
    if (error != net::ERR_ABORTED) {
      navigation_simulator_->CommitErrorPage();
    }
    navigation_simulator_.reset();
  }

  void NavigateAndCommitMainFrame(const GURL& url) {
    CreateTestNavigation(url, main_rfh());
    SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
    SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  }

  bool ManagerHasRulesetHandle() {
    return throttle_manager_->ruleset_handle_for_testing();
  }

  int disallowed_notification_count() { return disallowed_notification_count_; }

 protected:
  // content::WebContentsObserver
  void RenderFrameCreated(content::RenderFrameHost* new_host) override {
    CreateAgentForHost(new_host);
  }

  void FrameDeleted(content::RenderFrameHost* host) override {
    agent_map_.erase(host);
  }

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsSameDocument())
      return;

    // Inject the proper throttles at this time.
    std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
    PageActivationNotificationTiming state =
        ::testing::UnitTest::GetInstance()->current_test_info()->value_param()
            ? GetParam()
            : WILL_PROCESS_RESPONSE;
    throttles.push_back(std::make_unique<MockPageStateActivationThrottle>(
        navigation_handle, state));
    throttle_manager_->MaybeAppendNavigationThrottles(navigation_handle,
                                                      &throttles);
    for (auto& it : throttles) {
      navigation_handle->RegisterThrottleForTesting(std::move(it));
    }
  }

  void CreateAgentForHost(content::RenderFrameHost* host) {
    auto new_agent = std::make_unique<FakeSubresourceFilterAgent>();
    host->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
        mojom::SubresourceFilterAgent::Name_,
        base::BindRepeating(
            &FakeSubresourceFilterAgent::OnSubresourceFilterAgentRequest,
            base::Unretained(new_agent.get())));
    agent_map_[host] = std::move(new_agent);
  }

  // SubresourceFilterClient:
  void ShowNotification() override { ++disallowed_notification_count_; }
  mojom::ActivationLevel OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      mojom::ActivationLevel effective_activation_level,
      ActivationDecision* decision) override {
    return effective_activation_level;
  }

  ContentSubresourceFilterThrottleManager* throttle_manager() {
    return throttle_manager_.get();
  }

 private:
  testing::TestRulesetCreator test_ruleset_creator_;
  testing::TestRulesetPair test_ruleset_pair_;

  std::unique_ptr<VerifiedRulesetDealer::Handle> dealer_handle_;

  std::unique_ptr<ContentSubresourceFilterThrottleManager> throttle_manager_;

  std::map<content::RenderFrameHost*,
           std::unique_ptr<FakeSubresourceFilterAgent>>
      agent_map_;

  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;

  // Incremented on every OnFirstSubresourceLoadDisallowed call.
  int disallowed_notification_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterThrottleManagerTest);
};

INSTANTIATE_TEST_CASE_P(,
                        ContentSubresourceFilterThrottleManagerTest,
                        ::testing::Values(WILL_START_REQUEST,
                                          WILL_PROCESS_RESPONSE));

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ActivateMainFrameAndFilterSubframeNavigation) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation should be successfully filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  SimulateStartAndExpectResult(
      content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);

  EXPECT_EQ(1, disallowed_notification_count());
}

TEST_P(ContentSubresourceFilterThrottleManagerTest, NoPageActivation) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithNoActivation));
  ExpectActivationSignalForFrame(main_rfh(), false /* expect_activation */);
  EXPECT_FALSE(ManagerHasRulesetHandle());

  // A disallowed subframe navigation should not be filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);

  EXPECT_EQ(0, disallowed_notification_count());
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ActivateMainFrameAndDoNotFilterDryRun) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation should not be filtered in dry-run mode.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* child =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  // But it should still be activated.
  ExpectActivationSignalForFrame(child, true /* expect_activation */,
                                 true /* is_ad_subframe */);

  EXPECT_EQ(0, disallowed_notification_count());
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ActivateMainFrameAndFilterSubframeNavigationOnRedirect) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation via redirect should be successfully
  // filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/before-redirect.html"), main_rfh());
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::NavigationThrottle::ThrottleAction expected_action =
      content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE;
  SimulateRedirectAndExpectResult(
      GURL("https://www.example.com/disallowed.html"), expected_action);

  EXPECT_EQ(1, disallowed_notification_count());
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ActivateMainFrameAndDoNotFilterSubframeNavigation) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // An allowed subframe navigation should complete successfully.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/allowed1.html"), main_rfh());
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  SimulateRedirectAndExpectResult(GURL("https://www.example.com/allowed2.html"),
                                  content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* child =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  ExpectActivationSignalForFrame(child, true /* expect_activation */);

  EXPECT_EQ(0, disallowed_notification_count());
}

// This should fail if the throttle manager notifies the delegate twice of a
// disallowed load for the same page load.
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ActivateMainFrameAndFilterTwoSubframeNavigations) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation should be successfully filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/1/disallowed.html"), main_rfh());
  SimulateStartAndExpectResult(
      content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);

  EXPECT_EQ(1, disallowed_notification_count());

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/2/disallowed.html"), main_rfh());
  SimulateStartAndExpectResult(
      content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);

  EXPECT_EQ(1, disallowed_notification_count());
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ActivateTwoMainFramesAndFilterTwoSubframeNavigations) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation should be successfully filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/1/disallowed.html"), main_rfh());
  SimulateStartAndExpectResult(
      content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);

  EXPECT_EQ(1, disallowed_notification_count());

  // Commit another navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation2));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/2/disallowed.html"), main_rfh());
  SimulateStartAndExpectResult(
      content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);

  EXPECT_EQ(2, disallowed_notification_count());
}

// Test that the disallow load notification will not be repeated for the first
// disallowed load that follows a same-document navigation.
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ActivateMainFrameDoNotNotifyAfterSameDocumentNav) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation should be successfully filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/1/disallowed.html"), main_rfh());
  SimulateStartAndExpectResult(
      content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);

  EXPECT_EQ(1, disallowed_notification_count());

  // Commit another navigation that triggers page level activation.
  GURL url2 = GURL(base::StringPrintf("%s#ref", kTestURLWithActivation));
  CreateTestNavigation(url2, main_rfh());
  SimulateSameDocumentCommit();
  ExpectActivationSignalForFrame(main_rfh(), false /* expect_activation */);

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/2/disallowed.html"), main_rfh());
  SimulateStartAndExpectResult(
      content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);

  EXPECT_EQ(1, disallowed_notification_count());
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       DoNotFilterForInactiveFrame) {
  NavigateAndCommitMainFrame(GURL("https://do-not-activate.html"));
  ExpectActivationSignalForFrame(main_rfh(), false /* expect_activation */);

  // A subframe navigation should complete successfully.
  CreateSubframeWithTestNavigation(GURL("https://www.example.com/allowed.html"),
                                   main_rfh());
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* child =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  ExpectActivationSignalForFrame(child, false /* expect_activation */);

  EXPECT_EQ(0, disallowed_notification_count());
}

// Once there are no activated frames, the manager drops its ruleset handle. If
// another frame is activated, make sure the handle is regenerated.
TEST_P(ContentSubresourceFilterThrottleManagerTest, RulesetHandleRegeneration) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  SimulateStartAndExpectResult(
      content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);

  EXPECT_EQ(1, disallowed_notification_count());

  // Simulate a renderer crash which should delete the frame.
  EXPECT_TRUE(ManagerHasRulesetHandle());
  process()->SimulateCrash();
  EXPECT_FALSE(ManagerHasRulesetHandle());

  NavigateAndCommit(GURL("https://example.reset"));
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  SimulateStartAndExpectResult(
      content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);

  EXPECT_EQ(2, disallowed_notification_count());
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       SameSiteNavigation_RulesetGoesAway) {
  GURL same_site_inactive_url =
      GURL(base::StringPrintf("%sinactive.html", kTestURLWithActivation));

  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);
  EXPECT_TRUE(ManagerHasRulesetHandle());

  NavigateAndCommitMainFrame(same_site_inactive_url);
  ExpectActivationSignalForFrame(main_rfh(), false /* expect_activation */);
  EXPECT_FALSE(ManagerHasRulesetHandle());

  // A subframe navigation should complete successfully.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* child =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  ExpectActivationSignalForFrame(child, false /* expect_activation */);

  EXPECT_EQ(0, disallowed_notification_count());
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       SameSiteFailedNavigation_MaintainActivation) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);
  EXPECT_TRUE(ManagerHasRulesetHandle());

  GURL same_site_inactive_url =
      GURL(base::StringPrintf("%sinactive.html", kTestURLWithActivation));

  CreateTestNavigation(same_site_inactive_url, main_rfh());
  SimulateFailedNavigation(net::ERR_ABORTED);
  EXPECT_TRUE(ManagerHasRulesetHandle());
  ExpectActivationSignalForFrame(main_rfh(), false /* expect_activation */);

  // A subframe navigation fail.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  SimulateStartAndExpectResult(
      content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);

  EXPECT_EQ(1, disallowed_notification_count());
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       FailedNavigationToErrorPage_NoActivation) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);
  EXPECT_TRUE(ManagerHasRulesetHandle());

  GURL same_site_inactive_url =
      GURL(base::StringPrintf("%sinactive.html", kTestURLWithActivation));

  CreateTestNavigation(same_site_inactive_url, main_rfh());
  SimulateFailedNavigation(net::ERR_FAILED);
  EXPECT_FALSE(ManagerHasRulesetHandle());
  ExpectActivationSignalForFrame(main_rfh(), false /* expect_activation */);

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* child =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  ExpectActivationSignalForFrame(child, false /* expect_activation */);

  EXPECT_EQ(0, disallowed_notification_count());
}

// Ensure activation propagates into great-grandchild frames, including cross
// process ones.
TEST_P(ContentSubresourceFilterThrottleManagerTest, ActivationPropagation) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // Navigate a subframe to a URL that is not itself disallowed. Subresource
  // filtering for this subframe document should still be activated.
  CreateSubframeWithTestNavigation(GURL("https://www.a.com/allowed.html"),
                                   main_rfh());
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* subframe1 =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  ExpectActivationSignalForFrame(subframe1, true /* expect_activation */);

  // Navigate a sub-subframe to a URL that is not itself disallowed. Subresource
  // filtering for this subframe document should still be activated.
  CreateSubframeWithTestNavigation(GURL("https://www.b.com/allowed.html"),
                                   subframe1);
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* subframe2 =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  ExpectActivationSignalForFrame(subframe2, true /* expect_activation */);

  // A final, nested subframe navigation is filtered.
  CreateSubframeWithTestNavigation(GURL("https://www.c.com/disallowed.html"),
                                   subframe2);
  SimulateStartAndExpectResult(
      content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);

  EXPECT_EQ(1, disallowed_notification_count());
}

// Ensure activation propagates through whitelisted documents.
TEST_P(ContentSubresourceFilterThrottleManagerTest, ActivationPropagation2) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // Navigate a subframe that is not filtered, but should still activate.
  CreateSubframeWithTestNavigation(GURL("https://whitelist.com"), main_rfh());
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* subframe1 =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  ExpectActivationSignalForFrame(subframe1, true /* expect_activation */);

  // Navigate a sub-subframe that is not filtered due to the whitelist.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), subframe1);
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* subframe2 =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  ExpectActivationSignalForFrame(subframe2, true /* expect_activation */);

  EXPECT_EQ(0, disallowed_notification_count());

  // An identical series of events that don't match whitelist rules cause
  // filtering.
  CreateSubframeWithTestNavigation(GURL("https://average-joe.com"), main_rfh());
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* subframe3 =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  ExpectActivationSignalForFrame(subframe3, true /* expect_activation */);

  // Navigate a sub-subframe that is not filtered due to the whitelist.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), subframe3);
  SimulateStartAndExpectResult(
      content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);

  EXPECT_EQ(1, disallowed_notification_count());
}

// Same-site navigations within a single RFH do not persist activation.
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       SameSiteNavigationStopsActivation) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // Mock a same-site navigation, in the same RFH, this URL does not trigger
  // page level activation.
  NavigateAndCommitMainFrame(
      GURL(base::StringPrintf("%s/some_path/", kTestURLWithActivation)));
  ExpectActivationSignalForFrame(main_rfh(), false /* expect_activation */);

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* child =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  ExpectActivationSignalForFrame(child, false /* expect_activation */);

  EXPECT_EQ(0, disallowed_notification_count());
}

TEST_F(ContentSubresourceFilterThrottleManagerTest, LogActivation) {
  base::HistogramTester tester;
  const char kActivationStateHistogram[] =
      "SubresourceFilter.PageLoad.ActivationState";
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  tester.ExpectBucketCount(kActivationStateHistogram,
                           static_cast<int>(mojom::ActivationLevel::kDryRun),
                           1);

  NavigateAndCommitMainFrame(GURL(kTestURLWithNoActivation));
  tester.ExpectBucketCount(kActivationStateHistogram,
                           static_cast<int>(mojom::ActivationLevel::kDisabled),
                           1);

  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  tester.ExpectBucketCount(kActivationStateHistogram,
                           static_cast<int>(mojom::ActivationLevel::kEnabled),
                           1);

  // Navigate a subframe that is not filtered, but should still activate.
  CreateSubframeWithTestNavigation(GURL("https://whitelist.com"), main_rfh());
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* subframe1 =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  ExpectActivationSignalForFrame(subframe1, true /* expect_activation */);

  tester.ExpectTotalCount(kActivationStateHistogram, 3);
  // Only those with page level activation do ruleset lookups.
  tester.ExpectTotalCount("SubresourceFilter.PageLoad.Activation.WallDuration",
                          2);
  // The *.CPUDuration histograms are recorded only if base::ThreadTicks is
  // supported.
  tester.ExpectTotalCount("SubresourceFilter.PageLoad.Activation.CPUDuration",
                          base::ThreadTicks::IsSupported() ? 2 : 0);
}

// Check to make sure we don't send an IPC with the ad tag bit for ad frames
// that are successfully filtered.
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ActivateMainFrameAndFilterSubframeNavigationTaggedAsAd) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */,
                                 false /* is_ad_subframe */);

  // A disallowed subframe navigation should be successfully filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());

  SimulateStartAndExpectResult(
      content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);

  EXPECT_EQ(1, disallowed_notification_count());
}

// If the RenderFrame determines that the frame is an ad, then any navigation
// for that frame should be considered an ad.
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       SubframeNavigationTaggedAsAdByRenderer) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */,
                                 false /* is_ad_subframe */);

  content::RenderFrameHost* subframe = CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/allowed.html"), main_rfh());

  EXPECT_FALSE(throttle_manager()->IsFrameTaggedAsAdForTesting(subframe));
  throttle_manager()->OnFrameIsAdSubframe(subframe);
  EXPECT_TRUE(throttle_manager()->IsFrameTaggedAsAdForTesting(subframe));

  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);

  subframe =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  EXPECT_TRUE(subframe);

  ExpectActivationSignalForFrame(subframe, true /* expect_activation */,
                                 true /* is_ad_subframe */);

  // A non-ad navigation for the same frame should be considered an ad
  // subframe as well.
  CreateTestNavigation(GURL("https://example.com/allowed2.html"), subframe);
  SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  ExpectActivationSignalForFrame(subframe, true /* expect_activation */,
                                 true /* is_ad_subframe */);
}

// If the RenderFrame determines that the frame is an ad, and the frame changes
// processes, then the new frame host should still be considered an ad.
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       AdTagCarriesAcrossProcesses) {
  content::IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */,
                                 false /* is_ad_subframe */);

  // Create a subframe to a different site. It will start as a same-process
  // frame but transition to a cross-process frame just before commit (after
  // the throttle has marked the frame as an ad.)
  content::RenderFrameHost* initial_subframe = CreateSubframeWithTestNavigation(
      GURL("https://www.example2.com/allowed.html"), main_rfh());

  // Simulate the render process telling the manager that the frame is an ad.
  throttle_manager()->OnFrameIsAdSubframe(initial_subframe);
  EXPECT_TRUE(
      throttle_manager()->IsFrameTaggedAsAdForTesting(initial_subframe));

  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);

  content::RenderFrameHost* final_subframe =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  EXPECT_TRUE(final_subframe);
  EXPECT_NE(initial_subframe, final_subframe);

  EXPECT_TRUE(throttle_manager()->IsFrameTaggedAsAdForTesting(final_subframe));
  EXPECT_FALSE(
      throttle_manager()->IsFrameTaggedAsAdForTesting(initial_subframe));
  ExpectActivationSignalForFrame(final_subframe, true /* expect_activation */,
                                 true /* is_ad_subframe */);
}

// If the RenderFrame determines that the frame is an ad, then its child frames
// should also be considered ads.
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       GrandchildNavigationTaggedAsAdByRenderer) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */,
                                 false /* is_ad_subframe */);

  // Create a subframe that's marked as an ad by the render process.
  content::RenderFrameHost* subframe = CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/allowed.html"), main_rfh());

  // Simulate the render process telling the manager that the frame is an ad.
  throttle_manager()->OnFrameIsAdSubframe(subframe);

  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);

  subframe =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  ExpectActivationSignalForFrame(subframe, true /* expect_activation */,
                                 true /* is_ad_subframe */);

  // Create a grandchild frame that is marked as an ad because its parent is.
  content::RenderFrameHost* grandchild_frame = CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/foo/allowed.html"), subframe);
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  grandchild_frame =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  ExpectActivationSignalForFrame(grandchild_frame, true /* expect_activation */,
                                 true /* is_ad_subframe */);
  EXPECT_TRUE(
      throttle_manager()->IsFrameTaggedAsAdForTesting(grandchild_frame));
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       DryRun_FrameTaggingDeleted) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       DryRun_FrameTaggingAsAdPropagatesToChildFrame) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation should not be filtered in dry-run mode.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* child =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  EXPECT_TRUE(child);

  // But it should still be activated.
  ExpectActivationSignalForFrame(child, true /* expect_activation */,
                                 true /* is_ad_subframe */);
  EXPECT_TRUE(throttle_manager()->IsFrameTaggedAsAdForTesting(child));

  // Create a subframe which is allowed as per ruleset but should still be
  // tagged as ad because of its parent.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/allowed_by_ruleset.html"), child);
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* grandchild =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  EXPECT_TRUE(grandchild);
  ExpectActivationSignalForFrame(grandchild, true /* expect_activation */,
                                 true /* is_ad_subframe */);
  EXPECT_TRUE(throttle_manager()->IsFrameTaggedAsAdForTesting(grandchild));

  // Verify that a 2nd level nested frame should also be tagged.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/great_grandchild_allowed_by_ruleset.html"),
      child);
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* greatGrandchild =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  EXPECT_TRUE(greatGrandchild);
  ExpectActivationSignalForFrame(greatGrandchild, true /* expect_activation */,
                                 true /* is_ad_subframe */);
  EXPECT_TRUE(throttle_manager()->IsFrameTaggedAsAdForTesting(greatGrandchild));

  EXPECT_EQ(0, disallowed_notification_count());
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       DryRun_AllowedFrameNotTaggedAsAd) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/allowed_by_ruleset.html"), main_rfh());
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* child =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  EXPECT_TRUE(child);
  ExpectActivationSignalForFrame(child, true /* expect_activation */,
                                 false /* is_ad_subframe */);
  EXPECT_FALSE(throttle_manager()->IsFrameTaggedAsAdForTesting(child));

  // Create a subframe which is allowed as per ruleset and should not be tagged
  // as ad because its parent is not tagged as well.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/also_allowed_by_ruleset.html"), child);
  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  content::RenderFrameHost* grandchild =
      SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
  EXPECT_TRUE(grandchild);
  ExpectActivationSignalForFrame(grandchild, true /* expect_activation */,
                                 false /* is_ad_subframe */);
  EXPECT_FALSE(throttle_manager()->IsFrameTaggedAsAdForTesting(grandchild));

  EXPECT_EQ(0, disallowed_notification_count());
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       FirstDisallowedLoadCalledOutOfOrder) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  NavigateAndCommitMainFrame(GURL(kTestURLWithNoActivation));

  // Simulate the previous navigation sending an IPC that a load was disallowed.
  // This could happen e.g. for cross-process navigations, which have no
  // ordering guarantees.
  throttle_manager()->DidDisallowFirstSubresource();
  EXPECT_EQ(0, disallowed_notification_count());
}

// TODO(csharrison): Make sure the following conditions are exercised in tests:
//
// - Synchronous navigations to about:blank. These hit issues with the
//   NavigationSimulator currently.

}  // namespace subresource_filter
