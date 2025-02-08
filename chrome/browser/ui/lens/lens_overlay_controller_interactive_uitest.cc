// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class runs CUJ tests for lens overlay. These tests simulate input events
// and cannot be run in parallel.

#include <utility>

#include "base/strings/string_util.h"
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
#include "chrome/browser/ui/lens/test_lens_overlay_query_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "ui/base/clipboard/clipboard.h"

namespace {

// The fake server session id.
constexpr char kTestServerSessionId[] = "server_session_id";

// The fake search session id.
constexpr char kTestSearchSessionId[] = "search_session_id";

// The fake suggest signals.
constexpr char kTestSuggestSignals[] = "encoded_image_signals";

constexpr char kDocumentWithNamedElement[] = "/select.html";
constexpr char kDocumentWithImage[] = "/test_visual.html";
constexpr char kDocumentWithVideo[] = "/media/bigbuck-player.html";
constexpr char kPdfDocument[] = "/pdf/test.pdf";

lens::Text CreateTestText(const std::vector<std::string>& words) {
  lens::Text text;
  text.set_content_language("es");
  // Create a paragraph.
  lens::TextLayout::Paragraph* paragraph =
      text.mutable_text_layout()->add_paragraphs();
  // Create a line.
  lens::TextLayout::Line* line = paragraph->add_lines();

  for (size_t i = 0; i < words.size(); ++i) {
    lens::TextLayout::Word* word = line->add_words();
    word->set_plain_text(words[i]);
    word->set_text_separator(" ");
    word->mutable_geometry()->mutable_bounding_box()->set_center_x(0.1 * i);
    word->mutable_geometry()->mutable_bounding_box()->set_center_y(0.1);
    word->mutable_geometry()->mutable_bounding_box()->set_width(0.1);
    word->mutable_geometry()->mutable_bounding_box()->set_height(0.1);
    word->mutable_geometry()->mutable_bounding_box()->set_coordinate_type(
        lens::NORMALIZED);
  }
  return text;
}

// Stubs out network requests.
class LensOverlayControllerFake : public LensOverlayController {
 public:
  LensOverlayControllerFake(tabs::TabInterface* tab,
                            variations::VariationsClient* variations_client,
                            signin::IdentityManager* identity_manager,
                            PrefService* pref_service,
                            syncer::SyncService* sync_service,
                            ThemeService* theme_service,
                            Profile* profile)
      : LensOverlayController(tab,
                              variations_client,
                              identity_manager,
                              pref_service,
                              sync_service,
                              theme_service) {}

  std::unique_ptr<lens::LensOverlayQueryController> CreateLensQueryController(
      lens::LensOverlayFullImageResponseCallback full_image_callback,
      lens::LensOverlayUrlResponseCallback url_callback,
      lens::LensOverlaySuggestInputsCallback suggest_inputs_callback,
      lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      Profile* profile,
      lens::LensOverlayInvocationSource invocation_source,
      bool use_dark_mode,
      lens::LensOverlayGen204Controller* gen204_controller) override {
    auto fake_query_controller =
        std::make_unique<lens::TestLensOverlayQueryController>(
            full_image_callback, url_callback, suggest_inputs_callback,
            thumbnail_created_callback, variations_client, identity_manager,
            profile, invocation_source, use_dark_mode, gen204_controller);

    // Set up the fake responses for the query controller.
    lens::LensOverlayServerClusterInfoResponse cluster_info_response;
    cluster_info_response.set_server_session_id(kTestServerSessionId);
    cluster_info_response.set_search_session_id(kTestSearchSessionId);
    fake_query_controller->set_fake_cluster_info_response(
        cluster_info_response);

    lens::LensOverlayObjectsResponse objects_response;
    objects_response.mutable_text()->CopyFrom(
        CreateTestText({"This", "is", "test", "text."}));
    objects_response.mutable_cluster_info()->set_server_session_id(
        kTestServerSessionId);
    objects_response.mutable_cluster_info()->set_search_session_id(
        kTestSearchSessionId);
    fake_query_controller->set_fake_objects_response(objects_response);

    lens::LensOverlayInteractionResponse interaction_response;
    interaction_response.set_encoded_response(kTestSuggestSignals);
    fake_query_controller->set_fake_interaction_response(interaction_response);
    return fake_query_controller;
  }
};

class TabFeaturesFake : public tabs::TabFeatures {
 public:
  TabFeaturesFake() = default;

 protected:
  std::unique_ptr<LensOverlayController> CreateLensController(
      tabs::TabInterface* tab,
      Profile* profile) override {
    auto* theme_service = ThemeServiceFactory::GetForProfile(profile);
    // Set browser color scheme to light mode for consistency.
    theme_service->SetBrowserColorScheme(
        ThemeService::BrowserColorScheme::kLight);
    return std::make_unique<LensOverlayControllerFake>(
        tab, profile->GetVariationsClient(),
        IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs(),
        SyncServiceFactory::GetForProfile(profile), theme_service, profile);
  }
};

class LensOverlayControllerCUJTest : public InteractiveFeaturePromoTest {
 public:
  template <typename... Args>
  explicit LensOverlayControllerCUJTest(Args&&... args)
      : InteractiveFeaturePromoTest(
            UseDefaultTrackerAllowingPromos({std::forward<Args>(args)...})) {
    tabs::TabFeatures::ReplaceTabFeaturesForTesting(
        base::BindRepeating(&LensOverlayControllerCUJTest::CreateTabFeatures,
                            base::Unretained(this)));
  }
  ~LensOverlayControllerCUJTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{lens::features::kLensOverlay, {}},
                              {lens::features::kLensOverlayTranslateButton, {}},
                              {media::kContextMenuSearchForVideoFrame, {}},
                              {lens::features::kLensOverlayContextualSearchbox,
                               {{"use-pdfs-as-context", "true"},
                                {"use-inner-html-as-context", "true"}}}},
        /*disabled_features=*/{});
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveFeaturePromoTest::SetUp();
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

  std::unique_ptr<tabs::TabFeatures> CreateTabFeatures() {
    return std::make_unique<TabFeaturesFake>();
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

    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                        kFirstPaintState);
    return Steps(
        InstrumentTab(kActiveTab), NavigateWebContents(kActiveTab, url),
        EnsurePresent(kActiveTab, kPathToImg),
        // TODO(https://crbug.com/331859922): This functionality should be built
        // into test framework.
        PollState(kFirstPaintState,
                  [this]() {
                    return browser()
                        ->tab_strip_model()
                        ->GetActiveTab()
                        ->GetContents()
                        ->CompletedFirstVisuallyNonEmptyPaint();
                  }),
        WaitForState(kFirstPaintState, true),
        MoveMouseTo(kActiveTab, kPathToImg), ClickMouse(ui_controls::RIGHT),
        WaitForShow(RenderViewContextMenu::kSearchForImageItem),
        // Required to fully render the menu before selection.

        SelectMenuItem(RenderViewContextMenu::kSearchForImageItem,
                       InputType::kMouse));
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
        MoveMouseTo(kActiveTab, kPathToVideo), ClickMouse(ui_controls::RIGHT),
        WaitForShow(RenderViewContextMenu::kSearchForVideoFrameItem),
        // Required to fully render the menu before selection.
        SelectMenuItem(RenderViewContextMenu::kSearchForVideoFrameItem,
                       InputType::kMouse));
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

 private:
  base::test::ScopedFeatureList feature_list_;
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
      InAnyContext(Steps(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL)))),
      // Wait for the webview to finish loading to prevent re-entrancy.
      InSameContext(Steps(EnsurePresent(kOverlayId, kPathToCloseButton),
                          ExecuteJsAt(kOverlayId, kPathToCloseButton, kClickFn,
                                      ExecuteJsMode::kFireAndForget),
                          WaitForHide(kOverlayId))));
}

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User opens lens overlay.
//  (3) User presses the escape key to close lens overlay.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerCUJTest, EscapeKeyClose) {
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
      InAnyContext(Steps(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL)))),
      // Wait for the webview to finish loading to prevent re-entrancy.
      InSameContext(Steps(SendAccelerator(kOverlayId, escape_key),
                          WaitForHide(kOverlayId))));
}

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User opens lens overlay.
//  (3) User highlights some text.
//  (4) User presses CTRL+C on some text.
//  (5) Highlighted text gets copied.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerCUJTest, CopyKeyCommandCopies) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlaySidePanelWebViewId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kTextCopiedState);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);

  // In kDocumentWithNamedElement.
  const DeepQuery kPathToBody{
      "body",
  };

  // Path to text
  const DeepQuery kPathToWord{
      "lens-overlay-app",
      "lens-selection-overlay",
      "lens-text-layer",
      ".word",
  };

  const ui::Accelerator ctrl_c_accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN);

  RunTestSequence(
      OpenLensOverlay(),

      // The overlay controller is an independent floating widget associated
      // with a tab rather than a browser window, so by convention gets its own
      // element context.
      InAnyContext(Steps(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL)))),

      // Wait for the webview to finish loading to prevent re-entrancy. Then
      // click a word to highlight it. Flush tasks after click to prevent
      // flakiness.
      InSameContext(Steps(WaitForShow(LensOverlayController::kOverlayId),
                          WaitForScreenshotRendered(kOverlayId),
                          EnsurePresent(kOverlayId, kPathToWord),
                          MoveMouseTo(kOverlayId, kPathToWord),
                          ClickMouse(ui_controls::LEFT))),

      // Clicking the text should have opened the side panel with the results
      // frame.
      InAnyContext(Steps(InstrumentNonTabWebView(
                             kOverlaySidePanelWebViewId,
                             LensOverlayController::kOverlaySidePanelWebViewId),
                         WaitForWebContentsReady(kOverlaySidePanelWebViewId))),

      //   Press CTRL+C command and ensure the highlighted text is saved to
      //   clipboard. We send the command to the side panel web view because in
      //   actual usage, the side panel is the view with focus so it receives
      //   the event right after selecting text.
      InSameContext(
          Steps(WaitForShow(kOverlaySidePanelWebViewId),
                SendAccelerator(kOverlaySidePanelWebViewId, ctrl_c_accelerator),
                PollState(kTextCopiedState,
                          [&]() {
                            ui::Clipboard* clipboard =
                                ui::Clipboard::GetForCurrentThread();
                            std::u16string clipboard_text;
                            clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                                /* data_dst = */ nullptr,
                                                &clipboard_text);
                            return base::EqualsASCII(clipboard_text, "This");
                          }),
                WaitForState(kTextCopiedState, true))));
}

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User opens lens overlay.
//  (3) User makes a selection that opens the results side panel.
//  (4) User presses the escape key to close lens overlay.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerCUJTest,
                       EscapeKeyCloseWithResultsPanel) {
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
      InAnyContext(Steps(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL)))),
      // Wait for the webview to finish loading to prevent re-entrancy. Then do
      // a drag offset from the center. Flush tasks after drag to prevent
      // flakiness.
      InSameContext(Steps(
          WaitForShow(LensOverlayController::kOverlayId),
          WaitForScreenshotRendered(kOverlayId),
          EnsurePresent(kOverlayId, kPathToRegionSelection),
          MoveMouseTo(LensOverlayController::kOverlayId),
          DragMouseTo(LensOverlayController::kOverlayId, off_center_point))),

      // The drag should have opened the side panel with the results frame.
      InAnyContext(Steps(
          InstrumentNonTabWebView(
              kOverlaySidePanelWebViewId,
              LensOverlayController::kOverlaySidePanelWebViewId),
          WaitForWebContentsReady(kOverlaySidePanelWebViewId),
          EnsurePresent(kOverlaySidePanelWebViewId, kPathToResultsFrame))),
      // Press the escape key to and ensure the overlay closes.
      InSameContext(
          Steps(WaitForShow(kOverlaySidePanelWebViewId),
                SendAccelerator(kOverlaySidePanelWebViewId, escape_key),
                WaitForHide(kOverlayId))));
}

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User opens lens overlay.
//  (3) User drags to select a manual region on the overlay.
//  (4) Side panel opens with results.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerCUJTest, SelectManualRegion) {
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
      InAnyContext(Steps(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL)))),
      // Wait for the webview to finish loading to prevent re-entrancy. Then do
      // a drag offset from the center. Flush tasks after drag to prevent
      // flakiness.
      InSameContext(Steps(WaitForShow(LensOverlayController::kOverlayId),
                          WaitForScreenshotRendered(kOverlayId),
                          EnsurePresent(kOverlayId, kPathToRegionSelection),
                          MoveMouseTo(LensOverlayController::kOverlayId),
                          DragMouseTo(off_center_point))),

      // The drag should have opened the side panel with the results frame.
      InAnyContext(Steps(

          InstrumentNonTabWebView(
              kOverlaySidePanelWebViewId,
              LensOverlayController::kOverlaySidePanelWebViewId),

          EnsurePresent(kOverlaySidePanelWebViewId, kPathToResultsFrame))));
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
      InAnyContext(Steps(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL)))),

      // The side panel should open with the results frame.
      InAnyContext(Steps(

          InstrumentNonTabWebView(
              kOverlaySidePanelWebViewId,
              LensOverlayController::kOverlaySidePanelWebViewId),

          EnsurePresent(kOverlaySidePanelWebViewId, kPathToResultsFrame))));
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
      InAnyContext(Steps(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL)))),

      // The side panel should open with the results frame.
      InAnyContext(Steps(

          InstrumentNonTabWebView(
              kOverlaySidePanelWebViewId,
              LensOverlayController::kOverlaySidePanelWebViewId),

          EnsurePresent(kOverlaySidePanelWebViewId, kPathToResultsFrame))));
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
      InAnyContext(Steps(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL)))),

      // The CSB should be in the overlay with the text "Ask about this
      // document".
      InSameContext(
          Steps(WaitForShow(LensOverlayController::kOverlayId),
                WaitForScreenshotRendered(kOverlayId),
                CheckSearchboxHintText(kOverlayId, kPathToOverlaySearchboxInput,
                                       "Ask about this page"),
                CheckGhostLoaderText(kOverlayId, kPathToOverlayGhostLoaderText,
                                     "Generating suggestions for this page…"))),

      // The use makes a query in the searchbox and the side panel opens.
      InSameContext(Steps(
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
              ExecuteJsMode::kFireAndForget))),

      // Side panel should open.
      InAnyContext(Steps(InstrumentNonTabWebView(
                             kOverlaySidePanelWebViewId,
                             LensOverlayController::kOverlaySidePanelWebViewId),
                         WaitForWebContentsReady(kOverlaySidePanelWebViewId))),

      // The CSB in the side panel should say "Ask about this document"
      InSameContext(
          Steps(CheckSearchboxHintText(kOverlaySidePanelWebViewId,
                                       kPathToSidePanelSearchboxInput,
                                       "Ask about this page"),
                CheckGhostLoaderText(kOverlaySidePanelWebViewId,
                                     kPathToSidePanelGhostLoaderText,
                                     "Generating suggestions for this page…"))),

      // The user navigates to a webpage.
      InAnyContext(Steps(InstrumentTab(kActiveTab),
                         NavigateWebContents(kActiveTab, pdf_url))),

      // The CSB in the overlay should eventually say "Ask about this page"
      InAnyContext(Steps(
          WaitForStateChange(kOverlaySidePanelWebViewId, hint_text_updated),
          CheckSearchboxHintText(kOverlaySidePanelWebViewId,
                                 kPathToSidePanelSearchboxInput,
                                 "Ask about this document"),
          CheckGhostLoaderText(kOverlaySidePanelWebViewId,
                               kPathToSidePanelGhostLoaderText,
                               "Generating suggestions for this document…"))));
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
      InAnyContext(Steps(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL)))),
      // Wait for the webview to finish loading to prevent re-entrancy. Then do
      // a drag offset from the center. Flush tasks after drag to prevent
      // flakiness.
      InSameContext(Steps(WaitForShow(LensOverlayController::kOverlayId),
                          WaitForScreenshotRendered(kOverlayId),
                          EnsurePresent(kOverlayId, kPathToRegionSelection),
                          MoveMouseTo(LensOverlayController::kOverlayId),
                          DragMouseTo(off_center_point))),

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
IN_PROC_BROWSER_TEST_F(LensOverlayControllerTranslatePromoTest,
                       ShowsTranslatePromo) {
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
      InAnyContext(Steps(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL)))),

      // Wait for the webview to finish loading to prevent re-entrancy.
      InSameContext(Steps(WaitForShow(LensOverlayController::kOverlayId),
                          WaitForScreenshotRendered(kOverlayId),
                          EnsurePresent(kOverlayId, kPathToTranslateButton))),

      // Wait for the initial translate promo help bubble.
      WaitForPromo(feature_engagement::kIPHLensOverlayTranslateButtonFeature),

      // Click the translate button element.
      ClickElement(kOverlayId, kPathToTranslateButton),

      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

}  // namespace
