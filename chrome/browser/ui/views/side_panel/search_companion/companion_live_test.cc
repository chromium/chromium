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
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
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

  void WaitForCompanionToBeLoaded() {
    content::WebContents* companion_web_contents =
        GetCompanionWebContents(browser());
    EXPECT_TRUE(companion_web_contents);

    // Wait for the navigations in both the frames to complete.
    content::TestNavigationObserver nav_observer(companion_web_contents, 2);
    nav_observer.Wait();
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
        companion::features::kSidePanelCompanion, params);

    EnableMsbb(true);
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

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Test will only run when passed the --run-live-tests flag. To run, use
// browser_tests --gtest_filter=CompanionLiveTest.* --run-live-tests
IN_PROC_BROWSER_TEST_F(CompanionLiveTest, InitialNavigation) {
  // Navigate to a website, open side panel, and ensure that companion loads.
  TestAccount ta;
  // Intelligence test account has lens server access.
  CHECK(GetTestAccountsUtil()->GetAccount("INTELLIGENCE_ACCOUNT", ta));
  sign_in_functions.SignInFromWeb(ta, 0);

  const GURL google_url("https://google.com/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), google_url));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);
}

}  // namespace signin::test
