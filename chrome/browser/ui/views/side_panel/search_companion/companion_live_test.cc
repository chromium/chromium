// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/companion/core/companion_metrics_logger.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "chrome/browser/companion/core/proto/companion_url_params.pb.h"
#include "chrome/browser/signin/e2e_tests/account_capabilities_observer.h"
#include "chrome/browser/signin/e2e_tests/accounts_removed_waiter.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/sign_in_test_observer.h"
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/side_panel/companion/companion_tab_helper.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace signin::test {

// Live tests for Companion.
// These tests can be run with:
// browser_tests --gtest_filter=LiveSignInTest.* --run-live-tests --run-manual
class CompanionLiveTest : public signin::test::LiveTest {
 public:
  CompanionLiveTest() = default;
  ~CompanionLiveTest() override = default;

  void SetUp() override {
    SetUpFeatureList();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    LiveTest::SetUp();
    // Always disable animation for stability.
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

  SidePanelCoordinator* side_panel_coordinator() {
    return SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  }

  syncer::SyncService* sync_service() {
    return signin::test::sync_service(browser());
  }

  SignInFunctions sign_in_functions = SignInFunctions(
      base::BindLambdaForTesting(
          [this]() -> Browser* { return this->browser(); }),
      base::BindLambdaForTesting([this](int index,
                                        const GURL& url,
                                        ui::PageTransition transition) -> bool {
        return this->AddTabAtIndex(index, url, transition);
      }));
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::WebContents* GetCompanionWebContents(Browser* browser) {
    auto* companion_helper =
        companion::CompanionTabHelper::FromWebContents(web_contents());
    auto* web_contents = companion_helper->GetCompanionWebContentsForTesting();
    DCHECK(web_contents);
    return web_contents;
  }

  content::EvalJsResult EvalJs(const std::string& code) {
    // Execute test in iframe.
    content::RenderFrameHost* iframe =
        content::ChildFrameAt(GetCompanionWebContents(browser()), 0);

    return content::EvalJs(iframe, code);
  }

  ::testing::AssertionResult ExecJs(const std::string& code) {
    // Execute test in iframe.
    content::RenderFrameHost* iframe =
        content::ChildFrameAt(GetCompanionWebContents(browser()), 0);

    return content::ExecJs(iframe, code);
  }

  void ClickButtonByAriaLabel(const std::string& aria_label) {
    // Clicks a button in the side panel by its aria-label attribute.
    std::string js_string = "document.querySelectorAll('button[aria-label=\"" +
                            aria_label + "\"]')[0].click();";
    EXPECT_TRUE(ExecJs(js_string));
  }

  void WaitForCompanionToBeLoaded() {
    content::WebContents* companion_web_contents =
        GetCompanionWebContents(browser());
    EXPECT_TRUE(companion_web_contents);

    // Wait for the navigations in both the frames to complete.
    content::TestNavigationObserver nav_observer(companion_web_contents, 2);
    nav_observer.Wait();
  }

  void WaitForCompanionIframeReload() {
    content::WebContents* companion_web_contents =
        GetCompanionWebContents(browser());
    EXPECT_TRUE(companion_web_contents);

    // Wait for the navigations in the inner iframe to complete.
    content::TestNavigationObserver nav_observer(companion_web_contents, 1);
    nav_observer.Wait();
  }

  void WaitForHistogram(const std::string& histogram_name) {
    // Continue if histogram was already recorded.
    if (base::StatisticsRecorder::FindHistogram(histogram_name)) {
      return;
    }

    // Else, wait until the histogram is recorded.
    base::RunLoop run_loop;
    auto histogram_observer = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        histogram_name,
        base::BindLambdaForTesting(
            [&](const char* histogram_name, uint64_t name_hash,
                base::HistogramBase::Sample sample) { run_loop.Quit(); }));
    run_loop.Run();
  }

  void WaitForHistogramSample(const std::string& histogram_name,
                              base::HistogramBase::Sample expected_sample) {
    // Continue if histogram sample was already recorded.
    if (base::StatisticsRecorder::FindHistogram(histogram_name) &&
        histogram_tester_->GetBucketCount(histogram_name, expected_sample)) {
      return;
    }

    // Else, wait until a new sample is recorded for the histogram.
    base::RunLoop run_loop;
    auto histogram_observer = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        histogram_name,
        base::BindLambdaForTesting(
            [&](const char* histogram_name, uint64_t name_hash,
                base::HistogramBase::Sample sample) { run_loop.Quit(); }));
    run_loop.Run();
  }

  void WaitForTabCount(int expected) {
    while (browser()->tab_strip_model()->count() != expected) {
      base::RunLoop().RunUntilIdle();
    }
  }

  void EnableMsbb(bool enable_msbb) {
    if (enable_msbb) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          companion::switches::kDisableCheckUserPermissionsForCompanion);
    } else {
      base::CommandLine::ForCurrentProcess()->RemoveSwitch(
          companion::switches::kDisableCheckUserPermissionsForCompanion);
    }
  }

  virtual void SetUpFeatureList() {
    base::FieldTrialParams params;
    const GURL lens_url("https://lens.google.com/companion");
    params["companion-homepage-url"] = lens_url.spec();
    feature_list_.InitAndEnableFeatureWithParameters(
        companion::features::internal::kSidePanelCompanion, params);

    EnableMsbb(true);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Test will only run when passed the --run-live-tests flag. To run, use
// browser_tests --gtest_filter=CompanionLiveTest.* --run-live-tests
IN_PROC_BROWSER_TEST_F(CompanionLiveTest, InitialNavigation) {
  // Navigate to a website, open the side panel, and verify that companion
  // experiments appear in the side panel for an opted in account.
  TestAccount ta;
  // Test account is opted in to labs.
  CHECK(GetTestAccountsUtil()->GetAccount("INTELLIGENCE_ACCOUNT", ta));
  sign_in_functions.SignInFromWeb(ta, 0);

  // Navigate to google.com and open side panel.
  const GURL google_url("https://google.com/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), google_url));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Verify that CQ loads.
  WaitForHistogram("Companion.CQ.Shown");
  histogram_tester_->ExpectBucketCount("Companion.CQ.Shown",
                                       /*sample=*/true, /*expected_count=*/1);
  // Close the side panel.
  side_panel_coordinator()->Close();
  WaitForHistogram("SidePanel.OpenDuration");
}

IN_PROC_BROWSER_TEST_F(CompanionLiveTest, InitialNavigationNotOptedIn) {
  // Navigate to a website, open the side panel, and verify that companion
  // experiments do not appear in the side panel for a non-opted in account.
  TestAccount ta;
  // Test account has not opted in to labs.
  CHECK(GetTestAccountsUtil()->GetAccount("INTELLIGENCE_ACCOUNT_2", ta));
  sign_in_functions.SignInFromWeb(ta, 0);

  // Ensure sync is on.
  sign_in_functions.TurnOnSync(ta, 0);
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());

  // Navigate to google.com and open side panel.
  const GURL google_url("https://google.com/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), google_url));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  WaitForCompanionToBeLoaded();

  // Verify the kExpsShown promo event is shown and that CQ does not load.
  WaitForHistogramSample("Companion.PromoEvent",
                         (int)companion::PromoEvent::kExpsShown);
  histogram_tester_->ExpectBucketCount(
      "Companion.PromoEvent",
      /*sample=*/companion::PromoEvent::kExpsShown, /*expected_count=*/1);
  histogram_tester_->ExpectTotalCount("Companion.CQ.Shown", 0);

  // Close the side panel.
  side_panel_coordinator()->Close();
  WaitForHistogram("SidePanel.OpenDuration");
}

IN_PROC_BROWSER_TEST_F(CompanionLiveTest, InitialNavigationLoggedOut) {
  // Navigate to a website, open the side panel, and ensure the sign in promo is
  // shown for a logged out account. Verify the sign-in promo functionality.
  EnableMsbb(false);
  const GURL google_url("https://google.com/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), google_url));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Expect the sign-in promo and no CQ shown.
  WaitForHistogramSample("Companion.PromoEvent",
                         (int)companion::PromoEvent::kSignInShown);
  histogram_tester_->ExpectBucketCount(
      "Companion.PromoEvent",
      /*sample=*/companion::PromoEvent::kSignInShown, /*expected_count=*/1);
  histogram_tester_->ExpectTotalCount("Companion.CQ.Shown", 0);

  // Click on sign-in promo and expect sign in site in new tab.
  int tab_count = browser()->tab_strip_model()->count();
  ClickButtonByAriaLabel("Sign in button");
  WaitForTabCount(tab_count + 1);

  // Wait for page to load.
  content::TestNavigationObserver nav_observer(web_contents(), 1);
  nav_observer.Wait();

  // Verify that the sign-in page appears and PromoEvent histogram is updated.
  EXPECT_THAT(web_contents()->GetLastCommittedURL().spec(),
              ::testing::HasSubstr("accounts.google.com/signin"));
  WaitForHistogramSample("Companion.PromoEvent",
                         (int)companion::PromoEvent::kSignInAccepted);
  histogram_tester_->ExpectBucketCount(
      "Companion.PromoEvent",
      /*sample=*/companion::PromoEvent::kSignInAccepted,
      /*expected_count=*/1);

  // Close the side panel.
  side_panel_coordinator()->Close();
  WaitForHistogram("SidePanel.OpenDuration");
}

IN_PROC_BROWSER_TEST_F(CompanionLiveTest, SearchBox) {
  // Navigate to a website, open the side panel, and ensure that the multi-modal
  // search box functions as intended.
  TestAccount ta;
  // Test account has opted in to labs.
  CHECK(GetTestAccountsUtil()->GetAccount("INTELLIGENCE_ACCOUNT", ta));
  sign_in_functions.SignInFromWeb(ta, 0);
  const GURL google_url("https://google.com/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), google_url));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  WaitForCompanionToBeLoaded();

  // Ensure multimodal search box is present.
  std::string search("Search");
  ASSERT_EQ(EvalJs("document.querySelectorAll('input')[0].placeholder"),
            search);

  // Conduct a side search.
  ExecJs("document.querySelectorAll('input')[0].value = 'test search';");
  ClickButtonByAriaLabel("Search");
  WaitForHistogram("Companion.SearchBox.Clicked");
  histogram_tester_->ExpectBucketCount("Companion.SearchBox.Clicked",
                                       /*sample=*/true, /*expected_count=*/1);

  // Return to zero state.
  ClickButtonByAriaLabel("Back");
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Click the region search button.
  ClickButtonByAriaLabel("Search by image");
  WaitForHistogram("Companion.RegionSearch.Clicked");
  histogram_tester_->ExpectBucketCount("Companion.RegionSearch.Clicked",
                                       /*sample=*/true, /*expected_count=*/1);
}

}  // namespace signin::test
