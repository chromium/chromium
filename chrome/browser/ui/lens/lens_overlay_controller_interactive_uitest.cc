// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class runs CUJ tests for lens overlay. These tests simulate input events
// and cannot be run in parallel.

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_gen204_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/test_lens_search_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/location_bar/lens_overlay_homework_page_action_icon_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/contextual_tasks/public/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/point.h"

namespace {

constexpr char kDocumentWithNamedElement[] = "/select.html";
constexpr char kDocumentWithImage[] = "/test_visual.html";
constexpr char kDocumentWithVideo[] = "/media/bigbuck-player.html";
constexpr char kPdfDocument[] = "/pdf/test.pdf";

class LensOverlayControllerCUJTest : public InteractiveFeaturePromoTest {
 public:
  template <typename... Args>
  explicit LensOverlayControllerCUJTest(Args&&... args)
      : InteractiveFeaturePromoTest(
            UseDefaultTrackerAllowingPromos({std::forward<Args>(args)...})) {
    lens_search_controller_override_ =
        tabs::TabFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
            base::BindRepeating([](tabs::TabInterface& tab) {
              return std::make_unique<lens::TestLensSearchController>(&tab);
            }));
  }
  ~LensOverlayControllerCUJTest() override = default;

  void SetUp() override {
    SetUpFeatureList();
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveFeaturePromoTest::SetUp();
  }

  virtual void SetUpFeatureList() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{lens::features::kLensOverlay, {}},
                              {lens::features::kLensOverlayTranslateButton, {}},
                              {media::kContextMenuSearchForVideoFrame, {}},
                              {lens::features::kLensOverlayContextualSearchbox,
                               {{"use-pdfs-as-context", "true"},
                                {"auto-focus-searchbox", "false"}}}},
        /*disabled_features=*/{lens::features::kLensSearchZeroStateCsb});
  }

  void WaitForTemplateURLServiceToLoad() {
    auto* const template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
  }

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();

    // Permits sharing the page screenshot by default.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, true);
    prefs->SetBoolean(lens::prefs::kLensSharingPageContentEnabled, true);
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveFeaturePromoTest::TearDownOnMainThread();

    // Disallow sharing the page screenshot by default.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, false);
  }

  InteractiveTestApi::MultiStep OpenArbitraryNewTab() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTab);
    const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);

    // In kDocumentWithNamedElement.
    const DeepQuery kPathToBody{
        "body",
    };

    return Steps(AddInstrumentedTab(kNewTab, url),
                 EnsurePresent(kNewTab, kPathToBody),
                 WaitForWebContentsReady(kNewTab));
  }

  InteractiveTestApi::MultiStep OpenLensOverlay() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
    const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);

    // In kDocumentWithNamedElement.
    const DeepQuery kPathToBody{
        "body",
    };

    return Steps(InstrumentTab(kActiveTab),
                 NavigateWebContents(kActiveTab, url),
                 EnsurePresent(kActiveTab, kPathToBody),
                 WaitForWebContentsPainted(kActiveTab),

                 // Open the three dot menu and select the Lens Overlay option.
                 PressButton(kToolbarAppMenuButtonElementId),
                 WaitForShow(AppMenuModel::kShowLensOverlay),
                 SelectMenuItem(AppMenuModel::kShowLensOverlay));
  }

  InteractiveTestApi::MultiStep OpenLensOverlayFromImage() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
    const GURL url = embedded_test_server()->GetURL(kDocumentWithImage);

    // In kDocumentWithImage.
    const DeepQuery kPathToImg{
        "img",
    };

    return Steps(InstrumentTab(kActiveTab),
                 NavigateWebContents(kActiveTab, url),
                 WaitForWebContentsPainted(kActiveTab),

                 MoveMouseTo(kActiveTab, kPathToImg),
                 MayInvolveNativeContextMenu(
                     ClickMouse(ui_controls::RIGHT),
                     SelectMenuItem(RenderViewContextMenu::kSearchForImageItem,
                                    InputType::kMouse)));
  }

  InteractiveTestApi::MultiStep OpenLensOverlayFromVideo() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kVideoIsPlaying);

    const GURL url = embedded_test_server()->GetURL(kDocumentWithVideo);
    const char kPlayVideo[] = "(el) => { el.play(); }";
    const DeepQuery kPathToVideo{"video"};
    constexpr char kMediaIsPlaying[] =
        "(el) => { return el.currentTime > 0.1 && !el.paused && !el.ended && "
        "el.readyState > 2; }";

    StateChange video_is_playing;
    video_is_playing.event = kVideoIsPlaying;
    video_is_playing.where = kPathToVideo;
    video_is_playing.test_function = kMediaIsPlaying;

    return Steps(
        InstrumentTab(kActiveTab), NavigateWebContents(kActiveTab, url),
        EnsurePresent(kActiveTab, kPathToVideo),
        ExecuteJsAt(kActiveTab, kPathToVideo, kPlayVideo),
        WaitForStateChange(kActiveTab, video_is_playing),
        MoveMouseTo(kActiveTab, kPathToVideo),
        MayInvolveNativeContextMenu(
            ClickMouse(ui_controls::RIGHT),
            SelectMenuItem(RenderViewContextMenu::kSearchForVideoFrameItem,
                           InputType::kMouse)));
  }

  InteractiveTestApi::MultiStep WaitForScreenshotRendered(
      ui::ElementIdentifier overlayId) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kScreenshotIsRendered);

    const DeepQuery kPathToSelectionOverlay{"lens-overlay-app",
                                            "lens-selection-overlay"};
    constexpr char kSelectionOverlayHasBounds[] =
        "(el) => { return el.getBoundingClientRect().width > 0 && "
        "el.getBoundingClientRect().height > 0; }";

    StateChange screenshot_is_rendered;
    screenshot_is_rendered.event = kScreenshotIsRendered;
    screenshot_is_rendered.where = kPathToSelectionOverlay;
    screenshot_is_rendered.test_function = kSelectionOverlayHasBounds;

    return Steps(EnsurePresent(overlayId),
                 WaitForStateChange(overlayId, screenshot_is_rendered));
  }

  template <typename T>
  InteractiveTestApi::MultiStep OpenLensOverlayWithRegionSearch(
      ui::ElementIdentifier tab_id,
      ui::ElementIdentifier overlay_id,
      T&& target_point) {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
    const GURL url = embedded_test_server()->GetURL(kDocumentWithImage);

    // In kDocumentWithNamedElement.
    const DeepQuery kPathToBody{
        "body",
    };

    const DeepQuery kPathToRegionSelection{
        "lens-overlay-app",
        "lens-selection-overlay",
        "#regionSelectionLayer",
    };
    return Steps(
        InAnyContext(
            InstrumentTab(tab_id), NavigateWebContents(tab_id, url),
            EnsurePresent(tab_id, kPathToBody),
            WaitForWebContentsPainted(tab_id),
            WaitForWebContentsReady(tab_id, url),

            // Open the three dot menu and select the Lens Overlay option.
            PressButton(kToolbarAppMenuButtonElementId),
            WaitForShow(AppMenuModel::kShowLensOverlay),
            SelectMenuItem(AppMenuModel::kShowLensOverlay)),
        InAnyContext(
            InstrumentNonTabWebView(overlay_id,
                                    LensOverlayController::kOverlayId),
            WaitForWebContentsReady(
                overlay_id, GURL(chrome::kChromeUILensOverlayUntrustedURL))),
        InSameContext(WaitForShow(LensOverlayController::kOverlayId),
                      WaitForScreenshotRendered(overlay_id),
                      EnsurePresent(overlay_id, kPathToRegionSelection),
                      MoveMouseTo(LensOverlayController::kOverlayId),
                      DragMouseTo(std::forward<T>(target_point))));
  }

  bool TriggerLenOverlayHomeworkPageAction() {
    auto* icon_view =
        BrowserElementsViews::From(browser())->GetViewAs<IconLabelBubbleView>(
            kLensOverlayHomeworkPageActionIconElementId);

    views::FocusManager* focus_manager = icon_view->GetFocusManager();
    focus_manager->ClearFocus();
    EXPECT_FALSE(focus_manager->GetFocusedView());
    return icon_view->GetVisible();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  ui::UserDataFactory::ScopedOverride lens_search_controller_override_;
};

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User opens lens overlay.
//  (3) User clicks the "close" button to close lens overlay.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerCUJTest, OpenAndClose) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);

  // In kDocumentWithNamedElement.
  const DeepQuery kPathToBody{
      "body",
  };

  // In the lens overlay.
  const DeepQuery kPathToCloseButton{
      "lens-overlay-app",
      "#closeButton",
  };
  constexpr char kClickFn[] = "(el) => { el.click(); }";

  RunTestSequence(
      OpenLensOverlay(),

      // The overlay controller is an independent floating widget associated
      // with a tab rather than a browser window, so by convention gets its own
      // element context.
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),
      // Wait for the webview to finish loading to prevent re-entrancy.
      InSameContext(EnsurePresent(kOverlayId, kPathToCloseButton),
                    ExecuteJsAt(kOverlayId, kPathToCloseButton, kClickFn,
                                ExecuteJsMode::kFireAndForget),
                    WaitForHide(kOverlayId)));
}

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User opens lens overlay.
//  (3) User presses the escape key to close lens overlay.
#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER)
// Flaky on ASAN on Linux.
#define MAYBE_EscapeKeyClose DISABLED_EscapeKeyClose
#else
#define MAYBE_EscapeKeyClose EscapeKeyClose
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerCUJTest, MAYBE_EscapeKeyClose) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);

  // In kDocumentWithNamedElement.
  const DeepQuery kPathToBody{
      "body",
  };

  const ui::Accelerator escape_key(ui::VKEY_ESCAPE, ui::EF_NONE);

  RunTestSequence(
      OpenLensOverlay(),

      // The overlay controller is an independent floating widget associated
      // with a tab rather than a browser window, so by convention gets its own
      // element context.
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL)),
          WaitForWebContentsPainted(kOverlayId)),
      // Wait for the webview to finish loading to prevent re-entrancy.
      InSameContext(FocusWebContents(kOverlayId),
                    SendAccelerator(kOverlayId, escape_key),
                    WaitForHide(kOverlayId)));
}

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User opens lens overlay.
//  (3) User selects a region with text.
//  (4) User presses CTRL+C on some text.
//  (5) Text in region gets copied.
// TODO(crbug.com/399520257): Fix test failure on Linux, and ASAN.
#if BUILDFLAG(IS_LINUX) || defined(ADDRESS_SANITIZER)
// Flaky on ASAN, and on Linux.
#define MAYBE_CopyKeyCommandCopies DISABLED_CopyKeyCommandCopies
#else
#define MAYBE_CopyKeyCommandCopies CopyKeyCommandCopies
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerCUJTest,
                       MAYBE_CopyKeyCommandCopies) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlaySidePanelWebViewId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kTextCopiedState);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  auto top_left_point = base::BindLambdaForTesting([&](ui::TrackedElement* el) {
    return gfx::Point(el->AsA<views::TrackedElementViews>()
                          ->view()
                          ->GetBoundsInScreen()
                          .origin());
  });

  // Path to region selection layer.
  const DeepQuery kPathToRegionSelection{
      "lens-overlay-app",
      "lens-selection-overlay",
      "region-selection",
  };

  const ui::Accelerator ctrl_c_accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN);

  RunTestSequence(
      OpenLensOverlay(),

      // The overlay controller is an independent floating widget associated
      // with a tab rather than a browser window, so by convention gets its own
      // element context.
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),

      // Wait for the webview to finish loading to prevent re-entrancy. Then
      // click the center of the region selection layer to select a region.
      // Flush tasks after click to prevent flakiness.
      InSameContext(
          WaitForShow(LensOverlayController::kOverlayId),
          WaitForScreenshotRendered(kOverlayId),
          EnsurePresent(kOverlayId, kPathToRegionSelection),
          MoveMouseTo(kOverlayId, kPathToRegionSelection),
          DragMouseTo(LensOverlayController::kOverlayId, top_left_point)),

      // Clicking the overlay should have opened the side panel with the results
      // frame.
      InAnyContext(InstrumentNonTabWebView(
                       kOverlaySidePanelWebViewId,
                       LensOverlayController::kOverlaySidePanelWebViewId),
                   WaitForWebContentsReady(kOverlaySidePanelWebViewId),
                   WaitForWebContentsPainted(kOverlaySidePanelWebViewId)),

      // Press CTRL+C command and ensure the selected region is saved to
      // clipboard. Send the command to the side panel web view because in
      // actual usage, the side panel is the view with focus so it receives
      // the event right after selecting the region.
      InSameContext(
          WaitForShow(kOverlaySidePanelWebViewId),
          FocusWebContents(kOverlaySidePanelWebViewId),
          SendAccelerator(kOverlaySidePanelWebViewId, ctrl_c_accelerator),
          PollState(
              kTextCopiedState,
              [&]() {
                ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
                std::u16string clipboard_text;
                clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                    /* data_dst = */ nullptr, &clipboard_text);
                return base::EqualsASCII(clipboard_text, "This is test text.");
              }),
          WaitForState(kTextCopiedState, true)));
}

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User opens lens overlay.
//  (3) User makes a selection that opens the results side panel.
//  (4) User presses the escape key to close lens overlay.
#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER)
// Flaky on ASAN on Linux.
#define MAYBE_EscapeKeyCloseWithResultsPanel \
  DISABLED_EscapeKeyCloseWithResultsPanel
#else
#define MAYBE_EscapeKeyCloseWithResultsPanel EscapeKeyCloseWithResultsPanel
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerCUJTest,
                       MAYBE_EscapeKeyCloseWithResultsPanel) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlaySidePanelWebViewId);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);

  // In kDocumentWithNamedElement.
  const DeepQuery kPathToBody{
      "body",
  };

  const DeepQuery kPathToRegionSelection{
      "lens-overlay-app",
      "lens-selection-overlay",
      "#regionSelectionLayer",
  };
  const DeepQuery kPathToResultsFrame{
      "lens-side-panel-app",
      "#results",
  };

  auto off_center_point =
      base::BindLambdaForTesting([&](ui::TrackedElement* el) {
        return el->AsA<views::TrackedElementViews>()
                   ->view()
                   ->GetBoundsInScreen()
                   .CenterPoint() +
               gfx::Vector2d(50, 50);
      });

  const ui::Accelerator escape_key(ui::VKEY_ESCAPE, ui::EF_NONE);

  RunTestSequence(
      OpenLensOverlay(),

      // The overlay controller is an independent floating widget
      // associated with a tab rather than a browser window, so by
      // convention gets its own element context.
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),
      // Wait for the webview to finish loading to prevent re-entrancy. Then do
      // a drag offset from the center. Flush tasks after drag to prevent
      // flakiness.
      InSameContext(
          WaitForShow(LensOverlayController::kOverlayId),
          WaitForScreenshotRendered(kOverlayId),
          EnsurePresent(kOverlayId, kPathToRegionSelection),
          MoveMouseTo(LensOverlayController::kOverlayId),
          DragMouseTo(LensOverlayController::kOverlayId, off_center_point)),

      // The drag should have opened the side panel with the results frame.
      InAnyContext(
          InstrumentNonTabWebView(
              kOverlaySidePanelWebViewId,
              LensOverlayController::kOverlaySidePanelWebViewId),
          WaitForWebContentsReady(kOverlaySidePanelWebViewId),
          WaitForWebContentsPainted(kOverlaySidePanelWebViewId),
          EnsurePresent(kOverlaySidePanelWebViewId, kPathToResultsFrame)),
      // Press the escape key to and ensure the overlay closes.
      InSameContext(WaitForShow(kOverlaySidePanelWebViewId),
                    FocusWebContents(kOverlaySidePanelWebViewId),
                    SendAccelerator(kOverlaySidePanelWebViewId, escape_key),
                    WaitForHide(kOverlayId)));
}

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User opens lens overlay.
//  (3) User drags to select a manual region on the overlay.
//  (4) Side panel opens with results.
// TODO(crbug.com/355224013): Re-enable this test
IN_PROC_BROWSER_TEST_F(LensOverlayControllerCUJTest,
                       DISABLED_SelectManualRegion) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlaySidePanelWebViewId);

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  const DeepQuery kPathToRegionSelection{
      "lens-overlay-app",
      "lens-selection-overlay",
      "#regionSelectionLayer",
  };
  const DeepQuery kPathToResultsFrame{
      "lens-side-panel-app",
      "#results",
  };

  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->contents_web_view()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  RunTestSequence(
      OpenLensOverlay(),

      // The overlay controller is an independent floating widget
      // associated with a tab rather than a browser window, so by
      // convention gets its own element context.
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),
      // Wait for the webview to finish loading to prevent re-entrancy. Then do
      // a drag offset from the center. Flush tasks after drag to prevent
      // flakiness.
      InSameContext(WaitForShow(LensOverlayController::kOverlayId),
                    WaitForScreenshotRendered(kOverlayId),
                    EnsurePresent(kOverlayId, kPathToRegionSelection),
                    MoveMouseTo(LensOverlayController::kOverlayId),
                    DragMouseTo(off_center_point)),

      // The drag should have opened the side panel with the results frame.
      InAnyContext(
          InstrumentNonTabWebView(
              kOverlaySidePanelWebViewId,
              LensOverlayController::kOverlaySidePanelWebViewId),
          EnsurePresent(kOverlaySidePanelWebViewId, kPathToResultsFrame)));
}

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User right-clicks an image and opens lens overlay.
//  (3) Side panel opens with results.
// Disabled on mac because the mac interaction test
// util implementation does not support setting the input (mouse / keyboard)
// type for a context menu item selection.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SearchForImage DISABLED_SearchForImage
#else
#define MAYBE_SearchForImage SearchForImage
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerCUJTest, MAYBE_SearchForImage) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlaySidePanelWebViewId);

  const DeepQuery kPathToRegionSelection{
      "lens-overlay-app",
      "lens-selection-overlay",
      "#regionSelectionLayer",
  };
  const DeepQuery kPathToResultsFrame{
      "lens-side-panel-app",
      "#results",
  };

  RunTestSequence(
      OpenLensOverlayFromImage(),

      // The overlay controller is an independent floating widget
      // associated with a tab rather than a browser window, so by
      // convention gets its own element context.
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),

      // The side panel should open with the results frame.
      InAnyContext(
          InstrumentNonTabWebView(
              kOverlaySidePanelWebViewId,
              LensOverlayController::kOverlaySidePanelWebViewId),
          EnsurePresent(kOverlaySidePanelWebViewId, kPathToResultsFrame)));
}

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User right-clicks a video and opens "Search with Google Lens".
//  (3) Side panel opens with results.
// Disabled on mac because the mac interaction test
// util implementation does not support setting the input (mouse / keyboard)
// type for a context menu item selection.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SearchForVideoFrame DISABLED_SearchForVideoFrame
#else
#define MAYBE_SearchForVideoFrame SearchForVideoFrame
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerCUJTest,
                       MAYBE_SearchForVideoFrame) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlaySidePanelWebViewId);

  const DeepQuery kPathToRegionSelection{
      "lens-overlay-app",
      "lens-selection-overlay",
      "#regionSelectionLayer",
  };
  const DeepQuery kPathToResultsFrame{
      "lens-side-panel-app",
      "#results",
  };

  RunTestSequence(
      OpenLensOverlayFromVideo(),

      // The overlay controller is an independent floating widget
      // associated with a tab rather than a browser window, so by
      // convention gets its own element context.
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),

      // The side panel should open with the results frame.
      InAnyContext(
          InstrumentNonTabWebView(
              kOverlaySidePanelWebViewId,
              LensOverlayController::kOverlaySidePanelWebViewId),
          EnsurePresent(kOverlaySidePanelWebViewId, kPathToResultsFrame)));
}

// This tests the following CUJ:
//  (1) User navigates to a PDF.
//  (2) User opens lens overlay.
//  (3) The CSB should say "Ask about this document"
//  (3) User make a query
//  (4) This side panel opens
//  (5) The CSB should say "Ask about this document"
//  (6) The user navigates to a webpage.
//  (7) The CSB should say "Ask about this page"
IN_PROC_BROWSER_TEST_F(LensOverlayControllerCUJTest, NavigationsUpdateCSB) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlaySidePanelWebViewId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kHintTextUpdatedEvent);

  const GURL pdf_url = embedded_test_server()->GetURL(kPdfDocument);

  // Paths to searchbox hint text.
  const DeepQuery kPathToOverlaySearchboxInput{
      "lens-overlay-app",
      "cr-searchbox",
      "input",
  };
  const DeepQuery kPathToSidePanelSearchboxInput{
      "lens-side-panel-app",
      "cr-searchbox",
      "input",
  };
  const DeepQuery kPathToOverlayGhostLoaderText{
      "lens-overlay-app", "cr-searchbox-ghost-loader", "#hint-text1"};
  const DeepQuery kPathToSidePanelGhostLoaderText{
      "lens-side-panel-app", "cr-searchbox-ghost-loader", "#hint-text1"};

  // Helper function to check for specific text in an element.
  auto CheckSearchboxHintText = [](ui::ElementIdentifier web_contents_id,
                                   const DeepQuery& query,
                                   const std::string& expected_text) {
    return CheckJsResultAt(web_contents_id, query,
                           base::StringPrintf("el => el.placeholder === '%s'",
                                              expected_text.c_str()));
  };
  auto CheckGhostLoaderText = [](ui::ElementIdentifier web_contents_id,
                                 const DeepQuery& query,
                                 const std::string& expected_text) {
    return CheckJsResultAt(
        web_contents_id, query,
        base::StringPrintf("el => el.innerText.trim() === '%s'",
                           expected_text.c_str()));
  };

  // State change to wait for the hint text to update.
  StateChange hint_text_updated;
  hint_text_updated.event = kHintTextUpdatedEvent;
  hint_text_updated.type = StateChange::Type::kExistsAndConditionTrue;
  hint_text_updated.where = kPathToSidePanelSearchboxInput;
  hint_text_updated.test_function =
      "el => el.placeholder === 'Ask about this document'";

  RunTestSequence(
      // TODO(crbug.com/355224013): Ideally, this opens with a PDF to start,
      // but the right click menu item is not accessible in the UI test on PDF.
      // Once these tests can open without the rigth click menu, this should be
      // updated to open a PDF.
      OpenLensOverlay(),

      // The overlay controller is an independent floating widget associated
      // with a tab rather than a browser window, so by convention gets its own
      // element context.
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),

      // The CSB should be in the overlay with the text "Ask about this
      // document".
      InSameContext(
          WaitForShow(LensOverlayController::kOverlayId),
          WaitForScreenshotRendered(kOverlayId),
          CheckSearchboxHintText(kOverlayId, kPathToOverlaySearchboxInput,
                                 "Ask about this page"),
          CheckGhostLoaderText(kOverlayId, kPathToOverlayGhostLoaderText,
                               "Generating suggestions for this page…")),

      // The use makes a query in the searchbox and the side panel opens.
      InSameContext(
          // Focus the overlay to receive input events.
          FocusWebContents(kOverlayId),

          // Focus the searchbox.
          ExecuteJsAt(kOverlayId, kPathToOverlaySearchboxInput,
                      "(el) => { el.focus(); }",
                      ExecuteJsMode::kWaitForCompletion),

          // Emulate focus into the searchbox.
          ExecuteJsAt(
              kOverlayId, kPathToOverlaySearchboxInput,
              base::StringPrintf(
                  "(el) => { el.value = '%s'; el.dispatchEvent(new "
                  "Event('input', { bubbles: true })); el.dispatchEvent(new "
                  "Event('change', { bubbles: true }));}",
                  "test query"),
              ExecuteJsMode::kWaitForCompletion),

          // Simulate the enter key being pressed.
          ExecuteJsAt(
              kOverlayId, kPathToOverlaySearchboxInput,
              base::StringPrintf(
                  "(el) => { el.dispatchEvent(new KeyboardEvent('keydown', { "
                  "key:'%s', bubbles: true }));}",
                  "Enter"),
              ExecuteJsMode::kFireAndForget)),

      // Side panel should open.
      InAnyContext(InstrumentNonTabWebView(
                       kOverlaySidePanelWebViewId,
                       LensOverlayController::kOverlaySidePanelWebViewId),
                   WaitForWebContentsReady(kOverlaySidePanelWebViewId)),

      // The CSB in the side panel should say "Ask about this document"
      InSameContext(
          CheckSearchboxHintText(kOverlaySidePanelWebViewId,
                                 kPathToSidePanelSearchboxInput,
                                 "Ask about this page"),
          CheckGhostLoaderText(kOverlaySidePanelWebViewId,
                               kPathToSidePanelGhostLoaderText,
                               "Generating suggestions for this page…")),

      // The user navigates to a webpage.
      InAnyContext(InstrumentTab(kActiveTab),
                   NavigateWebContents(kActiveTab, pdf_url)),

      // The CSB in the overlay should eventually say "Ask about this page"
      InAnyContext(
          WaitForStateChange(kOverlaySidePanelWebViewId, hint_text_updated),
          CheckSearchboxHintText(kOverlaySidePanelWebViewId,
                                 kPathToSidePanelSearchboxInput,
                                 "Ask about this document"),
          CheckGhostLoaderText(kOverlaySidePanelWebViewId,
                               kPathToSidePanelGhostLoaderText,
                               "Generating suggestions for this document…")));
}

class LensOverlayControllerPromoTest : public LensOverlayControllerCUJTest {
 public:
  LensOverlayControllerPromoTest()
      : LensOverlayControllerCUJTest(
            feature_engagement::kIPHSidePanelLensOverlayPinnableFeature,
            feature_engagement::
                kIPHSidePanelLensOverlayPinnableFollowupFeature) {}
  ~LensOverlayControllerPromoTest() override = default;
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerPromoTest, ShowsPromo) {
  // Use the same setup and initial sequence as `SelectManualRegion` above in
  // order to trigger the side panel.

  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  const DeepQuery kPathToRegionSelection{
      "lens-overlay-app",
      "lens-selection-overlay",
      "#regionSelectionLayer",
  };
  const DeepQuery kPathToResultsFrame{
      "lens-side-panel-app",
      "#results",
  };

  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->contents_web_view()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  RunTestSequence(
      OpenLensOverlay(),

      // The overlay controller is an independent floating widget
      // associated with a tab rather than a browser window, so by
      // convention gets its own element context.
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),
      // Wait for the webview to finish loading to prevent re-entrancy. Then do
      // a drag offset from the center. Flush tasks after drag to prevent
      // flakiness.
      InSameContext(WaitForShow(LensOverlayController::kOverlayId),
                    WaitForScreenshotRendered(kOverlayId),
                    EnsurePresent(kOverlayId, kPathToRegionSelection),
                    MoveMouseTo(LensOverlayController::kOverlayId),
                    DragMouseTo(off_center_point)),

      // The drag should have opened the side panel with the results frame.
      WaitForShow(LensOverlayController::kOverlaySidePanelWebViewId),

      // Wait for the initial "do you want to pin this?" help bubble.
      WaitForPromo(feature_engagement::kIPHSidePanelLensOverlayPinnableFeature),

      // Pin the side panel. This should dismiss the IPH, but also launch
      // another, so wait for the first hide to avoid the next check picking up
      // the wrong help bubble.
      PressButton(kSidePanelPinButtonElementId),

      // Specifying transition-only-on-event here means even if there is a
      // different help bubble already in-frame, this step will succeed when any
      // help bubble goes away.
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting)
          .SetTransitionOnlyOnEvent(true),

      // A second IPH should appear showing where the item was pinned.
      WaitForPromo(
          feature_engagement::kIPHSidePanelLensOverlayPinnableFollowupFeature));
}

class LensOverlayControllerTranslatePromoTest
    : public LensOverlayControllerCUJTest {
 public:
  LensOverlayControllerTranslatePromoTest()
      : LensOverlayControllerCUJTest(
            feature_engagement::kIPHLensOverlayTranslateButtonFeature) {}
  ~LensOverlayControllerTranslatePromoTest() override = default;
};

// This tests the following promo flow:
//  (1) User opens the Lens Overlay.
//  (2) Promo shows. After, user clicks the translate button.
//  (3) Promo hides.
// TODO(crbug.com/392907122): Re-enable this test once the translate button is
// in a launchable state.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerTranslatePromoTest,
                       DISABLED_ShowsTranslatePromo) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);

  const DeepQuery kPathToTranslateButton{
      "lens-overlay-app",
      "#translateButton",
      "#translateEnableButton",
  };
  RunTestSequence(
      OpenLensOverlay(),

      // The overlay controller is an independent floating widget
      // associated with a tab rather than a browser window, so by
      // convention gets its own element context.
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),

      // Wait for the webview to finish loading to prevent re-entrancy.
      InSameContext(WaitForShow(LensOverlayController::kOverlayId),
                    WaitForScreenshotRendered(kOverlayId),
                    EnsurePresent(kOverlayId, kPathToTranslateButton)),

      // Wait for the initial translate promo help bubble.
      WaitForPromo(feature_engagement::kIPHLensOverlayTranslateButtonFeature),

      // Click the translate button element.
      ClickElement(kOverlayId, kPathToTranslateButton),

      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

class LensPreselectionBubbleInteractiveUiTest
    : public LensOverlayControllerCUJTest {
 public:
  LensPreselectionBubbleInteractiveUiTest() = default;
  ~LensPreselectionBubbleInteractiveUiTest() override = default;
  LensPreselectionBubbleInteractiveUiTest(
      const LensPreselectionBubbleInteractiveUiTest&) = delete;
  void operator=(const LensPreselectionBubbleInteractiveUiTest&) = delete;

  auto SetConnectionOffline() {
    return Do(base::BindLambdaForTesting([&]() {
      // Set the network connection type to being offline.
      scoped_mock_network_change_notifier =
          std::make_unique<net::test::ScopedMockNetworkChangeNotifier>();
      scoped_mock_network_change_notifier->mock_network_change_notifier()
          ->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
    }));
  }

  void TearDownOnMainThread() override {
    scoped_mock_network_change_notifier.reset();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<views::Widget> preselection_widget_;
  std::unique_ptr<net::test::ScopedMockNetworkChangeNotifier>
      scoped_mock_network_change_notifier;
};

// This tests the following CUJ:
//  (1) User opens the Lens Overlay while offline..
//  (2) The user presses the exit button in the preselection bubble.
//  (3) The overlay should close.
IN_PROC_BROWSER_TEST_F(LensPreselectionBubbleInteractiveUiTest,
                       PermissionBubbleOffline) {
  RunTestSequence(EnsureNotPresent(kLensPreselectionBubbleExitButtonElementId),
                  SetConnectionOffline(), OpenLensOverlay(),
                  WaitForShow(kLensPreselectionBubbleExitButtonElementId),
                  PressButton(kLensPreselectionBubbleExitButtonElementId),
                  WaitForHide(LensOverlayController::kOverlayId));
}

using LensOverlayControllerReturnToPageCUJTest = LensOverlayControllerCUJTest;

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User opens lens overlay and the side panel opens.
//  (3) User navigates to a new page in the same tab.
//  (4) The overlay should close, but the side panel should remain open.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerReturnToPageCUJTest,
                       HidesOverlayOnClobberTab) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  const GURL second_url = embedded_test_server()->GetURL(kDocumentWithVideo);
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  const DeepQuery kPathToRegionSelection{
      "lens-overlay-app",
      "lens-selection-overlay",
      "#regionSelectionLayer",
  };

  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->contents_web_view()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  RunTestSequence(
      // Open lens overlay.
      OpenLensOverlay(),

      // The overlay controller is an independent floating widget associated
      // with a tab rather than a browser window, so by convention gets its own
      // element context.
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),

      // Wait for the webview to finish loading to prevent re-entrancy. Then do
      // a drag offset from the center.
      InSameContext(WaitForShow(LensOverlayController::kOverlayId),
                    WaitForScreenshotRendered(kOverlayId),
                    EnsurePresent(kOverlayId, kPathToRegionSelection),
                    MoveMouseTo(LensOverlayController::kOverlayId),
                    DragMouseTo(off_center_point)),

      // The drag should have opened the side panel with the results frame.
      WaitForShow(LensOverlayController::kOverlaySidePanelWebViewId),

      // Navigate to another page in the same tab.
      // The user navigates to a webpage.
      InAnyContext(InstrumentTab(kActiveTab),
                   NavigateWebContents(kActiveTab, second_url)),

      // Ensure overlay is not visible but side panel is.
      WaitForHide(kOverlayId), EnsureNotPresent(kOverlayId),
      EnsurePresent(LensOverlayController::kOverlaySidePanelWebViewId));
}

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User opens lens overlay.
//  (3) User searches a region and the side panel opens.
//  (4) User clicks the close button.
//  (5) The overlay should close, but the side panel should remain open.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerReturnToPageCUJTest,
                       CloseButtonHidesOnlyOverlay) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  // In kDocumentWithNamedElement.
  const DeepQuery kPathToBody{
      "body",
  };

  // In the lens overlay.
  const DeepQuery kPathToCloseButton{
      "lens-overlay-app",
      "lens-selection-overlay",
      "#closeButton",
  };
  const DeepQuery kPathToRegionSelection{
      "lens-overlay-app",
      "lens-selection-overlay",
      "#regionSelectionLayer",
  };
  constexpr char kClickFn[] = "(el) => { el.click(); }";

  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->contents_web_view()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  RunTestSequence(
      OpenLensOverlay(),

      // The overlay controller is an independent floating widget associated
      // with a tab rather than a browser window, so by convention gets its own
      // element context.
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),
      // Wait for the webview to finish loading to prevent re-entrancy. Then do
      // a drag offset from the center.
      InSameContext(WaitForShow(LensOverlayController::kOverlayId),
                    WaitForScreenshotRendered(kOverlayId),
                    EnsurePresent(kOverlayId, kPathToRegionSelection),
                    MoveMouseTo(LensOverlayController::kOverlayId),
                    DragMouseTo(off_center_point)),

      // The drag should have opened the side panel with the results frame.
      WaitForShow(LensOverlayController::kOverlaySidePanelWebViewId),

      // Wait for the webview to finish loading to prevent re-entrancy.
      InSameContext(EnsurePresent(kOverlayId, kPathToCloseButton),
                    ExecuteJsAt(kOverlayId, kPathToCloseButton, kClickFn,
                                ExecuteJsMode::kFireAndForget),
                    WaitForHide(kOverlayId)),

      // Ensure side panel is still visible.
      EnsurePresent(LensOverlayController::kOverlaySidePanelWebViewId));
}

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User opens lens overlay.
//  (3) User searches a region and the side panel opens.
//  (4) User opens a new tab.
//  (5) The overlay and side panel should close/hide.
//  (6) User navigates back to the original tab.
//  (7) The overlay and side panel should reshow.
// NOTE: The image context menu item is not supported on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_OverlayReshowsWhenTabIsSwitchedBackToForeground \
  DISABLED_OverlayReshowsWhenTabIsSwitchedBackToForeground
#else
#define MAYBE_OverlayReshowsWhenTabIsSwitchedBackToForeground \
  OverlayReshowsWhenTabIsSwitchedBackToForeground
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerReturnToPageCUJTest,
                       MAYBE_OverlayReshowsWhenTabIsSwitchedBackToForeground) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->contents_web_view()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  RunTestSequence(
      OpenLensOverlayFromImage(),

      // The overlay controller is an independent floating widget associated
      // with a tab rather than a browser window, so by convention gets its own
      // element context.
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),

      // The opening from an image should have opened the side panel with the
      // results frame.
      WaitForShow(LensOverlayController::kOverlaySidePanelWebViewId),

      // Wait for the webview to finish loading to prevent re-entrancy.
      OpenArbitraryNewTab(),

      // Ensure side panel and overlay are not visible.
      EnsureNotPresent(LensOverlayController::kOverlayId),
      EnsureNotPresent(LensOverlayController::kOverlaySidePanelWebViewId),

      // Switch back to the original tab.
      SelectTab(kTabStripElementId, 0),

      // Overlay and side panel should be visible again.
      WaitForShow(LensOverlayController::kOverlayId),
      WaitForShow(LensOverlayController::kOverlaySidePanelWebViewId));
}

class LensOverlayControllerStraightToSrpTest
    : public LensOverlayControllerCUJTest {
 public:
  LensOverlayControllerStraightToSrpTest() = default;
  ~LensOverlayControllerStraightToSrpTest() override = default;
  LensOverlayControllerStraightToSrpTest(
      const LensOverlayControllerStraightToSrpTest&) = delete;
  void operator=(const LensOverlayControllerStraightToSrpTest&) = delete;

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(
             lens::features::kLensOverlayStraightToSrp, {}),
         base::test::FeatureRefAndParams(
             lens::features::kLensOverlayEduActionChip,
             {{"url-allow-filters", "[\"*\"]"},
              {"url-path-match-allow-filters", "[\"select\"]"}})},
        {lens::features::kLensOverlayOptimizationFilter});
  }
};

// This tests the following CUJ:
//  (1) User navigates to a website that triggers the homework action chip.
//  (2) User clicks the action chip and the side panel opens with CSB results.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerStraightToSrpTest,
                       HomeworkActionChipOpensCsbResults) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlaySidePanelWebViewId);

  const DeepQuery kPathToSidePanelSearchboxInput{
      "lens-side-panel-app",
      "cr-searchbox",
      "input",
  };

  // Helper function to check for specific text in an element.
  auto CheckSearchboxValue = [](ui::ElementIdentifier web_contents_id,
                                const DeepQuery& query,
                                const std::string& expected_text) {
    return CheckJsResultAt(
        web_contents_id, query,
        base::StringPrintf("el => el.value === '%s'", expected_text.c_str()));
  };

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  // Navigate to a matching page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
  // We need to wait for paint in order to take a screenshot of the page.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()
        ->tab_strip_model()
        ->GetActiveTab()
        ->GetContents()
        ->CompletedFirstVisuallyNonEmptyPaint();
  }));
  ASSERT_TRUE(TriggerLenOverlayHomeworkPageAction());

  RunTestSequence(
      PressButton(kLensOverlayHomeworkPageActionIconElementId),
      // Side panel should open.
      InAnyContext(InstrumentNonTabWebView(
                       kOverlaySidePanelWebViewId,
                       LensOverlayController::kOverlaySidePanelWebViewId),
                   WaitForWebContentsReady(kOverlaySidePanelWebViewId)),

      // The CSB query in the side panel should say "help me with this"
      InSameContext(CheckSearchboxValue(kOverlaySidePanelWebViewId,
                                        kPathToSidePanelSearchboxInput,
                                        "help me with this")));
}

class LensOverlayControllerStraightToSrpCustomQueryTest
    : public LensOverlayControllerCUJTest {
 public:
  LensOverlayControllerStraightToSrpCustomQueryTest() = default;
  ~LensOverlayControllerStraightToSrpCustomQueryTest() override = default;
  LensOverlayControllerStraightToSrpCustomQueryTest(
      const LensOverlayControllerStraightToSrpCustomQueryTest&) = delete;
  void operator=(const LensOverlayControllerStraightToSrpCustomQueryTest&) =
      delete;

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(
             lens::features::kLensOverlayStraightToSrp,
             {{"query", "use this query instead"}}),
         base::test::FeatureRefAndParams(
             lens::features::kLensOverlayEduActionChip,
             {{"url-allow-filters", "[\"*\"]"},
              {"url-path-match-allow-filters", "[\"select\"]"}})},
        {lens::features::kLensOverlayOptimizationFilter});
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerStraightToSrpCustomQueryTest,
                       HomeworkActionChipOpensCsbResults) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlaySidePanelWebViewId);

  const DeepQuery kPathToSidePanelSearchboxInput{
      "lens-side-panel-app",
      "cr-searchbox",
      "input",
  };

  // Helper function to check for specific text in an element.
  auto CheckSearchboxValue = [](ui::ElementIdentifier web_contents_id,
                                const DeepQuery& query,
                                const std::string& expected_text) {
    return CheckJsResultAt(
        web_contents_id, query,
        base::StringPrintf("el => el.value === '%s'", expected_text.c_str()));
  };

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  // Navigate to a matching page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
  // We need to wait for paint in order to take a screenshot of the page.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()
        ->tab_strip_model()
        ->GetActiveTab()
        ->GetContents()
        ->CompletedFirstVisuallyNonEmptyPaint();
  }));
  ASSERT_TRUE(TriggerLenOverlayHomeworkPageAction());

  RunTestSequence(
      PressButton(kLensOverlayHomeworkPageActionIconElementId),
      // Side panel should open.
      InAnyContext(InstrumentNonTabWebView(
                       kOverlaySidePanelWebViewId,
                       LensOverlayController::kOverlaySidePanelWebViewId),
                   WaitForWebContentsReady(kOverlaySidePanelWebViewId)),

      // The CSB query in the side panel should say "use this query instead"
      InSameContext(CheckSearchboxValue(kOverlaySidePanelWebViewId,
                                        kPathToSidePanelSearchboxInput,
                                        "use this query instead")));
}

class LensOverlayControllerEduActionChipTest
    : public LensOverlayControllerCUJTest {
 public:
  LensOverlayControllerEduActionChipTest() = default;
  ~LensOverlayControllerEduActionChipTest() override = default;
  LensOverlayControllerEduActionChipTest(
      const LensOverlayControllerEduActionChipTest&) = delete;
  void operator=(const LensOverlayControllerEduActionChipTest&) = delete;

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(
             lens::features::kLensOverlayEduActionChip,
             {{"max-shown-count", "5"}}),
         base::test::FeatureRefAndParams(
             lens::features::kLensOverlayOptimizationFilter, {})},
        {lens::features::kLensOverlayStraightToSrp,
         lens::features::kLensSearchZeroStateCsb});
  }

  void SetupOptimizationFilter() {
    auto* optimization_guide_decider =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    // Simulate the URL being allowed by both the allowlist and the blocklist.
    optimization_guide_decider->AddHintWithMultipleOptimizationsForTesting(
        GURL(embedded_test_server()->GetURL(kDocumentWithNamedElement)),
        {optimization_guide::proto::LENS_OVERLAY_EDU_ACTION_CHIP_ALLOWLIST,
         optimization_guide::proto::LENS_OVERLAY_EDU_ACTION_CHIP_BLOCKLIST});
  }
};

// This tests the following CUJ:
//  (1) User navigates to a website that triggers the homework action chip.
//  (2) User clicks the action chip and the overlay opens. The chip should hide.
//  (3) User opens a new tab, then switches back. The chip should remain hidden.
//  (4) User closes the overlay.
//  (5) The chip should reshow.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerEduActionChipTest,
                       HomeworkActionChipHidesWhenOverlayOpen) {
  SetupOptimizationFilter();
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);

  // In kDocumentWithNamedElement.
  const DeepQuery kPathToBody{
      "body",
  };

  // In the lens overlay.
  const DeepQuery kPathToCloseButton{
      "lens-overlay-app",
      "#closeButton",
  };
  constexpr char kClickFn[] = "(el) => { el.click(); }";

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  // Navigate to a matching page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
  // We need to wait for paint in order to take a screenshot of the page.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()
        ->tab_strip_model()
        ->GetActiveTab()
        ->GetContents()
        ->CompletedFirstVisuallyNonEmptyPaint();
  }));
  ASSERT_TRUE(TriggerLenOverlayHomeworkPageAction());

  RunTestSequence(
      // Ensure homework chip is visible.
      EnsurePresent(kLensOverlayHomeworkPageActionIconElementId),

      PressButton(kLensOverlayHomeworkPageActionIconElementId),

      // The overlay controller is an independent floating widget associated
      // with a tab rather than a browser window, so by convention gets its own
      // element context.
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),

      // Ensure homework chip is not visible after the overlay opens.
      EnsureNotPresent(kLensOverlayHomeworkPageActionIconElementId),

      OpenArbitraryNewTab(),

      // Switch back to the original tab.
      SelectTab(kTabStripElementId, 0),

      // Ensure homework chip is still not visible.
      EnsureNotPresent(kLensOverlayHomeworkPageActionIconElementId),

      InSameContext(EnsurePresent(kOverlayId, kPathToCloseButton),
                    ExecuteJsAt(kOverlayId, kPathToCloseButton, kClickFn,
                                ExecuteJsMode::kFireAndForget),
                    WaitForHide(kOverlayId)),

      // Ensure homework chip is visible again.
      EnsurePresent(kLensOverlayHomeworkPageActionIconElementId));
}

class LensOverlayControllerZeroStateCsbTest
    : public LensOverlayControllerCUJTest {
 public:
  LensOverlayControllerZeroStateCsbTest() = default;
  ~LensOverlayControllerZeroStateCsbTest() override = default;
  LensOverlayControllerZeroStateCsbTest(
      const LensOverlayControllerStraightToSrpTest&) = delete;
  void operator=(const LensOverlayControllerZeroStateCsbTest&) = delete;

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(
            lens::features::kLensSearchZeroStateCsb, {})},
        {});
  }
};

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User opens lens overlay and the side panel opens with CSB results.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerZeroStateCsbTest,
                       OpenLensOverlayOpensResults) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlaySidePanelWebViewId);

  const DeepQuery kPathToSidePanelSearchboxInput{
      "lens-side-panel-app",
      "cr-searchbox",
      "input",
  };

  // Helper function to check for specific text in an element.
  auto CheckSearchboxValue = [](ui::ElementIdentifier web_contents_id,
                                const DeepQuery& query,
                                const std::string& expected_text) {
    return CheckJsResultAt(
        web_contents_id, query,
        base::StringPrintf("el => el.value === '%s'", expected_text.c_str()));
  };

  RunTestSequence(
      OpenLensOverlay(),
      // Side panel should open.
      InAnyContext(InstrumentNonTabWebView(
                       kOverlaySidePanelWebViewId,
                       LensOverlayController::kOverlaySidePanelWebViewId),
                   WaitForWebContentsReady(kOverlaySidePanelWebViewId)),

      // The CSB query in the side panel should be empty.
      InSameContext(CheckSearchboxValue(kOverlaySidePanelWebViewId,
                                        kPathToSidePanelSearchboxInput, "")));
}

class ContextualTasksLensOverlayControllerInteractiveUiTest
    : public LensOverlayControllerCUJTest {
 public:
  ContextualTasksLensOverlayControllerInteractiveUiTest() = default;
  ~ContextualTasksLensOverlayControllerInteractiveUiTest() override = default;

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{contextual_tasks::kContextualTasks, {}}},
        /*disabled_features=*/{lens::features::kLensSearchZeroStateCsb});
  }

  InteractiveTestApi::MultiStep WaitForContextualPanelAndLensToClose(
      int tab_index = 0) {
    return Steps(
        WaitForHide(kContextualTasksSidePanelWebViewElementId),
        Do([this, tab_index]() {
          // Verify Lens Overlay is closed.
          content::WebContents* web_contents =
              browser()->tab_strip_model()->GetWebContentsAt(tab_index);
          auto* lens_controller =
              LensSearchController::FromTabWebContents(web_contents);
          EXPECT_TRUE(lens_controller->IsClosing() || lens_controller->IsOff());
        }));
  }
};

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/469373433): Flaky on Windows.
#define MAYBE_LensSessionClosesOnSidePanelClose \
  DISABLED_LensSessionClosesOnSidePanelClose
#else
#define MAYBE_LensSessionClosesOnSidePanelClose \
  LensSessionClosesOnSidePanelClose
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksLensOverlayControllerInteractiveUiTest,
                       MAYBE_LensSessionClosesOnSidePanelClose) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);

  browser()->GetFeatures().side_panel_ui()->DisableAnimationsForTesting();
  contextual_tasks::ContextualTasksSidePanelCoordinator* coordinator =
      contextual_tasks::ContextualTasksSidePanelCoordinator::From(browser());

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->contents_web_view()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  RunTestSequence(
      OpenLensOverlayWithRegionSearch(kFirstTab, kOverlayId, off_center_point),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Close the panel after it is opened.
        coordinator->Close();
      }),
      WaitForContextualPanelAndLensToClose());
}

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/469373433): Flaky on Windows.
#define MAYBE_LensSessionsCloseOnSidePanelClose_MultiTab \
  DISABLED_LensSessionsCloseOnSidePanelClose_MultiTab
#else
#define MAYBE_LensSessionsCloseOnSidePanelClose_MultiTab \
  LensSessionsCloseOnSidePanelClose_MultiTab
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksLensOverlayControllerInteractiveUiTest,
                       MAYBE_LensSessionsCloseOnSidePanelClose_MultiTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);

  browser()->GetFeatures().side_panel_ui()->DisableAnimationsForTesting();
  contextual_tasks::ContextualTasksSidePanelCoordinator* coordinator =
      contextual_tasks::ContextualTasksSidePanelCoordinator::From(browser());
  contextual_tasks::ContextualTasksContextController*
      contextual_tasks_controller =
          contextual_tasks::ContextualTasksContextControllerFactory::
              GetForProfile(browser()->profile());

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->contents_web_view()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  RunTestSequence(
      OpenLensOverlayWithRegionSearch(kFirstTab, kOverlayId, off_center_point),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      OpenArbitraryNewTab(),
      EnsureNotPresent(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Associate the task from tab0 to this new tab.
        SessionID tab_id0 = sessions::SessionTabHelper::IdForTab(
            browser()->tab_strip_model()->GetWebContentsAt(0));
        auto task =
            contextual_tasks_controller->GetContextualTaskForTab(tab_id0);
        contextual_tasks_controller->AssociateTabWithTask(
            task->GetTaskId(),
            sessions::SessionTabHelper::IdForTab(
                browser()->tab_strip_model()->GetWebContentsAt(1)));

        // Show contextual tasks side panel.
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Close the panel after it is opened.
        coordinator->Close();
      }),
      WaitForContextualPanelAndLensToClose());
}

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/469373433): Flaky on Windows.
#define MAYBE_LensSessionsCloseOnSidePanelClose_MultipleLensSessions \
  DISABLED_LensSessionsCloseOnSidePanelClose_MultipleLensSessions
#else
#define MAYBE_LensSessionsCloseOnSidePanelClose_MultipleLensSessions \
  LensSessionsCloseOnSidePanelClose_MultipleLensSessions
#endif
IN_PROC_BROWSER_TEST_F(
    ContextualTasksLensOverlayControllerInteractiveUiTest,
    MAYBE_LensSessionsCloseOnSidePanelClose_MultipleLensSessions) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);

  browser()->GetFeatures().side_panel_ui()->DisableAnimationsForTesting();
  contextual_tasks::ContextualTasksSidePanelCoordinator* coordinator =
      contextual_tasks::ContextualTasksSidePanelCoordinator::From(browser());

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->contents_web_view()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  RunTestSequence(
      OpenLensOverlayWithRegionSearch(kFirstTab, kOverlayId, off_center_point),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      OpenArbitraryNewTab(),
      EnsureNotPresent(kContextualTasksSidePanelWebViewElementId),
      OpenLensOverlayWithRegionSearch(kSecondTab, kSecondOverlayId,
                                      off_center_point),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Close the panel after it is opened.
        coordinator->Close();
      }),
      WaitForHide(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Verify Lens Overlay is not closing on the first tab.
        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetWebContentsAt(0);
        auto* lens_controller =
            LensSearchController::FromTabWebContents(web_contents);
        EXPECT_FALSE(lens_controller->IsClosing() || lens_controller->IsOff());

        // Verify Lens Overlay is closed on the second tab.
        content::WebContents* web_contents1 =
            browser()->tab_strip_model()->GetWebContentsAt(1);
        auto* lens_controller1 =
            LensSearchController::FromTabWebContents(web_contents1);
        EXPECT_TRUE(lens_controller1->IsClosing() || lens_controller1->IsOff());
      }));
}

class TabScopedContextualTasksLensOverlayControllerInteractiveUiTest
    : public ContextualTasksLensOverlayControllerInteractiveUiTest {
 public:
  TabScopedContextualTasksLensOverlayControllerInteractiveUiTest() = default;
  ~TabScopedContextualTasksLensOverlayControllerInteractiveUiTest() override =
      default;

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{contextual_tasks::kContextualTasks, {}},
                              {contextual_tasks::kContextualTasksContext,
                               {{"TaskScopedSidePanel", "false"}}}},
        /*disabled_features=*/{lens::features::kLensSearchZeroStateCsb});
  }
};

IN_PROC_BROWSER_TEST_F(
    TabScopedContextualTasksLensOverlayControllerInteractiveUiTest,
    LensSessionClosesOnSidePanelClose) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);

  browser()->GetFeatures().side_panel_ui()->DisableAnimationsForTesting();
  contextual_tasks::ContextualTasksSidePanelCoordinator* coordinator =
      contextual_tasks::ContextualTasksSidePanelCoordinator::From(browser());

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->contents_web_view()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  RunTestSequence(
      OpenLensOverlayWithRegionSearch(kFirstTab, kOverlayId, off_center_point),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Close the panel after it is opened.
        coordinator->Close();
      }),
      WaitForContextualPanelAndLensToClose());
}

IN_PROC_BROWSER_TEST_F(
    TabScopedContextualTasksLensOverlayControllerInteractiveUiTest,
    LensSessionsCloseOnSidePanelClose_MultiTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);

  browser()->GetFeatures().side_panel_ui()->DisableAnimationsForTesting();
  contextual_tasks::ContextualTasksSidePanelCoordinator* coordinator =
      contextual_tasks::ContextualTasksSidePanelCoordinator::From(browser());
  contextual_tasks::ContextualTasksContextController*
      contextual_tasks_controller =
          contextual_tasks::ContextualTasksContextControllerFactory::
              GetForProfile(browser()->profile());

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->contents_web_view()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  RunTestSequence(
      OpenLensOverlayWithRegionSearch(kFirstTab, kOverlayId, off_center_point),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      OpenArbitraryNewTab(),
      EnsureNotPresent(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Associate the task from tab0 to this new tab.
        SessionID tab_id0 = sessions::SessionTabHelper::IdForTab(
            browser()->tab_strip_model()->GetWebContentsAt(0));
        auto task =
            contextual_tasks_controller->GetContextualTaskForTab(tab_id0);
        contextual_tasks_controller->AssociateTabWithTask(
            task->GetTaskId(),
            sessions::SessionTabHelper::IdForTab(
                browser()->tab_strip_model()->GetWebContentsAt(1)));

        // Show contextual tasks side panel.
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Close the panel after it is opened.
        coordinator->Close();
      }),
      WaitForHide(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Verify Lens Overlay did not close.
        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetWebContentsAt(0);
        auto* lens_controller =
            LensSearchController::FromTabWebContents(web_contents);
        EXPECT_FALSE(lens_controller->IsClosing() || lens_controller->IsOff());
      }));
}

}  // namespace
