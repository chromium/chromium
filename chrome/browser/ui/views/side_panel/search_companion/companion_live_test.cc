// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

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
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_tab_helper.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/test_accounts.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace companion {

namespace {
const std::string kExpsUrl("https://labs.google.com/search/experiments/");
const std::string kFailureUrl("https://labs.google.com/search/error");
const std::string kGoogleUrl("https://www.google.com/");
const std::string kLensUrl("https://lens.google.com/companion");
const std::string kNpsUrl("https://www.nps.gov/articles/route-66-overview.htm");
}  // namespace

// Live tests for Companion.
// These tests can be run with:
// browser_tests --gtest_filter=CompanionLiveTest.* --run-live-tests
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

  void SetUpInProcessBrowserTestFixture() override {
    // Allowlists hosts.
    host_resolver()->AllowDirectLookup("*.nps.gov");

    LiveTest::SetUpInProcessBrowserTestFixture();
  }

  SidePanelCoordinator* side_panel_coordinator() {
    return browser()->GetFeatures().side_panel_coordinator();
  }

  syncer::SyncService* sync_service() {
    return signin::test::sync_service(browser());
  }

  signin::test::SignInFunctions sign_in_functions =
      signin::test::SignInFunctions(
          base::BindLambdaForTesting(
              [this]() -> Browser* { return this->browser(); }),
          base::BindLambdaForTesting(
              [this](int index,
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

  void ClickButtonByAriaLabel(const std::string& aria_label, int index = 0) {
    // Clicks a button in the side panel by its aria-label attribute.
    std::string js_string = "document.querySelectorAll('button[aria-label=\"" +
                            aria_label + "\"]')[" +
                            base::NumberToString(index) + "].click();";
    EXPECT_TRUE(ExecJs(js_string));
  }

  void ClickButtonByInnerText(const std::string& inner_text) {
    // Clicks a button in the side panel by its inner text.
    std::string js_string =
        "Array.from(document.querySelectorAll('button')).find(el => "
        "el.textContent === '" +
        inner_text + "').click();";
    EXPECT_TRUE(ExecJs(js_string));
  }

  void EnableExps(bool enable) {
    // Enables or disables exps.
    std::string button_string =
        "document.querySelectorAll('button[aria-label=\"Switch\"]')[3]";
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExpsUrl)));

    // Certain IP Addresses are restricted from exps site.
    if (web_contents()->GetLastCommittedURL().spec() == kFailureUrl) {
      LOG(INFO) << "Unable to access exps url from this device. Skipping.";
      skip_test_ = true;
      return;
    }

    // Toggle the switch if state change is desired.
    bool already_enabled =
        content::EvalJs(web_contents(), button_string + ".ariaChecked") ==
        "true";
    // If the exps state does not match the desired state, toggle.
    if (already_enabled ^ enable) {
      LOG(INFO) << "Toggling exps from " << already_enabled << " to " << enable;
      EXPECT_TRUE(content::ExecJs(web_contents(), button_string + ".click();"));
    }
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
                              base::HistogramBase::Sample expected_sample,
                              int expected_count = 1) {
    // Continue if histogram sample was already recorded.
    if (base::StatisticsRecorder::FindHistogram(histogram_name) &&
        histogram_tester_->GetBucketCount(histogram_name, expected_sample) >=
            expected_count) {
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

  void ConfirmFeaturesShown(const std::vector<std::string>& features,
                            int bucket_count) {
    // For each feature, ensure the Companion.|feature|.Shown histogram
    // populates to |bucket_count|.
    for (const auto& feature : features) {
      if (bucket_count) {
        WaitForHistogramSample("Companion." + feature + ".Shown", true,
                               bucket_count);
      }
      histogram_tester_->ExpectBucketCount("Companion." + feature + ".Shown",
                                           /*sample=*/true,
                                           /*expected_count=*/bucket_count);
    }
  }

  void ConfirmFeaturesClicked(const std::vector<std::string>& features,
                              int bucket_count) {
    // For each feature, ensure the Companion.|feature|.Clicked histogram
    // populates to |bucket_count|.
    for (const auto& feature : features) {
      if (bucket_count) {
        WaitForHistogramSample("Companion." + feature + ".Clicked", true,
                               bucket_count);
      }
      histogram_tester_->ExpectBucketCount("Companion." + feature + ".Clicked",
                                           /*sample=*/true,
                                           /*expected_count=*/bucket_count);
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
    params["companion-homepage-url"] = kLensUrl;
    feature_list_.InitAndEnableFeatureWithParameters(
        companion::features::internal::kSidePanelCompanion, params);

    EnableMsbb(true);
  }

  void TearDown() override {
    // This test was skipped, no need to tear down.
    if (skip_test_) {
      return;
    }
    LiveTest::TearDown();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  bool skip_test_ = false;
};

// Test will only run when passed the --run-live-tests flag. To run, use
// browser_tests --gtest_filter=CompanionLiveTest.* --run-live-tests
IN_PROC_BROWSER_TEST_F(CompanionLiveTest, InitialNavigation) {
// Navigate to a website, open the side panel, and verify that companion
// experiments appear in the side panel for an opted in account. Note that
// sync and signin utilities are only supported on Windows.
#if BUILDFLAG(IS_WIN)
  std::optional<signin::TestAccountSigninCredentials> test_account =
      GetTestAccounts()->GetAccount("INTELLIGENCE_ACCOUNT");
  // Sign in to opted in test account.
  CHECK(test_account.has_value());
  sign_in_functions.TurnOnSync(*test_account, 0);
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());

  // Navigate to nps.gov article and open side panel.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kNpsUrl)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Verify that experiments load.
  std::vector<std::string> expected_features = {"ATX", "CQ", "RelQs", "RelQr"};
  ConfirmFeaturesShown(expected_features, 1);

  // Generate PH.
  ClickButtonByInnerText("Generate");
  ConfirmFeaturesShown({"PHResult"}, 1);
  ConfirmFeaturesClicked({"PH"}, 1);

  // Close the side panel.
  side_panel_coordinator()->Close();
  WaitForHistogram("SidePanel.OpenDuration");
#endif  // BUILDFLAG(IS_WIN)
}

IN_PROC_BROWSER_TEST_F(CompanionLiveTest, InitialNavigationNotOptedIn) {
// Navigate to a website, open the side panel, and verify that companion
// experiments do not appear in the side panel for a non-opted in account.
// Note that sync and signin utilities are only supported on Windows.
#if BUILDFLAG(IS_WIN)
  std::optional<signin::TestAccountSigninCredentials> test_account =
      GetTestAccounts()->GetAccount("INTELLIGENCE_ACCOUNT_2");
  // Sign in to non-opted in test account.
  CHECK(test_account.has_value());
  sign_in_functions.TurnOnSync(*test_account, 0);
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());

  // Navigate to google.com and open side panel.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kNpsUrl)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  WaitForCompanionToBeLoaded();

  // Verify that correct experiments load.
  std::vector<std::string> expected = {"ATX", "RelQr", "RelQs"};
  std::vector<std::string> unexpected = {"CQ", "PH"};
  ConfirmFeaturesShown(expected, 1);
  ConfirmFeaturesShown(unexpected, 0);

  // Close the side panel.
  side_panel_coordinator()->Close();
  WaitForHistogram("SidePanel.OpenDuration");
#endif  // BUILDFLAG(IS_WIN)
}

IN_PROC_BROWSER_TEST_F(CompanionLiveTest, InitialNavigationLoggedOut) {
  // Navigate to a website, open the side panel, and ensure the sign in promo is
  // shown for a logged out account. Verify the sign-in promo functionality.
  EnableMsbb(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kNpsUrl)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Expect the sign-in promo and no features shown.
  WaitForHistogramSample("Companion.PromoEvent",
                         (int)companion::PromoEvent::kSignInShown);
  histogram_tester_->ExpectBucketCount(
      "Companion.PromoEvent",
      /*sample=*/companion::PromoEvent::kSignInShown, /*expected_count=*/1);

  std::vector<std::string> all_features = {"ATX", "CQ", "PH", "RelQr", "RelQs"};
  ConfirmFeaturesShown(all_features, 0);

  // Click on sign-in promo and expect sign in site in new tab.
  int tab_count = browser()->tab_strip_model()->count();
  ClickButtonByAriaLabel("Sign in to Chrome");
  WaitForTabCount(tab_count + 1);

  // Wait for page to load.
  content::TestNavigationObserver nav_observer(web_contents(), 1);
  nav_observer.Wait();

  // Verify that the sign-in page appears and PromoEvent histogram is updated.
  EXPECT_THAT(web_contents()->GetLastCommittedURL().spec(),
              ::testing::HasSubstr("accounts.google.com"));
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

IN_PROC_BROWSER_TEST_F(CompanionLiveTest, ToggleExps) {
// Toggle exps, ensuring companion updates to reflect changes.
// Note that sync and signin utilities are only supported on Windows.
#if BUILDFLAG(IS_WIN)
  std::optional<signin::TestAccountSigninCredentials> test_account =
      GetTestAccounts()->GetAccount("INTELLIGENCE_ACCOUNT");
  // Test account has opted in to exps.
  CHECK(test_account.has_value());
  sign_in_functions.TurnOnSync(*test_account, 0);
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());

  // Toggle exps.
  EnableExps(true);
  if (skip_test_) {
    GTEST_SKIP();
  }
  // Open side panel and expect experiments to load.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kNpsUrl)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  WaitForCompanionToBeLoaded();

  std::vector<std::string> all_features = {"ATX", "CQ", "RelQr", "RelQs"};
  ConfirmFeaturesShown(all_features, 1);
  side_panel_coordinator()->Close();

  // Turn off exps and expect features to not be shown.
  EnableExps(false);
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kNpsUrl)));
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  WaitForCompanionToBeLoaded();

  // Verify that new samples populate for expected features.
  std::vector<std::string> exps = {"CQ"};
  std::vector<std::string> non_exps = {"ATX", "RelQr", "RelQs"};
  ConfirmFeaturesShown(exps, 1);
  ConfirmFeaturesShown(non_exps, 2);
  side_panel_coordinator()->Close();

  // Turn exps back on and open side panel again.
  EnableExps(true);
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kNpsUrl)));
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  WaitForCompanionToBeLoaded();

  // Expect sample count increases in both groups.
  ConfirmFeaturesShown(exps, 2);
  ConfirmFeaturesShown(non_exps, 3);
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace companion
