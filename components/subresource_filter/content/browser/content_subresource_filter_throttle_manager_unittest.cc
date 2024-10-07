// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"

#include <map>
#include <memory>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_web_contents_helper.h"
#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"
#include "components/subresource_filter/content/browser/profile_interaction_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/content/browser/throttle_manager_test_support.h"
#include "components/subresource_filter/content/mojom/subresource_filter.mojom.h"
#include "components/subresource_filter/content/shared/browser/child_frame_navigation_test_utils.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/constants.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/subresource_filter/content/browser/ads_blocked_message_delegate.h"
#endif

namespace subresource_filter {

namespace proto = url_pattern_index::proto;

const char kTestURLWithActivation[] = "https://www.page-with-activation.com/";
const char kTestURLWithActivation2[] =
    "https://www.page-with-activation-2.com/";
const char kTestURLWithDryRun[] = "https://www.page-with-dryrun.com/";
const char kTestURLWithNoActivation[] =
    "https://www.page-without-activation.com/";

const char kReadyToCommitResultsInCommitHistogram[] =
    "SubresourceFilter.Experimental.ReadyToCommitResultsInCommit2";
const char kReadyToCommitResultsInCommitRestrictedAdFrameNavigationHistogram[] =
    "SubresourceFilter.Experimental.ReadyToCommitResultsInCommit2."
    "RestrictedAdFrameNavigation";

// Enum determining when the mock page state throttle notifies the throttle
// manager of page level activation state.
enum PageActivationNotificationTiming {
  WILL_START_REQUEST,
  WILL_PROCESS_RESPONSE,
};

class FakeSubresourceFilterAgent : public mojom::SubresourceFilterAgent {
 public:
  FakeSubresourceFilterAgent() = default;
  ~FakeSubresourceFilterAgent() override = default;

  void OnSubresourceFilterAgentReceiver(
      mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.reset();
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<mojom::SubresourceFilterAgent>(
            std::move(handle)));
  }

  // mojom::SubresourceFilterAgent:
  void ActivateForNextCommittedLoad(
      mojom::ActivationStatePtr activation_state,
      const std::optional<blink::FrameAdEvidence>& ad_evidence) override {
    last_activation_ = std::move(activation_state);
    is_ad_frame_ = ad_evidence.has_value() && ad_evidence->IndicatesAdFrame();
  }

  // These methods reset state back to default when they are called.
  bool LastAdFrame() {
    bool is_ad_frame = is_ad_frame_;
    is_ad_frame_ = false;
    return is_ad_frame;
  }
  std::optional<bool> LastActivated() {
    if (!last_activation_)
      return std::nullopt;
    bool activated =
        last_activation_->activation_level != mojom::ActivationLevel::kDisabled;
    last_activation_.reset();
    return activated;
  }

 private:
  mojom::ActivationStatePtr last_activation_;
  bool is_ad_frame_ = false;
  mojo::AssociatedReceiver<mojom::SubresourceFilterAgent> receiver_{this};
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

  MockPageStateActivationThrottle(const MockPageStateActivationThrottle&) =
      delete;
  MockPageStateActivationThrottle& operator=(
      const MockPageStateActivationThrottle&) = delete;

  ~MockPageStateActivationThrottle() override = default;

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
};

class ContentSubresourceFilterThrottleManagerTest
    : public content::RenderViewHostTestHarness,
      public content::WebContentsObserver,
      public ::testing::WithParamInterface<PageActivationNotificationTiming> {
 public:
  ContentSubresourceFilterThrottleManagerTest() = default;

  ContentSubresourceFilterThrottleManagerTest(
      const ContentSubresourceFilterThrottleManagerTest&) = delete;
  ContentSubresourceFilterThrottleManagerTest& operator=(
      const ContentSubresourceFilterThrottleManagerTest&) = delete;

  ~ContentSubresourceFilterThrottleManagerTest() override = default;

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    content::WebContents* web_contents =
        RenderViewHostTestHarness::web_contents();
    CreateAgentForHost(web_contents->GetPrimaryMainFrame());

    // Initialize the ruleset dealer. Allowlisted URLs must also match a
    // disallowed rule in order to work correctly.
    std::vector<proto::UrlRule> rules;
    rules.push_back(testing::CreateAllowlistRuleForDocument(
        "allowlist.com", proto::ACTIVATION_TYPE_DOCUMENT,
        {"page-with-activation.com"}));
    rules.push_back(testing::CreateRuleForDocument(
        "allowlist.com", proto::ACTIVATION_TYPE_DOCUMENT,
        {"page-with-activation.com"}));

    rules.push_back(testing::CreateAllowlistSuffixRule("not_disallowed.html"));
    rules.push_back(testing::CreateSuffixRule("disallowed.html"));
    ASSERT_NO_FATAL_FAILURE(test_ruleset_creator_.CreateRulesetWithRules(
        rules, &test_ruleset_pair_));

    // Make the blocking task runner run on the current task runner for the
    // tests, to ensure that the NavigationSimulator properly runs all necessary
    // tasks while waiting for throttle checks to finish.
    dealer_handle_ = std::make_unique<VerifiedRulesetDealer::Handle>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        kSafeBrowsingRulesetConfig);
    dealer_handle_->TryOpenAndSetRulesetFile(test_ruleset_pair_.indexed.path,
                                             /*expected_checksum=*/0,
                                             base::DoNothing());

    throttle_manager_test_support_ =
        std::make_unique<ThrottleManagerTestSupport>(web_contents);

    // Turn off smart UI to make it easier to reason about expectations on
    // ShowNotification() being invoked.
    throttle_manager_test_support_->SetShouldUseSmartUI(false);

    ContentSubresourceFilterWebContentsHelper::CreateForWebContents(
        web_contents, throttle_manager_test_support_->profile_context(),
        /*database_manager=*/nullptr, dealer_handle_.get());

    Observe(web_contents);

    NavigateAndCommit(GURL("https://example.first"));

#if BUILDFLAG(IS_ANDROID)
    message_dispatcher_bridge_.SetMessagesEnabledForEmbedder(true);
    messages::MessageDispatcherBridge::SetInstanceForTesting(
        &message_dispatcher_bridge_);
#endif
  }

  void TearDown() override {
    throttle_manager_test_support_.reset();
    dealer_handle_.reset();
    base::RunLoop().RunUntilIdle();
    content::RenderViewHostTestHarness::TearDown();
#if BUILDFLAG(IS_ANDROID)
    messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
#endif
  }

  void ExpectActivationSignalForFrame(
      content::RenderFrameHost* rfh,
      bool expect_activation,
      bool expect_is_ad_frame = false,
      bool expect_activation_sent_to_agent = true) {
    // In some cases we need to verify that messages were _not_ sent, in which
    // case using a Wait() idiom would cause hangs. RunUntilIdle instead to
    // ensure mojo calls make it to the fake agent.
    base::RunLoop().RunUntilIdle();
    FakeSubresourceFilterAgent* agent = agent_map_[rfh].get();
    std::optional<bool> last_activated = agent->LastActivated();
    EXPECT_EQ(expect_activation, last_activated && *last_activated);
    EXPECT_EQ(expect_is_ad_frame, agent->LastAdFrame());
    EXPECT_EQ(expect_activation_sent_to_agent, last_activated.has_value());
  }

  // Helper methods:

  void CreateTestNavigation(const GURL& url,
                            content::RenderFrameHost* render_frame_host) {
    CHECK(render_frame_host);
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
              SimulateStartAndGetResult(navigation_simulator()));
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              SimulateCommitAndGetResult(navigation_simulator()));
  }

  bool ManagerHasRulesetHandle() {
    return throttle_manager()->ruleset_handle_for_testing();
  }

#if BUILDFLAG(IS_ANDROID)
  void SimulateMessageDismissal() {
    throttle_manager()
        ->profile_interaction_manager_for_testing()
        ->ads_blocked_message_delegate_for_testing()
        ->DismissMessageForTesting(messages::DismissReason::SCOPE_DESTROYED);
  }
#endif

  bool ads_blocked_in_content_settings() {
    auto* content_settings =
        content_settings::PageSpecificContentSettings::GetForFrame(
            content::RenderViewHostTestHarness::web_contents()
                ->GetPrimaryMainFrame());

    return content_settings->IsContentBlocked(ContentSettingsType::ADS);
  }

 protected:
  // content::WebContentsObserver
  void RenderFrameCreated(content::RenderFrameHost* new_host) override {
    CreateAgentForHost(new_host);
  }

  void RenderFrameDeleted(content::RenderFrameHost* host) override {
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

    ContentSubresourceFilterThrottleManager::FromNavigationHandle(
        *navigation_handle)
        ->MaybeAppendNavigationThrottles(navigation_handle, &throttles);

    created_safe_browsing_throttle_for_last_navigation_ = false;
    for (auto& it : throttles) {
      if (strcmp(it->GetNameForLogging(),
                 "SafeBrowsingPageActivationThrottle") == 0) {
        created_safe_browsing_throttle_for_last_navigation_ = true;
      }

      navigation_handle->RegisterThrottleForTesting(std::move(it));
    }
  }

  void CreateAgentForHost(content::RenderFrameHost* host) {
    auto new_agent = std::make_unique<FakeSubresourceFilterAgent>();
    host->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
        mojom::SubresourceFilterAgent::Name_,
        base::BindRepeating(
            &FakeSubresourceFilterAgent::OnSubresourceFilterAgentReceiver,
            base::Unretained(new_agent.get())));
    agent_map_[host] = std::move(new_agent);
  }

  ContentSubresourceFilterThrottleManager* throttle_manager() {
    return ContentSubresourceFilterThrottleManager::FromPage(
        RenderViewHostTestHarness::web_contents()->GetPrimaryPage());
  }

  bool created_safe_browsing_throttle_for_current_navigation() const {
    return created_safe_browsing_throttle_for_last_navigation_;
  }

  void CreateSafeBrowsingDatabaseManager() {
    scoped_refptr<FakeSafeBrowsingDatabaseManager> database_manager =
        base::MakeRefCounted<FakeSafeBrowsingDatabaseManager>();

    web_contents_helper()->SetDatabaseManagerForTesting(
        std::move(database_manager));
  }

  VerifiedRulesetDealer::Handle* dealer_handle() {
    return dealer_handle_.get();
  }

#if BUILDFLAG(IS_ANDROID)
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
#endif

 private:
  ContentSubresourceFilterWebContentsHelper* web_contents_helper() {
    return ContentSubresourceFilterWebContentsHelper::FromWebContents(
        RenderViewHostTestHarness::web_contents());
  }

  testing::TestRulesetCreator test_ruleset_creator_;
  testing::TestRulesetPair test_ruleset_pair_;
  std::unique_ptr<ThrottleManagerTestSupport> throttle_manager_test_support_;

  std::unique_ptr<VerifiedRulesetDealer::Handle> dealer_handle_;

  std::map<content::RenderFrameHost*,
           std::unique_ptr<FakeSubresourceFilterAgent>>
      agent_map_;

  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;

  bool created_safe_browsing_throttle_for_last_navigation_ = false;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ContentSubresourceFilterThrottleManagerTest,
                         ::testing::Values(WILL_START_REQUEST,
                                           WILL_PROCESS_RESPONSE));

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ActivateMainFrameAndFilterSubframeNavigation) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation should be successfully filtered.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

#if BUILDFLAG(IS_ANDROID)
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       NoCrashWhenMessageDelegateIsNotPresent) {
  auto* web_contents = RenderViewHostTestHarness::web_contents();
  web_contents->RemoveUserData(
      subresource_filter::AdsBlockedMessageDelegate::UserDataKey());

  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation should be successfully filtered, and the
  // lack of infobar manager should not cause a crash.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
}
#endif

TEST_P(ContentSubresourceFilterThrottleManagerTest, NoPageActivation) {
  // This test assumes that we're not in DryRun mode.
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndDisableFeature(kAdTagging);

  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithNoActivation));
  ExpectActivationSignalForFrame(main_rfh(), false /* expect_activation */);
  EXPECT_FALSE(ManagerHasRulesetHandle());

  // A disallowed subframe navigation should not be filtered.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage).Times(0);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  EXPECT_FALSE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ActivateMainFrameAndDoNotFilterDryRun) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation should not be filtered in dry-run mode.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage).Times(0);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* child =
      navigation_simulator()->GetFinalRenderFrameHost();
  // But it should still be activated.
  ExpectActivationSignalForFrame(child, true /* expect_activation */,
                                 true /* is_ad_frame */);

  EXPECT_FALSE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ActivateMainFrameAndFilterSubframeNavigationOnRedirect) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation via redirect should be successfully
  // filtered.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/before-redirect.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateRedirectAndGetResult(
                navigation_simulator(),
                GURL("https://www.example.com/disallowed.html")));
  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ActivateMainFrameAndDoNotFilterSubframeNavigation) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // An allowed subframe navigation should complete successfully.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage).Times(0);
#endif
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
  ExpectActivationSignalForFrame(child, true /* expect_activation */);

  EXPECT_FALSE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

// This should fail if the throttle manager notifies the delegate twice of a
// disallowed load for the same page load.
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ActivateMainFrameAndFilterTwoSubframeNavigations) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation should be successfully filtered.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/1/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/2/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ActivateTwoMainFramesAndFilterTwoSubframeNavigations) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation should be successfully filtered.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/1/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif

  // Commit another navigation that triggers page level activation.
#if BUILDFLAG(IS_ANDROID)
  // Since the MessageDispatcherBridge is mocked, navigation events are not
  // tracked by the messages system to automatically dismiss the message on
  // navigation. The message dismissal is therefore simulated.
  SimulateMessageDismissal();
#endif
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation2));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  EXPECT_FALSE(ads_blocked_in_content_settings());

#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/2/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       DoNotFilterForInactiveFrame) {
  // This test assumes that we're not in DryRun mode.
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndDisableFeature(kAdTagging);

  NavigateAndCommitMainFrame(GURL("https://do-not-activate.html"));
  ExpectActivationSignalForFrame(main_rfh(), false /* expect_activation */);

  // A subframe navigation should complete successfully.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage).Times(0);
#endif
  CreateSubframeWithTestNavigation(GURL("https://www.example.com/allowed.html"),
                                   main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* child =
      navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(child, false /* expect_activation */);

  EXPECT_FALSE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

// Once there are no activated frames, the manager drops its ruleset handle. If
// another frame is activated, make sure the handle is regenerated.
TEST_P(ContentSubresourceFilterThrottleManagerTest, RulesetHandleRegeneration) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif

  // Simulate a renderer crash which should delete the frame.
  EXPECT_TRUE(ManagerHasRulesetHandle());
  process()->SimulateCrash();
  EXPECT_FALSE(ManagerHasRulesetHandle());

#if BUILDFLAG(IS_ANDROID)
  // Since the MessageDispatcherBridge is mocked, navigation events are not
  // tracked by the messages system to automatically dismiss the message on
  // navigation. The message dismissal is therefore simulated.
  SimulateMessageDismissal();
#endif
  NavigateAndCommit(GURL("https://example.reset"));
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  EXPECT_FALSE(ads_blocked_in_content_settings());

#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       SameSiteNavigation_RulesetGoesAway) {
  // The test assumes the previous page gets deleted after navigation and
  // ManagerHasRulesetHandle() will return false. Disable back/forward cache to
  // ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(
      RenderViewHostTestHarness::web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  // This test assumes that we're not in DryRun mode.
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndDisableFeature(kAdTagging);

  GURL same_site_inactive_url =
      GURL(base::StringPrintf("%sinactive.html", kTestURLWithActivation));

  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);
  EXPECT_TRUE(ManagerHasRulesetHandle());

  NavigateAndCommitMainFrame(same_site_inactive_url);
  ExpectActivationSignalForFrame(main_rfh(), false /* expect_activation */);
  EXPECT_FALSE(ManagerHasRulesetHandle());

  // A subframe navigation should complete successfully.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage).Times(0);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* child =
      navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(child, false /* expect_activation */);

  EXPECT_FALSE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       SameSiteFailedNavigation_MaintainActivation) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);
  EXPECT_TRUE(ManagerHasRulesetHandle());

  GURL same_site_inactive_url =
      GURL(base::StringPrintf("%sinactive.html", kTestURLWithActivation));

  CreateTestNavigation(same_site_inactive_url, main_rfh());
  SimulateFailedNavigation(navigation_simulator(), net::ERR_ABORTED);
  EXPECT_TRUE(ManagerHasRulesetHandle());

  // The aborted navigation does not pass through ReadyToCommitNavigation so no
  // ActivateForNextCommittedLoad mojo call is expected.
  ExpectActivationSignalForFrame(main_rfh(), false /* expect_activation */,
                                 false /* expect_is_ad_frame */,
                                 false /* expect_activation_sent_to_agent */);

  // A subframe navigation fail.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       FailedNavigationToErrorPage_NoActivation) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);
  EXPECT_TRUE(ManagerHasRulesetHandle());

  GURL same_site_inactive_url =
      GURL(base::StringPrintf("%sinactive.html", kTestURLWithActivation));

  CreateTestNavigation(same_site_inactive_url, main_rfh());
  SimulateFailedNavigation(navigation_simulator(), net::ERR_FAILED);
  ExpectActivationSignalForFrame(main_rfh(), false /* expect_activation */);

#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage).Times(0);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* child =
      navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(child, false /* expect_activation */);

  EXPECT_FALSE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
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
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* subframe1 =
      navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(subframe1, true /* expect_activation */);

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
  ExpectActivationSignalForFrame(subframe2, true /* expect_activation */);

  // A final, nested subframe navigation is filtered.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
#endif
  CreateSubframeWithTestNavigation(GURL("https://www.c.com/disallowed.html"),
                                   subframe2);
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

// Ensure activation propagates through allowlisted documents.
// crbug.com/1010000: crashes on win
#if BUILDFLAG(IS_WIN)
#define MAYBE_ActivationPropagation2 DISABLED_ActivationPropagation2
#else
#define MAYBE_ActivationPropagation2 ActivationPropagation2
#endif
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       MAYBE_ActivationPropagation2) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // Navigate a subframe that is not filtered, but should still activate.
  CreateSubframeWithTestNavigation(GURL("https://allowlist.com"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* subframe1 =
      navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(subframe1, true /* expect_activation */);

  // Navigate a sub-subframe that is not filtered due to the allowlist.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage).Times(0);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), subframe1);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* subframe2 =
      navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(subframe2, true /* expect_activation */);

  EXPECT_FALSE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif

  // An identical series of events that don't match allowlist rules cause
  // filtering.
  CreateSubframeWithTestNavigation(GURL("https://average-joe.com"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* subframe3 =
      navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(subframe3, true /* expect_activation */);

  // Navigate a sub-subframe that is not filtered due to the allowlist.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), subframe3);
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

// Same-site navigations within a single RFH do not persist activation.
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       SameSiteNavigationStopsActivation) {
  // This test assumes that we're not in DryRun mode.
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndDisableFeature(kAdTagging);

  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // Mock a same-site navigation, in the same RFH, this URL does not trigger
  // page level activation.
  NavigateAndCommitMainFrame(
      GURL(base::StringPrintf("%s/some_path/", kTestURLWithActivation)));
  ExpectActivationSignalForFrame(main_rfh(), false /* expect_activation */);

#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage).Times(0);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* child =
      navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(child, false /* expect_activation */);

  EXPECT_FALSE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       CreateHelperForWebContents) {
  auto web_contents =
      content::RenderViewHostTestHarness::CreateTestWebContents();
  ASSERT_EQ(ContentSubresourceFilterWebContentsHelper::FromWebContents(
                web_contents.get()),
            nullptr);

  ThrottleManagerTestSupport throttle_manager_test_support(web_contents.get());
  SubresourceFilterProfileContext* profile_context =
      throttle_manager_test_support.profile_context();

  {
    base::test::ScopedFeatureList scoped_feature;
    scoped_feature.InitAndDisableFeature(kSafeBrowsingSubresourceFilter);

    // CreateForWebContents() should not do anything if the subresource filter
    // feature is not enabled.
    ContentSubresourceFilterWebContentsHelper::CreateForWebContents(
        web_contents.get(), profile_context, /*database_manager=*/nullptr,
        dealer_handle());
    EXPECT_EQ(ContentSubresourceFilterWebContentsHelper::FromWebContents(
                  web_contents.get()),
              nullptr);
  }

  // If the subresource filter feature is enabled (as it is by default),
  // CreateForWebContents() should create and attach an instance.
  ContentSubresourceFilterWebContentsHelper::CreateForWebContents(
      web_contents.get(), profile_context,
      /*database_manager=*/nullptr, dealer_handle());
  auto* helper = ContentSubresourceFilterWebContentsHelper::FromWebContents(
      web_contents.get());
  EXPECT_NE(helper, nullptr);

  // A second call should not attach a different instance.
  ContentSubresourceFilterWebContentsHelper::CreateForWebContents(
      web_contents.get(), profile_context,
      /*database_manager=*/nullptr, dealer_handle());
  EXPECT_EQ(ContentSubresourceFilterWebContentsHelper::FromWebContents(
                web_contents.get()),
            helper);
}

// Check to make sure we don't send an IPC with the ad tag bit for ad frames
// that are successfully filtered.
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ActivateMainFrameAndFilterSubframeNavigationTaggedAsAd) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */,
                                 false /* is_ad_frame */);

  // A disallowed subframe navigation should be successfully filtered.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

// If the RenderFrame determines that the frame is an ad due to creation by ad
// script, then any navigation for that frame should be considered an ad.
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       SubframeNavigationTaggedAsAdByRenderer) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */,
                                 false /* is_ad_frame */);

  content::RenderFrameHost* subframe = CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/allowed.html"), main_rfh());

  EXPECT_FALSE(throttle_manager()->IsRenderFrameHostTaggedAsAd(subframe));
  throttle_manager()->OnChildFrameWasCreatedByAdScript(subframe);
  throttle_manager()->OnFrameIsAd(subframe);

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  subframe = navigation_simulator()->GetFinalRenderFrameHost();
  EXPECT_TRUE(subframe);
  EXPECT_TRUE(throttle_manager()->IsRenderFrameHostTaggedAsAd(subframe));
  ExpectActivationSignalForFrame(subframe, true /* expect_activation */,
                                 true /* is_ad_frame */);

  // A non-ad navigation for the same frame should be considered an ad
  // subframe as well.
  CreateTestNavigation(GURL("https://example.com/allowed2.html"), subframe);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  subframe = navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(subframe, true /* expect_activation */,
                                 true /* is_ad_frame */);
}

// Helper class to make sure strict site isolation is on for tests that need
// it. This is already the default on desktop platforms, so doing this is
// mainly to provide coverage on Android. Note that these tests can't just call
// IsolateAllSitesForTesting() in the test body, as the SetUp() method in the
// test harness also performs a navigation, so site isolation must be turned on
// early enough so that it can be in effect for that navigation.
class SitePerProcessContentSubresourceFilterThrottleManagerTest
    : public ContentSubresourceFilterThrottleManagerTest {
 public:
  SitePerProcessContentSubresourceFilterThrottleManagerTest() {
    content::IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SitePerProcessContentSubresourceFilterThrottleManagerTest,
    ::testing::Values(WILL_START_REQUEST, WILL_PROCESS_RESPONSE));

// If the RenderFrame determines that the frame is an ad due to creation by ad
// script, and the frame changes processes, then the frame should still be
// considered an ad.
TEST_P(SitePerProcessContentSubresourceFilterThrottleManagerTest,
       AdTagCarriesAcrossProcesses) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */,
                                 false /* is_ad_frame */);

  // Create a subframe to a different site. It will start as a same-process
  // frame but transition to a cross-process frame just before commit (after
  // the throttle has marked the frame as an ad.)
  content::RenderFrameHost* initial_subframe = CreateSubframeWithTestNavigation(
      GURL("https://www.example2.com/allowed.html"), main_rfh());

  // Simulate the render process telling the manager that the frame is an ad due
  // to creation by ad script.
  throttle_manager()->OnChildFrameWasCreatedByAdScript(initial_subframe);
  throttle_manager()->OnFrameIsAd(initial_subframe);

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* final_subframe =
      navigation_simulator()->GetFinalRenderFrameHost();
  EXPECT_TRUE(final_subframe);
  EXPECT_NE(initial_subframe, final_subframe);

  EXPECT_TRUE(throttle_manager()->IsRenderFrameHostTaggedAsAd(final_subframe));
  ExpectActivationSignalForFrame(final_subframe, true /* expect_activation */,
                                 true /* is_ad_frame */);
}

// If the RenderFrame determines that the frame was created by ad script, it
// should be tagged and then its child frames should also be tagged as ads.
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       GrandchildNavigationTaggedAsAdByRenderer) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */,
                                 false /* is_ad_frame */);

  // Create a subframe that's marked as an ad by the render process.
  content::RenderFrameHost* subframe = CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/allowed.html"), main_rfh());

  // Simulate the render process telling the manager that the frame is an ad due
  // to creation by ad script.
  throttle_manager()->OnChildFrameWasCreatedByAdScript(subframe);
  throttle_manager()->OnFrameIsAd(subframe);

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  subframe = navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(subframe, true /* expect_activation */,
                                 true /* is_ad_frame */);

  // Create a grandchild frame that is marked as an ad because its parent is.
  content::RenderFrameHost* grandchild_frame = CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/foo/allowed.html"), subframe);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  grandchild_frame = navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(grandchild_frame, true /* expect_activation */,
                                 true /* is_ad_frame */);
  EXPECT_TRUE(
      throttle_manager()->IsRenderFrameHostTaggedAsAd(grandchild_frame));
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
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* child =
      navigation_simulator()->GetFinalRenderFrameHost();
  EXPECT_TRUE(child);

  // But it should still be activated.
  ExpectActivationSignalForFrame(child, true /* expect_activation */,
                                 true /* is_ad_frame */);
  EXPECT_TRUE(throttle_manager()->IsRenderFrameHostTaggedAsAd(child));

  // Create a subframe which is allowed as per ruleset but should still be
  // tagged as ad because of its parent.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/allowed_by_ruleset.html"), child);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* grandchild =
      navigation_simulator()->GetFinalRenderFrameHost();
  EXPECT_TRUE(grandchild);
  ExpectActivationSignalForFrame(grandchild, true /* expect_activation */,
                                 true /* is_ad_frame */);
  EXPECT_TRUE(throttle_manager()->IsRenderFrameHostTaggedAsAd(grandchild));

  // Verify that a 2nd level nested frame should also be tagged.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage).Times(0);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/great_grandchild_allowed_by_ruleset.html"),
      child);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* greatGrandchild =
      navigation_simulator()->GetFinalRenderFrameHost();
  EXPECT_TRUE(greatGrandchild);
  ExpectActivationSignalForFrame(greatGrandchild, true /* expect_activation */,
                                 true /* is_ad_frame */);
  EXPECT_TRUE(throttle_manager()->IsRenderFrameHostTaggedAsAd(greatGrandchild));

  EXPECT_FALSE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       DryRun_AllowedFrameNotTaggedAsAd) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/allowed_by_ruleset.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* child =
      navigation_simulator()->GetFinalRenderFrameHost();
  EXPECT_TRUE(child);
  ExpectActivationSignalForFrame(child, true /* expect_activation */,
                                 false /* is_ad_frame */);
  EXPECT_FALSE(throttle_manager()->IsRenderFrameHostTaggedAsAd(child));

  // Create a subframe which is allowed as per ruleset and should not be tagged
  // as ad because its parent is not tagged as well.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage).Times(0);
#endif
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/also_allowed_by_ruleset.html"), child);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* grandchild =
      navigation_simulator()->GetFinalRenderFrameHost();
  EXPECT_TRUE(grandchild);
  ExpectActivationSignalForFrame(grandchild, true /* expect_activation */,
                                 false /* is_ad_frame */);
  EXPECT_FALSE(throttle_manager()->IsRenderFrameHostTaggedAsAd(grandchild));

  EXPECT_FALSE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       FirstDisallowedLoadCalledOutOfOrder) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));

  auto* web_contents = RenderViewHostTestHarness::web_contents();
  mojo::AssociatedRemote<mojom::SubresourceFilterHost> remote;
  ContentSubresourceFilterThrottleManager::BindReceiver(
      remote.BindNewEndpointAndPassDedicatedReceiver(),
      &web_contents->GetPrimaryPage().GetMainDocument());
  ASSERT_TRUE(remote.is_bound());
  ASSERT_TRUE(remote.is_connected());

#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage).Times(0);
#endif
  NavigateAndCommitMainFrame(GURL(kTestURLWithNoActivation));

  // Simulate the previous navigation sending an IPC that a load was
  // disallowed.  This could happen e.g. for cross-process navigations, which
  // have no ordering guarantees. Navigating to a new page will usually dispose
  // the remote end of the mojo binding so the mojo call will be a no-op,
  // though in some cases the binding may survive (e.g. page is put into
  // BFCache). Ensure that even if it does survive, a stale call doesn't show
  // UI.
  remote.FlushForTesting();
  remote->DidDisallowFirstSubresource();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       NavigationIsReadyToCommitThenFinishes_HistogramIssued) {
  for (bool does_commit : {true, false}) {
    base::HistogramTester tester;
    NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
    ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

    // Make sure main frames are excluded.
    tester.ExpectTotalCount(kReadyToCommitResultsInCommitHistogram, 0);
    tester.ExpectTotalCount(
        kReadyToCommitResultsInCommitRestrictedAdFrameNavigationHistogram, 0);

    CreateSubframeWithTestNavigation(GURL("https://www.example.com/test.html"),
                                     main_rfh());

    navigation_simulator()->ReadyToCommit();

    if (does_commit) {
      navigation_simulator()->Commit();
    } else {
      navigation_simulator()->AbortFromRenderer();
    }

    tester.ExpectUniqueSample(kReadyToCommitResultsInCommitHistogram,
                              does_commit, 1);
    tester.ExpectTotalCount(
        kReadyToCommitResultsInCommitRestrictedAdFrameNavigationHistogram, 0);
  }
}

TEST_P(
    ContentSubresourceFilterThrottleManagerTest,
    RestrictedAdFrameNavigationIsReadyToCommitThenFinishes_HistogramsIssued) {
  for (bool does_commit : {true, false}) {
    NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
    ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

    // Ensure frame is tagged as an ad.
    content::RenderFrameHost* subframe = CreateSubframeWithTestNavigation(
        GURL("https://www.example.com/disallowed.html"), main_rfh());
    navigation_simulator()->Commit();
    subframe = navigation_simulator()->GetFinalRenderFrameHost();
    EXPECT_TRUE(subframe);
    EXPECT_TRUE(throttle_manager()->IsRenderFrameHostTaggedAsAd(subframe));

    // Navigate to an allowlisted URL to make it a 'restricted' navigation.
    base::HistogramTester tester;
    CreateTestNavigation(GURL("https://www.example.com/not_disallowed.html"),
                         subframe);

    navigation_simulator()->ReadyToCommit();

    if (does_commit) {
      navigation_simulator()->Commit();
    } else {
      navigation_simulator()->AbortFromRenderer();
    }

    tester.ExpectUniqueSample(kReadyToCommitResultsInCommitHistogram,
                              does_commit, 1);
    tester.ExpectUniqueSample(
        kReadyToCommitResultsInCommitRestrictedAdFrameNavigationHistogram,
        does_commit, 1);
  }
}

TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ReadyToCommitNavigationThenRenderFrameDeletes_MetricsNotRecorded) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // Ensure frame is tagged as an ad.
  content::RenderFrameHost* subframe = CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  navigation_simulator()->Commit();
  subframe = navigation_simulator()->GetFinalRenderFrameHost();
  EXPECT_TRUE(subframe);
  EXPECT_TRUE(throttle_manager()->IsRenderFrameHostTaggedAsAd(subframe));

  // Navigate to an allowlisted URL to make it a 'restricted' navigation.
  base::HistogramTester tester;
  CreateTestNavigation(GURL("https://www.example.com/not_disallowed.html"),
                       subframe);

  navigation_simulator()->ReadyToCommit();

  static_cast<content::MockRenderProcessHost*>(
      navigation_simulator()->GetFinalRenderFrameHost()->GetProcess())
      ->SimulateCrash();

  tester.ExpectTotalCount(kReadyToCommitResultsInCommitHistogram, 0);
  tester.ExpectTotalCount(
      kReadyToCommitResultsInCommitRestrictedAdFrameNavigationHistogram, 0);
}

// Basic test of throttle manager lifetime and getter methods. Ensure a new
// page creating navigation creates a new throttle manager and it's reachable
// using FromNavigationHandle until commit time. Once committed that same
// throttle manager should now be associated with the new page.
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ThrottleManagerLifetime_Basic) {
  auto* initial_throttle_manager =
      ContentSubresourceFilterThrottleManager::FromPage(main_rfh()->GetPage());
  EXPECT_TRUE(initial_throttle_manager);

  CreateTestNavigation(GURL(kTestURLWithNoActivation), main_rfh());
  navigation_simulator()->Start();

  auto* throttle_manager_at_start =
      ContentSubresourceFilterThrottleManager::FromNavigationHandle(
          *navigation_simulator()->GetNavigationHandle());

  // Starting a main-frame, cross-document navigation creates a new throttle
  // manager but doesn't replace the one on the current page yet.
  EXPECT_TRUE(throttle_manager_at_start);
  EXPECT_NE(throttle_manager_at_start, initial_throttle_manager);
  EXPECT_EQ(
      ContentSubresourceFilterThrottleManager::FromPage(main_rfh()->GetPage()),
      initial_throttle_manager);

  navigation_simulator()->ReadyToCommit();

  EXPECT_EQ(ContentSubresourceFilterThrottleManager::FromNavigationHandle(
                *navigation_simulator()->GetNavigationHandle()),
            throttle_manager_at_start);
  EXPECT_EQ(
      ContentSubresourceFilterThrottleManager::FromPage(main_rfh()->GetPage()),
      initial_throttle_manager);

  navigation_simulator()->Commit();

  // Now that the navigation committed, it should be associated with the current
  // page.
  EXPECT_FALSE(navigation_simulator()->GetNavigationHandle());
  ASSERT_EQ(main_rfh()->GetLastCommittedURL(), kTestURLWithNoActivation);
  EXPECT_EQ(
      ContentSubresourceFilterThrottleManager::FromPage(main_rfh()->GetPage()),
      throttle_manager_at_start);

  // A new navigation creates a new throttle manager.
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));
  EXPECT_NE(
      ContentSubresourceFilterThrottleManager::FromPage(main_rfh()->GetPage()),
      throttle_manager_at_start);
}

// Ensure subframe navigations do not create a new throttle manager and
// FromNavigation gets the correct one.
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ThrottleManagerLifetime_Subframe) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));

  auto* throttle_manager =
      ContentSubresourceFilterThrottleManager::FromPage(main_rfh()->GetPage());
  ASSERT_TRUE(throttle_manager);

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/not_disallowed.html"), main_rfh());
  navigation_simulator()->Start();

  // Using FromNavigation on a subframe navigation should retrieve the throttle
  // manager from the current Page.
  EXPECT_EQ(ContentSubresourceFilterThrottleManager::FromNavigationHandle(
                *navigation_simulator()->GetNavigationHandle()),
            throttle_manager);

  navigation_simulator()->Commit();

  // Committing the subframe navigation should not change the Page's throttle
  // manager.
  EXPECT_EQ(
      ContentSubresourceFilterThrottleManager::FromPage(main_rfh()->GetPage()),
      throttle_manager);
}

// Same document navigations are similar to subframes: do not create a new
// throttle manager and FromNavigation gets the existing one.
// TODO(bokan): Would be good to test lifetime from some WebContentsObserver
// methods that can see the navigation handle for a same-document navigation.
// Some additional tests that would be good is to verify the behavior of the
// FromNavigationHandle/FromPage methods around the DidFinishNavigation time
// when it's transferred.
TEST_P(ContentSubresourceFilterThrottleManagerTest,
       ThrottleManagerLifetime_SameDocument) {
  const GURL kUrl = GURL(kTestURLWithDryRun);
  const GURL kSameDocumentUrl =
      GURL(base::StringPrintf("%s#ref", kTestURLWithDryRun));

  NavigateAndCommitMainFrame(kUrl);

  auto* throttle_manager =
      ContentSubresourceFilterThrottleManager::FromPage(main_rfh()->GetPage());
  ASSERT_TRUE(throttle_manager);

  CreateTestNavigation(kSameDocumentUrl, main_rfh());
  navigation_simulator()->CommitSameDocument();

  // Committing the same-document navigation should not change the Page's
  // throttle manager.
  EXPECT_EQ(
      ContentSubresourceFilterThrottleManager::FromPage(main_rfh()->GetPage()),
      throttle_manager);
}

class ContentSubresourceFilterThrottleManagerFencedFrameTest
    : public ContentSubresourceFilterThrottleManagerTest {
 public:
  ContentSubresourceFilterThrottleManagerFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~ContentSubresourceFilterThrottleManagerFencedFrameTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ContentSubresourceFilterThrottleManagerFencedFrameTest,
                         ::testing::Values(WILL_START_REQUEST,
                                           WILL_PROCESS_RESPONSE));

TEST_P(ContentSubresourceFilterThrottleManagerFencedFrameTest,
       ActivateMainFrameAndFilterFencedFrameNavigation) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed fenced frame navigation should be successfully filtered.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
#endif
  CreateFencedFrameWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

TEST_P(ContentSubresourceFilterThrottleManagerFencedFrameTest,
       ActivateMainFrameAndFilterFencedFrameNavigationOnRedirect) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation via redirect should be successfully
  // filtered.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
#endif
  CreateFencedFrameWithTestNavigation(
      GURL("https://www.example.com/before-redirect.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateRedirectAndGetResult(
                navigation_simulator(),
                GURL("https://www.example.com/disallowed.html")));
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif
}

// Ensure activation propagates into great-grandchild fenced frames, including
// cross process ones.
TEST_P(ContentSubresourceFilterThrottleManagerFencedFrameTest,
       ActivationPropagation) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // Navigate a fenced frame to a URL that is not itself disallowed. Subresource
  // filtering for this fenced frame document should still be activated.
  CreateFencedFrameWithTestNavigation(GURL("https://www.a.com/allowed.html"),
                                      main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* fenced_frame1 =
      navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(fenced_frame1, true /* expect_activation */);

  // Navigate a nested fenced frame to a URL that is not itself disallowed.
  // Subresource filtering for this fenced frame document should still be
  // activated.
  CreateFencedFrameWithTestNavigation(GURL("https://www.b.com/allowed.html"),
                                      fenced_frame1);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* fenced_frame2 =
      navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(fenced_frame2, true /* expect_activation */);

  // A final, nested fenced frame navigation is filtered.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
#endif
  CreateFencedFrameWithTestNavigation(GURL("https://www.c.com/disallowed.html"),
                                      fenced_frame2);
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));
#if BUILDFLAG(IS_ANDROID)
  ::testing::Mock::VerifyAndClearExpectations(&message_dispatcher_bridge_);
#endif

  // A subframe navigation inside the nested fenced frame is filtered.
  CreateSubframeWithTestNavigation(GURL("https://www.c.com/disallowed.html"),
                                   fenced_frame2);
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));
}

TEST_P(ContentSubresourceFilterThrottleManagerFencedFrameTest,
       SafeBrowsingThrottleCreation) {
  // If no safe browsing database is present, the throttle should not be
  // created on a navigation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithNoActivation));
  EXPECT_FALSE(created_safe_browsing_throttle_for_current_navigation());

  CreateSafeBrowsingDatabaseManager();

  // With a safe browsing database present, the throttle should be created on
  // a main frame navigation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithNoActivation));
  EXPECT_TRUE(created_safe_browsing_throttle_for_current_navigation());

  // However, it still should not be created on a subframe navigation.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_FALSE(created_safe_browsing_throttle_for_current_navigation());

  // It should also not be created on a fenced frame navigation.
  CreateFencedFrameWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_FALSE(created_safe_browsing_throttle_for_current_navigation());
}

TEST_P(ContentSubresourceFilterThrottleManagerFencedFrameTest, LogActivation) {
  // This test assumes that we're not in DryRun mode.
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndDisableFeature(kAdTagging);

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
  CreateSubframeWithTestNavigation(GURL("https://allowlist.com"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* subframe1 =
      navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(subframe1, true /* expect_activation */);

  // Navigate a fenced frame that is not filtered, but should still activate.
  CreateFencedFrameWithTestNavigation(GURL("https://allowlist.com"),
                                      main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));
  content::RenderFrameHost* fenced_frame1 =
      navigation_simulator()->GetFinalRenderFrameHost();
  ExpectActivationSignalForFrame(fenced_frame1, true /* expect_activation */);

  tester.ExpectTotalCount(kActivationStateHistogram, 3);
}

// Ensure fenced frame navigations do not create a new throttle manager and
// FromNavigation gets the correct one.
TEST_P(ContentSubresourceFilterThrottleManagerFencedFrameTest,
       ThrottleManagerLifetime_FencedFrame) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithDryRun));

  auto* throttle_manager =
      ContentSubresourceFilterThrottleManager::FromPage(main_rfh()->GetPage());
  ASSERT_TRUE(throttle_manager);

  content::RenderFrameHost* fenced_frame_root =
      CreateFencedFrameWithTestNavigation(
          GURL("https://www.example.com/not_disallowed.html"), main_rfh());
  EXPECT_TRUE(fenced_frame_root);
  navigation_simulator()->Start();

  // Using FromNavigation on a fenced frame navigation should retrieve the
  // throttle manager from the initial (outer-most) Page.
  EXPECT_EQ(ContentSubresourceFilterThrottleManager::FromNavigationHandle(
                *navigation_simulator()->GetNavigationHandle()),
            throttle_manager);

  navigation_simulator()->Commit();
  fenced_frame_root = navigation_simulator()->GetFinalRenderFrameHost();

  // Committing the fenced frame navigation should not change the Page's
  // throttle manager.
  EXPECT_EQ(
      ContentSubresourceFilterThrottleManager::FromPage(main_rfh()->GetPage()),
      throttle_manager);

  // The throttle manager on the fenced frame page should be the outer-most
  // Page's throttle manager.
  EXPECT_EQ(ContentSubresourceFilterThrottleManager::FromPage(
                fenced_frame_root->GetPage()),
            throttle_manager);
}

class ContentSubresourceFilterThrottleManagerInfoBarUiTest
    : public ContentSubresourceFilterThrottleManagerTest {
 public:
  void SetUp() override {
    ContentSubresourceFilterThrottleManagerTest::SetUp();
#if BUILDFLAG(IS_ANDROID)
    message_dispatcher_bridge_.SetMessagesEnabledForEmbedder(false);
    messages::MessageDispatcherBridge::SetInstanceForTesting(
        &message_dispatcher_bridge_);
#endif
  }

  bool presenting_ads_blocked_infobar() {
    auto* infobar_manager = infobars::ContentInfoBarManager::FromWebContents(
        content::RenderViewHostTestHarness::web_contents());
    if (infobar_manager->infobars().empty()) {
      return false;
    }

    // No infobars other than the ads blocked infobar should be displayed in the
    // context of these tests.
    EXPECT_EQ(infobar_manager->infobars().size(), 1u);
    auto* infobar = infobar_manager->infobars()[0].get();
    EXPECT_EQ(infobar->GetIdentifier(),
              infobars::InfoBarDelegate::ADS_BLOCKED_INFOBAR_DELEGATE_ANDROID);

    return true;
  }

 protected:
#if BUILDFLAG(IS_ANDROID)
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
#endif
};

INSTANTIATE_TEST_SUITE_P(All,
                         ContentSubresourceFilterThrottleManagerInfoBarUiTest,
                         ::testing::Values(WILL_START_REQUEST,
                                           WILL_PROCESS_RESPONSE));

#if BUILDFLAG(IS_ANDROID)
TEST_P(ContentSubresourceFilterThrottleManagerInfoBarUiTest,
       NoCrashWhenInfoBarManagerIsNotPresent) {
  auto* web_contents = RenderViewHostTestHarness::web_contents();
  web_contents->RemoveUserData(infobars::ContentInfoBarManager::UserDataKey());

  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation should be successfully filtered, and the
  // lack of infobar manager should not cause a crash.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
}
#endif

// Test that once presented, the ads blocked infobar will remain present after a
// same-document navigation.
TEST_P(ContentSubresourceFilterThrottleManagerInfoBarUiTest,
       InfoBarStaysPresentAfterSameDocumentNav) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation should be successfully filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/1/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(presenting_ads_blocked_infobar());
#endif

  // Commit another navigation that triggers page level activation.
  GURL url2 = GURL(base::StringPrintf("%s#ref", kTestURLWithActivation));
  CreateTestNavigation(url2, main_rfh());
  navigation_simulator()->CommitSameDocument();

  // Same-document navigations do not pass through ReadyToCommitNavigation so no
  // ActivateForNextCommittedLoad mojo call is expected.
  ExpectActivationSignalForFrame(main_rfh(), false /* expect_activation */,
                                 false /* expect_is_ad_frame */,
                                 false /* expect_activation_sent_to_agent */);

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(presenting_ads_blocked_infobar());
#endif

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/2/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(presenting_ads_blocked_infobar());
#endif
}

// This should fail if the throttle manager notifies the delegate twice of a
// disallowed load for the same page load.
TEST_P(ContentSubresourceFilterThrottleManagerInfoBarUiTest,
       ActivateMainFrameAndFilterTwoSubframeNavigations) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation should be successfully filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/1/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(presenting_ads_blocked_infobar());
#endif

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/2/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(presenting_ads_blocked_infobar());
#endif
}

TEST_P(ContentSubresourceFilterThrottleManagerInfoBarUiTest,
       ActivateTwoMainFramesAndFilterTwoSubframeNavigations) {
  // Commit a navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  // A disallowed subframe navigation should be successfully filtered.
  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/1/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(presenting_ads_blocked_infobar());
#endif

  // Commit another navigation that triggers page level activation.
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation2));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  EXPECT_FALSE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(presenting_ads_blocked_infobar());
#endif

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/2/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(presenting_ads_blocked_infobar());
#endif
}

// Once there are no activated frames, the manager drops its ruleset handle. If
// another frame is activated, make sure the handle is regenerated.
TEST_P(ContentSubresourceFilterThrottleManagerInfoBarUiTest,
       RulesetHandleRegeneration) {
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(presenting_ads_blocked_infobar());
#endif

  // Simulate a renderer crash which should delete the frame.
  EXPECT_TRUE(ManagerHasRulesetHandle());
  process()->SimulateCrash();
  EXPECT_FALSE(ManagerHasRulesetHandle());

  NavigateAndCommit(GURL("https://example.reset"));
  NavigateAndCommitMainFrame(GURL(kTestURLWithActivation));
  ExpectActivationSignalForFrame(main_rfh(), true /* expect_activation */);

  EXPECT_FALSE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(presenting_ads_blocked_infobar());
#endif

  CreateSubframeWithTestNavigation(
      GURL("https://www.example.com/disallowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateStartAndGetResult(navigation_simulator()));

  EXPECT_TRUE(ads_blocked_in_content_settings());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(presenting_ads_blocked_infobar());
#endif
}

// TODO(csharrison): Make sure the following conditions are exercised in tests:
//
// - Synchronous navigations to about:blank. These hit issues with the
//   NavigationSimulator currently.

}  // namespace subresource_filter
