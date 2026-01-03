// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_overlay_wait_for_paint_utils.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"

namespace {

constexpr char kDocumentWithNamedElement[] = "/select.html";

class LensSearchControllerHelper : public LensSearchController {
 public:
  explicit LensSearchControllerHelper(tabs::TabInterface* tab)
      : LensSearchController(tab) {}
  ~LensSearchControllerHelper() override = default;

  bool should_route_to_contextual_tasks() const override { return true; }
};

// Override the factory to create our helper.
std::unique_ptr<LensSearchController> CreateLensSearchControllerHelper(
    tabs::TabInterface& tab) {
  return std::make_unique<LensSearchControllerHelper>(&tab);
}

}  // namespace

class ContextualTasksLensInteractionBrowserTest : public InProcessBrowserTest {
 public:
  ContextualTasksLensInteractionBrowserTest() {
    lens_search_controller_override_ =
        tabs::TabFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
            base::BindRepeating(&CreateLensSearchControllerHelper));
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    feature_list_.InitWithFeatures(
        {contextual_tasks::kContextualTasks, lens::features::kLensOverlay,
         lens::features::kLensOverlayContextualSearchbox},
        {lens::features::kLensSearchZeroStateCsb});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();

    // Permits sharing the page screenshot by default.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, true);
    prefs->SetBoolean(lens::prefs::kLensSharingPageContentEnabled, true);

    // Ensure the DSE is Google.
    TemplateURLService* service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    TemplateURLData data;
    data.SetShortName(u"Google");
    data.SetKeyword(u"google.com");
    data.SetURL("https://www.google.com/search?q={searchTerms}");
    data.image_url = "https://www.google.com/searchbyimage/upload";
    data.image_translate_url = "https://www.google.com/search?q={searchTerms}";
    data.new_tab_url = "https://www.google.com/_/chrome/newtab";
    data.contextual_search_url = "https://www.google.com/_/contextualsearch";
    data.logo_url = GURL(
        "https://www.google.com/images/branding/googlelogo/2x/"
        "googlelogo_color_272x92dp.png");
    data.alternate_urls = {"https://google.com/search?q={searchTerms}"};
    data.search_intent_params = {"q", "sclient", "tbm", "source", "tbs"};

    TemplateURL* template_url =
        service->Add(std::make_unique<TemplateURL>(data));
    service->SetUserSelectedDefaultSearchProvider(template_url);
  }

  LensSearchControllerHelper* GetLensSearchController() {
    return static_cast<LensSearchControllerHelper*>(
        LensSearchController::From(browser()->GetActiveTabInterface()));
  }

  bool IsSidePanelOpen() {
    auto* coordinator =
        GetLensSearchController()->lens_overlay_side_panel_coordinator();
    return coordinator && coordinator->IsEntryShowing();
  }

  // Lens overlay takes a screenshot of the tab. In order to take a screenshot
  // the tab must not be about:blank and must be painted. By default opens in
  // the current tab.
  void WaitForPaint(
      std::string_view relative_url = kDocumentWithNamedElement,
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB,
      int browser_test_flags = ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP) {
    const GURL url = embedded_test_server()->GetURL(relative_url);
    lens::WaitForPaint(browser(), url, disposition, browser_test_flags);
  }

  void WaitForOverlayToOpen(LensSearchController* controller) {
    auto* overlay_controller = controller->lens_overlay_controller();
    ASSERT_EQ(overlay_controller->state(),
              LensOverlayController::State::kScreenshot);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return overlay_controller->state() ==
             LensOverlayController::State::kOverlay;
    }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ui::UserDataFactory::ScopedOverride lens_search_controller_override_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksLensInteractionBrowserTest,
                       RegionSelectionCapturesContextAndKeepsOverlayOpen) {
  // Wait for the page to be painted to prevent flakiness when screenshotting.
  WaitForPaint();

  // Verify feature flag is enabled.
  ASSERT_TRUE(contextual_tasks::GetEnableLensInContextualTasks());

  auto* controller = GetLensSearchController();
  ASSERT_TRUE(controller);

  // Open Lens Overlay via Composebox button.
  controller->OpenLensOverlay(
      lens::LensOverlayInvocationSource::kContextualTasksComposebox);

  // Wait for the screenshot to be captured.
  WaitForOverlayToOpen(controller);
  ASSERT_TRUE(controller->IsShowingUI());

  // Simulate a region selection which calls IssueLensRegionRequest.
  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(0.5, 0.5, 0.1, 0.1);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kNormalized;
  controller->lens_overlay_controller()->IssueLensRegionRequestForTesting(
      std::move(region), /*is_click=*/false);

  // This should trigger the logic to capture the region, but the overlay should
  // remain open.

  // Verify Side Panel is NOT open (Lens results panel shouldn't open).
  EXPECT_FALSE(IsSidePanelOpen());

  // Verify Overlay state is STILL showing.
  EXPECT_TRUE(controller->IsShowingUI());
}
