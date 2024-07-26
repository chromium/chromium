// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/e2e_tests/account_capabilities_observer.h"
#include "chrome/browser/signin/e2e_tests/accounts_removed_waiter.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/sign_in_test_observer.h"
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_invocation_source.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace signin::test {

namespace {
using State = LensOverlayController::State;
using LensOverlayInvocationSource = lens::LensOverlayInvocationSource;

// Helper script to verify that the overlay WebUI has rendered text that it has
// received from the server.
constexpr char kFindWordDivScript[] = R"(
      function findWordDiv(parentElement) {
        if (parentElement.querySelector('div.word')) {
            return true;
        }
        for (const child of parentElement.children) {
            if (findWordDiv(child) ||
                (child.shadowRoot && findWordDiv(child.shadowRoot))) {
                return true;
            }
        }
        return false;
      }
      findWordDiv(document.body);
)";

const char kNpsUrl[] = "https://www.nps.gov/articles/route-66-overview.htm";
}  // namespace

// Live tests for Companion.
// These tests can be run with:
// browser_tests --gtest_filter=LensOverlayLiveTest.* --run-live-tests
class LensOverlayLiveTest : public signin::test::LiveTest {
 public:
  LensOverlayLiveTest() = default;
  ~LensOverlayLiveTest() override = default;

  void SetUp() override {
    SetUpFeatureList();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    LiveTest::SetUp();
    // Always disable animation for stability.
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

  void SetUpOnMainThread() override {
    LiveTest::SetUpOnMainThread();

    // Permits sharing the page screenshot by default. This disables the
    // permission dialog.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, true);
  }

  void SetUpInProcessBrowserTestFixture() override {
    // Allowlists hosts.
    host_resolver()->AllowDirectLookup("*.nps.gov");

    LiveTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LiveTest::SetUpCommandLine(command_line);
    // Because we are taking a screenshot of a live page, we need to enable
    // pixel output in tests.
    command_line->AppendSwitch(::switches::kEnablePixelOutputInTests);
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

  content::WebContents* GetOverlayWebContents() {
    auto* controller = browser()
                           ->tab_strip_model()
                           ->GetActiveTab()
                           ->tab_features()
                           ->lens_overlay_controller();
    return controller->GetOverlayWebViewForTesting()->GetWebContents();
  }

  content::EvalJsResult EvalJs(const std::string& code) {
    // Execute JS in Overlay WebUI.
    return content::EvalJs(GetOverlayWebContents()->GetPrimaryMainFrame(),
                           code);
  }

  void WaitForHistogram(const std::string& histogram_name) {
    // Continue if histogram was already recorded.
    if (base::StatisticsRecorder::FindHistogram(histogram_name)) {
      return;
    }

    // Else, wait until the histogram is recorded.
    base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Seconds(10));
    base::RunLoop run_loop;
    auto histogram_observer = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        histogram_name,
        base::BindLambdaForTesting(
            [&](const char* histogram_name, uint64_t name_hash,
                base::HistogramBase::Sample sample) { run_loop.Quit(); }));
    run_loop.Run();
  }

  // Lens overlay takes a screenshot of the tab. In order to take a screenshot
  // the tab must not be about:blank and must be painted. By default opens in
  // the current tab.
  void WaitForPaint(
      std::string_view url,
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB,
      int browser_test_flags = ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP) {
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(url), disposition, browser_test_flags));
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return browser()
          ->tab_strip_model()
          ->GetActiveTab()
          ->contents()
          ->CompletedFirstVisuallyNonEmptyPaint();
    }));
  }

  virtual void SetUpFeatureList() {
    feature_list_.InitAndEnableFeature(lens::features::kLensOverlay);
  }

  void TearDown() override { LiveTest::TearDown(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(LensOverlayLiveTest, OverlayHasText) {
  TestAccount ta;
  // Sign in to opted in test account.
  CHECK(GetTestAccountsUtil()->GetAccount("INTELLIGENCE_ACCOUNT", ta));
  sign_in_functions.TurnOnSync(ta, 0);
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());

  // Navigate to a website and wait for paint before starting controller.
  WaitForPaint(kNpsUrl);
  content::WaitForLoadStop(web_contents());

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Confirm that the WebUI has reported that it is ready. This means the local
  // DOM should be initialized on our WebUI.
  WaitForHistogram("Lens.Overlay.TimeToWebUIReady");

  // Verify that the page returns text that is selectable on the overlay.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return EvalJs(kFindWordDivScript).ExtractBool(); }));
}

}  // namespace signin::test
