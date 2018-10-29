// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/safe_browsing_triggered_popup_blocker.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_triggering_event_info.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

const char kNumBlockedHistogram[] =
    "ContentSettings.Popups.StrongBlocker.NumBlocked";

class SafeBrowsingTriggeredPopupBlockerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SafeBrowsingTriggeredPopupBlockerTest() {}
  ~SafeBrowsingTriggeredPopupBlockerTest() override {}

  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    system_request_context_getter_ =
        base::MakeRefCounted<net::TestURLRequestContextGetter>(
            base::CreateSingleThreadTaskRunnerWithTraits(
                {content::BrowserThread::IO}));
    TestingBrowserProcess::GetGlobal()->SetSystemRequestContext(
        system_request_context_getter_.get());
    // Set up safe browsing service with the fake database manager.
    //
    // TODO(csharrison): This is a bit ugly. See if the instructions in
    // test_safe_browsing_service.h can be adapted to be used in unit tests.
    safe_browsing::TestSafeBrowsingServiceFactory sb_service_factory;
    fake_safe_browsing_database_ = new FakeSafeBrowsingDatabaseManager();
    sb_service_factory.SetTestDatabaseManager(
        fake_safe_browsing_database_.get());
    safe_browsing::SafeBrowsingService::RegisterFactory(&sb_service_factory);
    auto* safe_browsing_service =
        sb_service_factory.CreateSafeBrowsingService();
    safe_browsing::SafeBrowsingService::RegisterFactory(nullptr);
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        safe_browsing_service);
    g_browser_process->safe_browsing_service()->Initialize();

    // Required for the safe browsing notifications on navigation.
    ChromeSubresourceFilterClient::CreateForWebContents(web_contents());

    scoped_feature_list_ = DefaultFeatureList();
    SafeBrowsingTriggeredPopupBlocker::MaybeCreate(web_contents());
    popup_blocker_ =
        SafeBrowsingTriggeredPopupBlocker::FromWebContents(web_contents());
  }

  void TearDown() override {
    fake_safe_browsing_database_ = nullptr;
    TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();

    // Must explicitly set these to null and pump the run loop to ensure that
    // all cleanup related to these classes actually happens.
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    base::RunLoop().RunUntilIdle();
    TestingBrowserProcess::GetGlobal()->SetSystemRequestContext(nullptr);
    system_request_context_getter_ = nullptr;

    ChromeRenderViewHostTestHarness::TearDown();
  }

  virtual std::unique_ptr<base::test::ScopedFeatureList> DefaultFeatureList() {
    auto feature_list = std::make_unique<base::test::ScopedFeatureList>();
    feature_list->InitAndEnableFeature(kAbusiveExperienceEnforce);
    return feature_list;
  }

  FakeSafeBrowsingDatabaseManager* fake_safe_browsing_database() {
    return fake_safe_browsing_database_.get();
  }

  base::test::ScopedFeatureList* ResetFeatureAndGet() {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    return scoped_feature_list_.get();
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
    fake_safe_browsing_database()->AddBlacklistedUrl(
        url, safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER, metadata);
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

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  scoped_refptr<FakeSafeBrowsingDatabaseManager> fake_safe_browsing_database_;
  SafeBrowsingTriggeredPopupBlocker* popup_blocker_ = nullptr;
  scoped_refptr<net::URLRequestContextGetter> system_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingTriggeredPopupBlockerTest);
};

struct RedirectSamplesAndResults {
  GURL initial_url;
  GURL redirect_url;
  bool expect_strong_blocker;
};

TEST_F(SafeBrowsingTriggeredPopupBlockerTest,
       MatchOnSafeBrowsingWithRedirectDetection) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      subresource_filter::kSafeBrowsingSubresourceFilterConsiderRedirects);

  GURL enforce_url("https://example.enforce");
  GURL warning_url("https://example.warning");
  GURL regular_url("https://example.regular");
  MarkUrlAsAbusiveEnforce(enforce_url);
  MarkUrlAsAbusiveWarning(warning_url);

  const RedirectSamplesAndResults kTestCases[] = {
      {enforce_url, regular_url, true},  {regular_url, enforce_url, true},
      {warning_url, enforce_url, true},  {enforce_url, warning_url, true},
      {regular_url, warning_url, false}, {warning_url, regular_url, false}};

  for (const auto& test_case : kTestCases) {
    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(
            test_case.initial_url, web_contents()->GetMainFrame());
    simulator->Start();
    simulator->Redirect(test_case.redirect_url);
    simulator->Commit();
    EXPECT_EQ(test_case.expect_strong_blocker,
              popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
  }
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest,
       MatchOnSafeBrowsingWithoutRedirectDetection) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      subresource_filter::kSafeBrowsingSubresourceFilterConsiderRedirects);

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
            test_case.initial_url, web_contents()->GetMainFrame());
    simulator->Start();
    simulator->Redirect(test_case.redirect_url);
    simulator->Commit();
    EXPECT_EQ(test_case.expect_strong_blocker,
              popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
  }
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest, MatchingURL_BlocksPopupAndLogs) {
  const GURL url("https://example.test/");
  MarkUrlAsAbusiveEnforce(url);
  NavigateAndCommit(url);
  EXPECT_TRUE(GetMainFrameConsoleMessages().empty());

  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
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
  content::OpenURLParams params(
      GURL("https://example.popup/"), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      true /* is_renderer_initiated */);
  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(&params));

  params.triggering_event_info =
      blink::WebTriggeringEventInfo::kFromUntrustedEvent;
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(&params));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest, NoMatch_NoBlocking) {
  const GURL url("https://example.test/");
  NavigateAndCommit(url);
  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
  EXPECT_TRUE(GetMainFrameConsoleMessages().empty());
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest, FeatureEnabledByDefault) {
  ResetFeatureAndGet();
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
  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));

  NavigateAndCommit(url2);
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));

  NavigateAndCommit(url3);
  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));

  NavigateAndCommit(url1);
  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest,
       SameDocumentNavigation_MaintainsBlocking) {
  const GURL url("https://example.first/");
  const GURL hash_url("https://example.first/#hash");

  MarkUrlAsAbusiveEnforce(url);
  NavigateAndCommit(url);
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));

  // This is merely a same document navigation, keep the popup blocker.
  NavigateAndCommit(hash_url);
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest,
       FailNavigation_MaintainsBlocking) {
  const GURL url("https://example.first/");
  const GURL fail_url("https://example.fail/");

  MarkUrlAsAbusiveEnforce(url);
  NavigateAndCommit(url);
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));

  // Abort the navigation before it commits.
  content::NavigationSimulator::NavigateAndFailFromDocument(
      fail_url, net::ERR_ABORTED, main_rfh());
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));

  // Committing an error page should probably reset the blocker though, despite
  // the fact that it is probably a bug for an error page to spawn popups.
  content::NavigationSimulator::NavigateAndFailFromDocument(
      fail_url, net::ERR_CONNECTION_RESET, main_rfh());
  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
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
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kConsidered, 1);
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kBlocked, 1);
  histogram_tester.ExpectTotalCount(kActionHistogram, total_count);

  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kConsidered, 2);
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kBlocked, 2);
  histogram_tester.ExpectTotalCount(kActionHistogram, total_count);

  // Only log the num blocked histogram after navigation.
  histogram_tester.ExpectTotalCount(kNumBlockedHistogram, 0);

  // Navigate to a warn site.
  NavigateAndCommit(url_warn);
  histogram_tester.ExpectBucketCount(kNumBlockedHistogram, 2, 1);

  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kNavigation, 2);
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kWarningSite, 1);
  histogram_tester.ExpectTotalCount(kActionHistogram, total_count);

  // Let one popup through.
  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kConsidered, 3);
  histogram_tester.ExpectTotalCount(kActionHistogram, total_count);

  // Navigate to a site not matched in Safe Browsing.
  NavigateAndCommit(url_nothing);
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kNavigation, 3);
  histogram_tester.ExpectTotalCount(kActionHistogram, total_count);

  // Let one popup through.
  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
  check_histogram(SafeBrowsingTriggeredPopupBlocker::Action::kConsidered, 4);
  histogram_tester.ExpectTotalCount(kActionHistogram, total_count);

  histogram_tester.ExpectTotalCount(kNumBlockedHistogram, 1);
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest, LogBlockMetricsOnClose) {
  base::HistogramTester histogram_tester;
  const GURL url_enforce("https://example.enforce/");
  MarkUrlAsAbusiveEnforce(url_enforce);

  NavigateAndCommit(url_enforce);
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));

  histogram_tester.ExpectTotalCount(kNumBlockedHistogram, 0);
  // Simulate deleting the web contents.
  SimulateDeleteContents();
  histogram_tester.ExpectUniqueSample(kNumBlockedHistogram, 1, 1);
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest,
       WarningMatchWithoutAdBlockOnAbusiveSites_OnlyLogs) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      subresource_filter::kFilterAdsOnAbusiveSites);

  const GURL url("https://example.test/");
  MarkUrlAsAbusiveWarning(url);
  NavigateAndCommit(url);

  // Warning should come at navigation commit time, not at popup time.
  EXPECT_EQ(1u, GetMainFrameConsoleMessages().size());
  EXPECT_EQ(GetMainFrameConsoleMessages().front(), kAbusiveWarnMessage);

  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest,
       WarningMatchWithAdBlockOnAbusiveSites_OnlyLogs) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      subresource_filter::kFilterAdsOnAbusiveSites);

  const GURL url("https://example.test/");
  MarkUrlAsAbusiveWarning(url);
  NavigateAndCommit(url);

  // Warning should come at navigation commit time, not at popup time.
  EXPECT_EQ(2u, GetMainFrameConsoleMessages().size());
  EXPECT_EQ(GetMainFrameConsoleMessages().front(), kAbusiveWarnMessage);
  EXPECT_EQ(GetMainFrameConsoleMessages().back(),
            subresource_filter::kActivationWarningConsoleMessage);

  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest, ActivationPosition) {
  // Turn on the feature to perform safebrowsing on redirects.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      subresource_filter::kSafeBrowsingSubresourceFilterConsiderRedirects);

  const GURL enforce_url("https://enforce.test/");
  const GURL warn_url("https://warn.test/");
  MarkUrlAsAbusiveEnforce(enforce_url);
  MarkUrlAsAbusiveWarning(warn_url);

  using subresource_filter::ActivationPosition;
  struct {
    std::vector<const char*> urls;
    base::Optional<ActivationPosition> expected_position;
  } kTestCases[] = {
      {{"https://normal.test/"}, base::nullopt},
      {{"https://enforce.test/"}, ActivationPosition::kOnly},
      {{"https://warn.test/"}, ActivationPosition::kOnly},

      {{"https://normal.test/", "https://warn.test/"},
       ActivationPosition::kLast},
      {{"https://normal.test/", "https://normal.test/",
        "https://enforce.test/"},
       ActivationPosition::kLast},

      {{"https://enforce.test", "https://normal.test/", "https://warn.test/"},
       ActivationPosition::kFirst},
      {{"https://warn.test/", "https://normal.test/"},
       ActivationPosition::kFirst},

      {{"https://normal.test/", "https://enforce.test/",
        "https://normal.test/"},
       base::nullopt},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;
    const GURL& first_url = GURL(test_case.urls.front());
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(first_url,
                                                              main_rfh());
    for (size_t i = 1; i < test_case.urls.size(); ++i) {
      navigation_simulator->Redirect(GURL(test_case.urls[i]));
    }
    navigation_simulator->Commit();

    histograms.ExpectTotalCount(
        "ContentSettings.Popups.StrongBlockerActivationPosition",
        test_case.expected_position.has_value() ? 1 : 0);
    if (test_case.expected_position.has_value()) {
      histograms.ExpectUniqueSample(
          "ContentSettings.Popups.StrongBlockerActivationPosition",
          static_cast<int>(test_case.expected_position.value()), 1);
    }
  }
}
