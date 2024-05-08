// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class runs functional tests for lens overlay. These tests spin up a full
// web browser, but allow for inspection and modification of internal state of
// LensOverlayController and other business-logic classes.

#include "chrome/browser/ui/lens/lens_overlay_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/polygon.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/lens_search_bubble_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/tabs/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lens/lens_features.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/url_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"
#include "url/origin.h"

namespace {

constexpr char kDocumentWithNamedElement[] = "/select.html";

using State = LensOverlayController::State;

constexpr char kNewTabLinkClickScript[] =
    "(function() {const anchor = document.createElement('a');anchor.href = "
    "$1;anchor.target = "
    "'_blank';document.body.appendChild(anchor);anchor.click();})();";

constexpr char kSameTabLinkClickScript[] =
    "(function() {const anchor = document.createElement('a');anchor.href = "
    "$1;document.body.appendChild(anchor);anchor.click();})();";

constexpr char kCheckSearchboxInput[] =
    "(function() {const root = "
    "document.getElementsByTagName('lens-side-panel-app')[0].shadowRoot;"
    "const searchboxInputLoaded = "
    "  root.getElementById('realbox').shadowRoot.getElementById('input').value "
    "  === $1; return  searchboxInputLoaded;})();";

constexpr char kRequestNotificationsScript[] = R"(
      new Promise(resolve => {
        Notification.requestPermission().then(function (permission) {
          resolve(permission);
        });
      })
      )";

constexpr char kCheckSidePanelResultsLoadedScript[] =
    "(function() {const root = "
    "document.getElementsByTagName('lens-side-panel-app')[0].shadowRoot; "
    "const iframeSrcLoaded = "
    "  root.getElementById('results').src.includes('q=' + $1);"
    "const searchboxInputLoaded = "
    "  root.getElementById('realbox').shadowRoot.getElementById('input').value "
    "  === $1; return iframeSrcLoaded && searchboxInputLoaded;})();";

constexpr char kCheckSidePanelThumbnailShownScript[] =
    "(function() {const appRoot = "
    "document.getElementsByTagName('lens-side-panel-app')[0].shadowRoot;"
    "const realboxRoot = appRoot.getElementById('realbox').shadowRoot;"
    "const thumbContainer = realboxRoot.getElementById('thumbnailContainer');"
    "const thumbnailRoot = realboxRoot.getElementById('thumbnail').shadowRoot;"
    "const imageSrc = thumbnailRoot.getElementById('image').src;"
    "return window.getComputedStyle(thumbContainer).display !== 'none' && "
    "       imageSrc.startsWith('data:image/jpeg');})();";

constexpr char kTestSuggestSignals[] = "suggest_signals";

constexpr char kStartTimeQueryParamKey[] = "qsubts";

const lens::mojom::GeometryPtr kTestGeometry =
    lens::mojom::Geometry::New(lens::mojom::CenterRotatedBox::New(
        gfx::RectF(0.1, 0.1, 0.8, 0.8),
        0.1,
        lens::mojom::CenterRotatedBox_CoordinateType::kNormalized),
        std::vector<lens::mojom::PolygonPtr>());
const lens::mojom::OverlayObjectPtr kTestOverlayObject =
    lens::mojom::OverlayObject::New("unique_id", kTestGeometry->Clone());
const lens::mojom::TextPtr kTestText =
    lens::mojom::Text::New(lens::mojom::TextLayout::New(), "es");

class LensOverlayPageFake : public lens::mojom::LensPage {
 public:
  void ScreenshotDataUriReceived(const std::string& data_uri) override {
    last_received_screenshot_data_uri_ = data_uri;
    // Do the real call on the open WebUI we intercepted.
    overlay_page_->ScreenshotDataUriReceived(data_uri);
  }

  void ObjectsReceived(
      std::vector<lens::mojom::OverlayObjectPtr> objects) override {
    last_received_objects_ = std::move(objects);
  }

  void TextReceived(lens::mojom::TextPtr text) override {
    last_received_text_ = std::move(text);
  }

  void NotifyResultsPanelOpened() override {
    did_notify_results_opened_ = true;
  }

  void SetPostRegionSelection(
      lens::mojom::CenterRotatedBoxPtr region) override {
    post_region_selection_ = std::move(region);
  }

  void SetTextSelection(int selection_start_index,
                        int selection_end_index) override {
    text_selection_indexes_ =
        std::make_pair(selection_start_index, selection_end_index);
  }

  void ClearAllSelections() override { did_clear_selections_ = true; }

  // The real side panel page that was opened by the lens overlay. Needed to
  // call real functions on the WebUI.
  mojo::Remote<lens::mojom::LensPage> overlay_page_;

  std::string last_received_screenshot_data_uri_;
  std::vector<lens::mojom::OverlayObjectPtr> last_received_objects_;
  lens::mojom::TextPtr last_received_text_;
  bool did_notify_results_opened_ = false;
  lens::mojom::CenterRotatedBoxPtr post_region_selection_;
  std::pair<int, int> text_selection_indexes_;
  bool did_clear_selections_ = false;
};

// TODO(b/334147680): Since both our interactive UI tests and our browser tests
// both mock out network calls via this method, we should factor this out so it
// can be used across files.
class LensOverlayQueryControllerFake : public lens::LensOverlayQueryController {
 public:
  explicit LensOverlayQueryControllerFake(
      lens::LensOverlayFullImageResponseCallback full_image_callback,
      lens::LensOverlayUrlResponseCallback url_callback,
      lens::LensOverlayInteractionResponseCallback interaction_data_callback,
      lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager)
      : LensOverlayQueryController(full_image_callback,
                                   url_callback,
                                   interaction_data_callback,
                                   thumbnail_created_callback,
                                   variations_client,
                                   identity_manager) {}

  void StartQueryFlow(const SkBitmap& screenshot,
                      std::optional<GURL> page_url,
                      std::optional<std::string> page_title) override {
    // Send response for full image callback / HandleStartQueryResponse.
    std::vector<lens::mojom::OverlayObjectPtr> test_objects;
    test_objects.push_back(kTestOverlayObject->Clone());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(full_image_callback_, std::move(test_objects),
                                  kTestText->Clone()));

    // Send response for interaction data callback /
    // HandleInteractionDataResponse.
    lens::proto::LensOverlayInteractionResponse interaction_response;
    interaction_response.set_suggest_signals(kTestSuggestSignals);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(interaction_data_callback_, interaction_response));
  }
};

// Stubs out network requests and mojo calls.
class LensOverlayControllerFake : public LensOverlayController {
 public:
  LensOverlayControllerFake(tabs::TabInterface* tab,
                            variations::VariationsClient* variations_client,
                            signin::IdentityManager* identity_manager,
                            PrefService* pref_service,
                            syncer::SyncService* sync_service)
      : LensOverlayController(tab,
                              variations_client,
                              identity_manager,
                              pref_service,
                              sync_service) {}

  std::unique_ptr<lens::LensOverlayQueryController> CreateLensQueryController(
      lens::LensOverlayFullImageResponseCallback full_image_callback,
      lens::LensOverlayUrlResponseCallback url_callback,
      lens::LensOverlayInteractionResponseCallback interaction_data_callback,
      lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager) override {
    return std::make_unique<LensOverlayQueryControllerFake>(
        full_image_callback, url_callback, interaction_data_callback,
        thumbnail_created_callback, variations_client, identity_manager);
  }

  void BindOverlay(mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
                   mojo::PendingRemote<lens::mojom::LensPage> page) override {
    fake_overlay_page_.overlay_page_.Bind(std::move(page));
    // Set up the fake overlay page to intercept the mojo call.
    LensOverlayController::BindOverlay(
        std::move(receiver),
        fake_overlay_page_receiver_.BindNewPipeAndPassRemote());
  }

  void FlushForTesting() { fake_overlay_page_receiver_.FlushForTesting(); }

  LensOverlayPageFake fake_overlay_page_;
  mojo::Receiver<lens::mojom::LensPage> fake_overlay_page_receiver_{
      &fake_overlay_page_};
};

class TabFeaturesFake : public tabs::TabFeatures {
 public:
  TabFeaturesFake() = default;

 protected:
  std::unique_ptr<LensOverlayController> CreateLensController(
      tabs::TabInterface* tab,
      Profile* profile) override {
    return std::make_unique<LensOverlayControllerFake>(
        tab, profile->GetVariationsClient(),
        IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs(),
        SyncServiceFactory::GetForProfile(profile));
  }
};

class LensOverlayControllerBrowserTest : public InProcessBrowserTest {
 protected:
  LensOverlayControllerBrowserTest() {
    tabs::TabFeatures::ReplaceTabFeaturesForTesting(base::BindRepeating(
        &LensOverlayControllerBrowserTest::CreateTabFeatures,
        base::Unretained(this)));
    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlay, {
                                          {"search-bubble", "true"},
                                      });
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  ~LensOverlayControllerBrowserTest() override {
    tabs::TabFeatures::ReplaceTabFeaturesForTesting(base::NullCallback());
  }

  std::unique_ptr<tabs::TabFeatures> CreateTabFeatures() {
    return std::make_unique<TabFeaturesFake>();
  }

  content::WebContents* GetOverlayWebContents() {
    auto* controller = browser()
                           ->tab_strip_model()
                           ->GetActiveTab()
                           ->tab_features()
                           ->lens_overlay_controller();
    raw_ptr<views::WebView> overlay_web_view =
        views::AsViewClass<views::WebView>(
            controller->GetOverlayWidgetForTesting()
                ->GetContentsView()
                ->children()[0]);
    return overlay_web_view->GetWebContents();
  }

  void SimulateLeftClickDrag(gfx::Point from, gfx::Point to) {
    auto* overlay_web_contents = GetOverlayWebContents();
    // We should wait for the main frame's hit-test data to be ready before
    // sending the click event below to avoid flakiness.
    content::WaitForHitTestData(overlay_web_contents->GetPrimaryMainFrame());
    content::SimulateMouseEvent(overlay_web_contents,
                                blink::WebInputEvent::Type::kMouseDown,
                                blink::WebMouseEvent::Button::kLeft, from);
    content::SimulateMouseEvent(overlay_web_contents,
                                blink::WebInputEvent::Type::kMouseMove,
                                blink::WebMouseEvent::Button::kLeft, to);
    content::SimulateMouseEvent(overlay_web_contents,
                                blink::WebInputEvent::Type::kMouseUp,
                                blink::WebMouseEvent::Button::kLeft, to);
    content::RunUntilInputProcessed(
        overlay_web_contents->GetRenderWidgetHostView()->GetRenderWidgetHost());
  }

  // Lens overlay takes a screenshot of the tab. In order to take a screenshot
  // the tab must not be about:blank and must be painted. By default opens in
  // the current tab.
  void WaitForPaint(
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB,
      int browser_test_flags = ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP) {
    const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, disposition, browser_test_flags));
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return browser()
          ->tab_strip_model()
          ->GetActiveTab()
          ->contents()
          ->CompletedFirstVisuallyNonEmptyPaint();
    }));
  }

  // Helper to remove the start time query param from the url.
  GURL RemoveStartTimeParam(const GURL& url_to_process) {
    std::string actual_start_time;
    bool has_start_time = net::GetValueForKeyInQuery(
        GURL(url_to_process), kStartTimeQueryParamKey, &actual_start_time);
    EXPECT_TRUE(has_start_time);
    return net::AppendOrReplaceQueryParameter(
        url_to_process, kStartTimeQueryParamKey, std::nullopt);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, CaptureScreenshot) {
  WaitForPaint();
  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayController::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify screenshot was captured and stored.
  auto screenshot_bitmap = controller->current_screenshot();
  EXPECT_FALSE(screenshot_bitmap.empty());

  // Verify screenshot was encoded and passed to WebUI.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_FALSE(fake_controller->fake_overlay_page_
                   .last_received_screenshot_data_uri_.empty());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, CreateAndLoadWebUI) {
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayController::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Assert that the web view was created and loaded WebUI.
  GURL webui_url(chrome::kChromeUILensUntrustedURL);
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  ASSERT_EQ(GetOverlayWebContents()->GetLastCommittedURL(), webui_url);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, ShowSidePanel) {
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayController::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Before showing the results panel, there should be no notification sent to
  // WebUI.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_FALSE(fake_controller->fake_overlay_page_.did_notify_results_opened_);

  // Now show the side panel.
  controller->side_panel_coordinator()->RegisterEntryAndShow();

  // Prevent flakiness by flushing the tasks.
  fake_controller->FlushForTesting();

  auto* coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);
  EXPECT_TRUE(fake_controller->fake_overlay_page_.did_notify_results_opened_);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, CloseSidePanel) {
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayController::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Now show the side panel.
  controller->side_panel_coordinator()->RegisterEntryAndShow();

  // Ensure the side panel is showing.
  auto* coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  EXPECT_TRUE(coordinator->IsSidePanelShowing());

  // Close the side panel.
  coordinator->Close();

  // Ensure the side panel closes too.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
}

#if BUILDFLAG(IS_CHROMEOS_LACROS) && !BUILDFLAG(IS_CHROMEOS_DEVICE) && \
    defined(MEMORY_SANTIZER)
#define MAYBE_DelayPermissionsPrompt DISABLED_DelayPermissionsPrompt
#else
#define MAYBE_DelayPermissionsPrompt DelayPermissionsPrompt
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       MAYBE_DelayPermissionsPrompt) {
  // Navigate to a page so we can request permissions
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayController::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  permissions::PermissionRequestObserver observer(contents);

  // Request permission in tab under overlay.
  EXPECT_TRUE(content::ExecJs(
      contents->GetPrimaryMainFrame(), kRequestNotificationsScript,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Verify no prompt was shown
  observer.Wait();
  EXPECT_FALSE(observer.request_shown());

  // Close overlay
  controller->CloseUI();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));

  // Verify a prompt was shown
  ASSERT_TRUE(base::test::RunUntil([&]() { return observer.request_shown(); }));
}

// TODO(b/335801964): Test flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ShowSidePanelAfterManualRegionSelection \
  DISABLED_ShowSidePanelAfterManualRegionSelection
#else
#define MAYBE_ShowSidePanelAfterManualRegionSelection \
  ShowSidePanelAfterManualRegionSelection
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       MAYBE_ShowSidePanelAfterManualRegionSelection) {
  WaitForPaint();

  std::string text_query = "Apples";
  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayController::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  // We need to flush the mojo receiver calls to make sure the screenshot was
  // passed back to the WebUI or else the region selection UI will not render.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->FlushForTesting();
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Simulate mouse events on the overlay for drawing a manual region.
  gfx::Point center =
      GetOverlayWebContents()->GetContainerBounds().CenterPoint();
  gfx::Point off_center = gfx::Point(center);
  off_center.Offset(100, 100);
  SimulateLeftClickDrag(center, off_center);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));
  auto* coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  // Expect the Lens Overlay results panel to open.
  ASSERT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);

  // Verify that the side panel searchbox displays a thumbnail.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return true ==
           content::EvalJs(
               controller->GetSidePanelWebContentsForTesting(),
               content::JsReplace(kCheckSidePanelThumbnailShownScript));
  }));

  // Verify that after text selection, the thumbnail is no longer shown.
  controller->IssueTextSelectionRequestForTesting(text_query,
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return false ==
           content::EvalJs(
               controller->GetSidePanelWebContentsForTesting(),
               content::JsReplace(kCheckSidePanelThumbnailShownScript));
  }));

}

// TODO(b/335028577): Test flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ShowSidePanelAfterTextSelectionRequest \
  DISABLED_ShowSidePanelAfterTextSelectionRequest
#else
#define MAYBE_ShowSidePanelAfterTextSelectionRequest \
  ShowSidePanelAfterTextSelectionRequest
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       MAYBE_ShowSidePanelAfterTextSelectionRequest) {
  WaitForPaint();

  std::string text_query = "Apples";
  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayController::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  controller->IssueTextSelectionRequestForTesting(text_query,
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));

  // Expect the Lens Overlay results panel to open.
  auto* coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  EXPECT_TRUE(coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntry::Id::kLensOverlayResults)));

  // Verify that the side panel displays our query.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return true ==
           content::EvalJs(controller->GetSidePanelWebContentsForTesting(),
                           content::JsReplace(
                               kCheckSidePanelResultsLoadedScript, text_query));
  }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       HandleStartQueryResponse) {
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Before showing the UI, there should be no set objects or text as
  // no query flow has started.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_TRUE(
      fake_controller->fake_overlay_page_.last_received_objects_.empty());
  EXPECT_FALSE(fake_controller->fake_overlay_page_.last_received_text_);

  // Showing UI should eventually result in overlay state. When the overlay is
  // bound, it should start the query flow which returns a response for the
  // full image callback.
  controller->ShowUI(LensOverlayController::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Prevent flakiness by flushing the tasks.
  fake_controller->FlushForTesting();

  // After flushing the mojo calls, the data should be present.
  EXPECT_FALSE(
      fake_controller->fake_overlay_page_.last_received_objects_.empty());

  auto* object =
      fake_controller->fake_overlay_page_.last_received_objects_[0].get();
  auto* text = fake_controller->fake_overlay_page_.last_received_text_.get();
  EXPECT_TRUE(object);
  EXPECT_TRUE(text);
  EXPECT_TRUE(kTestOverlayObject->Equals(*object));
  EXPECT_TRUE(kTestText->Equals(*text));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       HandleInteractionDataResponse) {
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Before showing the UI, there should be no suggest signals as no query flow
  // has started.
  EXPECT_FALSE(controller->GetLensResponseForTesting().has_suggest_signals());

  // Showing UI should eventually result in overlay state. When the overlay is
  // bound, it should start the query flow which returns a response for the
  // interaction data callback.
  controller->ShowUI(LensOverlayController::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // The lens response should have been correctly set for use by the searchbox.
  EXPECT_TRUE(controller->GetLensResponseForTesting().has_suggest_signals());
  EXPECT_EQ(controller->GetLensResponseForTesting().suggest_signals(),
            kTestSuggestSignals);

  // And the current page URL should be made available for use by the searchbox.
  EXPECT_TRUE(base::EndsWith(controller->GetPageURLForTesting().spec(),
                             kDocumentWithNamedElement,
                             base::CompareCase::INSENSITIVE_ASCII));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       BackgroundAndForegroundUI) {
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Grab the index of the currently active tab so we can return to it later.
  int active_controller_tab_index =
      browser()->tab_strip_model()->active_index();

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayController::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayWidgetForTesting()->IsVisible());

  // Open a side panel to test that the side panel persists between tab
  // switches.
  controller->side_panel_coordinator()->RegisterEntryAndShow();
  auto* coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);

  // Opening a new tab should background the overlay UI.
  WaitForPaint(WindowOpenDisposition::NEW_FOREGROUND_TAB,
               ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
                   ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kBackground; }));
  EXPECT_FALSE(controller->GetOverlayWidgetForTesting()->IsVisible());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !coordinator->IsSidePanelShowing(); }));

  // Returning back to the previous tab should show the overlay UI again.
  browser()->tab_strip_model()->ActivateTabAt(active_controller_tab_index);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(controller->GetOverlayWidgetForTesting()->IsVisible());
  // Side panel should come back when returning to previous tab.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return coordinator->IsSidePanelShowing(); }));
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       LoadURLInResultsFrame) {
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayController::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayWidgetForTesting()->IsVisible());

  // Side panel is not showing at first.
  auto* coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  EXPECT_FALSE(coordinator->IsSidePanelShowing());

  // Loading a url in the side panel should show the results page.
  const GURL search_url("https://www.google.com/search");
  controller->LoadURLInResultsFrame(search_url);

  // Expect the Lens Overlay results panel to open.
  ASSERT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       LoadURLInResultsFrameOverlayNotShowing) {
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);
  const GURL search_url("https://www.google.com/search");
  controller->LoadURLInResultsFrame(search_url);

  // Controller should not open and load URLs when overlay is not showing.
  auto* coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  EXPECT_FALSE(coordinator->IsSidePanelShowing());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_SameTabSameOriginLinkClick) {
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayController::kAppMenu);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayWidgetForTesting()->IsVisible());

  // Loading a url in the side panel should show the results page. This needs to
  // be done to set up the WebContentsObserver.
  const GURL search_url("https://www.google.com/search");
  controller->LoadURLInResultsFrame(search_url);

  // Expect the Lens Overlay results panel to open.
  auto* coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));
  int tabs = browser()->tab_strip_model()->count();

  // The results frame should be the only child frame of the side panel web
  // contents.
  content::RenderFrameHost* results_frame = content::ChildFrameAt(
      controller->GetSidePanelWebContentsForTesting()->GetPrimaryMainFrame(),
      0);
  EXPECT_TRUE(results_frame);

  // Simulate a same-origin navigation on the results frame.
  const GURL nav_url("https://www.google.com/search?q=apples&gsc=1&masfc=c");
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kSameTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // It should not open a new tab as this is a same-origin navigation.
  EXPECT_EQ(tabs, browser()->tab_strip_model()->count());

  // We should find that the input text on the searchbox is the same as the text
  // query of the nav_url.
  EXPECT_TRUE(content::EvalJs(
                  controller->GetSidePanelWebContentsForTesting()
                      ->GetPrimaryMainFrame(),
                  content::JsReplace(kCheckSearchboxInput, "apples"),
                  content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES)
                  .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_SameTabCrossOriginLinkClick) {
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayController::kAppMenu);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayWidgetForTesting()->IsVisible());

  // Loading a url in the side panel should show the results page. This needs to
  // be done to set up the WebContentsObserver.
  const GURL search_url("https://www.google.com/search");
  controller->LoadURLInResultsFrame(search_url);

  // Expect the Lens Overlay results panel to open.
  auto* coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // The results frame should be the only child frame of the side panel web
  // contents.
  content::RenderFrameHost* results_frame = content::ChildFrameAt(
      controller->GetSidePanelWebContentsForTesting()->GetPrimaryMainFrame(),
      0);
  EXPECT_TRUE(results_frame);

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  const GURL nav_url("http://new.domain.com/");
  // Simulate a cross-origin navigation on the results frame.
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kSameTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Verify the new tab has the URL.
  content::WebContents* new_tab = add_tab.Wait();
  content::WaitForLoadStop(new_tab);
  EXPECT_EQ(new_tab->GetLastCommittedURL(), nav_url);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_NewTabCrossOriginLinkClick) {
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayController::kAppMenu);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayWidgetForTesting()->IsVisible());

  // Loading a url in the side panel should show the results page. This needs to
  // be done to set up the WebContentsObserver.
  const GURL search_url("https://www.google.com/search");
  controller->LoadURLInResultsFrame(search_url);

  // Expect the Lens Overlay results panel to open.
  auto* coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // The results frame should be the only child frame of the side panel web
  // contents.
  content::RenderFrameHost* results_frame = content::ChildFrameAt(
      controller->GetSidePanelWebContentsForTesting()->GetPrimaryMainFrame(),
      0);
  const GURL nav_url("http://new.domain.com/");
  content::OverrideLastCommittedOrigin(results_frame,
                                       url::Origin::Create(search_url));
  EXPECT_TRUE(results_frame);

  // Simulate a cross-origin navigation on the results frame.
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kNewTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Verify the new tab has the URL.
  content::WebContents* new_tab = add_tab.Wait();
  content::WaitForLoadStop(new_tab);
  EXPECT_EQ(new_tab->GetLastCommittedURL(), nav_url);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_NewTabCrossOriginLinkClickFromUntrustedSite) {
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayController::kAppMenu);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayWidgetForTesting()->IsVisible());

  // Loading a url in the side panel should show the results page. This needs to
  // be done to set up the WebContentsObserver.
  const GURL search_url("https://www.google.com/search");
  controller->LoadURLInResultsFrame(search_url);

  // Expect the Lens Overlay results panel to open.
  auto* coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));
  int tabs = browser()->tab_strip_model()->count();

  // The results frame should be the only child frame of the side panel web
  // contents.
  content::RenderFrameHost* results_frame = content::ChildFrameAt(
      controller->GetSidePanelWebContentsForTesting()->GetPrimaryMainFrame(),
      0);
  const GURL nav_url("http://new.domain.com/");
  content::OverrideLastCommittedOrigin(results_frame,
                                       url::Origin::Create(nav_url));
  EXPECT_TRUE(results_frame);

  // Simulate a cross-origin navigation on the results frame.
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kNewTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // It should not open a new tab as the initatior origin should not be
  // considered "trusted".
  EXPECT_EQ(tabs, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       CloseSearchBubbleOnOverlayInteraction) {
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state. When the overlay is
  // bound, it should start the query flow which returns a response for the
  // interaction data callback.
  controller->ShowUI(LensOverlayController::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  auto* bubble_controller =
      lens::LensSearchBubbleController::FromBrowser(browser());
  EXPECT_TRUE(!!bubble_controller->bubble_view_for_testing());

  // We need to flush the mojo receiver calls to make sure the screenshot was
  // passed back to the WebUI or else the region selection UI will not render.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->FlushForTesting();
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Simulate mouse events on the overlay for drawing a manual region.
  gfx::Point center =
      GetOverlayWebContents()->GetContainerBounds().CenterPoint();
  gfx::Point off_center = gfx::Point(center);
  off_center.Offset(100, 100);
  SimulateLeftClickDrag(center, off_center);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));
  EXPECT_FALSE(!!bubble_controller->bubble_view_for_testing());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       PopAndLoadQueryFromHistory) {
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayController::kAppMenu);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayWidgetForTesting()->IsVisible());

  // Loading a url in the side panel should show the results page.
  const GURL first_search_url(
      "https://www.google.com/search?q=oranges&gsc=1&masfc=c&hl=en-US");
  controller->LoadURLInResultsFrame(first_search_url);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // The search query history stack should be empty and the currently loaded
  // query should be set.
  EXPECT_TRUE(controller->GetSearchQueryHistoryForTesting().empty());
  auto loaded_search_query = controller->GetLoadedSearchQueryForTesting();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "oranges");
  EXPECT_EQ(loaded_search_query->search_query_url_, first_search_url);
  EXPECT_TRUE(loaded_search_query->search_query_region_thumbnail_.empty());
  EXPECT_FALSE(loaded_search_query->search_query_region_);
  EXPECT_FALSE(loaded_search_query->selected_text_);

  // Loading a second url in the side panel should show the results page.
  const GURL second_search_url(
      "https://www.google.com/search?q=kiwi&gsc=1&masfc=c&hl=en-US");
  // We can't use content::WaitForLoadStop here since the last navigation is
  // successful.
  content::TestNavigationObserver observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->LoadURLInResultsFrame(second_search_url);
  observer.Wait();

  // The search query history stack should have 1 entry and the currently loaded
  // query should be set to the new query
  EXPECT_EQ(controller->GetSearchQueryHistoryForTesting().size(), 1UL);
  loaded_search_query = controller->GetLoadedSearchQueryForTesting();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "kiwi");
  EXPECT_EQ(loaded_search_query->search_query_url_, second_search_url);
  EXPECT_TRUE(loaded_search_query->search_query_region_thumbnail_.empty());
  EXPECT_FALSE(loaded_search_query->search_query_region_);
  EXPECT_FALSE(loaded_search_query->selected_text_);
  EXPECT_EQ(observer.last_navigation_url(), second_search_url);

  // Popping the query should load the previous query into the results frame.
  content::TestNavigationObserver pop_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->PopAndLoadQueryFromHistory();
  pop_observer.Wait();

  // The search query history stack should be empty and the currently loaded
  // query should be set to the previous query.
  EXPECT_TRUE(controller->GetSearchQueryHistoryForTesting().empty());
  loaded_search_query = controller->GetLoadedSearchQueryForTesting();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "oranges");
  EXPECT_EQ(loaded_search_query->search_query_url_, first_search_url);
  EXPECT_TRUE(loaded_search_query->search_query_region_thumbnail_.empty());
  EXPECT_FALSE(loaded_search_query->search_query_region_);
  EXPECT_FALSE(loaded_search_query->selected_text_);
  EXPECT_EQ(pop_observer.last_navigation_url(), first_search_url);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       PopAndLoadQueryFromHistoryWithTextSelection) {
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayController::kAppMenu);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayWidgetForTesting()->IsVisible());

  // Loading a url in the side panel should show the results page.
  const GURL first_search_url(
      "https://www.google.com/search?q=oranges&gsc=1&masfc=c&hl=en-US");
  controller->IssueTextSelectionRequestForTesting("oranges", 20, 200);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // The search query history stack should be empty and the currently loaded
  // query should be set.
  EXPECT_TRUE(controller->GetSearchQueryHistoryForTesting().empty());
  auto loaded_search_query = controller->GetLoadedSearchQueryForTesting();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "oranges");
  GURL url_without_start_time =
      RemoveStartTimeParam(loaded_search_query->search_query_url_);
  EXPECT_EQ(url_without_start_time, first_search_url);
  EXPECT_TRUE(loaded_search_query->selected_text_);
  EXPECT_EQ(loaded_search_query->selected_text_->first, 20);
  EXPECT_EQ(loaded_search_query->selected_text_->second, 200);
  EXPECT_TRUE(loaded_search_query->search_query_region_thumbnail_.empty());
  EXPECT_FALSE(loaded_search_query->search_query_region_);

  // Loading a second url in the side panel should show the results page.
  const GURL second_search_url(
      "https://www.google.com/search?q=kiwi&gsc=1&masfc=c&hl=en-US");
  // We can't use content::WaitForLoadStop here since the last navigation is
  // successful.
  content::TestNavigationObserver observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueTextSelectionRequestForTesting("kiwi", 1, 100);
  observer.Wait();

  // The search query history stack should have 1 entry and the currently loaded
  // query should be set to the new query
  EXPECT_EQ(controller->GetSearchQueryHistoryForTesting().size(), 1UL);
  loaded_search_query = controller->GetLoadedSearchQueryForTesting();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "kiwi");
  url_without_start_time =
      RemoveStartTimeParam(loaded_search_query->search_query_url_);
  EXPECT_EQ(url_without_start_time, second_search_url);
  EXPECT_TRUE(loaded_search_query->selected_text_);
  EXPECT_EQ(loaded_search_query->selected_text_->first, 1);
  EXPECT_EQ(loaded_search_query->selected_text_->second, 100);
  EXPECT_TRUE(loaded_search_query->search_query_region_thumbnail_.empty());
  EXPECT_FALSE(loaded_search_query->search_query_region_);
  url_without_start_time = RemoveStartTimeParam(observer.last_navigation_url());
  EXPECT_EQ(url_without_start_time, second_search_url);

  // Popping the query should load the previous query into the results frame.
  content::TestNavigationObserver pop_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->PopAndLoadQueryFromHistory();
  pop_observer.Wait();

  // The search query history stack should be empty and the currently loaded
  // query should be set to the previous query.
  EXPECT_TRUE(controller->GetSearchQueryHistoryForTesting().empty());
  loaded_search_query = controller->GetLoadedSearchQueryForTesting();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "oranges");
  url_without_start_time =
      RemoveStartTimeParam(loaded_search_query->search_query_url_);
  EXPECT_EQ(url_without_start_time, first_search_url);
  EXPECT_TRUE(loaded_search_query->search_query_region_thumbnail_.empty());
  EXPECT_FALSE(loaded_search_query->search_query_region_);
  EXPECT_TRUE(loaded_search_query->selected_text_);
  EXPECT_EQ(loaded_search_query->selected_text_->first, 20);
  EXPECT_EQ(loaded_search_query->selected_text_->second, 200);
  url_without_start_time =
      RemoveStartTimeParam(pop_observer.last_navigation_url());
  EXPECT_EQ(url_without_start_time, first_search_url);

  // Verify the text selection was sent back to mojo and any old selections were
  // cleared.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_TRUE(fake_controller->fake_overlay_page_.did_clear_selections_);
  EXPECT_EQ(fake_controller->fake_overlay_page_.text_selection_indexes_,
            loaded_search_query->selected_text_);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       RecordInvocationHistogram) {
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // State should start in off.
  auto* controller = browser()
                         ->tab_strip_model()
                         ->GetActiveTab()
                         ->tab_features()
                         ->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state. When the overlay is
  // bound, it should start the query flow which returns a response for the
  // interaction data callback.
  controller->ShowUI(LensOverlayController::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Showing the UI should have recorded an entry in the appropriate bucket and
  // the total count of invocations should be 1.
  histogram_tester.ExpectBucketCount("Lens.Overlay.Invoked",
                                     LensOverlayController::kAppMenu,
                                     /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked",
                                    /*expected_count=*/1);

  // Attenmpting to invoke the overlay again without closing it first should not
  // record any new entries.
  controller->ShowUI(LensOverlayController::kAppMenu);
  histogram_tester.ExpectBucketCount("Lens.Overlay.Invoked",
                                     LensOverlayController::kAppMenu,
                                     /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked",
                                    /*expected_count=*/1);

  // Closing the UI and then reopening it via any entry point should record a
  // new entry in the appropriate bucket.
  controller->CloseUI();
  controller->ShowUI(LensOverlayController::kAppMenu);
  histogram_tester.ExpectBucketCount("Lens.Overlay.Invoked",
                                     LensOverlayController::kAppMenu,
                                     /*expected_count=*/2);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked",
                                    /*expected_count=*/2);

  controller->CloseUI();
  controller->ShowUI(LensOverlayController::kContentAreaContextMenuPage);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.Invoked",
      LensOverlayController::kContentAreaContextMenuPage,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked",
                                    /*expected_count=*/3);

  controller->CloseUI();
  controller->ShowUI(LensOverlayController::kContentAreaContextMenuImage);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.Invoked",
      LensOverlayController::kContentAreaContextMenuImage,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked",
                                    /*expected_count=*/4);

  controller->CloseUI();
  controller->ShowUI(LensOverlayController::kToolbar);
  histogram_tester.ExpectBucketCount("Lens.Overlay.Invoked",
                                     LensOverlayController::kToolbar,
                                     /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked",
                                    /*expected_count=*/5);

  controller->CloseUI();
  controller->ShowUI(LensOverlayController::kFindInPage);
  histogram_tester.ExpectBucketCount("Lens.Overlay.Invoked",
                                     LensOverlayController::kFindInPage,
                                     /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked",
                                    /*expected_count=*/6);

  controller->CloseUI();
  controller->ShowUI(LensOverlayController::kOmnibox);
  histogram_tester.ExpectBucketCount("Lens.Overlay.Invoked",
                                     LensOverlayController::kOmnibox,
                                     /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked",
                                    /*expected_count=*/7);
}

}  // namespace
