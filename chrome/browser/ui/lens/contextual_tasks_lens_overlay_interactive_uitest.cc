// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_extension_bridge.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_interactive_test_base.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_test_user_variation.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_interface.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service_delegate.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_interactive_test_base.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/test_lens_search_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_metrics.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/views/controls/webview/webview.h"

namespace {

void SimulateLeftClickDrag(content::WebContents* web_contents,
                           const gfx::Point& from,
                           const gfx::Point& to) {
  content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
  content::SimulateMouseEvent(web_contents,
                              blink::WebInputEvent::Type::kMouseDown,
                              blink::WebMouseEvent::Button::kLeft, from);
  content::SimulateMouseEvent(web_contents,
                              blink::WebInputEvent::Type::kMouseMove,
                              blink::WebMouseEvent::Button::kLeft, to);
  content::SimulateMouseEvent(web_contents,
                              blink::WebInputEvent::Type::kMouseUp,
                              blink::WebMouseEvent::Button::kLeft, to);
}

class ContextualTasksLensOverlayControllerInteractiveUiTest
    : public contextual_tasks::ContextualTasksInteractiveTestBaseT<
          contextual_tasks::UserVariation> {
 public:
  ContextualTasksLensOverlayControllerInteractiveUiTest() = default;
  ~ContextualTasksLensOverlayControllerInteractiveUiTest() override = default;

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{contextual_tasks::kContextualTasks, {}},
         {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}},
         {lens::features::kLensOverlayTextSelectionContextMenuEntrypoint,
          {{"contextualize", "true"}}},
         {lens::features::kLensSidePanelUnification,
          {{"allow-signed-out", "true"}}}},
        /*disabled_features=*/
        {features::kNonBlockingOsClipboardReads,
         // These tests expect closing the side panel to close Lens sessions in
         // all tabs that share its task.
         contextual_tasks::kContextualTasksSidePanelTabState});
  }

  void SetUpOnMainThread() override {
    contextual_tasks::ContextualTasksInteractiveTestBase::SetUpOnMainThread();
    url_loader_interceptor_.reset();
    contextual_tasks::SetForcedEmbeddedPageHostOverride(std::nullopt);
    WaitForTemplateURLServiceToLoad();
  }

  InteractiveTestApi::MultiStep WaitForContextualPanelAndLensToClose(
      int tab_index = 0) {
    return Steps(
        WaitForHide(kContextualTasksSidePanelWebViewElementId),
        Do([this, tab_index]() {
          // Verify Lens Overlay is closed.
          content::WebContents* web_contents =
              browser()->GetTabStripModel()->GetWebContentsAt(tab_index);
          auto* lens_controller =
              LensSearchController::FromTabWebContents(web_contents);
          EXPECT_TRUE(lens_controller->IsClosing() || lens_controller->IsOff());
        }));
  }

  // Simulates a right-click directly in the `web_contents_id` renderer (at the
  // center of `where` if specified, or the center of the WebContents view
  // otherwise) without using OS-level mouse button input. On Windows,
  // `ui_controls::SendMouseEventsNotifyWhenDone` waits with no timeout or
  // occlusion fallback for `WM_RBUTTONUP`, which can hang the step until the
  // sequence times out.
  InteractiveTestApi::StepBuilder RightClickInWebContents(
      ui::ElementIdentifier web_contents_id,
      std::optional<DeepQuery> where = std::nullopt) {
    return WithElement(
        web_contents_id, [where = std::move(where)](ui::TrackedElement* el) {
          auto* const util = AsInstrumentedWebContents(el);
          content::WebContents* const web_contents = util->web_contents();
          content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
          content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents);
          if (where.has_value()) {
            const gfx::Point point =
                util->GetElementBoundsInScreen(*where).CenterPoint() -
                el->GetScreenBounds().OffsetFromOrigin();
            content::SimulateMouseClickAt(web_contents, /*modifiers=*/0,
                                          blink::WebMouseEvent::Button::kRight,
                                          point);
          } else {
            content::SimulateMouseClick(web_contents, /*modifiers=*/0,
                                        blink::WebMouseEvent::Button::kRight);
          }
        });
  }

  InteractiveTestApi::MultiStep PrepareSidePanelComposeboxForTesting(
      ui::ElementIdentifier web_contents_id) {
    return Steps(
        WaitForWebContentsReady(web_contents_id),
        WaitForJsResultAt(
            web_contents_id, DeepQuery{"contextual-tasks-app"},
            "el => {"
            "  const frame = el.shadowRoot?.querySelector('#threadFrame');"
            "  if (!frame || !el.setPlayZeroStateAnimationsForTesting || "
            "      !el.browserProxy_?.callbackRouter?.onZeroStateChange) {"
            "    return false;"
            "  }"
            "  if (!el.shadowRoot.querySelector('#disableAnimations')) {"
            "    const style = document.createElement('style');"
            "    style.id = 'disableAnimations';"
            "    style.textContent = '* { animation: none !important; "
            "transition: none !important; }';"
            "    el.shadowRoot.appendChild(style);"
            "  }"
            "  if (!el._zeroStateListenerAdded) {"
            "    el._zeroStateListenerAdded = true;"
            "    el.browserProxy_.callbackRouter.onZeroStateChange.addListener("
            "        (isZeroState) => {"
            "          if (isZeroState) {"
            "            el._zeroStateAck = true;"
            "          }"
            "        });"
            "  }"
            "  el.setPlayZeroStateAnimationsForTesting(() => {});"
            "  el.clearZeroStateAnimations_();"
            "  el.forceComposeboxFocus = () => {};"
            "  el.removeThreadFrameListenersForTesting();"
            "  return Boolean(frame.src);"
            "}",
            true),
        PollUntil(
            [this]() {
              auto* panel_controller =
                  contextual_tasks::ContextualTasksPanelController::From(
                      browser());
              if (!panel_controller ||
                  !panel_controller->GetActiveWebContents()) {
                return false;
              }
              auto* ui_interface = contextual_tasks::GetWebUiInterface(
                  panel_controller->GetActiveWebContents());
              if (!ui_interface ||
                  !ui_interface->GetInnerFrameUrl().SchemeIsHTTPOrHTTPS()) {
                return false;
              }
              ui_interface->OnZeroStateChange(true);
              return true;
            },
            "Inner frame navigation committed"),
        WaitForJsResultAt(web_contents_id, DeepQuery{"contextual-tasks-app"},
                          "el => {"
                          "  if (!el._zeroStateAck) {"
                          "    return false;"
                          "  }"
                          "  el.clearZeroStateAnimations_();"
                          "  el.isLoadError_ = false;"
                          "  el.isZeroState_ = true;"
                          "  return true;"
                          "}",
                          true));
  }
};

IN_PROC_BROWSER_TEST_P(ContextualTasksLensOverlayControllerInteractiveUiTest,
                       LensSessionClosesOnSidePanelClose) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);

  SidePanelUI::From(browser())->DisableAnimationsForTesting();
  contextual_tasks::ContextualTasksPanelController* controller =
      contextual_tasks::ContextualTasksPanelController::From(browser());

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->GetContentsView()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  RunTestSequence(
      OpenLensOverlayWithRegionSearch(kFirstTab, kOverlayId, off_center_point),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Close the panel after it is opened.
        controller->Close();
      }),
      WaitForContextualPanelAndLensToClose());
}

IN_PROC_BROWSER_TEST_P(ContextualTasksLensOverlayControllerInteractiveUiTest,
                       LensSessionsCloseOnSidePanelClose_MultiTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);

  SidePanelUI::From(browser())->DisableAnimationsForTesting();
  contextual_tasks::ContextualTasksPanelController* controller =
      contextual_tasks::ContextualTasksPanelController::From(browser());
  contextual_tasks::ContextualTasksService* contextual_tasks_service =
      contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
          browser()->GetProfile());

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->GetContentsView()->bounds().CenterPoint();
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
            browser()->GetTabStripModel()->GetWebContentsAt(0));
        auto task = contextual_tasks_service->GetContextualTaskForTab(tab_id0);
        contextual_tasks_service->AssociateTabWithTask(
            task->GetTaskId(),
            sessions::SessionTabHelper::IdForTab(
                browser()->GetTabStripModel()->GetWebContentsAt(1)));

        // Show contextual tasks side panel.
        controller->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Close the panel after it is opened.
        controller->Close();
      }),
      WaitForContextualPanelAndLensToClose());
}

IN_PROC_BROWSER_TEST_P(ContextualTasksLensOverlayControllerInteractiveUiTest,
                       LensSessionsCloseOnSidePanelClose_MultipleLensSessions) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);

  SidePanelUI::From(browser())->DisableAnimationsForTesting();
  contextual_tasks::ContextualTasksPanelController* controller =
      contextual_tasks::ContextualTasksPanelController::From(browser());

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->GetContentsView()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  RunTestSequence(
      OpenLensOverlayWithRegionSearch(kFirstTab, kOverlayId, off_center_point,
                                      0),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      OpenArbitraryNewTab(),
      EnsureNotPresent(kContextualTasksSidePanelWebViewElementId),
      OpenLensOverlayWithRegionSearch(kSecondTab, kSecondOverlayId,
                                      off_center_point, 1),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Close the panel after it is opened.
        controller->Close();
      }),
      WaitForHide(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Verify Lens Overlay is not closing on the first tab.
        content::WebContents* web_contents =
            browser()->GetTabStripModel()->GetWebContentsAt(0);
        auto* lens_controller =
            LensSearchController::FromTabWebContents(web_contents);
        EXPECT_FALSE(lens_controller->IsClosing() || lens_controller->IsOff());

        // Verify Lens Overlay is closed on the second tab.
        content::WebContents* web_contents1 =
            browser()->GetTabStripModel()->GetWebContentsAt(1);
        auto* lens_controller1 =
            LensSearchController::FromTabWebContents(web_contents1);
        EXPECT_TRUE(lens_controller1->IsClosing() || lens_controller1->IsOff());
      }));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksLensOverlayControllerInteractiveUiTest,
                       TitleResetsWhenTransitioningFromAimToLensPage) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInnerWebContentsId);

  const DeepQuery kPathToToolbarTitle{"contextual-tasks-app", "top-toolbar",
                                      ".top-toolbar-title"};

  SidePanelUI::From(browser())->DisableAnimationsForTesting();
  contextual_tasks::ContextualTasksPanelController* controller =
      contextual_tasks::ContextualTasksPanelController::From(browser());

  contextual_tasks::SetForcedEmbeddedPageHostOverride(
      contextual_tasks::HostOverride{"www.google.com"});
  base::ScopedClosureRunner clear_host_override(base::BindOnce([]() {
    contextual_tasks::SetForcedEmbeddedPageHostOverride(std::nullopt);
  }));

  url_loader_interceptor_.reset();
  url_loader_interceptor_ =
      std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
          [](content::URLLoaderInterceptor::RequestParams* params) {
            if (params->url_request.url.host() == "www.google.com" ||
                params->url_request.url.host() == "www.g.ai") {
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-Type: text/html\n\n",
                  "<html><body>Mock Page</body></html>", params->client.get());
              return true;
            }
            return false;
          }));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  const GURL page_url =
      embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const GURL aim_url("https://www.g.ai/?q=summary");
  const DeepQuery kPathToBody{"body"};

  auto off_center_point = base::BindLambdaForTesting([this]() {
    auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    gfx::Point off_center =
        browser_view->GetContentsView()->GetBoundsInScreen().CenterPoint();
    off_center.Offset(50, 50);
    return off_center;
  });

  RunTestSequence(
      // 1. Prepare active tab and open Contextual Tasks with an AIM query URL.
      InstrumentTab(kActiveTab), NavigateWebContents(kActiveTab, page_url),
      EnsurePresent(kActiveTab, kPathToBody),
      WaitForWebContentsPainted(kActiveTab),
      WaitForWebContentsReady(kActiveTab, page_url), Do([&]() {
        contextual_tasks::ContextualTasksUiService* ui_service =
            contextual_tasks::ContextualTasksUiServiceFactory::
                GetForBrowserContext(browser()->GetProfile());
        tabs::TabInterface* tab = browser()->GetTabStripModel()->GetActiveTab();
        ui_service->StartTaskUiInSidePanel(browser(), tab, aim_url, nullptr);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](contextual_tasks::ContextualTasksWebView* web_view) {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      WaitForWebContentsReady(kSidePanelWebContentsId),
      EnsurePresent(kSidePanelWebContentsId,
                    DeepQuery{"contextual-tasks-app", "top-toolbar"}),
      WaitForJsResultAt(kSidePanelWebContentsId, kPathToToolbarTitle,
                        "el => el.textContent.trim()", "summary"),
      // Wait for the inner AIM page to finish loading and notify the WebUI of
      // the AI page status before opening the Lens overlay. This ensures
      // SetIsAiPage() executes while Lens is closed (State::kOff) rather than
      // triggering an asynchronous CloseLensAsync() teardown mid-overlay.
      InstrumentInnerWebContents(kInnerWebContentsId, kSidePanelWebContentsId,
                                 0),

      // 2. Open Lens Overlay via the three-dot app menu and make a visual
      // selection to trigger a Lens query.
      InAnyContext(PressButton(kToolbarAppMenuButtonElementId),
                   WaitForShow(AppMenuModel::kShowLensOverlay),
                   SelectMenuItem(AppMenuModel::kShowLensOverlay),
                   WaitForHide(AppMenuModel::kShowLensOverlay),
                   WaitForShow(LensOverlayController::kOverlayId)),
      Do([&]() {
        auto* web_contents =
            browser()->GetTabStripModel()->GetActiveWebContents();
        auto* lens_controller =
            LensSearchController::FromTabWebContents(web_contents);
        CHECK(lens_controller);
        auto* query_router = static_cast<lens::FakeLensQueryFlowRouter*>(
            lens_controller->query_router());
        CHECK(query_router);
        auto* session_handle =
            static_cast<contextual_search::MockContextualSearchSessionHandle*>(
                query_router->GetContextualSearchSessionHandle());
        CHECK(session_handle);
        ON_CALL(*session_handle, CreateSearchUrl(::testing::_, ::testing::_))
            .WillByDefault(::testing::WithArg<1>(
                [](base::OnceCallback<void(GURL)> callback) {
                  std::move(callback).Run(
                      GURL("https://www.google.com/search?q=lens_result"));
                }));
      }),
      SelectRegionInLensOverlay(kOverlayId, std::move(off_center_point),
                                /*tab_id_int=*/0),

      // 3. Verify that the Lens query resets the toolbar title.
      WaitForJsResultAt(kSidePanelWebContentsId, kPathToToolbarTitle,
                        "el => el.textContent.trim()", ""),
      Do([&]() {
        auto* web_ui_interface = contextual_tasks::GetWebUiInterface(
            controller->GetToolbarWebContents());
        ASSERT_TRUE(web_ui_interface);
        EXPECT_EQ(web_ui_interface->GetThreadTitle(), std::nullopt);
      }));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksLensOverlayControllerInteractiveUiTest,
                       ContextualTextQueryClosesOverlay) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);

  const DeepQuery kPathToOverlaySearchboxInput{
      "lens-overlay-app",
      "cr-lens-searchbox",
      "cr-searchbox-input",
      "input",
  };

  RunTestSequence(
      OpenLensOverlay(),
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),
      InSameContext(
          WaitForShow(LensOverlayController::kOverlayId),
          WaitForScreenshotRendered(kOverlayId),
          EnsurePresent(kOverlayId, kPathToOverlaySearchboxInput),
          ExecuteJsAt(kOverlayId, kPathToOverlaySearchboxInput,
                      "(el) => { el.focus(); }",
                      ExecuteJsMode::kWaitForCompletion),
          ExecuteJsAt(
              kOverlayId, kPathToOverlaySearchboxInput,
              "(el) => { el.value = 'test query'; el.dispatchEvent(new "
              "Event('input', { bubbles: true })); el.dispatchEvent(new "
              "Event('change', { bubbles: true }));}",
              ExecuteJsMode::kWaitForCompletion),
          ExecuteJsAt(
              kOverlayId, kPathToOverlaySearchboxInput,
              "(el) => { el.dispatchEvent(new KeyboardEvent('keydown', { "
              "key:'Enter', bubbles: true, cancelable: true, composed: true "
              "})); }",
              ExecuteJsMode::kFireAndForget)),
      // Screenshot is implicitly uploaded with CSB query.
      FinishScreenshotUpload(), WaitForHide(kOverlayId),
      WaitForShow(kContextualTasksSidePanelWebViewElementId));
}

// TODO(crbug.com/499004589): Re-enable this test when it's fixed.
IN_PROC_BROWSER_TEST_P(ContextualTasksLensOverlayControllerInteractiveUiTest,
                       ComposeboxLensButtonClearsThenTogglesOverlay) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kLensButtonExists);

  const DeepQuery kPathToLensButton{"contextual-tasks-app",
                                    "contextual-tasks-composebox",
                                    "#composebox", "#lensIcon"};

  SidePanelUI::From(browser())->DisableAnimationsForTesting();

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->GetContentsView()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  StateChange lens_button_exists;
  lens_button_exists.event = kLensButtonExists;
  lens_button_exists.where = kPathToLensButton;
  lens_button_exists.type = StateChange::Type::kExistsAndConditionTrue;
  lens_button_exists.test_function =
      "(el) => { const r = el.getBoundingClientRect(); return r.width > 0 && "
      "r.height > 0 && r.top >= 0 && r.bottom <= window.innerHeight; }";

  RunTestSequence(
      // 1. Open Lens Overlay and make a selection to open the side panel.
      OpenLensOverlayWithRegionSearch(kFirstTab, kOverlayId, off_center_point),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](contextual_tasks::ContextualTasksWebView* web_view) {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      PrepareSidePanelComposeboxForTesting(kSidePanelWebContentsId),

      // 2. Click the Lens button in the side panel to clear the overlay.
      WaitForStateChange(kSidePanelWebContentsId, lens_button_exists),
      ClickElement(kSidePanelWebContentsId, kPathToLensButton),

      // 3. Click the Lens button again to close the overlay.
      EnsurePresent(kOverlayId),
      ClickElement(kSidePanelWebContentsId, kPathToLensButton),
      WaitForHide(LensOverlayController::kOverlayId));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksLensOverlayControllerInteractiveUiTest,
                       ClearComposeboxClearsOverlayRegionSelection) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kLensButtonExists);

  const DeepQuery kPathToLensButton{"contextual-tasks-app",
                                    "contextual-tasks-composebox",
                                    "#composebox", "#lensIcon"};
  const DeepQuery kPathToClearButton{
      "contextual-tasks-app", "contextual-tasks-composebox", "#composebox",
      "#composeboxInput", "#cancelIcon"};
  const DeepQuery kPathToSelectionOverlay{"lens-overlay-app",
                                          "lens-selection-overlay"};
  constexpr char kHasPostSelection[] =
      "el => el.shadowRoot.querySelector('#postSelectionRenderer')"
      ".hasSelection()";
  const GURL url = embedded_test_server()->GetURL(kDocumentWithImage);

  SidePanelUI::From(browser())->DisableAnimationsForTesting();

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->GetContentsView()->GetBoundsInScreen().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  auto get_lens_controller = [this]() {
    return LensSearchController::FromTabWebContents(
        browser()->GetTabStripModel()->GetWebContentsAt(0));
  };
  auto has_region_selection = [get_lens_controller]() {
    return get_lens_controller()
        ->lens_overlay_controller()
        ->HasRegionSelection();
  };

  StateChange lens_button_exists;
  lens_button_exists.event = kLensButtonExists;
  lens_button_exists.where = kPathToLensButton;
  lens_button_exists.type = StateChange::Type::kExistsAndConditionTrue;
  lens_button_exists.test_function =
      "(el) => { const r = el.getBoundingClientRect(); return r.width > 0 && "
      "r.height > 0 && r.top >= 0 && r.bottom <= window.innerHeight; }";

  RunTestSequence(
      // 1. Open a page and show the Contextual Tasks side panel.
      InstrumentTab(kFirstTab), NavigateWebContents(kFirstTab, url),
      EnsurePresent(kFirstTab, DeepQuery{"body"}),
      WaitForWebContentsPainted(kFirstTab),
      WaitForWebContentsReady(kFirstTab, url), Do([this]() {
        // The composebox drops attached context whose input type is not
        // allowed by the searchbox config, so allow the Lens input types.
        // Browser tabs are implicitly allowed when both are present.
        omnibox::AimEligibilityResponse response;
        auto* config = response.mutable_searchbox_config();
        config->add_input_type_configs()->set_input_type(
            omnibox::INPUT_TYPE_LENS_IMAGE);
        config->add_input_type_configs()->set_input_type(
            omnibox::INPUT_TYPE_LENS_FILE);
        AimEligibilityServiceFactory::GetForProfile(browser()->GetProfile())
            ->SetEligibilityResponseForDebugging(
                base::Base64Encode(response.SerializeAsString()));

        contextual_tasks::ContextualTasksPanelController::From(browser())->Show(
            false,
            omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](contextual_tasks::ContextualTasksWebView* web_view) {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      PrepareSidePanelComposeboxForTesting(kSidePanelWebContentsId),

      // 2. Open the Lens overlay from the composebox Lens button and select a
      // region.
      WaitForStateChange(kSidePanelWebContentsId, lens_button_exists),
      ClickElement(kSidePanelWebContentsId, kPathToLensButton),
      SelectRegionInLensOverlay(kOverlayId, std::move(off_center_point)),
      CheckResult(
          [get_lens_controller]() {
            return get_lens_controller()->invocation_source();
          },
          std::make_optional(
              lens::LensOverlayInvocationSource::kContextualTasksComposebox)),

      // 3. Verify the region selection is shown on the overlay.
      InAnyContext(WaitForJsResultAt(kOverlayId, kPathToSelectionOverlay,
                                     kHasPostSelection, true)),
      CheckResult(has_region_selection, true),

      // 4. Clear the composebox with its clear button.
      WaitForJsResultAt(
          kSidePanelWebContentsId, kPathToClearButton,
          "el => !el.disabled && el.getBoundingClientRect().width > 0", true),
      ClickElement(kSidePanelWebContentsId, kPathToClearButton),

      // 5. Verify the region selection is cleared from the overlay.
      InAnyContext(WaitForJsResultAt(kOverlayId, kPathToSelectionOverlay,
                                     kHasPostSelection, false)),
      CheckResult(has_region_selection, false));
}

// This tests the following CUJ:
//  (1) User navigates to a webpage containing an image.
//  (2) User right-clicks the image to open the context menu.
//  (3) User selects the "Search image with Google Lens" item from the context
//      menu.
//  (4) Lens overlay opens with the image preselected as the search region.
//  (5) The image region query opens the Contextual Tasks side panel.
// Disabled on Mac because the Mac interaction test util implementation does
// not support selecting an item in the native context menu.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ImageContextMenuClick DISABLED_ImageContextMenuClick
#else
#define MAYBE_ImageContextMenuClick ImageContextMenuClick
#endif
IN_PROC_BROWSER_TEST_P(ContextualTasksLensOverlayControllerInteractiveUiTest,
                       MAYBE_ImageContextMenuClick) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

  SidePanelUI::From(browser())->DisableAnimationsForTesting();

  const GURL page_url = embedded_test_server()->GetURL(kDocumentWithImage);
  const DeepQuery kPathToImg{"img"};

  RunTestSequence(
      // Step 1: Navigate the active tab to a webpage containing an image and
      // wait for the image to finish loading so the context menu offers the
      // image search item.
      InstrumentTab(kTabId, 0), NavigateWebContents(kTabId, page_url),
      EnsurePresent(kTabId, kPathToImg), WaitForWebContentsPainted(kTabId),
      WaitForWebContentsReady(kTabId, page_url),
      WaitForJsResultAt(kTabId, kPathToImg,
                        "(el) => el.complete && el.naturalWidth > 0"),

      // Steps 2-3: Right-click the image and select the Lens image search item
      // from the context menu.
      RightClickInWebContents(kTabId, kPathToImg),
      WaitForShow(RenderViewContextMenu::kSearchForImageItem),
      SelectMenuItem(RenderViewContextMenu::kSearchForImageItem,
                     InputType::kMouse),

      // Step 4: The Lens overlay opens from the image context menu with the
      // image preselected as the search region.
      InAnyContext(WaitForShow(LensOverlayController::kOverlayId)),
      CheckResult(
          [this]() {
            return LensSearchController::FromTabWebContents(
                       browser()->GetTabStripModel()->GetWebContentsAt(0))
                ->invocation_source();
          },
          std::make_optional(
              lens::LensOverlayInvocationSource::kContentAreaContextMenuImage),
          "Lens overlay was opened from the image context menu"),

      // Step 5: The preselected image region query opens the Contextual Tasks
      // side panel.
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      CheckResult(
          [this]() {
            return SidePanelUI::From(browser())->IsSidePanelShowing();
          },
          true));
}

// This tests the following CUJ:
//  (1) User navigates to a webpage.
//  (2) User right-clicks a blank area of the webpage.
//  (3) User selects the Google Lens page search item from the context menu.
//  (4) Lens overlay opens over a screenshot of the page.
//  (5) User drags to select a region of the page.
//  (6) Contextual Tasks side panel opens and the WebUI app initializes with
//      the composebox.
// Disabled on Mac because the Mac interaction test util implementation does
// not support selecting an item in the native context menu.
#if BUILDFLAG(IS_MAC)
#define MAYBE_WebpageContextMenuClick DISABLED_WebpageContextMenuClick
#else
#define MAYBE_WebpageContextMenuClick WebpageContextMenuClick
#endif
IN_PROC_BROWSER_TEST_P(ContextualTasksLensOverlayControllerInteractiveUiTest,
                       MAYBE_WebpageContextMenuClick) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

  const GURL page_url =
      embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const DeepQuery kPathToBody{"body"};

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->GetContentsView()->GetBoundsInScreen().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  RunTestSequence(
      // Step 1: Navigate the active tab to a webpage.
      InstrumentTab(kTabId, 0), NavigateWebContents(kTabId, page_url),
      EnsurePresent(kTabId, kPathToBody), WaitForWebContentsPainted(kTabId),
      WaitForWebContentsReady(kTabId, page_url),

      // Steps 2-3: Right-click a blank area in the middle of the page (the
      // test page only has content at the top-left) and select the Lens page
      // search item from the context menu.
      RightClickInWebContents(kTabId),
      WaitForShow(RenderViewContextMenu::kRegionSearchItem),
      SelectMenuItem(RenderViewContextMenu::kRegionSearchItem,
                     InputType::kMouse),

      // Step 4: The Lens overlay opens from the page context menu.
      InAnyContext(WaitForShow(LensOverlayController::kOverlayId)),
      CheckResult(
          [this]() {
            return LensSearchController::FromTabWebContents(
                       browser()->GetTabStripModel()->GetWebContentsAt(0))
                ->invocation_source();
          },
          std::make_optional(
              lens::LensOverlayInvocationSource::kContentAreaContextMenuPage),
          "Lens overlay was opened from the page context menu"),

      // Step 5: Once the overlay screenshot renders, drag to select a region
      // of the page.
      SelectRegionInLensOverlay(kOverlayId, std::move(off_center_point),
                                /*tab_id_int=*/0),

      // Step 6: The region query opens the Contextual Tasks side panel.
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      CheckResult(
          [this]() {
            return SidePanelUI::From(browser())->IsSidePanelShowing();
          },
          true));
}

// This tests the following CUJ:
//  (1) User navigates to a webpage and selects text on the page.
//  (2) User right-clicks the selected text to open the context menu.
//  (3) User selects the "Search Google for <text>" item from the context menu.
//  (4) A contextual text search request is issued from the text selection
//      context menu and the Contextual Tasks side panel opens.
// Disabled on Mac because the Mac interaction test util implementation does
// not support selecting an item in the native context menu.
#if BUILDFLAG(IS_MAC)
#define MAYBE_TextSelectionContextMenuClick \
  DISABLED_TextSelectionContextMenuClick
#else
#define MAYBE_TextSelectionContextMenuClick TextSelectionContextMenuClick
#endif
IN_PROC_BROWSER_TEST_P(ContextualTasksLensOverlayControllerInteractiveUiTest,
                       MAYBE_TextSelectionContextMenuClick) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

  SidePanelUI::From(browser())->DisableAnimationsForTesting();
  // Disable asynchronous link-to-text selector generation so
  // LinkToTextMenuObserver does not rebuild all MenuItemViews mid-step while
  // the context menu is open.
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kScrollToTextFragmentEnabled, false);

  const GURL page_url =
      embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const DeepQuery kPathToParagraph{"p"};

  RunTestSequence(
      // Step 1: Navigate the active tab to a webpage and select text.
      InstrumentTab(kTabId, 0), NavigateWebContents(kTabId, page_url),
      EnsurePresent(kTabId, kPathToParagraph),
      WaitForWebContentsPainted(kTabId),
      WaitForWebContentsReady(kTabId, page_url),
      CheckJsResultAt(kTabId, kPathToParagraph,
                      "(el) => {"
                      "  el.style.display = 'inline';"
                      "  const range = document.createRange();"
                      "  range.selectNodeContents(el);"
                      "  const selection = window.getSelection();"
                      "  selection.removeAllRanges();"
                      "  selection.addRange(range);"
                      "  return selection.toString().trim().length > 0;"
                      "}",
                      true),

      // Steps 2-3: Right-click the selected text and select the search item
      // from the context menu.
      RightClickInWebContents(kTabId, kPathToParagraph),
      WaitForShow(RenderViewContextMenu::kSearchForTextItem),
      SelectMenuItem(RenderViewContextMenu::kSearchForTextItem,
                     InputType::kMouse),

      // Step 4: The text selection search opens the Contextual Tasks side
      // panel via the Lens text selection context menu entrypoint.
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      CheckResult(
          [this]() {
            return LensSearchController::FromTabWebContents(
                       browser()->GetTabStripModel()->GetWebContentsAt(0))
                ->invocation_source();
          },
          std::make_optional(
              lens::LensOverlayInvocationSource::kContentAreaContextMenuText),
          "Text query was issued from the text selection context menu"),
      CheckResult(
          [this]() {
            return SidePanelUI::From(browser())->IsSidePanelShowing();
          },
          true));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksLensOverlayControllerInteractiveUiTest,
    testing::Values(contextual_tasks::UserVariation::kSignedIn,
                    contextual_tasks::UserVariation::kSignedOut,
                    contextual_tasks::UserVariation::kIncognito),
    &contextual_tasks::UserVariationToString);

class ContextualTasksLensOverlayEphemeralButtonInteractiveUiTest
    : public ContextualTasksLensOverlayControllerInteractiveUiTest {
 public:
  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{contextual_tasks::kContextualTasks, {}},
         {contextual_tasks::kContextualTasksEphemeralBrandedEntryPoint,
          {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}},
         {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}},
         {lens::features::kLensSidePanelUnification,
          {{"allow-signed-out", "true"}}}},
        /*disabled_features=*/{features::kNonBlockingOsClipboardReads});
  }
};

IN_PROC_BROWSER_TEST_P(
    ContextualTasksLensOverlayEphemeralButtonInteractiveUiTest,
    ButtonShowsAfterClosingSidePanelWithLensQuery) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);

  SidePanelUI::From(browser())->DisableAnimationsForTesting();
  contextual_tasks::ContextualTasksPanelController* controller =
      contextual_tasks::ContextualTasksPanelController::From(browser());

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->GetContentsView()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  RunTestSequence(
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      OpenLensOverlayWithRegionSearch(kFirstTab, kOverlayId, off_center_point),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      Do([&]() {
        // Close the panel after it is opened.
        controller->Close();
      }),
      WaitForContextualPanelAndLensToClose(),
      GetParam() == contextual_tasks::UserVariation::kIncognito
          ? Steps(EnsureNotPresent(
                kContextualTasksEphemeralToolbarButtonElementId))
          : Steps(
                WaitForShow(kContextualTasksEphemeralToolbarButtonElementId)));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksLensOverlayEphemeralButtonInteractiveUiTest,
    testing::Values(contextual_tasks::UserVariation::kSignedIn,
                    contextual_tasks::UserVariation::kSignedOut,
                    contextual_tasks::UserVariation::kIncognito),
    &contextual_tasks::UserVariationToString);
enum class AimEligibilityTestState {
  kEligible,
  kAimIneligible,
  kCobrowseIneligible,
};

class ContextualTasksLensOverlayControllerEligibilityInteractiveUiTest
    : public contextual_tasks::ContextualTasksInteractiveTestBaseT<
          std::tuple<contextual_tasks::UserVariation,
                     AimEligibilityTestState>> {
 public:
  ContextualTasksLensOverlayControllerEligibilityInteractiveUiTest() = default;
  ~ContextualTasksLensOverlayControllerEligibilityInteractiveUiTest() override =
      default;

  AimEligibilityTestState GetAimEligibilityState() const {
    return std::get<1>(GetParam());
  }

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{contextual_tasks::kContextualTasks, {}},
                              {contextual_tasks::
                                   kContextualTasksForceEntryPointEligibility,
                               {}},
                              {lens::features::kLensSidePanelUnification,
                               {{"allow-signed-out", "true"}}}},
        /*disabled_features=*/{features::kNonBlockingOsClipboardReads});
  }

  std::unique_ptr<KeyedService> BuildMockAimServiceInstance(
      content::BrowserContext* context) override {
    Profile* profile = Profile::FromBrowserContext(context);
    AimEligibilityTestState state = GetAimEligibilityState();
    bool is_aim = state == AimEligibilityTestState::kEligible ||
                  state == AimEligibilityTestState::kCobrowseIneligible;
    bool is_cobrowse = state == AimEligibilityTestState::kEligible;
    return std::make_unique<TestingAimEligibilityService>(
        is_aim, is_cobrowse, *profile->GetPrefs(),
        /*template_url_service=*/nullptr);
  }

  void SetUpOnMainThread() override {
    contextual_tasks::ContextualTasksInteractiveTestBase::SetUpOnMainThread();
    WaitForTemplateURLServiceToLoad();
  }
};

IN_PROC_BROWSER_TEST_P(
    ContextualTasksLensOverlayControllerEligibilityInteractiveUiTest,
    RecordQueryEligibilityOnQuery) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);

  SidePanelUI::From(browser())->DisableAnimationsForTesting();

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->GetContentsView()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  base::HistogramTester histogram_tester;

  RunTestSequence(
      OpenLensOverlayWithRegionSearch(kFirstTab, kOverlayId, off_center_point),
      WaitForShow(kContextualTasksSidePanelWebViewElementId));

  // Verify metrics.
  AimEligibilityTestState state = GetAimEligibilityState();
  lens::LensContextualTasksQueryEligibility expected_eligibility;
  switch (state) {
    case AimEligibilityTestState::kEligible:
      expected_eligibility =
          lens::LensContextualTasksQueryEligibility::kEligible;
      break;
    case AimEligibilityTestState::kAimIneligible:
      expected_eligibility =
          lens::LensContextualTasksQueryEligibility::kAimIneligible;
      break;
    case AimEligibilityTestState::kCobrowseIneligible:
      expected_eligibility =
          lens::LensContextualTasksQueryEligibility::kCobrowseIneligible;
      break;
  }

  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualTasks.QueryEligibility", expected_eligibility, 1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualTasks.QueryEligibility.ByInvocationSource."
      "AppMenu",
      expected_eligibility, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksLensOverlayControllerEligibilityInteractiveUiTest,
    testing::Combine(
        testing::Values(contextual_tasks::UserVariation::kSignedIn,
                        contextual_tasks::UserVariation::kSignedOut,
                        contextual_tasks::UserVariation::kIncognito),
        testing::Values(AimEligibilityTestState::kEligible,
                        AimEligibilityTestState::kAimIneligible,
                        AimEligibilityTestState::kCobrowseIneligible)),
    [](const testing::TestParamInfo<
        std::tuple<contextual_tasks::UserVariation, AimEligibilityTestState>>&
           info) {
      std::string state_str;
      switch (std::get<1>(info.param)) {
        case AimEligibilityTestState::kEligible:
          state_str = "Eligible";
          break;
        case AimEligibilityTestState::kAimIneligible:
          state_str = "AimIneligible";
          break;
        case AimEligibilityTestState::kCobrowseIneligible:
          state_str = "CobrowseIneligible";
          break;
      }
      return base::StrCat({
          contextual_tasks::UserVariationToString(
              testing::TestParamInfo<contextual_tasks::UserVariation>(
                  std::get<0>(info.param), info.index)),
          "_",
          state_str,
      });
    });

class OmniboxContextualSuggestionInteractiveUiTest
    : public ContextualTasksLensOverlayControllerInteractiveUiTest {
 public:
  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{contextual_tasks::kContextualTasks, {}},
                              {contextual_tasks::kContextualTasksContext, {}},
                              {contextual_tasks::
                                   kContextualTasksForceEntryPointEligibility,
                               {}},
                              {lens::features::kLensOverlay, {}},
                              {lens::features::kLensOverlayContextualSearchbox,
                               {}},
                              {omnibox::kWebUIOmniboxAskGAboutThisPage,
                               {{"Omnibox_AskGBypassPrivacyNotice", "true"}}}},
        /*disabled_features=*/{features::kNonBlockingOsClipboardReads,
                               lens::features::kLensSidePanelUnification});
  }
};

IN_PROC_BROWSER_TEST_P(
    OmniboxContextualSuggestionInteractiveUiTest,
    IssueContextualSearchRequestBypassesPrivacyNoticeAndGrantsPermission) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);

  SidePanelUI::From(browser())->DisableAnimationsForTesting();

  RunTestSequence(
      InstrumentTab(kActiveTab), NavigateWebContents(kActiveTab, url),
      Do([this]() {
        // Ensure user has not granted Lens permissions in prefs.
        browser()->GetProfile()->GetPrefs()->SetBoolean(
            lens::prefs::kLensSharingPageScreenshotEnabled, false);
        browser()->GetProfile()->GetPrefs()->SetBoolean(
            lens::prefs::kLensSharingPageContentEnabled, false);

        auto* controller = LensSearchController::FromTabWebContents(
            browser()->GetTabStripModel()->GetActiveWebContents());
        ASSERT_TRUE(controller);
        ASSERT_TRUE(controller->IsOff());

        // Issue a contextual search request via
        // ChromeAutocompleteProviderClient.
        ChromeAutocompleteProviderClient client(
            browser()->GetProfile(),
            base::BindRepeating(
                &TabStripModel::GetActiveWebContents,
                base::Unretained(browser()->tab_strip_model())));
        client.IssueContextualSearchRequest(
            GURL("https://www.google.com/search?q=Help+me+with+this+page"),
            omnibox::AutocompleteMatchType::kSearchSuggest,
            /*is_zero_prefix_suggestion=*/true);
      }),
      // Verify the Contextual Tasks side panel web view shows.
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      CheckResult(
          [this]() {
            auto* controller = LensSearchController::FromTabWebContents(
                browser()->GetTabStripModel()->GetActiveWebContents());
            return controller && controller->lens_overlay_query_controller() &&
                   controller->lens_overlay_query_controller()
                       ->HasPermissionForSession();
          },
          true));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OmniboxContextualSuggestionInteractiveUiTest,
    testing::Values(contextual_tasks::UserVariation::kSignedIn,
                    contextual_tasks::UserVariation::kSignedOut,
                    contextual_tasks::UserVariation::kIncognito),
    &contextual_tasks::UserVariationToString);

class ContextualTasksContextMenuAskGoogleInteractiveUiTest
    : public ContextualTasksLensOverlayControllerInteractiveUiTest {
 public:
  ContextualTasksContextMenuAskGoogleInteractiveUiTest() = default;
  ~ContextualTasksContextMenuAskGoogleInteractiveUiTest() override = default;

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{contextual_tasks::kContextualTasks, {}},
         {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}},
         {contextual_tasks::kEnableContextualTasksPinButtonInToolbar, {}},
         {lens::features::kLensOverlay, {}},
         {lens::features::kLensOverlayEduActionChip,
          {{"max-shown-count", "5"}}},
         {lens::features::kLensSidePanelUnification,
          {{"allow-signed-out", "true"}}},
         {contextual_tasks::kContextualTasksUpdatedEntryPoints,
          {{"ContextualTasksContextMenuShowAskGoogle", "true"},
           {"ContextualTasksContextMenuSubmenu", "false"},
           {"ContextualTasksContextMenuRouteAskGoogleToOmnibox", "false"}}}},
        /*disabled_features=*/{features::kNonBlockingOsClipboardReads});
  }

  void SetupOptimizationFilter() {
    auto* optimization_guide_decider =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->GetProfile());
    optimization_guide_decider->AddHintWithMultipleOptimizationsForTesting(
        GURL(embedded_test_server()->GetURL(kDocumentWithNamedElement)),
        {optimization_guide::proto::LENS_OVERLAY_EDU_ACTION_CHIP_ALLOWLIST,
         optimization_guide::proto::LENS_OVERLAY_EDU_ACTION_CHIP_BLOCKLIST});
  }
};

// This tests the following CUJ:
//  (1) User navigates to a webpage.
//  (2) User right-clicks the page to open the context menu.
//  (3) User selects the "Ask Google about this page" item from the context
//      menu.
//  (4) The Contextual Tasks side panel opens in zero state with the
//      kContentAreaContextMenuPage invocation source.
// Disabled on Mac because the Mac interaction test util implementation does
// not support selecting an item in the native context menu.
#if BUILDFLAG(IS_MAC)
#define MAYBE_AskGoogleContextMenuClickOpensSidePanel \
  DISABLED_AskGoogleContextMenuClickOpensSidePanel
#else
#define MAYBE_AskGoogleContextMenuClickOpensSidePanel \
  AskGoogleContextMenuClickOpensSidePanel
#endif
IN_PROC_BROWSER_TEST_P(ContextualTasksContextMenuAskGoogleInteractiveUiTest,
                       MAYBE_AskGoogleContextMenuClickOpensSidePanel) {
  WaitForTemplateURLServiceToLoad();
  SidePanelUI::From(browser())->DisableAnimationsForTesting();

  contextual_tasks::SetForcedEmbeddedPageHostOverride(
      contextual_tasks::HostOverride{"www.google.com"});
  base::ScopedClosureRunner clear_host_override(base::BindOnce([]() {
    contextual_tasks::SetForcedEmbeddedPageHostOverride(std::nullopt);
  }));

  content::URLLoaderInterceptor url_loader_interceptor(base::BindRepeating(
      [](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "www.google.com" ||
            params->url_request.url.host() == "www.g.ai") {
          content::URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\nContent-Type: text/html\n\n",
              "<html><body>Mock Page</body></html>", params->client.get());
          return true;
        }
        return false;
      }));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const GURL page_url =
      embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const DeepQuery kPathToBody{"body"};

  RunTestSequence(
      InstrumentTab(kTabId, 0), NavigateWebContents(kTabId, page_url),
      EnsurePresent(kTabId, kPathToBody), WaitForWebContentsPainted(kTabId),
      WaitForWebContentsReady(kTabId, page_url),

      RightClickInWebContents(kTabId),
      WaitForShow(RenderViewContextMenu::kAskGoogleAboutThisPageItem),
      SelectMenuItem(RenderViewContextMenu::kAskGoogleAboutThisPageItem,
                     InputType::kMouse),

      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](contextual_tasks::ContextualTasksWebView* web_view) {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      WaitForWebContentsReady(kSidePanelWebContentsId),
      WaitForJsResultAt(kSidePanelWebContentsId,
                        DeepQuery{"contextual-tasks-app"},
                        "el => el.isZeroState_", true),
      CheckResult(
          [this]() -> bool {
            return SidePanelUI::From(browser())->IsSidePanelShowing();
          },
          true),
      CheckResult(
          [this]() -> std::optional<lens::LensOverlayInvocationSource> {
            auto* panel_controller =
                contextual_tasks::ContextualTasksPanelController::From(
                    browser());
            if (!panel_controller) {
              return std::nullopt;
            }
            auto* session_handle =
                panel_controller->GetContextualSearchSessionHandleForPanel();
            if (!session_handle) {
              return std::nullopt;
            }
            return session_handle->invocation_source();
          },
          std::make_optional(
              lens::LensOverlayInvocationSource::kContentAreaContextMenuPage),
          "Side panel session has kContentAreaContextMenuPage invocation "
          "source"));
}

// This tests the following CUJ:
//  (1) User navigates to a webpage.
//  (2) User clicks the 3-dot app menu button on the browser toolbar.
//  (3) User selects the "Ask Google about this page" item from the app menu.
//  (4) The Contextual Tasks side panel opens in zero state with the kAppMenu
//      invocation source.
IN_PROC_BROWSER_TEST_P(ContextualTasksContextMenuAskGoogleInteractiveUiTest,
                       AskGoogleAppMenuClickOpensSidePanel) {
  WaitForTemplateURLServiceToLoad();
  SidePanelUI::From(browser())->DisableAnimationsForTesting();

  contextual_tasks::SetForcedEmbeddedPageHostOverride(
      contextual_tasks::HostOverride{"www.google.com"});
  base::ScopedClosureRunner clear_host_override(base::BindOnce([]() {
    contextual_tasks::SetForcedEmbeddedPageHostOverride(std::nullopt);
  }));

  content::URLLoaderInterceptor url_loader_interceptor(base::BindRepeating(
      [](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "www.google.com" ||
            params->url_request.url.host() == "www.g.ai") {
          content::URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\nContent-Type: text/html\n\n",
              "<html><body>Mock Page</body></html>", params->client.get());
          return true;
        }
        return false;
      }));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const GURL page_url =
      embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const DeepQuery kPathToBody{"body"};

  RunTestSequence(
      InstrumentTab(kTabId, 0), NavigateWebContents(kTabId, page_url),
      EnsurePresent(kTabId, kPathToBody), WaitForWebContentsPainted(kTabId),
      WaitForWebContentsReady(kTabId, page_url),

      PressButton(kToolbarAppMenuButtonElementId),
      WaitForShow(AppMenuModel::kAskGoogleAboutThisPageItem),
      SelectMenuItem(AppMenuModel::kAskGoogleAboutThisPageItem),

      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](contextual_tasks::ContextualTasksWebView* web_view) {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      WaitForWebContentsReady(kSidePanelWebContentsId),
      WaitForJsResultAt(kSidePanelWebContentsId,
                        DeepQuery{"contextual-tasks-app"},
                        "el => el.isZeroState_", true),
      CheckResult(
          [this]() -> bool {
            return SidePanelUI::From(browser())->IsSidePanelShowing();
          },
          true),
      CheckResult(
          [this]() -> std::optional<lens::LensOverlayInvocationSource> {
            auto* panel_controller =
                contextual_tasks::ContextualTasksPanelController::From(
                    browser());
            if (!panel_controller) {
              return std::nullopt;
            }
            auto* session_handle =
                panel_controller->GetContextualSearchSessionHandleForPanel();
            if (!session_handle) {
              return std::nullopt;
            }
            return session_handle->invocation_source();
          },
          std::make_optional(lens::LensOverlayInvocationSource::kAppMenu),
          "Side panel session has kAppMenu invocation source"));
}

// This tests the following CUJ:
//  (1) User navigates to an eligible webpage where the Lens homework action
//      chip is shown.
//  (2) User clicks the homework action chip in the location bar.
//  (3) The Contextual Tasks side panel opens in zero state with the
//      kHomeworkActionChip invocation source.
IN_PROC_BROWSER_TEST_P(ContextualTasksContextMenuAskGoogleInteractiveUiTest,
                       HomeworkActionChipClickOpensSidePanel) {
  SkipIfIncognito("Lens homework action chip is disabled in Incognito mode.");
  SetupOptimizationFilter();
  WaitForTemplateURLServiceToLoad();
  SidePanelUI::From(browser())->DisableAnimationsForTesting();

  contextual_tasks::SetForcedEmbeddedPageHostOverride(
      contextual_tasks::HostOverride{"www.google.com"});
  base::ScopedClosureRunner clear_host_override(base::BindOnce([]() {
    contextual_tasks::SetForcedEmbeddedPageHostOverride(std::nullopt);
  }));

  content::URLLoaderInterceptor url_loader_interceptor(base::BindRepeating(
      [](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "www.google.com" ||
            params->url_request.url.host() == "www.g.ai") {
          content::URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\nContent-Type: text/html\n\n",
              "<html><body>Mock Page</body></html>", params->client.get());
          return true;
        }
        return false;
      }));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const GURL page_url =
      embedded_test_server()->GetURL(kDocumentWithNamedElement);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  ASSERT_TRUE(TriggerLenOverlayHomeworkPageAction());

  RunTestSequence(
      WaitForShow(kLensOverlayHomeworkPageActionIconElementId),
      PressButton(kLensOverlayHomeworkPageActionIconElementId),
      WaitForHide(kLensOverlayHomeworkPageActionIconElementId),

      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](contextual_tasks::ContextualTasksWebView* web_view) {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      WaitForWebContentsReady(kSidePanelWebContentsId),
      WaitForJsResultAt(kSidePanelWebContentsId,
                        DeepQuery{"contextual-tasks-app"},
                        "el => el.isZeroState_", true),
      CheckResult(
          [this]() -> bool {
            return SidePanelUI::From(browser())->IsSidePanelShowing();
          },
          true),
      CheckResult(
          [this]() -> std::optional<lens::LensOverlayInvocationSource> {
            auto* panel_controller =
                contextual_tasks::ContextualTasksPanelController::From(
                    browser());
            if (!panel_controller) {
              return std::nullopt;
            }
            auto* session_handle =
                panel_controller->GetContextualSearchSessionHandleForPanel();
            if (!session_handle) {
              return std::nullopt;
            }
            return session_handle->invocation_source();
          },
          std::make_optional(
              lens::LensOverlayInvocationSource::kHomeworkActionChip),
          "Side panel session has kHomeworkActionChip invocation source"));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksContextMenuAskGoogleInteractiveUiTest,
    testing::Values(contextual_tasks::UserVariation::kSignedIn,
                    contextual_tasks::UserVariation::kSignedOut,
                    contextual_tasks::UserVariation::kIncognito),
    &contextual_tasks::UserVariationToString);

class ContextualTasksContextMenuSubmenuInteractiveUiTest
    : public ContextualTasksLensOverlayControllerInteractiveUiTest {
 public:
  ContextualTasksContextMenuSubmenuInteractiveUiTest() = default;
  ~ContextualTasksContextMenuSubmenuInteractiveUiTest() override = default;

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{contextual_tasks::kContextualTasks, {}},
         {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}},
         {lens::features::kLensSidePanelUnification,
          {{"allow-signed-out", "true"}}},
         {contextual_tasks::kContextualTasksUpdatedEntryPoints,
          {{"ContextualTasksContextMenuShowAskGoogle", "true"},
           {"ContextualTasksContextMenuSubmenu", "true"},
           {"ContextualTasksContextMenuRouteAskGoogleToOmnibox", "false"}}}},
        /*disabled_features=*/{features::kNonBlockingOsClipboardReads});
  }
};

// This tests the following CUJ:
//  (1) User navigates to a webpage.
//  (2) User right-clicks the page to open the context menu.
//  (3) User opens the Contextual Tasks submenu and selects "Ask Google about
//      this page".
//  (4) The Contextual Tasks side panel opens in zero state with the
//      kContentAreaContextMenuPage invocation source.
// Disabled on Mac because the Mac interaction test util implementation does
// not support selecting an item in the native context menu.
#if BUILDFLAG(IS_MAC)
#define MAYBE_AskGoogleSubmenuItemClickOpensSidePanel \
  DISABLED_AskGoogleSubmenuItemClickOpensSidePanel
#else
#define MAYBE_AskGoogleSubmenuItemClickOpensSidePanel \
  AskGoogleSubmenuItemClickOpensSidePanel
#endif
IN_PROC_BROWSER_TEST_P(ContextualTasksContextMenuSubmenuInteractiveUiTest,
                       MAYBE_AskGoogleSubmenuItemClickOpensSidePanel) {
  WaitForTemplateURLServiceToLoad();
  SidePanelUI::From(browser())->DisableAnimationsForTesting();

  contextual_tasks::SetForcedEmbeddedPageHostOverride(
      contextual_tasks::HostOverride{"www.google.com"});
  base::ScopedClosureRunner clear_host_override(base::BindOnce([]() {
    contextual_tasks::SetForcedEmbeddedPageHostOverride(std::nullopt);
  }));

  content::URLLoaderInterceptor url_loader_interceptor(base::BindRepeating(
      [](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "www.google.com" ||
            params->url_request.url.host() == "www.g.ai") {
          content::URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\nContent-Type: text/html\n\n",
              "<html><body>Mock Page</body></html>", params->client.get());
          return true;
        }
        return false;
      }));
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const GURL page_url =
      embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const DeepQuery kPathToBody{"body"};

  RunTestSequence(
      InstrumentTab(kTabId, 0), NavigateWebContents(kTabId, page_url),
      EnsurePresent(kTabId, kPathToBody), WaitForWebContentsPainted(kTabId),
      WaitForWebContentsReady(kTabId, page_url),

      RightClickInWebContents(kTabId),
      WaitForShow(RenderViewContextMenu::kContextualTasksSubmenuItem),
      SelectMenuItem(RenderViewContextMenu::kContextualTasksSubmenuItem),
      WaitForShow(RenderViewContextMenu::kAskGoogleAboutThisPageItem),
      SelectMenuItem(RenderViewContextMenu::kAskGoogleAboutThisPageItem),

      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](contextual_tasks::ContextualTasksWebView* web_view) {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      WaitForWebContentsReady(kSidePanelWebContentsId),
      WaitForJsResultAt(kSidePanelWebContentsId,
                        DeepQuery{"contextual-tasks-app"},
                        "el => el.isZeroState_", true),
      CheckResult(
          [this]() -> bool {
            return SidePanelUI::From(browser())->IsSidePanelShowing();
          },
          true),
      CheckResult(
          [this]() -> std::optional<lens::LensOverlayInvocationSource> {
            auto* panel_controller =
                contextual_tasks::ContextualTasksPanelController::From(
                    browser());
            if (!panel_controller) {
              return std::nullopt;
            }
            auto* session_handle =
                panel_controller->GetContextualSearchSessionHandleForPanel();
            if (!session_handle) {
              return std::nullopt;
            }
            return session_handle->invocation_source();
          },
          std::make_optional(
              lens::LensOverlayInvocationSource::kContentAreaContextMenuPage),
          "Side panel session has kContentAreaContextMenuPage invocation "
          "source"));
}

// This tests the following CUJ:
//  (1) User navigates to a webpage.
//  (2) User clicks the 3-dot app menu button on the browser toolbar.
//  (3) User opens the Contextual Tasks submenu and selects "Ask Google about
//      this page".
//  (4) The Contextual Tasks side panel opens in zero state with the kAppMenu
//      invocation source.
IN_PROC_BROWSER_TEST_P(ContextualTasksContextMenuSubmenuInteractiveUiTest,
                       AskGoogleAppMenuSubmenuItemClickOpensSidePanel) {
  WaitForTemplateURLServiceToLoad();
  SidePanelUI::From(browser())->DisableAnimationsForTesting();

  contextual_tasks::SetForcedEmbeddedPageHostOverride(
      contextual_tasks::HostOverride{"www.google.com"});
  base::ScopedClosureRunner clear_host_override(base::BindOnce([]() {
    contextual_tasks::SetForcedEmbeddedPageHostOverride(std::nullopt);
  }));

  content::URLLoaderInterceptor url_loader_interceptor(base::BindRepeating(
      [](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "www.google.com" ||
            params->url_request.url.host() == "www.g.ai") {
          content::URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\nContent-Type: text/html\n\n",
              "<html><body>Mock Page</body></html>", params->client.get());
          return true;
        }
        return false;
      }));
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const GURL page_url =
      embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const DeepQuery kPathToBody{"body"};

  RunTestSequence(
      InstrumentTab(kTabId, 0), NavigateWebContents(kTabId, page_url),
      EnsurePresent(kTabId, kPathToBody), WaitForWebContentsPainted(kTabId),
      WaitForWebContentsReady(kTabId, page_url),

      PressButton(kToolbarAppMenuButtonElementId),
      WaitForShow(AppMenuModel::kContextualTasksSubmenuItem),
      SelectMenuItem(AppMenuModel::kContextualTasksSubmenuItem),
      WaitForShow(AppMenuModel::kAskGoogleAboutThisPageItem),
      SelectMenuItem(AppMenuModel::kAskGoogleAboutThisPageItem),

      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](contextual_tasks::ContextualTasksWebView* web_view) {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      WaitForWebContentsReady(kSidePanelWebContentsId),
      WaitForJsResultAt(kSidePanelWebContentsId,
                        DeepQuery{"contextual-tasks-app"},
                        "el => el.isZeroState_", true),
      CheckResult(
          [this]() -> bool {
            return SidePanelUI::From(browser())->IsSidePanelShowing();
          },
          true),
      CheckResult(
          [this]() -> std::optional<lens::LensOverlayInvocationSource> {
            auto* panel_controller =
                contextual_tasks::ContextualTasksPanelController::From(
                    browser());
            if (!panel_controller) {
              return std::nullopt;
            }
            auto* session_handle =
                panel_controller->GetContextualSearchSessionHandleForPanel();
            if (!session_handle) {
              return std::nullopt;
            }
            return session_handle->invocation_source();
          },
          std::make_optional(lens::LensOverlayInvocationSource::kAppMenu),
          "Side panel session has kAppMenu invocation source"));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksContextMenuSubmenuInteractiveUiTest,
    testing::Values(contextual_tasks::UserVariation::kSignedIn,
                    contextual_tasks::UserVariation::kSignedOut,
                    contextual_tasks::UserVariation::kIncognito),
    &contextual_tasks::UserVariationToString);

class ContextualTasksContextMenuRouteOmniboxInteractiveUiTest
    : public ContextualTasksLensOverlayControllerInteractiveUiTest {
 public:
  ContextualTasksContextMenuRouteOmniboxInteractiveUiTest() = default;
  ~ContextualTasksContextMenuRouteOmniboxInteractiveUiTest() override = default;

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{contextual_tasks::kContextualTasks, {}},
         {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}},
         {contextual_tasks::kEnableContextualTasksPinButtonInToolbar, {}},
         {lens::features::kLensOverlay, {}},
         {lens::features::kLensOverlayEduActionChip,
          {{"max-shown-count", "5"}}},
         {lens::features::kLensSidePanelUnification,
          {{"allow-signed-out", "true"}}},
         {contextual_tasks::kContextualTasksUpdatedEntryPoints,
          {{"ContextualTasksContextMenuShowAskGoogle", "true"},
           {"ContextualTasksContextMenuSubmenu", "false"},
           {"ContextualTasksContextMenuRouteAskGoogleToOmnibox", "true"}}},
         {omnibox::internal::kWebUIOmniboxAimPopup, {}}},
        /*disabled_features=*/{features::kNonBlockingOsClipboardReads});
  }

  void SetupOptimizationFilter() {
    auto* optimization_guide_decider =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->GetProfile());
    optimization_guide_decider->AddHintWithMultipleOptimizationsForTesting(
        GURL(embedded_test_server()->GetURL(kDocumentWithNamedElement)),
        {optimization_guide::proto::LENS_OVERLAY_EDU_ACTION_CHIP_ALLOWLIST,
         optimization_guide::proto::LENS_OVERLAY_EDU_ACTION_CHIP_BLOCKLIST});
  }
};

// This tests the following CUJ:
//  (1) User navigates to a webpage when Ask Google is configured to route to
//      the Omnibox.
//  (2) User right-clicks the page and selects "Ask Google about this page".
//  (3) The Omnibox popup opens in AIM mode (OmniboxPopupState::kAim).
// Disabled on Mac because the Mac interaction test util implementation does
// not support selecting an item in the native context menu.
#if BUILDFLAG(IS_MAC)
#define MAYBE_AskGoogleContextMenuClickOpensOmnibox \
  DISABLED_AskGoogleContextMenuClickOpensOmnibox
#else
#define MAYBE_AskGoogleContextMenuClickOpensOmnibox \
  AskGoogleContextMenuClickOpensOmnibox
#endif
IN_PROC_BROWSER_TEST_P(ContextualTasksContextMenuRouteOmniboxInteractiveUiTest,
                       MAYBE_AskGoogleContextMenuClickOpensOmnibox) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

  const GURL page_url =
      embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const DeepQuery kPathToBody{"body"};

  RunTestSequence(
      InstrumentTab(kTabId, 0), NavigateWebContents(kTabId, page_url),
      EnsurePresent(kTabId, kPathToBody), WaitForWebContentsPainted(kTabId),
      WaitForWebContentsReady(kTabId, page_url),

      RightClickInWebContents(kTabId),
      WaitForShow(RenderViewContextMenu::kAskGoogleAboutThisPageItem),
      SelectMenuItem(RenderViewContextMenu::kAskGoogleAboutThisPageItem,
                     InputType::kMouse),

      PollUntil(
          [this]() -> bool {
            LocationBar* location_bar =
                BrowserWindow::FromBrowser(browser())->GetLocationBar();
            if (!location_bar) {
              return false;
            }
            OmniboxController* controller =
                location_bar->GetOmniboxController();
            return controller &&
                   controller->popup_state_manager()->popup_state() ==
                       OmniboxPopupState::kAim;
          },
          "Omnibox popup state is kAim"));
}

// This tests the following CUJ:
//  (1) User navigates to a webpage when Ask Google is configured to route to
//      the Omnibox.
//  (2) User clicks the 3-dot app menu button and selects "Ask Google about this
//      page".
//  (3) The Omnibox popup opens in AIM mode (OmniboxPopupState::kAim).
IN_PROC_BROWSER_TEST_P(ContextualTasksContextMenuRouteOmniboxInteractiveUiTest,
                       AskGoogleAppMenuClickOpensOmnibox) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

  const GURL page_url =
      embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const DeepQuery kPathToBody{"body"};

  RunTestSequence(
      InstrumentTab(kTabId, 0), NavigateWebContents(kTabId, page_url),
      EnsurePresent(kTabId, kPathToBody), WaitForWebContentsPainted(kTabId),
      WaitForWebContentsReady(kTabId, page_url),

      PressButton(kToolbarAppMenuButtonElementId),
      WaitForShow(AppMenuModel::kAskGoogleAboutThisPageItem),
      SelectMenuItem(AppMenuModel::kAskGoogleAboutThisPageItem),

      PollUntil(
          [this]() -> bool {
            LocationBar* location_bar =
                BrowserWindow::FromBrowser(browser())->GetLocationBar();
            if (!location_bar) {
              return false;
            }
            OmniboxController* controller =
                location_bar->GetOmniboxController();
            return controller &&
                   controller->popup_state_manager()->popup_state() ==
                       OmniboxPopupState::kAim;
          },
          "Omnibox popup state is kAim"));
}

// This tests the following CUJ:
//  (1) User navigates to an eligible webpage where the Lens homework action
//      chip is shown and Ask Google is configured to route to the Omnibox.
//  (2) User clicks the homework action chip in the location bar.
//  (3) The Omnibox popup opens in AIM mode (OmniboxPopupState::kAim).
IN_PROC_BROWSER_TEST_P(ContextualTasksContextMenuRouteOmniboxInteractiveUiTest,
                       HomeworkActionChipClickOpensOmnibox) {
  SkipIfIncognito("Lens homework action chip is disabled in Incognito mode.");
  SetupOptimizationFilter();
  WaitForTemplateURLServiceToLoad();

  const GURL page_url =
      embedded_test_server()->GetURL(kDocumentWithNamedElement);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  ASSERT_TRUE(TriggerLenOverlayHomeworkPageAction());

  RunTestSequence(
      WaitForShow(kLensOverlayHomeworkPageActionIconElementId),
      PressButton(kLensOverlayHomeworkPageActionIconElementId),
      WaitForHide(kLensOverlayHomeworkPageActionIconElementId),

      PollUntil(
          [this]() -> bool {
            LocationBar* location_bar =
                BrowserWindow::FromBrowser(browser())->GetLocationBar();
            if (!location_bar) {
              return false;
            }
            OmniboxController* controller =
                location_bar->GetOmniboxController();
            return controller &&
                   controller->popup_state_manager()->popup_state() ==
                       OmniboxPopupState::kAim;
          },
          "Omnibox popup state is kAim"));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksContextMenuRouteOmniboxInteractiveUiTest,
    testing::Values(contextual_tasks::UserVariation::kSignedIn,
                    contextual_tasks::UserVariation::kSignedOut,
                    contextual_tasks::UserVariation::kIncognito),
    &contextual_tasks::UserVariationToString);

class RegionChipTestingContextualTasksUiService
    : public TestingContextualTasksUiService {
 public:
  using TestingContextualTasksUiService::TestingContextualTasksUiService;
  ~RegionChipTestingContextualTasksUiService() override = default;

  std::optional<GURL> GetInitialUrlForTask(const base::Uuid& uuid) override {
    if (std::optional<GURL> url =
            TestingContextualTasksUiService::GetInitialUrlForTask(uuid)) {
      return url;
    }
    return GetDefaultAiPageUrlForTask(uuid);
  }
};

class ContextualTasksLensRegionChipInteractiveUiTest
    : public ContextualTasksLensOverlayControllerInteractiveUiTest {
 public:
  ContextualTasksLensRegionChipInteractiveUiTest() {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  }

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{contextual_tasks::kContextualTasks, {}},
         {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}},
         {contextual_tasks::kEnableContextualTasksPinButtonInToolbar, {}},
         {contextual_tasks::kContextualTasksRearchitecture, {}},
         {contextual_tasks::kContextualTasksSidePanelRearchitecture, {}},
         {extensions_features::kApiContextualTasksPrivate, {}}},
        /*disabled_features=*/{features::kNonBlockingOsClipboardReads});
  }

  contextual_tasks::UserVariation GetUserVariation() const override {
    return contextual_tasks::UserVariation::kSignedIn;
  }

  std::unique_ptr<KeyedService> BuildMockContextualTasksUiServiceInstance(
      content::BrowserContext* context) override {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<RegionChipTestingContextualTasksUiService>(
        profile,
        contextual_tasks::ContextualTasksServiceFactory::GetForProfile(profile),
        IdentityManagerFactory::GetForProfile(profile),
        AimEligibilityServiceFactory::GetForProfile(profile),
        /*cookie_synchronizer=*/nullptr);
  }

  void SetUpOnMainThread() override {
    ContextualTasksLensOverlayControllerInteractiveUiTest::SetUpOnMainThread();
    contextual_tasks::ContextualTasksExtensionBridge::Get(
        browser()->GetProfile());
    PinnedToolbarActionsModel::Get(browser()->GetProfile())
        ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
  }

  InteractiveTestApi::MultiStep NavigateToTabWithImage(
      ui::ElementIdentifier tab_id,
      const GURL& url) {
    const DeepQuery kPathToBody{"body"};
    return Steps(InstrumentTab(tab_id), NavigateWebContents(tab_id, url),
                 EnsurePresent(tab_id, kPathToBody),
                 WaitForWebContentsPainted(tab_id),
                 WaitForWebContentsReady(tab_id, url));
  }

  InteractiveTestApi::MultiStep OpenContextualTasksSidePanelViaToolbarButton(
      ui::ElementIdentifier side_panel_contents_id) {
    return Steps(
        WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
        PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
        WaitForShow(kContextualTasksSidePanelWebViewElementId),
        NameViewRelative(
            kContextualTasksSidePanelWebViewElementId,
            "SidePanelContentWebViewName",
            [](contextual_tasks::ContextualTasksWebView* web_view) {
              return web_view->content_web_view();
            }),
        InstrumentNonTabWebView(side_panel_contents_id,
                                "SidePanelContentWebViewName"),
        WaitForWebContentsReady(side_panel_contents_id));
  }

  InteractiveTestApi::MultiStep WaitForAimHandshake(
      ui::ElementIdentifier side_panel_contents_id) {
    const DeepQuery kPathToBody{"body"};
    return Steps(WaitForJsResultAt(side_panel_contents_id, kPathToBody,
                                   "el => el.dataset.handshakeComplete",
                                   "true"));
  }

  InteractiveTestApi::StepBuilder ClickSidePanelLensButton() {
    return Do([this]() {
      auto* controller =
          contextual_tasks::ContextualTasksPanelController::From(browser());
      content::WebContents* side_panel_contents =
          controller->GetActiveWebContents();
      ASSERT_TRUE(side_panel_contents != nullptr);
      content::RenderFrameHost* button_rfh = nullptr;
      EXPECT_TRUE(base::test::RunUntil([&]() -> bool {
        button_rfh = content::FrameMatchingPredicateOrNullptr(
            side_panel_contents->GetPrimaryPage(),
            base::BindRepeating([](content::RenderFrameHost* rfh) {
              return rfh->GetLastCommittedURL().ExtractFileName() ==
                     "lens_button.html";
            }));
        return button_rfh != nullptr;
      }));
      ASSERT_TRUE(button_rfh != nullptr);

      auto result = content::EvalJs(button_rfh, R"(
        new Promise((resolve) => {
          const check = () => {
            const app = document.querySelector('lens-button-app');
            const btn = app?.shadowRoot?.querySelector('#lensButton');
            if (btn && !btn.disabled) {
              btn.click();
              resolve(true);
            } else {
              setTimeout(check, 50);
            }
          };
          check();
        });
      )");
      EXPECT_TRUE(result.is_ok() && result.ExtractBool());
    });
  }

  InteractiveTestApi::MultiStep WaitForLensOverlayAndSelectRegion(
      ui::ElementIdentifier overlay_id,
      base::OnceCallback<gfx::Point()> target_point) {
    const DeepQuery kPathToRegionSelection{
        "lens-overlay-app",
        "lens-selection-overlay",
        "#regionSelectionLayer",
    };
    return Steps(
        InAnyContext(
            InstrumentNonTabWebView(overlay_id,
                                    LensOverlayController::kOverlayId),
            WaitForWebContentsReady(
                overlay_id, GURL(chrome::kChromeUILensOverlayUntrustedURL))),
        InSameContext(
            WaitForShow(LensOverlayController::kOverlayId),
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
            WithElement(LensOverlayController::kOverlayId,
                        [this, target_point = std::move(target_point)](
                            ui::TrackedElement* el) mutable {
                          auto* const web_view = AsView<views::WebView>(el);
                          const gfx::Rect bounds =
                              web_view->GetBoundsInScreen();
                          SimulateLeftClickDrag(
                              web_view->GetWebContents(),
                              gfx::Rect(bounds.size()).CenterPoint(),
                              std::move(target_point).Run() -
                                  bounds.OffsetFromOrigin());
                        }),
            FinishScreenshotUpload(0)));
  }

  InteractiveTestApi::MultiStep WaitForLensChipMountedWithPreview(
      ui::ElementIdentifier side_panel_contents_id,
      content::RenderFrameHost** out_chip_rfh,
      std::string* out_crop_uri) {
    const DeepQuery kPathToBody{"body"};
    return Steps(
        WaitForJsResultAt(side_panel_contents_id, kPathToBody,
                          "el => el.dataset.chipMounted", "true"),
        WaitForJsResultAt(side_panel_contents_id, kPathToBody,
                          "el => el.dataset.mountCount", "1"),
        Do([this, out_chip_rfh, out_crop_uri]() {
          auto* controller =
              contextual_tasks::ContextualTasksPanelController::From(browser());
          content::WebContents* side_panel_contents =
              controller->GetActiveWebContents();
          ASSERT_TRUE(side_panel_contents != nullptr);
          EXPECT_TRUE(base::test::RunUntil([&]() -> bool {
            *out_chip_rfh = content::FrameMatchingPredicateOrNullptr(
                side_panel_contents->GetPrimaryPage(),
                base::BindRepeating([](content::RenderFrameHost* rfh) {
                  return rfh->GetLastCommittedURL().ExtractFileName() ==
                         "lens_chip.html";
                }));
            return *out_chip_rfh != nullptr;
          }));
          ASSERT_TRUE(*out_chip_rfh != nullptr);

          auto result = content::EvalJs(*out_chip_rfh, R"(
            new Promise((resolve) => {
              const check = () => {
                const app = document.querySelector('lens-chip-app');
                const img = app?.shadowRoot?.querySelector('#cropImage');
                if (img && img.src && img.src.startsWith('data:image/')) {
                  resolve(img.src);
                } else {
                  setTimeout(check, 50);
                }
              };
              check();
            });
          )");
          ASSERT_TRUE(result.is_ok());
          *out_crop_uri = result.ExtractString();
          ASSERT_FALSE(out_crop_uri->empty());
        }));
  }

  InteractiveTestApi::MultiStep SelectNewRegionOnOverlay(
      ui::ElementIdentifier overlay_id) {
    return Steps(InSameContext(ExecuteJsAt(overlay_id, {}, R"(
              () => {
                const overlay = document.querySelector('lens-overlay-app')
                    ?.shadowRoot?.querySelector('lens-selection-overlay');
                if (!overlay) return false;
                const rect = overlay.getBoundingClientRect();
                const startX = rect.left + rect.width / 2 - 50;
                const startY = rect.top + rect.height / 2 - 50;
                const endX = rect.left + rect.width / 2 - 120;
                const endY = rect.top + rect.height / 2 - 120;
                const savedSetPointerCapture = overlay.setPointerCapture;
                overlay.setPointerCapture = () => {};
                try {
                  overlay.dispatchEvent(new PointerEvent('pointerdown', {
                    button: 0,
                    buttons: 1,
                    clientX: startX,
                    clientY: startY,
                    bubbles: true,
                    composed: true,
                    isPrimary: true,
                  }));
                  overlay.dispatchEvent(new PointerEvent('pointermove', {
                    button: 0,
                    buttons: 1,
                    clientX: endX,
                    clientY: endY,
                    bubbles: true,
                    composed: true,
                    isPrimary: true,
                  }));
                  overlay.dispatchEvent(new PointerEvent('pointerup', {
                    button: 0,
                    buttons: 0,
                    clientX: endX,
                    clientY: endY,
                    bubbles: true,
                    composed: true,
                    isPrimary: true,
                  }));
                } finally {
                  overlay.setPointerCapture = savedSetPointerCapture;
                }
                return true;
              }
            )")));
  }

  InteractiveTestApi::StepBuilder VerifyLensChipUpdatedInPlace(
      content::RenderFrameHost* const* chip_rfh,
      const std::string* first_crop_uri) {
    return Do([this, chip_rfh, first_crop_uri]() {
      auto* controller =
          contextual_tasks::ContextualTasksPanelController::From(browser());
      content::WebContents* side_panel_contents =
          controller->GetActiveWebContents();
      ASSERT_TRUE(side_panel_contents != nullptr);
      auto* user_data =
          contextual_tasks::ContextualTasksWebContentsUserData::FromWebContents(
              side_panel_contents);
      ASSERT_TRUE(user_data != nullptr);
      auto model = user_data->input_state_model();
      ASSERT_TRUE(model != nullptr);

      base::test::TestFuture<void> crop_updated;
      base::CallbackListSubscription subscription;
      if (model->lens_crop().has_value() &&
          model->lens_crop()->data_uri != *first_crop_uri) {
        crop_updated.SetValue();
      } else {
        subscription = model->Subscribe(
            base::BindLambdaForTesting([&](const omnibox::InputState&) {
              if (model->lens_crop().has_value() &&
                  model->lens_crop()->data_uri != *first_crop_uri &&
                  !crop_updated.IsReady()) {
                crop_updated.SetValue();
              }
            }));
      }
      EXPECT_TRUE(crop_updated.Wait());

      EXPECT_TRUE(content::ExecJs(
          *chip_rfh, base::StringPrintf(R"(
        new Promise((resolve) => {
          const check = () => {
            const app = document.querySelector('lens-chip-app');
            const img = app?.shadowRoot?.querySelector('#cropImage');
            if (img && img.src && img.src === '%s') {
              resolve(true);
            } else {
              setTimeout(check, 50);
            }
          };
          check();
        });
      )",
                                        model->lens_crop()->data_uri.c_str())));

      EXPECT_EQ(content::EvalJs(side_panel_contents,
                                "document.body.dataset.mountCount"),
                "1");
    });
  }

  InteractiveTestApi::StepBuilder ClickLensChipCloseButton(
      content::RenderFrameHost* const* chip_rfh) {
    return Do([chip_rfh]() {
      EXPECT_TRUE(content::ExecJs(*chip_rfh, R"(
        new Promise((resolve) => {
          const check = () => {
            const app = document.querySelector('lens-chip-app');
            const btn = app?.shadowRoot?.querySelector('#closeButton');
            if (btn && !btn.disabled) {
              btn.click();
              resolve(true);
            } else {
              setTimeout(check, 50);
            }
          };
          check();
        });
      )"));
    });
  }

  InteractiveTestApi::MultiStep VerifyLensChipUnmountedAndOverlayClosed(
      ui::ElementIdentifier side_panel_contents_id) {
    const DeepQuery kPathToBody{"body"};
    return Steps(
        WaitForJsResultAt(side_panel_contents_id, kPathToBody,
                          "el => el.dataset.chipMounted", "false"),
        WaitForJsResultAt(side_panel_contents_id, kPathToBody,
                          "el => el.dataset.unmountCount", "1"),
        WaitForJsResultAt(
            side_panel_contents_id, kPathToBody,
            "() => document.getElementById('lens-chip-iframe') === null", true),
        Do([this]() {
          auto* controller =
              contextual_tasks::ContextualTasksPanelController::From(browser());
          content::WebContents* side_panel_contents =
              controller->GetActiveWebContents();
          auto* user_data =
              contextual_tasks::ContextualTasksWebContentsUserData::
                  FromWebContents(side_panel_contents);
          EXPECT_TRUE(user_data != nullptr);
          if (user_data) {
            auto model = user_data->input_state_model();
            EXPECT_TRUE(model != nullptr);
            if (model) {
              EXPECT_FALSE(model->lens_crop().has_value());
            }
          }

          content::WebContents* tab_contents =
              browser()->GetTabStripModel()->GetActiveWebContents();
          auto* search_controller =
              LensSearchController::FromTabWebContents(tab_contents);
          EXPECT_TRUE(search_controller != nullptr);
          if (search_controller) {
            EXPECT_TRUE(search_controller->IsOff() ||
                        search_controller->IsClosing());
            if (search_controller->lens_overlay_controller()) {
              EXPECT_FALSE(search_controller->lens_overlay_controller()
                               ->HasRegionSelection());
            }
          }
        }));
  }
  InteractiveTestApi::MultiStep SubmitQueryFromAim(
      ui::ElementIdentifier side_panel_contents_id) {
    const DeepQuery kPathToSubmitButton{"#submitQueryButton"};
    return Steps(EnsurePresent(side_panel_contents_id, kPathToSubmitButton),
                 MoveMouseTo(side_panel_contents_id, kPathToSubmitButton),
                 ClickMouse());
  }

  InteractiveTestApi::MultiStep VerifyLensChipMetrics(
      const base::HistogramTester* histogram_tester,
      const base::UserActionTester* user_action_tester) {
    return Steps(Do([histogram_tester, user_action_tester]() {
      histogram_tester->ExpectBucketCount(
          "ContextualTasks.LensRegionChip.Shown", true, 1);
      histogram_tester->ExpectBucketCount(
          "ContextualTasks.LensRegionChip.PreviewLoaded", true, 1);
      histogram_tester->ExpectBucketCount(
          "ContextualTasks.LensRegionChip.Dismissed", true, 1);
      EXPECT_EQ(1, user_action_tester->GetActionCount(
                       "ContextualTasks.LensRegionChip.Shown"));
      EXPECT_EQ(1, user_action_tester->GetActionCount(
                       "ContextualTasks.LensRegionChip.Dismissed"));
    }));
  }
};

// TODO(crbug.com/577352347): Flaky timeouts on MSan.
#if defined(MEMORY_SANITIZER)
#define MAYBE_OpenOverlaySelectRegionMountChipAndDismiss \
  DISABLED_OpenOverlaySelectRegionMountChipAndDismiss
#else
#define MAYBE_OpenOverlaySelectRegionMountChipAndDismiss \
  OpenOverlaySelectRegionMountChipAndDismiss
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksLensRegionChipInteractiveUiTest,
                       MAYBE_OpenOverlaySelectRegionMountChipAndDismiss) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  SidePanelUI::From(browser())->DisableAnimationsForTesting();

  contextual_tasks::SetForcedEmbeddedPageHostOverride(
      contextual_tasks::HostOverride{"www.google.com"});
  base::ScopedClosureRunner clear_host_override(base::BindOnce([]() {
    contextual_tasks::SetForcedEmbeddedPageHostOverride(std::nullopt);
  }));

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  std::string mock_aim_html;
  ASSERT_TRUE(base::ReadFileToString(
      test_data_dir.AppendASCII("lens/mock_aim_page.html"), &mock_aim_html));

  content::URLLoaderInterceptor url_loader_interceptor(base::BindRepeating(
      [](const std::string& html,
         content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "www.google.com") {
          content::URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\nContent-Type: text/html\n\n", html,
              params->client.get());
          return true;
        }
        return false;
      },
      mock_aim_html));

  const GURL tab_url = embedded_test_server()->GetURL(kDocumentWithImage);

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point_1 = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center = browser_view->contents_web_view_for_testing()
                                ->GetBoundsInScreen()
                                .CenterPoint();
    off_center.Offset(50, 50);
    return off_center;
  });

  content::RenderFrameHost* chip_rfh = nullptr;
  std::string first_crop_uri;

  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  RunTestSequence(
      NavigateToTabWithImage(kActiveTab, tab_url),
      OpenContextualTasksSidePanelViaToolbarButton(kSidePanelWebContentsId),
      WaitForAimHandshake(kSidePanelWebContentsId), ClickSidePanelLensButton(),
      WaitForLensOverlayAndSelectRegion(kOverlayId,
                                        std::move(off_center_point_1)),
      WaitForLensChipMountedWithPreview(kSidePanelWebContentsId, &chip_rfh,
                                        &first_crop_uri),
      SelectNewRegionOnOverlay(kOverlayId),
      VerifyLensChipUpdatedInPlace(&chip_rfh, &first_crop_uri),
      ClickLensChipCloseButton(&chip_rfh),
      VerifyLensChipUnmountedAndOverlayClosed(kSidePanelWebContentsId),
      VerifyLensChipMetrics(&histogram_tester, &user_action_tester));
}

// TODO(crbug.com/577352347): Flaky timeouts on MSan.
#if defined(MEMORY_SANITIZER)
#define MAYBE_OpenOverlaySelectRegionSubmitQueryAndDismissChip \
  DISABLED_OpenOverlaySelectRegionSubmitQueryAndDismissChip
#else
#define MAYBE_OpenOverlaySelectRegionSubmitQueryAndDismissChip \
  OpenOverlaySelectRegionSubmitQueryAndDismissChip
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksLensRegionChipInteractiveUiTest,
                       MAYBE_OpenOverlaySelectRegionSubmitQueryAndDismissChip) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  SidePanelUI::From(browser())->DisableAnimationsForTesting();

  contextual_tasks::SetForcedEmbeddedPageHostOverride(
      contextual_tasks::HostOverride{"www.google.com"});
  base::ScopedClosureRunner clear_host_override(base::BindOnce([]() {
    contextual_tasks::SetForcedEmbeddedPageHostOverride(std::nullopt);
  }));

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  std::string mock_aim_html;
  ASSERT_TRUE(base::ReadFileToString(
      test_data_dir.AppendASCII("lens/mock_aim_page.html"), &mock_aim_html));

  content::URLLoaderInterceptor url_loader_interceptor(base::BindRepeating(
      [](const std::string& html,
         content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "www.google.com") {
          content::URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\nContent-Type: text/html\n\n", html,
              params->client.get());
          return true;
        }
        return false;
      },
      mock_aim_html));

  const GURL tab_url = embedded_test_server()->GetURL(kDocumentWithImage);

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point_1 = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center = browser_view->contents_web_view_for_testing()
                                ->GetBoundsInScreen()
                                .CenterPoint();
    off_center.Offset(50, 50);
    return off_center;
  });

  content::RenderFrameHost* chip_rfh = nullptr;
  std::string first_crop_uri;

  RunTestSequence(
      NavigateToTabWithImage(kActiveTab, tab_url),
      OpenContextualTasksSidePanelViaToolbarButton(kSidePanelWebContentsId),
      WaitForAimHandshake(kSidePanelWebContentsId), ClickSidePanelLensButton(),
      WaitForLensOverlayAndSelectRegion(kOverlayId,
                                        std::move(off_center_point_1)),
      WaitForLensChipMountedWithPreview(kSidePanelWebContentsId, &chip_rfh,
                                        &first_crop_uri),
      SubmitQueryFromAim(kSidePanelWebContentsId),
      VerifyLensChipUnmountedAndOverlayClosed(kSidePanelWebContentsId));
}

}  // namespace
