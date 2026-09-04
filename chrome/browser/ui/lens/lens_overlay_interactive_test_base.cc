// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_interactive_test_base.h"

#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/test_lens_search_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_accessor.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_tasks/public/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "media/base/media_switches.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/views/focus/focus_manager.h"

TestingAimEligibilityService::TestingAimEligibilityService(
    bool is_aim_eligible,
    bool is_cobrowse_eligible,
    PrefService& pref_service,
    TemplateURLService* template_url_service)
    : ChromeAimEligibilityService(pref_service,
                                  /*template_url_service=*/template_url_service,
                                  /*url_loader_factory=*/nullptr,
                                  /*identity_manager=*/nullptr,
                                  /*configuration=*/{}),
      is_aim_eligible_(is_aim_eligible),
      is_cobrowse_eligible_(is_cobrowse_eligible) {}

TestingAimEligibilityService::~TestingAimEligibilityService() = default;

variations::VariationsService*
TestingAimEligibilityService::GetVariationsService() const {
  return nullptr;
}

bool TestingAimEligibilityService::IsAimEligible() const {
  return is_aim_eligible_;
}

bool TestingAimEligibilityService::IsCobrowseServerEligible() const {
  return is_cobrowse_eligible_;
}

bool TestingAimEligibilityService::IsCobrowseEligible() const {
  return is_cobrowse_eligible_;
}

TestingContextualTasksUiService::TestingContextualTasksUiService(
    Profile* profile,
    contextual_tasks::ContextualTasksService* contextual_tasks_service,
    signin::IdentityManager* identity_manager,
    AimEligibilityService* aim_eligibility_service,
    std::unique_ptr<contextual_tasks::ContextualTasksCookieSynchronizer>
        cookie_synchronizer)
    : ContextualTasksUiService(
          profile,
          std::make_unique<
              contextual_tasks::MockContextualTasksUiServiceDelegate>(),
          contextual_tasks_service,
          identity_manager,
          aim_eligibility_service,
          /*eligibility_manager=*/nullptr,
          std::move(cookie_synchronizer)) {}

TestingContextualTasksUiService::~TestingContextualTasksUiService() = default;

bool TestingContextualTasksUiService::CookieJarContainsPrimaryAccount() {
  return cookie_jar_contains_primary_account_;
}

void TestingContextualTasksUiService::SetCookieJarContainsPrimaryAccount(
    bool contains) {
  cookie_jar_contains_primary_account_ = contains;
}

const char kDocumentWithNamedElement[] = "/select.html";
const char kDocumentWithImage[] = "/test_visual.html";
const char kDocumentWithVideo[] = "/media/bigbuck-player.html";
const char kPdfDocument[] = "/pdf/test.pdf";

LensOverlayInteractiveTestBase::~LensOverlayInteractiveTestBase() = default;

void LensOverlayInteractiveTestBase::SetUp() {
  SetUpFeatureList();
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  InteractiveFeaturePromoTest::SetUp();
}

void LensOverlayInteractiveTestBase::SetUpFeatureList() {
  feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{lens::features::kLensOverlay, {}},
                            {lens::features::kLensOverlayTranslateButton, {}},
                            {media::kContextMenuSearchForVideoFrame, {}},
                            {lens::features::kLensOverlayContextualSearchbox,
                             {{"use-pdfs-as-context", "true"},
                              {"auto-focus-searchbox", "false"}}}},
      /*disabled_features=*/{contextual_tasks::kContextualTasks,
                             contextual_tasks::kContextualTasksSidePanel,
                             features::kNonBlockingOsClipboardReads});
}

void LensOverlayInteractiveTestBase::WaitForTemplateURLServiceToLoad() {
  auto* const template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
}

void LensOverlayInteractiveTestBase::SetUpOnMainThread() {
  InteractiveFeaturePromoTest::SetUpOnMainThread();
  embedded_test_server()->StartAcceptingConnections();

  // Permits sharing the page screenshot by default.
  PrefService* prefs = browser()->GetProfile()->GetPrefs();
  prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, true);
  prefs->SetBoolean(lens::prefs::kLensSharingPageContentEnabled, true);
}

void LensOverlayInteractiveTestBase::TearDownOnMainThread() {
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  InteractiveFeaturePromoTest::TearDownOnMainThread();

  // Disallow sharing the page screenshot by default.
  PrefService* prefs = browser()->GetProfile()->GetPrefs();
  prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, false);
}

ui::test::InteractiveTestApi::MultiStep
LensOverlayInteractiveTestBase::OpenArbitraryNewTab() {
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

ui::test::InteractiveTestApi::MultiStep
LensOverlayInteractiveTestBase::OpenLensOverlay() {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);

  // In kDocumentWithNamedElement.
  const DeepQuery kPathToBody{
      "body",
  };

  return Steps(InstrumentTab(kActiveTab), NavigateWebContents(kActiveTab, url),
               EnsurePresent(kActiveTab, kPathToBody),
               WaitForWebContentsPainted(kActiveTab),

               // Open the three dot menu and select the Lens Overlay option.
               PressButton(kToolbarAppMenuButtonElementId),
               WaitForShow(AppMenuModel::kShowLensOverlay),
               SelectMenuItem(AppMenuModel::kShowLensOverlay));
}

ui::test::InteractiveTestApi::MultiStep
LensOverlayInteractiveTestBase::OpenLensOverlayFromImage() {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithImage);

  // In kDocumentWithImage.
  const DeepQuery kPathToImg{
      "img",
  };

  return Steps(InstrumentTab(kActiveTab), NavigateWebContents(kActiveTab, url),
               WaitForWebContentsPainted(kActiveTab),

               MoveMouseTo(kActiveTab, kPathToImg),
               ClickMouse(ui_controls::RIGHT),
               SelectMenuItem(RenderViewContextMenu::kSearchForImageItem,
                              InputType::kMouse));
}

ui::test::InteractiveTestApi::MultiStep
LensOverlayInteractiveTestBase::OpenLensOverlayFromVideo() {
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

  return Steps(InstrumentTab(kActiveTab), NavigateWebContents(kActiveTab, url),
               EnsurePresent(kActiveTab, kPathToVideo),
               ExecuteJsAt(kActiveTab, kPathToVideo, kPlayVideo),
               WaitForStateChange(kActiveTab, video_is_playing),
               MoveMouseTo(kActiveTab, kPathToVideo),
               ClickMouse(ui_controls::RIGHT),
               SelectMenuItem(RenderViewContextMenu::kSearchForVideoFrameItem,
                              InputType::kMouse));
}

ui::test::InteractiveTestApi::MultiStep
LensOverlayInteractiveTestBase::WaitForScreenshotRendered(
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

ui::test::InteractiveTestApi::MultiStep
LensOverlayInteractiveTestBase::FinishScreenshotUpload(int tab_id) {
  // Get composebox query controller from session handle and router to
  // update file upload status to success for testing.
  return Steps(Do([this, tab_id]() {
    content::WebContents* web_contents =
        browser()->GetTabStripModel()->GetWebContentsAt(tab_id);
    auto* controller = LensSearchController::FromTabWebContents(web_contents);
    auto* router = controller->query_router();
    auto file_token = router->overlay_tab_context_file_token();

    router->OnContextUploadStatusChangedForTesting(
        *file_token, lens::MimeType::kImage,
        contextual_search::ContextUploadStatus::kUploadSuccessful,
        std::nullopt);
  }));
}

bool LensOverlayInteractiveTestBase::TriggerLenOverlayHomeworkPageAction() {
  page_actions::PageActionTestAccessor page_action_accessor(
      browser(), kActionLensOverlayHomework);
  views::FocusManager* focus_manager =
      BrowserView::GetBrowserViewForBrowser(browser())->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  return base::test::RunUntil(
      [&]() { return page_action_accessor.GetVisible(); });
}

ui::test::InteractiveTestApi::MultiStep
LensOverlayInteractiveTestBase::OpenLensOverlayWithRegionSearch(
    ui::ElementIdentifier tab_id,
    ui::ElementIdentifier overlay_id,
    base::OnceCallback<gfx::Point()> target_point,
    int tab_id_int) {
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
          EnsurePresent(tab_id, kPathToBody), WaitForWebContentsPainted(tab_id),
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
                    // Disable animations in the WebUI to prevent flakiness on
                    // bots. The duration is set to 0s instead of 'none' to
                    // ensure that animationend/transitionend events still fire,
                    // as the WebUI logic relies on them to transition states.
                    ExecuteJsAt(overlay_id, {}, R"(
                      () => {
                        const style = document.createElement('style');
                        style.textContent = `
                          * {
                            animation-duration: 0s !important;
                            transition-duration: 0s !important;
                          }
                        `;
                        document.head.appendChild(style);
                      }
                    )"),
                    WaitForScreenshotRendered(overlay_id),
                    EnsurePresent(overlay_id, kPathToRegionSelection),
                    MoveMouseTo(LensOverlayController::kOverlayId),
                    DragMouseTo(std::move(target_point)),
                    FinishScreenshotUpload(tab_id_int)));
}
