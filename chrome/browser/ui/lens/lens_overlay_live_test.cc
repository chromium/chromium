// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/feature_list.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/e2e_tests/account_capabilities_observer.h"
#include "chrome/browser/signin/e2e_tests/accounts_removed_waiter.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/sign_in_test_observer.h"
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_permission_utils.h"
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
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace lens {

namespace {
using State = LensOverlayController::State;
using LensOverlayInvocationSource = lens::LensOverlayInvocationSource;

constexpr char kResultsSearchBaseUrl[] = "https://www.google.com/search";

constexpr char kDivWordClass[] = "word";
constexpr char kDivObjectClass[] = "object";
constexpr char kDivTranslatedLineClass[] = "translated-line";

constexpr char kTranslateEnableButtonID[] = "translateEnableButton";

// Helper script to verify that the overlay WebUI has rendered divs with the CSS
// class provided.
constexpr char kFindAndClickDivWithClassScript[] = R"(
      function findAndClickDivWithClass(parentElement) {
        const div = parentElement.querySelector('div.' + $1);
        if (div) {
            const rect = div.getBoundingClientRect();
            const centerX = rect.left + rect.width / 2;
            const centerY = rect.top + rect.height / 2;

            div.dispatchEvent(new PointerEvent('pointerdown', {
              pointerId: 1,
              button: 0,
              clientX: centerX,
              clientY: centerY,
              isPrimary: true,
              bubbles: true,
              composed: true
            }));
            div.dispatchEvent(new PointerEvent('pointerup', {
              pointerId: 1,
              button: 0,
              clientX: centerX,
              clientY: centerY,
              isPrimary: true,
              bubbles: true,
              composed: true
            }));
            return true;
        }
        for (const child of parentElement.children) {
            if (findAndClickDivWithClass(child) ||
                (child.shadowRoot &&
                    findAndClickDivWithClass(child.shadowRoot))) {
                return true;
            }
        }
        return false;
      }
      findAndClickDivWithClass(document.body);
)";

// Helper script to fetch an element with a certain ID and click on it.
constexpr char kFindAndClickElementWithIDScript[] = R"(
  function findAndClickElementWithID(root, id) {
    const nodesToVisit = [root];
    while (nodesToVisit.length > 0) {
      const currentNode = nodesToVisit.shift();
      if (currentNode instanceof ShadowRoot) {
        const element = currentNode.getElementById(id);
        if (element) {
          element.click();
          return true;
        }
      }
      // Add all children (including those in shadowRoots) to the queue.
      for (const child of currentNode.children) {
        nodesToVisit.push(child);
        if (child.shadowRoot) {
          nodesToVisit.push(child.shadowRoot);
        }
      }
    }
    return false;
  }
  findAndClickElementWithID(document, $1);
)";

const char kNpsUrl[] = "https://www.nps.gov/articles/route-66-overview.htm";
const char kNpsObjectUrl[] =
    "https://www.nps.gov/common/commonspot/templates/images/graphics/404/"
    "04.jpg";
const char kNpsTranslateUrl[] =
    "https://www.nps.gov/subjects/historicpreservationfund/en-espanol.htm";
}  // namespace

// Live tests for Lens Overlay.
// These tests can be run with:
// browser_tests --gtest_filter=LensOverlayLiveTest.* --run-live-tests
class LensOverlayLiveTest : public signin::test::LiveTest {
 public:
  LensOverlayLiveTest() = default;
  ~LensOverlayLiveTest() override = default;

  void SetUp() override {
    SetUpFeatureList();
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

    // Set the default timeout for our run loops.
    base::test::ScopedRunLoopTimeout timeout(FROM_HERE,
                                             TestTimeouts::action_timeout());
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

  content::WebContents* GetOverlayWebContents() {
    auto* controller = browser()
                           ->tab_strip_model()
                           ->GetActiveTab()
                           ->GetTabFeatures()
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
    base::RunLoop run_loop;
    auto histogram_observer = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        histogram_name,
        base::BindLambdaForTesting(
            [&](std::string_view histogram_name, uint64_t name_hash,
                base::HistogramBase::Sample32 sample) { run_loop.Quit(); }));
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
          ->GetContents()
          ->CompletedFirstVisuallyNonEmptyPaint();
    }));
  }

  // Verifies the side panel opened and loaded a search URL in its iframe.
  void VerifySidePanelLoaded() {
    auto* controller = browser()
                           ->tab_strip_model()
                           ->GetActiveTab()
                           ->GetTabFeatures()
                           ->lens_overlay_controller();

    // Expect the Lens Overlay results panel to open.
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return controller->state() == State::kOverlayAndResults; }));
    auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
    ASSERT_TRUE(coordinator->IsSidePanelShowing());
    ASSERT_EQ(coordinator->GetCurrentEntryId(),
              SidePanelEntry::Id::kLensOverlayResults);

    // Wait for the panel to finish loading.
    EXPECT_TRUE(content::WaitForLoadStop(
        controller->GetSidePanelWebContentsForTesting()));

    // The results frame should be the only child frame of the side panel web
    // contents.
    content::RenderFrameHost* results_frame = content::ChildFrameAt(
        controller->GetSidePanelWebContentsForTesting()->GetPrimaryMainFrame(),
        0);
    EXPECT_TRUE(results_frame);
    EXPECT_TRUE(content::WaitForRenderFrameReady(results_frame));

    // Check the result frame URL matches a valid results URL.
    EXPECT_THAT(results_frame->GetLastCommittedURL().spec(),
                testing::MatchesRegex(
                    std::string(kResultsSearchBaseUrl) +
                    ".*source=chrome.cr.menu.*&gsc=2&hl=.*&biw=\\d+&bih=\\d+"));
  }

  virtual void SetUpFeatureList() {
    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlay,
        {{"enable-shimmer", "false"}, {"use-blur", "false"}});
  }

  void TearDown() override { LiveTest::TearDown(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LensOverlayLiveTest, ClickText_SignedInAndSynced) {
  std::optional<signin::TestAccountSigninCredentials> test_account =
      GetTestAccounts()->GetAccount("INTELLIGENCE_ACCOUNT");
  // Sign in and sync to opted in test account.
  CHECK(test_account.has_value());
  sign_in_functions.TurnOnSync(*test_account, 0);
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());

  // Navigate to a website and wait for paint before starting controller.
  WaitForPaint(kNpsUrl);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->GetTabFeatures()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  auto* search_controller = browser()
                                ->tab_strip_model()
                                ->GetActiveTab()
                                ->GetTabFeatures()
                                ->lens_search_controller();

  // Showing UI should change the state to screenshot and eventually to overlay.
  search_controller->OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Confirm that the WebUI has reported that it is ready. This means the local
  // DOM should be initialized on our WebUI.
  WaitForHistogram("Lens.Overlay.TimeToWebUIReady");

  // Verify that the page returns text that is selectable on the overlay.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return EvalJs(content::JsReplace(kFindAndClickDivWithClassScript,
                                     kDivWordClass))
        .ExtractBool();
  }));

  // After finding and clicking the div, make sure the side panel opens and
  // loaded a result.
  VerifySidePanelLoaded();
}

IN_PROC_BROWSER_TEST_F(LensOverlayLiveTest, ClickText_SignedInNotSynced) {
  std::optional<signin::TestAccountSigninCredentials> test_account =
      GetTestAccounts()->GetAccount("INTELLIGENCE_ACCOUNT");
  // Sign in but do not sync to opted in test account.
  CHECK(test_account.has_value());
  sign_in_functions.SignInFromWeb(*test_account, 0);
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());

  // Navigate to a website and wait for paint before starting controller.
  WaitForPaint(kNpsUrl);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->GetTabFeatures()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  auto* search_controller = browser()
                                ->tab_strip_model()
                                ->GetActiveTab()
                                ->GetTabFeatures()
                                ->lens_search_controller();

  // Showing UI should change the state to screenshot and eventually to overlay.
  search_controller->OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Confirm that the WebUI has reported that it is ready. This means the local
  // DOM should be initialized on our WebUI.
  WaitForHistogram("Lens.Overlay.TimeToWebUIReady");

  // Verify that the page returns text that is selectable on the overlay.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return EvalJs(content::JsReplace(kFindAndClickDivWithClassScript,
                                     kDivWordClass))
        .ExtractBool();
  }));

  // After finding and clicking the div, make sure the side panel opens and
  // loaded a result.
  VerifySidePanelLoaded();
}

IN_PROC_BROWSER_TEST_F(LensOverlayLiveTest, ClickText_SignedOut) {
  // Navigate to a website and wait for paint before starting controller.
  WaitForPaint(kNpsUrl);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->GetTabFeatures()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  auto* search_controller = browser()
                                ->tab_strip_model()
                                ->GetActiveTab()
                                ->GetTabFeatures()
                                ->lens_search_controller();

  // Showing UI should change the state to screenshot and eventually to overlay.
  search_controller->OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Confirm that the WebUI has reported that it is ready. This means the local
  // DOM should be initialized on our WebUI.
  WaitForHistogram("Lens.Overlay.TimeToWebUIReady");

  // Verify that the page returns text that is selectable on the overlay.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return EvalJs(content::JsReplace(kFindAndClickDivWithClassScript,
                                     kDivWordClass))
        .ExtractBool();
  }));

  // After finding and clicking the div, make sure the side panel opens and
  // loaded a result.
  VerifySidePanelLoaded();
}

IN_PROC_BROWSER_TEST_F(LensOverlayLiveTest, ClickObject_SignedInAndSynced) {
  std::optional<signin::TestAccountSigninCredentials> test_account =
      GetTestAccounts()->GetAccount("INTELLIGENCE_ACCOUNT");
  // Sign in and sync to opted in test account.
  CHECK(test_account.has_value());
  sign_in_functions.TurnOnSync(*test_account, 0);
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());

  // Navigate to a website and wait for paint before starting controller.
  WaitForPaint(kNpsObjectUrl);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->GetTabFeatures()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  auto* search_controller = browser()
                                ->tab_strip_model()
                                ->GetActiveTab()
                                ->GetTabFeatures()
                                ->lens_search_controller();

  // Showing UI should change the state to screenshot and eventually to overlay.
  search_controller->OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Confirm that the WebUI has reported that it is ready. This means the local
  // DOM should be initialized on our WebUI.
  WaitForHistogram("Lens.Overlay.TimeToWebUIReady");

  // Verify that the page returns objects that is selectable on the overlay.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return EvalJs(content::JsReplace(kFindAndClickDivWithClassScript,
                                     kDivObjectClass))
        .ExtractBool();
  }));

  // After finding and clicking the div, make sure the side panel opens and
  // loaded a result.
  VerifySidePanelLoaded();
}

IN_PROC_BROWSER_TEST_F(LensOverlayLiveTest, ClickObject_SignedInNotSynced) {
  std::optional<signin::TestAccountSigninCredentials> test_account =
      GetTestAccounts()->GetAccount("INTELLIGENCE_ACCOUNT");
  // Sign in but do not sync to opted in test account.
  CHECK(test_account.has_value());
  sign_in_functions.SignInFromWeb(*test_account, 0);
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());

  // Navigate to a website and wait for paint before starting controller.
  WaitForPaint(kNpsObjectUrl);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->GetTabFeatures()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  auto* search_controller = browser()
                                ->tab_strip_model()
                                ->GetActiveTab()
                                ->GetTabFeatures()
                                ->lens_search_controller();

  // Showing UI should change the state to screenshot and eventually to overlay.
  search_controller->OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Confirm that the WebUI has reported that it is ready. This means the local
  // DOM should be initialized on our WebUI.
  WaitForHistogram("Lens.Overlay.TimeToWebUIReady");

  // Verify that the page returns objects that is selectable on the overlay.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return EvalJs(content::JsReplace(kFindAndClickDivWithClassScript,
                                     kDivObjectClass))
        .ExtractBool();
  }));

  // After finding and clicking the div, make sure the side panel opens and
  // loaded a result.
  VerifySidePanelLoaded();
}

IN_PROC_BROWSER_TEST_F(LensOverlayLiveTest, ClickObject_SignedOut) {
  // Navigate to a website and wait for paint before starting controller.
  WaitForPaint(kNpsObjectUrl);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->GetTabFeatures()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  auto* search_controller = browser()
                                ->tab_strip_model()
                                ->GetActiveTab()
                                ->GetTabFeatures()
                                ->lens_search_controller();

  // Showing UI should change the state to screenshot and eventually to overlay.
  search_controller->OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Confirm that the WebUI has reported that it is ready. This means the local
  // DOM should be initialized on our WebUI.
  WaitForHistogram("Lens.Overlay.TimeToWebUIReady");

  // Verify that the page returns objects that is selectable on the overlay.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return EvalJs(content::JsReplace(kFindAndClickDivWithClassScript,
                                     kDivObjectClass))
        .ExtractBool();
  }));

  // After finding and clicking the div, make sure the side panel opens and
  // loaded a result.
  VerifySidePanelLoaded();
}

// Live tests for LensOverlayTranslateButton.
class LensOverlayTranslateLiveTest : public LensOverlayLiveTest {
 public:
  void ClickTranslateButtonAndThenText() {
    // Find and click the translate enable button when it appears on the
    // overlay.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return EvalJs(content::JsReplace(kFindAndClickElementWithIDScript,
                                       kTranslateEnableButtonID))
          .ExtractBool();
    }));

    // The translated lines render and need some time in order
    // for the overlay to compute their bounding boxes for highlighted lines.
    // For this reason, keep clicking on the line until the side panel actually
    // opens.
    auto* controller = browser()
                           ->tab_strip_model()
                           ->GetActiveTab()
                           ->GetTabFeatures()
                           ->lens_overlay_controller();
    ASSERT_TRUE(base::test::RunUntil([&]() {
      EvalJs(content::JsReplace(kFindAndClickDivWithClassScript,
                                kDivTranslatedLineClass));
      return controller->state() == State::kOverlayAndResults;
    }));
  }

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlay,
          {{"enable-shimmer", "false"}, {"use-blur", "false"}}},
         {features::kLensOverlayTranslateButton, {}}},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayTranslateLiveTest,
                       TranslateScreen_SignedInAndSynced) {
  std::optional<signin::TestAccountSigninCredentials> test_account =
      GetTestAccounts()->GetAccount("INTELLIGENCE_ACCOUNT");
  // Sign in and sync to opted in test account.
  CHECK(test_account.has_value());
  sign_in_functions.TurnOnSync(*test_account, 0);
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());

  // Navigate to a website and wait for paint before starting controller.
  WaitForPaint(kNpsTranslateUrl);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->GetTabFeatures()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  auto* search_controller = browser()
                                ->tab_strip_model()
                                ->GetActiveTab()
                                ->GetTabFeatures()
                                ->lens_search_controller();

  // Showing UI should change the state to screenshot and eventually to overlay.
  search_controller->OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Confirm that the WebUI has reported that it is ready. This means the local
  // DOM should be initialized on our WebUI.
  WaitForHistogram("Lens.Overlay.TimeToWebUIReady");

  // Check if the translate button exits and then click on a translated line.
  ClickTranslateButtonAndThenText();

  // After finding and clicking the div, make sure the side panel opens and
  // loaded a result.
  VerifySidePanelLoaded();
}

IN_PROC_BROWSER_TEST_F(LensOverlayTranslateLiveTest,
                       TranslateScreen_SignedInNotSynced) {
  std::optional<signin::TestAccountSigninCredentials> test_account =
      GetTestAccounts()->GetAccount("INTELLIGENCE_ACCOUNT");
  // Sign in but do not sync to opted in test account.
  CHECK(test_account.has_value());
  sign_in_functions.SignInFromWeb(*test_account, 0);
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());

  // Navigate to a website and wait for paint before starting controller.
  WaitForPaint(kNpsTranslateUrl);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->GetTabFeatures()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  auto* search_controller = browser()
                                ->tab_strip_model()
                                ->GetActiveTab()
                                ->GetTabFeatures()
                                ->lens_search_controller();

  // Showing UI should change the state to screenshot and eventually to overlay.
  search_controller->OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Confirm that the WebUI has reported that it is ready. This means the local
  // DOM should be initialized on our WebUI.
  WaitForHistogram("Lens.Overlay.TimeToWebUIReady");

  // Check if the translate button exits and then click on a translated line.
  ClickTranslateButtonAndThenText();

  // After finding and clicking the div, make sure the side panel opens and
  // loaded a result.
  VerifySidePanelLoaded();
}

IN_PROC_BROWSER_TEST_F(LensOverlayTranslateLiveTest,
                       TranslateScreen_SignedOut) {
  // Navigate to a website and wait for paint before starting controller.
  WaitForPaint(kNpsTranslateUrl);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->GetTabFeatures()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  auto* search_controller = browser()
                                ->tab_strip_model()
                                ->GetActiveTab()
                                ->GetTabFeatures()
                                ->lens_search_controller();

  // Showing UI should change the state to screenshot and eventually to overlay.
  search_controller->OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Confirm that the WebUI has reported that i1t is ready. This means the local
  // DOM should be initialized on our WebUI.
  WaitForHistogram("Lens.Overlay.TimeToWebUIReady");

  // Check if the translate button exits and then click on a translated line.
  ClickTranslateButtonAndThenText();

  // After finding and clicking the div, make sure the side panel opens and
  // loaded a result.
  VerifySidePanelLoaded();
}

}  // namespace lens
