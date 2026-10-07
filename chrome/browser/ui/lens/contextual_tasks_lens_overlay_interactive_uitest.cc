// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_interface.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service_delegate.h"
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
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_metrics.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"

namespace {

class ContextualTasksLensOverlayControllerInteractiveUiTest
    : public LensOverlayInteractiveTestBase {
 public:
  ContextualTasksLensOverlayControllerInteractiveUiTest() = default;
  ~ContextualTasksLensOverlayControllerInteractiveUiTest() override = default;

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{contextual_tasks::kContextualTasks, {}},
         {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}},
         {lens::features::kLensOverlayTextSelectionContextMenuEntrypoint,
          {{"contextualize", "true"}}}},
        /*disabled_features=*/{features::kNonBlockingOsClipboardReads});
  }

  void SetUpInProcessBrowserTestFixture() override {
    LensOverlayInteractiveTestBase::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ContextualTasksLensOverlayControllerInteractiveUiTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          return std::make_unique<TestingAimEligibilityService>(
              /*is_aim_eligible=*/true,
              /*is_cobrowse_eligible=*/true, *profile->GetPrefs(),
              /*template_url_service=*/nullptr);
        }));
    contextual_tasks::ContextualTasksUiServiceFactory::GetInstance()
        ->SetTestingFactory(
            context,
            base::BindLambdaForTesting([](content::BrowserContext* context) {
              Profile* profile = Profile::FromBrowserContext(context);
              return static_cast<std::unique_ptr<KeyedService>>(
                  std::make_unique<TestingContextualTasksUiService>(
                      profile,
                      contextual_tasks::ContextualTasksServiceFactory::
                          GetForProfile(profile),
                      IdentityManagerFactory::GetForProfile(profile),
                      AimEligibilityServiceFactory::GetForProfile(profile),
                      /*cookie_synchronizer=*/nullptr));
            }));
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("www.google.com", "127.0.0.1");
    host_resolver()->AddRule("www.g.ai", "127.0.0.1");
    LensOverlayInteractiveTestBase::SetUpOnMainThread();

    WaitForTemplateURLServiceToLoad();

    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->GetProfile());

    identity_test_environment_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable("user@example.com",
                                      signin::ConsentLevel::kSignin);
    identity_test_environment_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  void TearDownOnMainThread() override {
    identity_test_environment_adaptor_.reset();
    LensOverlayInteractiveTestBase::TearDownOnMainThread();
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

 private:
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksLensOverlayControllerInteractiveUiTest,
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

IN_PROC_BROWSER_TEST_F(ContextualTasksLensOverlayControllerInteractiveUiTest,
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

IN_PROC_BROWSER_TEST_F(ContextualTasksLensOverlayControllerInteractiveUiTest,
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

IN_PROC_BROWSER_TEST_F(ContextualTasksLensOverlayControllerInteractiveUiTest,
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

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  const GURL page_url =
      embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const GURL aim_url("https://www.g.ai/?q=summary");
  const DeepQuery kPathToBody{"body"};
  const DeepQuery kPathToRegionSelection{
      "lens-overlay-app",
      "lens-selection-overlay",
      "#regionSelectionLayer",
  };

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
                   WaitForHide(AppMenuModel::kShowLensOverlay)),
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),
      InSameContext(
          WaitForShow(LensOverlayController::kOverlayId), Do([&]() {
            auto* web_contents =
                browser()->GetTabStripModel()->GetActiveWebContents();
            auto* lens_controller =
                LensSearchController::FromTabWebContents(web_contents);
            CHECK(lens_controller);
            auto* query_router = static_cast<lens::FakeLensQueryFlowRouter*>(
                lens_controller->query_router());
            CHECK(query_router);
            auto* session_handle = static_cast<
                contextual_search::MockContextualSearchSessionHandle*>(
                query_router->GetContextualSearchSessionHandle());
            CHECK(session_handle);
            ON_CALL(*session_handle,
                    CreateSearchUrl(::testing::_, ::testing::_))
                .WillByDefault(::testing::WithArg<1>(
                    [](base::OnceCallback<void(GURL)> callback) {
                      std::move(callback).Run(
                          GURL("https://www.google.com/search?q=lens_result"));
                    }));
          }),
          ExecuteJsAt(kOverlayId, {}, R"(
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
          WaitForScreenshotRendered(kOverlayId),
          EnsurePresent(kOverlayId, kPathToRegionSelection),
          MoveMouseTo(LensOverlayController::kOverlayId),
          DragMouseTo(std::move(off_center_point)), FinishScreenshotUpload(0)),

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

IN_PROC_BROWSER_TEST_F(ContextualTasksLensOverlayControllerInteractiveUiTest,
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
IN_PROC_BROWSER_TEST_F(ContextualTasksLensOverlayControllerInteractiveUiTest,
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
      "r.height > 0; }";

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
      ExecuteJsAt(kSidePanelWebContentsId, DeepQuery{"contextual-tasks-app"},
                  "el => { "
                  "  el.removeThreadFrameListenersForTesting(); "
                  "  el.isLoadError_ = false; "
                  "  el.isZeroState_ = true; "
                  "}"),
      WaitForWebContentsReady(kSidePanelWebContentsId),

      // 2. Click the Lens button in the side panel to clear the overlay.
      WaitForStateChange(kSidePanelWebContentsId, lens_button_exists),
      ClickElement(kSidePanelWebContentsId, kPathToLensButton),

      // 3. Click the Lens button again to close the overlay.
      EnsurePresent(kOverlayId),
      ClickElement(kSidePanelWebContentsId, kPathToLensButton),
      WaitForHide(LensOverlayController::kOverlayId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksLensOverlayControllerInteractiveUiTest,
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
      "r.height > 0; }";

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
      ExecuteJsAt(kSidePanelWebContentsId, DeepQuery{"contextual-tasks-app"},
                  "el => { "
                  "  el.removeThreadFrameListenersForTesting(); "
                  "  el.isLoadError_ = false; "
                  "  el.isZeroState_ = true; "
                  "}"),
      WaitForWebContentsReady(kSidePanelWebContentsId),

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
      CheckJsResultAt(kSidePanelWebContentsId, kPathToClearButton,
                      "el => !el.disabled", true),
      ClickElement(kSidePanelWebContentsId, kPathToClearButton),

      // 5. Verify the region selection is cleared from the overlay.
      InAnyContext(WaitForJsResultAt(kOverlayId, kPathToSelectionOverlay,
                                     kHasPostSelection, false)),
      CheckResult(has_region_selection, false));
}

class ContextualTasksLensOverlayEphemeralButtonInteractiveUiTest
    : public ContextualTasksLensOverlayControllerInteractiveUiTest {
 public:
  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{contextual_tasks::kContextualTasks, {}},
         {contextual_tasks::kContextualTasksEphemeralBrandedEntryPoint,
          {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}},
         {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}}},
        /*disabled_features=*/{features::kNonBlockingOsClipboardReads});
  }
};

IN_PROC_BROWSER_TEST_F(
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
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId));
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
IN_PROC_BROWSER_TEST_F(ContextualTasksLensOverlayControllerInteractiveUiTest,
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
      MoveMouseTo(kTabId, kPathToImg), ClickMouse(ui_controls::RIGHT),
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
IN_PROC_BROWSER_TEST_F(ContextualTasksLensOverlayControllerInteractiveUiTest,
                       MAYBE_WebpageContextMenuClick) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

  const GURL page_url =
      embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const DeepQuery kPathToBody{"body"};

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->GetContentsView()->bounds().CenterPoint();
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
      MoveMouseTo(kTabId), ClickMouse(ui_controls::RIGHT),
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
IN_PROC_BROWSER_TEST_F(ContextualTasksLensOverlayControllerInteractiveUiTest,
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
      MoveMouseTo(kTabId, kPathToParagraph), ClickMouse(ui_controls::RIGHT),
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

enum class AimEligibilityTestState {
  kEligible,
  kAimIneligible,
  kCobrowseIneligible,
};

class ContextualTasksLensOverlayControllerEligibilityInteractiveUiTest
    : public LensOverlayInteractiveTestBase,
      public testing::WithParamInterface<AimEligibilityTestState> {
 public:
  ContextualTasksLensOverlayControllerEligibilityInteractiveUiTest() = default;
  ~ContextualTasksLensOverlayControllerEligibilityInteractiveUiTest() override =
      default;

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{contextual_tasks::kContextualTasks, {}},
                              {contextual_tasks::
                                   kContextualTasksForceEntryPointEligibility,
                               {}}},
        /*disabled_features=*/{features::kNonBlockingOsClipboardReads});
  }

  void SetUpInProcessBrowserTestFixture() override {
    LensOverlayInteractiveTestBase::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ContextualTasksLensOverlayControllerEligibilityInteractiveUiTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);

    AimEligibilityTestState state = GetParam();
    bool is_aim = state == AimEligibilityTestState::kEligible ||
                  state == AimEligibilityTestState::kCobrowseIneligible;
    bool is_cobrowse = state == AimEligibilityTestState::kEligible;

    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            [](bool aim, bool cobrowse, content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> {
              Profile* profile = Profile::FromBrowserContext(context);
              return std::make_unique<TestingAimEligibilityService>(
                  aim, cobrowse, *profile->GetPrefs(),
                  /*template_url_service=*/nullptr);
            },
            is_aim, is_cobrowse));

    contextual_tasks::ContextualTasksUiServiceFactory::GetInstance()
        ->SetTestingFactory(
            context,
            base::BindLambdaForTesting([](content::BrowserContext* context) {
              Profile* profile = Profile::FromBrowserContext(context);
              return static_cast<std::unique_ptr<KeyedService>>(
                  std::make_unique<TestingContextualTasksUiService>(
                      profile,
                      contextual_tasks::ContextualTasksServiceFactory::
                          GetForProfile(profile),
                      IdentityManagerFactory::GetForProfile(profile),
                      AimEligibilityServiceFactory::GetForProfile(profile),
                      /*cookie_synchronizer=*/nullptr));
            }));
  }

  void SetUpOnMainThread() override {
    LensOverlayInteractiveTestBase::SetUpOnMainThread();
    WaitForTemplateURLServiceToLoad();
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->GetProfile());
    identity_test_environment_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable("user@example.com",
                                      signin::ConsentLevel::kSignin);
    identity_test_environment_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  void TearDownOnMainThread() override {
    identity_test_environment_adaptor_.reset();
    LensOverlayInteractiveTestBase::TearDownOnMainThread();
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
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
  AimEligibilityTestState state = GetParam();
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
    testing::Values(AimEligibilityTestState::kEligible,
                    AimEligibilityTestState::kAimIneligible,
                    AimEligibilityTestState::kCobrowseIneligible));

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

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
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

class ContextualTasksContextMenuAskGoogleInteractiveUiTest
    : public LensOverlayInteractiveTestBase {
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
         {lens::features::kLensSidePanelUnification, {}},
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

  void SetUpInProcessBrowserTestFixture() override {
    LensOverlayInteractiveTestBase::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ContextualTasksContextMenuAskGoogleInteractiveUiTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          return std::make_unique<TestingAimEligibilityService>(
              /*is_aim_eligible=*/true,
              /*is_cobrowse_eligible=*/true, *profile->GetPrefs(),
              TemplateURLServiceFactory::GetForProfile(profile));
        }));
    contextual_tasks::ContextualTasksUiServiceFactory::GetInstance()
        ->SetTestingFactory(
            context,
            base::BindLambdaForTesting([](content::BrowserContext* context) {
              Profile* profile = Profile::FromBrowserContext(context);
              return static_cast<std::unique_ptr<KeyedService>>(
                  std::make_unique<TestingContextualTasksUiService>(
                      profile,
                      contextual_tasks::ContextualTasksServiceFactory::
                          GetForProfile(profile),
                      IdentityManagerFactory::GetForProfile(profile),
                      AimEligibilityServiceFactory::GetForProfile(profile),
                      /*cookie_synchronizer=*/nullptr));
            }));
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("www.google.com", "127.0.0.1");
    host_resolver()->AddRule("www.g.ai", "127.0.0.1");
    LensOverlayInteractiveTestBase::SetUpOnMainThread();
    WaitForTemplateURLServiceToLoad();
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
};

#if BUILDFLAG(IS_MAC)
#define MAYBE_AskGoogleContextMenuClickOpensSidePanel \
  DISABLED_AskGoogleContextMenuClickOpensSidePanel
#else
#define MAYBE_AskGoogleContextMenuClickOpensSidePanel \
  AskGoogleContextMenuClickOpensSidePanel
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksContextMenuAskGoogleInteractiveUiTest,
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

      MoveMouseTo(kTabId), ClickMouse(ui_controls::RIGHT),
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

IN_PROC_BROWSER_TEST_F(ContextualTasksContextMenuAskGoogleInteractiveUiTest,
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

IN_PROC_BROWSER_TEST_F(ContextualTasksContextMenuAskGoogleInteractiveUiTest,
                       PinnedToolbarButtonClickOpensAndClosesSidePanel) {
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

      Do([this]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),

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
              lens::LensOverlayInvocationSource::kCobrowsePinnedToolbarButton),
          "Side panel session has kCobrowsePinnedToolbarButton invocation "
          "source"),

      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      WaitForHide(kContextualTasksSidePanelWebViewElementId),
      CheckResult(
          [this]() -> bool {
            return SidePanelUI::From(browser())->IsSidePanelShowing();
          },
          false));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksContextMenuAskGoogleInteractiveUiTest,
                       HomeworkActionChipClickOpensSidePanel) {
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

class ContextualTasksContextMenuSubmenuInteractiveUiTest
    : public LensOverlayInteractiveTestBase {
 public:
  ContextualTasksContextMenuSubmenuInteractiveUiTest() = default;
  ~ContextualTasksContextMenuSubmenuInteractiveUiTest() override = default;

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{contextual_tasks::kContextualTasks, {}},
         {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}},
         {lens::features::kLensSidePanelUnification, {}},
         {contextual_tasks::kContextualTasksUpdatedEntryPoints,
          {{"ContextualTasksContextMenuShowAskGoogle", "true"},
           {"ContextualTasksContextMenuSubmenu", "true"},
           {"ContextualTasksContextMenuRouteAskGoogleToOmnibox", "false"}}}},
        /*disabled_features=*/{features::kNonBlockingOsClipboardReads});
  }

  void SetUpInProcessBrowserTestFixture() override {
    LensOverlayInteractiveTestBase::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ContextualTasksContextMenuSubmenuInteractiveUiTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          return std::make_unique<TestingAimEligibilityService>(
              /*is_aim_eligible=*/true,
              /*is_cobrowse_eligible=*/true, *profile->GetPrefs(),
              TemplateURLServiceFactory::GetForProfile(profile));
        }));
    contextual_tasks::ContextualTasksUiServiceFactory::GetInstance()
        ->SetTestingFactory(
            context,
            base::BindLambdaForTesting([](content::BrowserContext* context) {
              Profile* profile = Profile::FromBrowserContext(context);
              return static_cast<std::unique_ptr<KeyedService>>(
                  std::make_unique<TestingContextualTasksUiService>(
                      profile,
                      contextual_tasks::ContextualTasksServiceFactory::
                          GetForProfile(profile),
                      IdentityManagerFactory::GetForProfile(profile),
                      AimEligibilityServiceFactory::GetForProfile(profile),
                      /*cookie_synchronizer=*/nullptr));
            }));
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("www.google.com", "127.0.0.1");
    host_resolver()->AddRule("www.g.ai", "127.0.0.1");
    LensOverlayInteractiveTestBase::SetUpOnMainThread();
    WaitForTemplateURLServiceToLoad();
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
};

#if BUILDFLAG(IS_MAC)
#define MAYBE_AskGoogleSubmenuItemClickOpensSidePanel \
  DISABLED_AskGoogleSubmenuItemClickOpensSidePanel
#else
#define MAYBE_AskGoogleSubmenuItemClickOpensSidePanel \
  AskGoogleSubmenuItemClickOpensSidePanel
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksContextMenuSubmenuInteractiveUiTest,
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

      MoveMouseTo(kTabId), ClickMouse(ui_controls::RIGHT),
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

IN_PROC_BROWSER_TEST_F(ContextualTasksContextMenuSubmenuInteractiveUiTest,
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

class ContextualTasksContextMenuRouteOmniboxInteractiveUiTest
    : public LensOverlayInteractiveTestBase {
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
         {lens::features::kLensSidePanelUnification, {}},
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

  void SetUpInProcessBrowserTestFixture() override {
    LensOverlayInteractiveTestBase::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ContextualTasksContextMenuRouteOmniboxInteractiveUiTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          return std::make_unique<TestingAimEligibilityService>(
              /*is_aim_eligible=*/true,
              /*is_cobrowse_eligible=*/true, *profile->GetPrefs(),
              /*template_url_service=*/nullptr);
        }));
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
};

#if BUILDFLAG(IS_MAC)
#define MAYBE_AskGoogleContextMenuClickOpensOmnibox \
  DISABLED_AskGoogleContextMenuClickOpensOmnibox
#else
#define MAYBE_AskGoogleContextMenuClickOpensOmnibox \
  AskGoogleContextMenuClickOpensOmnibox
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksContextMenuRouteOmniboxInteractiveUiTest,
                       MAYBE_AskGoogleContextMenuClickOpensOmnibox) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

  const GURL page_url =
      embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const DeepQuery kPathToBody{"body"};

  RunTestSequence(
      InstrumentTab(kTabId, 0), NavigateWebContents(kTabId, page_url),
      EnsurePresent(kTabId, kPathToBody), WaitForWebContentsPainted(kTabId),
      WaitForWebContentsReady(kTabId, page_url),

      MoveMouseTo(kTabId), ClickMouse(ui_controls::RIGHT),
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

IN_PROC_BROWSER_TEST_F(ContextualTasksContextMenuRouteOmniboxInteractiveUiTest,
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

IN_PROC_BROWSER_TEST_F(ContextualTasksContextMenuRouteOmniboxInteractiveUiTest,
                       PinnedToolbarButtonClickOpensOmnibox) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

  const GURL page_url =
      embedded_test_server()->GetURL(kDocumentWithNamedElement);
  const DeepQuery kPathToBody{"body"};

  RunTestSequence(
      InstrumentTab(kTabId, 0), NavigateWebContents(kTabId, page_url),
      EnsurePresent(kTabId, kPathToBody), WaitForWebContentsPainted(kTabId),
      WaitForWebContentsReady(kTabId, page_url),

      Do([this]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),

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

IN_PROC_BROWSER_TEST_F(ContextualTasksContextMenuRouteOmniboxInteractiveUiTest,
                       HomeworkActionChipClickOpensOmnibox) {
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

}  // namespace
