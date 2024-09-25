// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/safe_browsing_triggered_popup_blocker.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/blocked_content/popup_blocker.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/blocked_content/popup_navigation_delegate.h"
#include "components/blocked_content/test/test_popup_navigation_delegate.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/browser/test_page_specific_content_settings_delegate.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"
#include "components/subresource_filter/content/browser/safe_browsing_page_activation_throttle.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"
#include "components/subresource_filter/content/shared/browser/utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace blocked_content {

class SafeBrowsingTriggeredPopupBlockerTestBase
    : public content::RenderViewHostTestHarness {
 public:
  SafeBrowsingTriggeredPopupBlockerTestBase() = default;

  SafeBrowsingTriggeredPopupBlockerTestBase(
      const SafeBrowsingTriggeredPopupBlockerTestBase&) = delete;
  SafeBrowsingTriggeredPopupBlockerTestBase& operator=(
      const SafeBrowsingTriggeredPopupBlockerTestBase&) = delete;

  ~SafeBrowsingTriggeredPopupBlockerTestBase() override {
    settings_map_->ShutdownOnUIThread();
  }

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    fake_safe_browsing_database_ =
        base::MakeRefCounted<FakeSafeBrowsingDatabaseManager>();

    user_prefs::UserPrefs::Set(browser_context(), &pref_service_);
    SafeBrowsingTriggeredPopupBlocker::RegisterProfilePrefs(
        pref_service_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(pref_service_.registry());
    settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        &pref_service_, false /* is_off_the_record */,
        false /* store_last_modified */, false /* restore_session*/,
        false /* should_record_metrics */);

    subresource_filter::SubresourceFilterObserverManager::CreateForWebContents(
        web_contents());
    PopupBlockerTabHelper::CreateForWebContents(web_contents());
    content_settings::PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<
            content_settings::TestPageSpecificContentSettingsDelegate>(
            /*prefs=*/nullptr, settings_map_.get()));
    popup_blocker_ =
        SafeBrowsingTriggeredPopupBlocker::FromWebContents(web_contents());

    throttle_inserter_ =
        std::make_unique<content::TestNavigationThrottleInserter>(
            web_contents(),
            base::BindRepeating(
                &SafeBrowsingTriggeredPopupBlockerTestBase::CreateThrottle,
                base::Unretained(this)));
  }
  void TearDown() override {
    popup_blocker_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }
  FakeSafeBrowsingDatabaseManager* fake_safe_browsing_database() {
    return fake_safe_browsing_database_.get();
  }

  SafeBrowsingTriggeredPopupBlocker* popup_blocker() { return popup_blocker_; }

  void SimulateDeleteContents() {
    DeleteContents();
    popup_blocker_ = nullptr;
  }

  void MarkUrlAsAbusiveWithLevel(const GURL& url,
                                 safe_browsing::SubresourceFilterLevel level) {
    safe_browsing::ThreatMetadata metadata;
    metadata.subresource_filter_match
        [safe_browsing::SubresourceFilterType::ABUSIVE] = level;
    fake_safe_browsing_database()->AddBlocklistedUrl(
        url, safe_browsing::SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER,
        metadata);
  }

  void MarkUrlAsAbusiveEnforce(const GURL& url) {
    MarkUrlAsAbusiveWithLevel(url,
                              safe_browsing::SubresourceFilterLevel::ENFORCE);
  }

  void MarkUrlAsAbusiveWarning(const GURL& url) {
    MarkUrlAsAbusiveWithLevel(url, safe_browsing::SubresourceFilterLevel::WARN);
  }

  const std::vector<std::string>& GetMainFrameConsoleMessages() {
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(main_rfh());
    return rfh_tester->GetConsoleMessages();
  }

  HostContentSettingsMap* settings_map() { return settings_map_.get(); }

 protected:
  std::unique_ptr<content::NavigationThrottle> CreateThrottle(
      content::NavigationHandle* handle) {
    // Activation is only computed when navigating a subresource filter root
    // (see content_subresource_filter_throttle_manager.h for the definition of
    // a root).
    if (subresource_filter::IsInSubresourceFilterRoot(handle)) {
      return std::make_unique<
          subresource_filter::SafeBrowsingPageActivationThrottle>(
          handle, /*delegate=*/nullptr, fake_safe_browsing_database_);
    }

    return nullptr;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  scoped_refptr<FakeSafeBrowsingDatabaseManager> fake_safe_browsing_database_;
  raw_ptr<SafeBrowsingTriggeredPopupBlocker> popup_blocker_ = nullptr;
  std::unique_ptr<content::TestNavigationThrottleInserter> throttle_inserter_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
};

class SafeBrowsingTriggeredPopupBlockerTest
    : public SafeBrowsingTriggeredPopupBlockerTestBase {
 public:
  SafeBrowsingTriggeredPopupBlockerTest() {
    scoped_feature_list_.InitAndEnableFeature(kAbusiveExperienceEnforce);
  }
};

struct RedirectSamplesAndResults {
  GURL initial_url;
  GURL redirect_url;
  bool expect_strong_blocker;
};

// We always make our decision to trigger on the last entry in the chain.
TEST_F(SafeBrowsingTriggeredPopupBlockerTest,
       MatchOnSafeBrowsingWithRedirectChain) {
  GURL enforce_url("https://example.enforce");
  GURL warning_url("https://example.warning");
  GURL regular_url("https://example.regular");
  MarkUrlAsAbusiveEnforce(enforce_url);
  MarkUrlAsAbusiveWarning(warning_url);

  const RedirectSamplesAndResults kTestCases[] = {
      {enforce_url, regular_url, false},
      {regular_url, enforce_url, true},
      {warning_url, enforce_url, true},
      {enforce_url, warning_url, false}};

  for (const auto& test_case : kTestCases) {
    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(
            test_case.initial_url, web_contents()->GetPrimaryMainFrame());
    simulator->Start();
    simulator->Redirect(test_case.redirect_url);
    simulator->Commit();
    EXPECT_EQ(test_case.expect_strong_blocker,
              popup_blocker()->ShouldApplyAbusivePopupBlocker(
                  web_contents()->GetPrimaryPage()));
  }
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest, MatchingURL_BlocksPopupAndLogs) {
  const GURL url("https://example.test/");
  MarkUrlAsAbusiveEnforce(url);
  NavigateAndCommit(url);
  EXPECT_TRUE(GetMainFrameConsoleMessages().empty());

  EXPECT_TRUE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));
  EXPECT_EQ(1u, GetMainFrameConsoleMessages().size());
  EXPECT_EQ(GetMainFrameConsoleMessages().front(), kAbusiveEnforceMessage);
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest,
       MatchingURL_BlocksPopupFromOpenURL) {
  const GURL url("https://example.test/");
  MarkUrlAsAbusiveEnforce(url);
  NavigateAndCommit(url);

  // If the popup is coming from OpenURL params, the strong popup blocker is
  // only going to look at the triggering event info. It will only block the
  // popup if we know the triggering event is untrusted.
  GURL popup_url("https://example.popup/");
  content::OpenURLParams params(
      popup_url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, true /* is_renderer_initiated */);
  params.user_gesture = true;
  params.triggering_event_info =
      blink::mojom::TriggeringEventInfo::kFromUntrustedEvent;
  params.source_render_frame_id = main_rfh()->GetRoutingID();
  params.source_render_process_id = main_rfh()->GetProcess()->GetID();

  MaybeBlockPopup(web_contents(), nullptr,
                  std::make_unique<TestPopupNavigationDelegate>(
                      popup_url, nullptr /* result_holder */),
                  &params, blink::mojom::WindowFeatures(), settings_map());

  EXPECT_EQ(1u, PopupBlockerTabHelper::FromWebContents(web_contents())
                    ->GetBlockedPopupsCount());
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest,
       MatchingURLTrusted_DoesNotBlockPopup) {
  const GURL url("https://example.test/");
  MarkUrlAsAbusiveEnforce(url);
  NavigateAndCommit(url);

  // If the popup is coming from OpenURL params, the strong popup blocker is
  // only going to look at the triggering event info. It will only block the
  // popup if we know the triggering event is untrusted.
  GURL popup_url("https://example.popup/");
  content::OpenURLParams params(
      popup_url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, true /* is_renderer_initiated */);
  params.user_gesture = true;
  params.triggering_event_info =
      blink::mojom::TriggeringEventInfo::kFromTrustedEvent;

  MaybeBlockPopup(web_contents(), nullptr,
                  std::make_unique<TestPopupNavigationDelegate>(
                      popup_url, nullptr /* result_holder */),
                  &params, blink::mojom::WindowFeatures(), settings_map());

  EXPECT_EQ(0u, PopupBlockerTabHelper::FromWebContents(web_contents())
                    ->GetBlockedPopupsCount());
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest, NoMatch_NoBlocking) {
  const GURL url("https://example.test/");
  NavigateAndCommit(url);
  EXPECT_FALSE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));
  EXPECT_TRUE(GetMainFrameConsoleMessages().empty());
}

class SafeBrowsingTriggeredPopupBlockerDefaultTest
    : public SafeBrowsingTriggeredPopupBlockerTestBase {};

TEST_F(SafeBrowsingTriggeredPopupBlockerDefaultTest, FeatureEnabledByDefault) {
  SafeBrowsingTriggeredPopupBlocker::MaybeCreate(web_contents());
  EXPECT_NE(nullptr,
            SafeBrowsingTriggeredPopupBlocker::FromWebContents(web_contents()));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest, OnlyBlockOnMatchingUrls) {
  const GURL url1("https://example.first/");
  const GURL url2("https://example.second/");
  const GURL url3("https://example.third/");
  // Only mark url2 as abusive.
  MarkUrlAsAbusiveEnforce(url2);

  NavigateAndCommit(url1);
  EXPECT_FALSE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));

  NavigateAndCommit(url2);
  EXPECT_TRUE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));

  NavigateAndCommit(url3);
  EXPECT_FALSE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));

  NavigateAndCommit(url1);
  EXPECT_FALSE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest,
       SameDocumentNavigation_MaintainsBlocking) {
  const GURL url("https://example.first/");
  const GURL hash_url("https://example.first/#hash");

  MarkUrlAsAbusiveEnforce(url);
  NavigateAndCommit(url);
  EXPECT_TRUE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));

  // This is merely a same document navigation, keep the popup blocker.
  NavigateAndCommit(hash_url);
  EXPECT_TRUE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest,
       FailNavigation_MaintainsBlocking) {
  const GURL url("https://example.first/");
  const GURL fail_url("https://example.fail/");

  MarkUrlAsAbusiveEnforce(url);
  NavigateAndCommit(url);
  EXPECT_TRUE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));

  // Abort the navigation before it commits.
  content::NavigationSimulator::NavigateAndFailFromDocument(
      fail_url, net::ERR_ABORTED, main_rfh());
  EXPECT_TRUE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));

  // Committing an error page should probably reset the blocker though, despite
  // the fact that it is probably a bug for an error page to spawn popups.
  content::NavigationSimulator::NavigateAndFailFromDocument(
      fail_url, net::ERR_CONNECTION_RESET, main_rfh());
  EXPECT_FALSE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest, LogActions) {
  base::HistogramTester histogram_tester;
  const char kActionHistogram[] = "ContentSettings.Popups.StrongBlockerActions";
  int total_count = 0;
  // Call this when a new histogram entry is logged. Call it multiple times if
  // multiple entries are logged.
  auto check_histogram = [&](SafeBrowsingTriggeredPopupBlocker::Action action,
                             int expected_count) {
    histogram_tester.ExpectBucketCount(
        kActionHistogram, static_cast<int>(action), expected_count);
    total_count++;
  };

  const GURL url_enforce("https://example.enforce/");
  const GURL url_warn("https://example.warn/");
  const GURL url_nothing("https://example.nothing/");
  MarkUrlAsAbusiveEnforce(url_enforce);
  MarkUrlAsAbusiveWarning(url_warn);

  // Navigate to an enforce site.
  NavigateAndCommit(url_enforce);
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kNavigation, 1);
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kEnforcedSite, 1);
  histogram_tester.ExpectTotalCount(kActionHistogram, total_count);

  // Block two popups.
  EXPECT_TRUE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kConsidered, 1);
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kBlocked, 1);
  histogram_tester.ExpectTotalCount(kActionHistogram, total_count);

  EXPECT_TRUE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kConsidered, 2);
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kBlocked, 2);
  histogram_tester.ExpectTotalCount(kActionHistogram, total_count);

  // Navigate to a warn site.
  NavigateAndCommit(url_warn);

  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kNavigation, 2);
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kWarningSite, 1);
  histogram_tester.ExpectTotalCount(kActionHistogram, total_count);

  // Let one popup through.
  EXPECT_FALSE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kConsidered, 3);
  histogram_tester.ExpectTotalCount(kActionHistogram, total_count);

  // Navigate to a site not matched in Safe Browsing.
  NavigateAndCommit(url_nothing);
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kNavigation, 3);
  histogram_tester.ExpectTotalCount(kActionHistogram, total_count);

  // Let one popup through.
  EXPECT_FALSE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kConsidered, 4);
  histogram_tester.ExpectTotalCount(kActionHistogram, total_count);
}

class SafeBrowsingTriggeredPopupBlockerFilterAdsDisabledTest
    : public SafeBrowsingTriggeredPopupBlockerTestBase {
 public:
  SafeBrowsingTriggeredPopupBlockerFilterAdsDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        subresource_filter::kFilterAdsOnAbusiveSites);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SafeBrowsingTriggeredPopupBlockerFilterAdsDisabledTest,
       WarningMatchWithoutAdBlockOnAbusiveSites_OnlyLogs) {
  const GURL url("https://example.test/");
  MarkUrlAsAbusiveWarning(url);
  NavigateAndCommit(url);

  // Warning should come at navigation commit time, not at popup time.
  EXPECT_EQ(1u, GetMainFrameConsoleMessages().size());
  EXPECT_EQ(GetMainFrameConsoleMessages().front(), kAbusiveWarnMessage);

  EXPECT_FALSE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));
}

class SafeBrowsingTriggeredPopupBlockerFilterAdsEnabledTest
    : public SafeBrowsingTriggeredPopupBlockerTestBase {
 public:
  SafeBrowsingTriggeredPopupBlockerFilterAdsEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(
        subresource_filter::kFilterAdsOnAbusiveSites);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SafeBrowsingTriggeredPopupBlockerFilterAdsEnabledTest,
       WarningMatchWithAdBlockOnAbusiveSites_OnlyLogs) {
  const GURL url("https://example.test/");
  MarkUrlAsAbusiveWarning(url);
  NavigateAndCommit(url);

  // Warning should come at navigation commit time, not at popup time.
  EXPECT_EQ(2u, GetMainFrameConsoleMessages().size());
  EXPECT_EQ(GetMainFrameConsoleMessages().front(), kAbusiveWarnMessage);
  EXPECT_EQ(GetMainFrameConsoleMessages().back(),
            subresource_filter::kActivationWarningConsoleMessage);

  EXPECT_FALSE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      web_contents()->GetPrimaryPage()));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest, NonPrimaryFrameTree) {
  const GURL url1("https://example.first/");
  const GURL url2("https://example.second/");
  // Only mark url2 as abusive.
  MarkUrlAsAbusiveEnforce(url2);

  {
    // Simulate a navigation in the primary main frame to an url not marked as
    // abusive.
    content::MockNavigationHandle handle(url1, main_rfh());
    handle.set_has_committed(true);
    auto throttle = CreateThrottle(&handle);
    auto result = throttle->WillProcessResponse();
    if (result.action() == content::NavigationThrottle::ThrottleAction::DEFER) {
      base::RunLoop loop;
      throttle->set_resume_callback_for_testing(loop.QuitClosure());
      loop.Run();
    }
    popup_blocker()->DidFinishNavigation(&handle);
    EXPECT_FALSE(
        popup_blocker()->ShouldApplyAbusivePopupBlocker(main_rfh()->GetPage()));
  }

  {
    // Reset the state.
    NavigateAndCommit(url1);
    EXPECT_FALSE(
        popup_blocker()->ShouldApplyAbusivePopupBlocker(main_rfh()->GetPage()));

    // Simulate a navigation in the primary main frame to an url marked as
    // abusive.
    content::MockNavigationHandle handle(url2, main_rfh());
    handle.set_has_committed(true);
    auto throttle = CreateThrottle(&handle);
    auto result = throttle->WillProcessResponse();
    if (result.action() == content::NavigationThrottle::ThrottleAction::DEFER) {
      base::RunLoop loop;
      throttle->set_resume_callback_for_testing(loop.QuitClosure());
      loop.Run();
    }
    popup_blocker()->DidFinishNavigation(&handle);
    EXPECT_TRUE(
        popup_blocker()->ShouldApplyAbusivePopupBlocker(main_rfh()->GetPage()));
  }

  {
    // Reset the state.
    NavigateAndCommit(url1);
    EXPECT_FALSE(
        popup_blocker()->ShouldApplyAbusivePopupBlocker(main_rfh()->GetPage()));

    // Simulate a navigation in a non-primary main frame to an url marked as
    // abusive.
    content::MockNavigationHandle handle(url2, main_rfh());
    handle.set_has_committed(true);
    handle.set_is_in_primary_main_frame(false);
    auto throttle = CreateThrottle(&handle);
    auto result = throttle->WillProcessResponse();
    if (result.action() == content::NavigationThrottle::ThrottleAction::DEFER) {
      base::RunLoop loop;
      throttle->set_resume_callback_for_testing(loop.QuitClosure());
      loop.Run();
    }
    popup_blocker()->DidFinishNavigation(&handle);
    EXPECT_TRUE(
        popup_blocker()->ShouldApplyAbusivePopupBlocker(main_rfh()->GetPage()));
  }
}

class SafeBrowsingTriggeredPopupBlockerFencedFrameTest
    : public SafeBrowsingTriggeredPopupBlockerTest {
 public:
  SafeBrowsingTriggeredPopupBlockerFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~SafeBrowsingTriggeredPopupBlockerFencedFrameTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensures that the popup blocker is not triggered by a fenced frame.
TEST_F(SafeBrowsingTriggeredPopupBlockerFencedFrameTest,
       ShouldNotTriggerPopupBlocker) {
  const GURL url("https://example.test/");
  MarkUrlAsAbusiveEnforce(url);
  NavigateAndCommit(url);

  // The popup blocker is triggered for a primary page.
  EXPECT_TRUE(
      popup_blocker()->ShouldApplyAbusivePopupBlocker(main_rfh()->GetPage()));

  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();

  // Navigate a fenced frame.
  const GURL fenced_frame_url("https://fencedframe.test");
  MarkUrlAsAbusiveEnforce(fenced_frame_url);
  std::unique_ptr<content::NavigationSimulator> navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(fenced_frame_url,
                                                            fenced_frame_root);
  navigation_simulator->Commit();
  fenced_frame_root = navigation_simulator->GetFinalRenderFrameHost();

  // The popup blocker is not triggered for a fenced frame.
  EXPECT_FALSE(popup_blocker()->ShouldApplyAbusivePopupBlocker(
      fenced_frame_root->GetPage()));
}

}  // namespace blocked_content
