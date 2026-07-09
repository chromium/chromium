// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_interactive_test_base.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_metrics.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"

namespace {

class ContextualTasksLensOverlayControllerInteractiveUiTest
    : public LensOverlayInteractiveTestBase {
 public:
  ContextualTasksLensOverlayControllerInteractiveUiTest() = default;
  ~ContextualTasksLensOverlayControllerInteractiveUiTest() override = default;

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
              browser()->tab_strip_model()->GetWebContentsAt(tab_index);
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

  browser()->GetFeatures().side_panel_ui()->DisableAnimationsForTesting();
  contextual_tasks::ContextualTasksPanelController* controller =
      contextual_tasks::ContextualTasksPanelController::From(browser());

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
        controller->Close();
      }),
      WaitForContextualPanelAndLensToClose());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksLensOverlayControllerInteractiveUiTest,
                       LensSessionsCloseOnSidePanelClose_MultiTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);

  browser()->GetFeatures().side_panel_ui()->DisableAnimationsForTesting();
  contextual_tasks::ContextualTasksPanelController* controller =
      contextual_tasks::ContextualTasksPanelController::From(browser());
  contextual_tasks::ContextualTasksService* contextual_tasks_service =
      contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
          browser()->GetProfile());

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
        auto task = contextual_tasks_service->GetContextualTaskForTab(tab_id0);
        contextual_tasks_service->AssociateTabWithTask(
            task->GetTaskId(),
            sessions::SessionTabHelper::IdForTab(
                browser()->tab_strip_model()->GetWebContentsAt(1)));

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

  browser()->GetFeatures().side_panel_ui()->DisableAnimationsForTesting();
  contextual_tasks::ContextualTasksPanelController* controller =
      contextual_tasks::ContextualTasksPanelController::From(browser());

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->contents_web_view()->bounds().CenterPoint();
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
                       DISABLED_ComposeboxLensButtonClearsThenTogglesOverlay) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kLensButtonExists);

  const DeepQuery kPathToLensButton{"contextual-tasks-app",
                                    "contextual-tasks-composebox",
                                    "#composebox", "#lensIcon"};

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->contents_web_view()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  StateChange lens_button_exists;
  lens_button_exists.event = kLensButtonExists;
  lens_button_exists.where = kPathToLensButton;
  lens_button_exists.type = StateChange::Type::kExists;

  RunTestSequence(
      // 1. Open Lens Overlay and make a selection to open the side panel.
      OpenLensOverlayWithRegionSearch(kFirstTab, kOverlayId, off_center_point),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              kContextualTasksSidePanelWebViewElementId),
      // Fix load-abort in the side panel by navigating the embedded frame
      // to a local URL. This keeps the searchbox visible.
      ExecuteJsAt(
          kSidePanelWebContentsId, {"contextual-tasks-app", "#threadFrame"},
          base::StringPrintf("el => { el.src = '%s'; }", url.spec().c_str())),
      // Force the searchbox to stay visible by mimicking an AI page status.
      ExecuteJsAt(kSidePanelWebContentsId, DeepQuery{"contextual-tasks-app"},
                  "el => { el.isAiPage_ = true; }"),
      WaitForWebContentsReady(kSidePanelWebContentsId),

      // 2. Click the Lens button in the side panel to clear the overlay.
      WaitForStateChange(kSidePanelWebContentsId, lens_button_exists),
      ClickElement(kSidePanelWebContentsId, kPathToLensButton),

      // 3. Click the Lens button again to close the overlay.
      EnsurePresent(kOverlayId),
      ClickElement(kSidePanelWebContentsId, kPathToLensButton),
      WaitForHide(LensOverlayController::kOverlayId));
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

  browser()->GetFeatures().side_panel_ui()->DisableAnimationsForTesting();

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->contents_web_view()->bounds().CenterPoint();
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

}  // namespace
