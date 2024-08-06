// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_test_harness.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_web_contents_helper.h"
#include "components/subresource_filter/content/browser/safe_browsing_ruleset_publisher.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/constants.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

constexpr char const SubresourceFilterTestHarness::kDefaultAllowedSuffix[];
constexpr char const SubresourceFilterTestHarness::kDefaultDisallowedSuffix[];
constexpr char const SubresourceFilterTestHarness::kDefaultDisallowedUrl[];

SubresourceFilterTestHarness::SubresourceFilterTestHarness() = default;
SubresourceFilterTestHarness::~SubresourceFilterTestHarness() = default;

// content::RenderViewHostTestHarness:
void SubresourceFilterTestHarness::SetUp() {
  content::RenderViewHostTestHarness::SetUp();

  // Set up prefs-related state needed by various tests.
  user_prefs::UserPrefs::Set(browser_context(), &pref_service_);

  // Ensure correct features.
  scoped_configuration_.ResetConfiguration(Configuration(
      mojom::ActivationLevel::kEnabled, ActivationScope::ACTIVATION_LIST,
      ActivationList::SUBRESOURCE_FILTER));

  // Set up the ruleset service.
  ASSERT_TRUE(ruleset_service_dir_.CreateUniqueTempDir());
  IndexedRulesetVersion::RegisterPrefs(pref_service_.registry(),
                                       kSafeBrowsingRulesetConfig.filter_tag);
  // TODO(csharrison): having separated blocking and background task runners
  // for |ContentRulesetService| and |RulesetService| would be a good idea, but
  // external unit tests code implicitly uses knowledge that blocking and
  // background task runners are initiazlied from
  // |base::SingleThreadTaskRunner::GetCurrentDefault()|:
  // 1. |TestRulesetPublisher| uses this knowledge in |SetRuleset| method. It
  //    is waiting for the ruleset published callback.
  // 2. Navigation simulator uses this knowledge. It knows that
  //    |AsyncDocumentSubresourceFilter| posts core initialization tasks on
  //    blocking task runner and this it is the current thread task runner.
  ruleset_service_ = std::make_unique<RulesetService>(
      kSafeBrowsingRulesetConfig, &pref_service_,
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      ruleset_service_dir_.GetPath(),
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      SafeBrowsingRulesetPublisher::Factory());

  // Publish the test ruleset.
  testing::TestRulesetCreator ruleset_creator;
  testing::TestRulesetPair test_ruleset_pair;
  ruleset_creator.CreateRulesetWithRules(
      {testing::CreateSuffixRule(kDefaultDisallowedSuffix),
       testing::CreateAllowlistSuffixRule(kDefaultAllowedSuffix)},
      &test_ruleset_pair);
  testing::TestRulesetPublisher test_ruleset_publisher(ruleset_service_.get());
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_publisher.SetRuleset(test_ruleset_pair.unindexed));

  VerifiedRulesetDealer::Handle* dealer =
      ruleset_service_.get()->GetRulesetDealer();
  throttle_manager_test_support_ =
      std::make_unique<ThrottleManagerTestSupport>(web_contents());
  database_manager_ = base::MakeRefCounted<FakeSafeBrowsingDatabaseManager>();
  infobars::ContentInfoBarManager::CreateForWebContents(web_contents());
  ContentSubresourceFilterWebContentsHelper::CreateForWebContents(
      web_contents(), throttle_manager_test_support_->profile_context(),
      database_manager_, dealer);

  // Observe web_contents() to add subresource filter navigation throttles at
  // the start of navigations.
  Observe(web_contents());

  NavigateAndCommit(GURL("https://example.first"));

  base::RunLoop().RunUntilIdle();
#if BUILDFLAG(IS_ANDROID)
  message_dispatcher_bridge_.SetMessagesEnabledForEmbedder(true);
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
#endif
}

void SubresourceFilterTestHarness::TearDown() {
  ruleset_service_.reset();

  content::RenderViewHostTestHarness::TearDown();
#if BUILDFLAG(IS_ANDROID)
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
#endif
}

// content::WebContentsObserver:
void SubresourceFilterTestHarness::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument())
    return;

  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
  ContentSubresourceFilterThrottleManager::FromNavigationHandle(
      *navigation_handle)
      ->MaybeAppendNavigationThrottles(navigation_handle, &throttles);

  AppendCustomNavigationThrottles(navigation_handle, &throttles);

  for (auto& it : throttles) {
    navigation_handle->RegisterThrottleForTesting(std::move(it));
  }
}

// Will return nullptr if the navigation fails.
content::RenderFrameHost*
SubresourceFilterTestHarness::SimulateNavigateAndCommit(
    const GURL& url,
    content::RenderFrameHost* rfh) {
  auto simulator =
      content::NavigationSimulator::CreateRendererInitiated(url, rfh);
  simulator->Commit();
  return simulator->GetLastThrottleCheckResult().action() ==
                 content::NavigationThrottle::PROCEED
             ? simulator->GetFinalRenderFrameHost()
             : nullptr;
}

// Returns the frame host the navigation commit in, or nullptr if it did not
// succeed.
content::RenderFrameHost*
SubresourceFilterTestHarness::CreateAndNavigateDisallowedSubframe(
    content::RenderFrameHost* parent) {
  auto* subframe =
      content::RenderFrameHostTester::For(parent)->AppendChild("subframe");
  return SimulateNavigateAndCommit(GURL(kDefaultDisallowedUrl), subframe);
}

void SubresourceFilterTestHarness::ConfigureAsSubresourceFilterOnlyURL(
    const GURL& url) {
  fake_safe_browsing_database()->AddBlocklistedUrl(
      url, safe_browsing::SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER);
}

void SubresourceFilterTestHarness::RemoveURLFromBlocklist(const GURL& url) {
  fake_safe_browsing_database()->RemoveBlocklistedUrl(url);
}

SubresourceFilterContentSettingsManager*
SubresourceFilterTestHarness::GetSettingsManager() {
  return throttle_manager_test_support_->profile_context()->settings_manager();
}

void SubresourceFilterTestHarness::SetIsAdFrame(
    content::RenderFrameHost* render_frame_host,
    bool is_ad_frame) {
  ContentSubresourceFilterThrottleManager::FromPage(
      render_frame_host->GetPage())
      ->SetIsAdFrameForTesting(render_frame_host, is_ad_frame);
}

}  // namespace subresource_filter
