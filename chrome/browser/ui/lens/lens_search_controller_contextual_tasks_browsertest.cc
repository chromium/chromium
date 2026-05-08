// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_overlay_wait_for_paint_utils.h"
#include "chrome/browser/ui/lens/lens_search_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/internal/composebox_query_controller.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"

namespace {

GURL g_test_server_url;

class TestingContextualTasksUiService
    : public contextual_tasks::ContextualTasksUiService,
      public content::WebContentsObserver {
 public:
  TestingContextualTasksUiService(
      Profile* profile,
      contextual_tasks::ContextualTasksService* contextual_tasks_service,
      signin::IdentityManager* identity_manager,
      AimEligibilityService* aim_eligibility_service)
      : ContextualTasksUiService(
            profile,
            /*delegate=*/nullptr,
            contextual_tasks_service,
            identity_manager,
            aim_eligibility_service,
            std::unique_ptr<
                contextual_tasks::ContextualTasksCookieSynchronizer>()),
        profile_(profile),
        contextual_tasks_service_(contextual_tasks_service) {}
  ~TestingContextualTasksUiService() override = default;

  void Shutdown() override {
    stub_web_contents_ = nullptr;
    contextual_tasks::ContextualTasksUiService::Shutdown();
  }

  bool CookieJarContainsPrimaryAccount() override {
    return cookie_jar_contains_primary_account_;
  }

  void SetCookieJarContainsPrimaryAccount(bool contains) {
    cookie_jar_contains_primary_account_ = contains;
  }

  void StartTaskUiInSidePanel(
      BrowserWindowInterface* browser_window_interface,
      tabs::TabInterface* tab_interface,
      const GURL& url,
      std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
          session_handle) override {
    is_panel_open_ = true;
    if (!stub_web_contents_) {
      content::WebContents::CreateParams params(profile_);
      auto web_contents = content::WebContents::Create(params);
      stub_web_contents_ = web_contents.get();
      Observe(stub_web_contents_);
      Browser* browser = static_cast<Browser*>(browser_window_interface);
      browser->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                    /*foreground=*/false);
    }
    std::string webui_url = "chrome://contextual-tasks/?aimUrl=" + url.spec();
    stub_web_contents_->GetController().LoadURL(
        GURL(webui_url), content::Referrer(), ui::PAGE_TRANSITION_LINK,
        std::string());
  }

  content::WebContents* stub_web_contents() { return stub_web_contents_.get(); }

  void StartTaskUiInSidePanelWithErrorPage(
      BrowserWindowInterface* browser_window_interface,
      tabs::TabInterface* tab_interface,
      std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
          session_handle) override {
    is_panel_open_ = true;
    session_handle_ = std::move(session_handle);

    auto web_contents = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    stub_web_contents_ = web_contents.get();
    Observe(stub_web_contents_);
    Browser* browser = static_cast<Browser*>(browser_window_interface);
    browser->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                  /*foreground=*/false);
    stub_web_contents_->GetController().LoadURL(
        GURL("about:blank"), content::Referrer(), ui::PAGE_TRANSITION_LINK,
        std::string());

    if (contextual_tasks_service_ && tab_interface) {
      auto task = contextual_tasks_service_->CreateTask();
      pending_error_page_task_id_ = task.GetTaskId();
      SessionID session_id =
          sessions::SessionTabHelper::IdForTab(tab_interface->GetContents());
      if (session_id.is_valid()) {
        contextual_tasks_service_->AssociateTabWithTask(task.GetTaskId(),
                                                        session_id);
      }
    }

    contextual_tasks::RecordErrorPageShown(
        contextual_search::ContextualSearchSource::kLens);

    auto* controller = contextual_tasks::ContextualTasksPanelController::From(
        browser_window_interface);
    if (controller) {
      controller->Show();
    }
  }

  bool IsPendingErrorPage(const base::Uuid& task_id) override {
    return task_id == pending_error_page_task_id_;
  }

  bool HasPendingErrorPage() const {
    return pending_error_page_task_id_.is_valid();
  }

  std::optional<GURL> GetInitialUrlForTask(const base::Uuid& uuid) override {
    if (uuid == pending_task_id_) {
      return pending_task_url_;
    }
    return std::nullopt;
  }

  bool IsPanelOpen() const { return is_panel_open_; }
  void SetPanelOpen(bool open) { is_panel_open_ = open; }

  contextual_search::ContextualSearchSessionHandle* GetSessionHandle() {
    return session_handle_.get();
  }

 private:
  bool cookie_jar_contains_primary_account_ = true;
  bool is_panel_open_ = false;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents, DanglingUntriaged> stub_web_contents_ = nullptr;
  raw_ptr<contextual_tasks::ContextualTasksService> contextual_tasks_service_;
  base::Uuid pending_error_page_task_id_;
  base::Uuid pending_task_id_;
  GURL pending_task_url_;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      session_handle_;
};

std::unique_ptr<KeyedService> BuildMockAimServiceEligibilityServiceInstance(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<MockAimEligibilityService> mock_aim_eligibility_service =
      std::make_unique<MockAimEligibilityService>(
          CHECK_DEREF(profile->GetPrefs()), /*template_url_service=*/nullptr,
          /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr,
          AimEligibilityService::Configuration{});

  omnibox::RuleSet* rule_set =
      mock_aim_eligibility_service->config().mutable_rule_set();
  auto* input_rule = rule_set->add_input_type_rules();
  input_rule->set_input_type(omnibox::INPUT_TYPE_LENS_IMAGE);
  input_rule->set_max_instance(1);
  input_rule->add_allowed_input_types(omnibox::INPUT_TYPE_LENS_IMAGE);

  auto* input_rule2 = rule_set->add_input_type_rules();
  input_rule2->set_input_type(omnibox::INPUT_TYPE_BROWSER_TAB);
  input_rule2->set_max_instance(5);
  input_rule2->add_allowed_input_types(omnibox::INPUT_TYPE_LENS_IMAGE);
  input_rule2->add_allowed_input_types(omnibox::INPUT_TYPE_LENS_FILE);
  input_rule2->add_allowed_input_types(omnibox::INPUT_TYPE_BROWSER_TAB);

  mock_aim_eligibility_service->config()
      .add_input_type_configs()
      ->set_input_type(omnibox::INPUT_TYPE_LENS_IMAGE);
  mock_aim_eligibility_service->config()
      .add_input_type_configs()
      ->set_input_type(omnibox::INPUT_TYPE_LENS_FILE);
  mock_aim_eligibility_service->config()
      .add_input_type_configs()
      ->set_input_type(omnibox::INPUT_TYPE_BROWSER_TAB);

  EXPECT_CALL(*mock_aim_eligibility_service, GetSearchboxConfig())
      .WillRepeatedly(testing::Return(&mock_aim_eligibility_service->config()));

  return std::move(mock_aim_eligibility_service);
}

constexpr char kDocumentWithNamedElement[] = "/select.html";

class MockLensSearchContextualizationController
    : public lens::LensSearchContextualizationController {
 public:
  explicit MockLensSearchContextualizationController(
      LensSearchController* lens_search_controller)
      : lens::LensSearchContextualizationController(lens_search_controller) {}
  ~MockLensSearchContextualizationController() override = default;

  void SetEligibility(bool eligible) { eligible_ = eligible; }

  bool GetCurrentPageContextEligibility() override { return eligible_; }

 private:
  bool eligible_ = true;
};

class TestingLensQueryFlowRouter : public lens::LensQueryFlowRouter {
 public:
  explicit TestingLensQueryFlowRouter(LensSearchController* controller)
      : LensQueryFlowRouter(controller) {
    mock_handle_ = std::make_unique<
        contextual_search::MockContextualSearchSessionHandle>();
    ON_CALL(*mock_handle_, CreateSearchUrl(::testing::_, ::testing::_))
        .WillByDefault(
            [this](std::unique_ptr<
                       contextual_search::ContextualSearchContextController::
                           CreateSearchUrlRequestInfo> Info,
                   base::OnceCallback<void(GURL)> callback) {
              GURL url = g_test_server_url;
              if (call_count_ > 0) {
                url = GURL(url.spec() +
                           "?q=" + base::NumberToString(call_count_));
              }
              call_count_++;
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(callback), url));
            });
  }
  ~TestingLensQueryFlowRouter() override = default;

  contextual_search::ContextualSearchSessionHandle*
  GetContextualSearchSessionHandle() const override {
    return mock_handle_.get();
  }

  bool IsActiveTabContextEligible() const override { return eligible_; }

  void SetEligibility(bool eligible) { eligible_ = eligible; }

 private:
  bool eligible_ = true;
  int call_count_ = 0;
  std::unique_ptr<contextual_search::MockContextualSearchSessionHandle>
      mock_handle_;
};

class MockTabContextualizationController
    : public lens::TabContextualizationController {
 public:
  explicit MockTabContextualizationController(tabs::TabInterface* tab)
      : TabContextualizationController(tab) {}
  ~MockTabContextualizationController() override = default;

  void GetPageContext(GetPageContextCallback callback) override {
    auto data = std::make_unique<lens::ContextualInputData>();
    std::move(callback).Run(std::move(data));
  }
};

class LensSearchControllerHelper : public LensSearchController {
 public:
  explicit LensSearchControllerHelper(tabs::TabInterface* tab)
      : LensSearchController(tab) {}
  ~LensSearchControllerHelper() override = default;

  bool should_route_to_contextual_tasks() const override { return true; }

  std::unique_ptr<lens::LensQueryFlowRouter> CreateLensQueryFlowRouter()
      override {
    auto router = std::make_unique<TestingLensQueryFlowRouter>(this);
    router->SetEligibility(eligibility_);
    mock_router_ = router.get();
    return router;
  }

  void SetContextEligibility(bool eligible) {
    eligibility_ = eligible;
    if (mock_controller_) {
      mock_controller_->SetEligibility(eligible);
    }
    if (mock_router_) {
      mock_router_->SetEligibility(eligible);
    }
  }

 protected:
  std::unique_ptr<lens::LensSearchContextualizationController>
  CreateLensSearchContextualizationController() override {
    auto controller =
        std::make_unique<MockLensSearchContextualizationController>(this);
    controller->SetEligibility(eligibility_);
    mock_controller_ = controller.get();
    return controller;
  }

 private:
  bool eligibility_ = true;
  raw_ptr<MockLensSearchContextualizationController> mock_controller_ = nullptr;
  raw_ptr<TestingLensQueryFlowRouter, DisableDanglingPtrDetection>
      mock_router_ = nullptr;
};

// Override the factory to create our helper.
std::unique_ptr<LensSearchController> CreateLensSearchControllerHelper(
    tabs::TabInterface& tab) {
  return std::make_unique<LensSearchControllerHelper>(&tab);
}

}  // namespace

class ContextualTasksLensInteractionBrowserTestBase
    : public InProcessBrowserTest {
 public:
  ContextualTasksLensInteractionBrowserTestBase() {
    controller_override_ =
        tabs::TabFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
            base::BindRepeating(
                [](tabs::TabInterface& tab)
                    -> std::unique_ptr<lens::TabContextualizationController> {
                  return std::make_unique<MockTabContextualizationController>(
                      &tab);
                }));
  }

  ui::UserDataFactory::ScopedOverride controller_override_;

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ContextualTasksLensInteractionBrowserTestBase::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindOnce(&BuildMockAimServiceEligibilityServiceInstance));

    contextual_tasks::ContextualTasksServiceFactory::GetInstance()
        ->SetTestingFactory(
            context, base::BindOnce([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
              auto mock_service = std::make_unique<
                  contextual_tasks::MockContextualTasksService>();
              ON_CALL(*mock_service, CreateTask()).WillByDefault([]() {
                return contextual_tasks::ContextualTask(
                    base::Uuid::GenerateRandomV4());
              });
              ON_CALL(*mock_service, CreateTaskFromUrl(::testing::_))
                  .WillByDefault([](const GURL& url) {
                    return contextual_tasks::ContextualTask(
                        base::Uuid::GenerateRandomV4());
                  });
              ON_CALL(*mock_service,
                      GetContextForTask(::testing::_, ::testing::_,
                                        ::testing::_, ::testing::_))
                  .WillByDefault(::testing::WithArg<3>(
                      [](base::OnceCallback<void(
                             std::unique_ptr<
                                 contextual_tasks::ContextualTaskContext>)>
                             callback) {
                        contextual_tasks::ContextualTask dummy_task(
                            base::Uuid::GenerateRandomV4());
                        std::move(callback).Run(
                            std::make_unique<
                                contextual_tasks::ContextualTaskContext>(
                                dummy_task));
                      }));
              return mock_service;
            }));

    contextual_tasks::ContextualTasksUiServiceFactory::GetInstance()
        ->SetTestingFactory(
            context,
            base::BindOnce(&ContextualTasksLensInteractionBrowserTestBase::
                               CreateTestingUiService,
                           base::Unretained(this)));
  }

 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    g_test_server_url = embedded_test_server()->GetURL("/empty.html");
    sessions::SessionTabHelper::CreateForWebContents(
        browser()->GetActiveTabInterface()->GetContents(),
        base::BindRepeating(
            [](content::WebContents*) -> sessions::SessionTabHelperDelegate* {
              return nullptr;
            }));
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

  void TearDownOnMainThread() override {
    testing_ui_service_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  LensSearchController* GetLensSearchController() {
    return LensSearchController::From(browser()->GetActiveTabInterface());
  }

  bool IsLensSidePanelOpen() {
    auto* coordinator =
        GetLensSearchController()->lens_overlay_side_panel_coordinator();
    return coordinator && coordinator->IsEntryShowing();
  }

  bool IsContextualTasksSidePanelOpen() {
    return testing_ui_service_ && testing_ui_service_->IsPanelOpen();
  }

  content::WebContents* GetContextualTasksWebContents() {
    if (testing_ui_service_ && testing_ui_service_->stub_web_contents()) {
      return testing_ui_service_->stub_web_contents();
    }

    auto* controller = contextual_tasks::ContextualTasksPanelController::From(
        GetBrowserWindowInterface());
    if (!controller) {
      return nullptr;
    }
    auto contents_list = controller->GetPanelWebContentsList();
    if (!contents_list.empty()) {
      return contents_list[0];
    }
    return controller->GetActiveWebContents();
  }

  content::WebContents* GetContextualTasksInnerWebContents() {
    auto* panel_contents = GetContextualTasksWebContents();
    if (!panel_contents) {
      return nullptr;
    }
    auto inner_contents = panel_contents->GetInnerWebContents();
    if (inner_contents.empty()) {
      return panel_contents;
    }
    return inner_contents[0];
  }

  bool IsContextualTasksErrorPageOpen() {
    auto* ui_service = static_cast<TestingContextualTasksUiService*>(
        contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
            browser()->profile()));
    if (ui_service && ui_service->HasPendingErrorPage()) {
      return true;
    }

    auto* contents = GetContextualTasksWebContents();
    if (!contents || !contents->GetWebUI()) {
      return false;
    }
    return content::EvalJs(contents,
                           "document.querySelector('contextual-tasks-app') ?"
                           "document.querySelector('contextual-tasks-app')."
                           "hasAttribute('is-error-page-visible_') : false;")
        .ExtractBool();
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
  BrowserWindowInterface* GetBrowserWindowInterface() {
    return browser()->GetActiveTabInterface()->GetBrowserWindowInterface();
  }

  std::unique_ptr<KeyedService> CreateTestingUiService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto service = std::make_unique<TestingContextualTasksUiService>(
        profile,
        contextual_tasks::ContextualTasksServiceFactory::GetForProfile(profile),
        IdentityManagerFactory::GetForProfile(profile),
        AimEligibilityServiceFactory::GetForProfile(profile));
    this->testing_ui_service_ = service.get();
    return service;
  }

  raw_ptr<TestingContextualTasksUiService> testing_ui_service_ = nullptr;
  base::CallbackListSubscription create_services_subscription_;
};

class ContextualTasksLensInteractionBrowserTest
    : public ContextualTasksLensInteractionBrowserTestBase {
 public:
  ContextualTasksLensInteractionBrowserTest() {
    lens_search_controller_override_ =
        tabs::TabFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
            base::BindRepeating(&CreateLensSearchControllerHelper));
  }

  void SetUp() override {
    // Mock the cluster info and upload endpoints so `ComposeboxQueryController`
    // correctly transitions background file uploads to `kUploadSuccessful`.
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (base::StartsWith(request.relative_url, "/v1/clusterinfo",
                               base::CompareCase::SENSITIVE)) {
            lens::LensOverlayServerClusterInfoResponse response;
            response.set_search_session_id("search_session_id");
            response.set_server_session_id("server_session_id");
            std::string response_string;
            response.SerializeToString(&response_string);

            auto http_response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            http_response->set_code(net::HTTP_OK);
            http_response->set_content(response_string);
            return http_response;
          }
          if (base::StartsWith(request.relative_url, "/v1/crupload",
                               base::CompareCase::SENSITIVE)) {
            lens::LensOverlayServerResponse response;
            std::string response_string;
            response.SerializeToString(&response_string);

            auto http_response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            http_response->set_code(net::HTTP_OK);
            http_response->set_content(response_string);
            return http_response;
          }
          return nullptr;
        }));
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    feature_list_.InitWithFeaturesAndParameters(
        {{contextual_tasks::kContextualTasks, {}},
         {lens::features::kLensOverlay,
          {{"endpoint-url",
            embedded_test_server()->GetURL("/v1/crupload").spec()}}},
         {lens::features::kLensOverlayContextualSearchbox,
          {{"cluster-info-endpoint-url",
            embedded_test_server()->GetURL("/v1/clusterinfo").spec()}}},
         {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}}},
        {});
    InProcessBrowserTest::SetUp();
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
  EXPECT_FALSE(IsLensSidePanelOpen());

  // Verify Overlay state is STILL showing.
  EXPECT_TRUE(controller->IsShowingUI());
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TextQueryFromComposeboxLoadsResult \
  DISABLED_TextQueryFromComposeboxLoadsResult
#else
#define MAYBE_TextQueryFromComposeboxLoadsResult \
  TextQueryFromComposeboxLoadsResult
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksLensInteractionBrowserTest,
                       MAYBE_TextQueryFromComposeboxLoadsResult) {
  // Wait for the page to be painted to prevent flakiness when screenshotting.
  WaitForPaint();

  auto* controller = GetLensSearchController();
  ASSERT_TRUE(controller);

  // Open Lens Overlay via Composebox button.
  controller->OpenLensOverlay(
      lens::LensOverlayInvocationSource::kContextualTasksComposebox);

  // Wait for the screenshot to be captured and overlay to be shown.
  WaitForOverlayToOpen(controller);
  ASSERT_TRUE(controller->IsShowingUI());

  // Issue a text query from composebox.
  controller->IssueTextSearchRequest(
      lens::LensOverlayInvocationSource::kContextualTasksComposebox,
      "test query", /*additional_query_parameters=*/{},
      AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      /*suppress_contextualization=*/false);

  // Wait for the side panel to open and the inner WebContents to be created.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetContextualTasksInnerWebContents() != nullptr; }));
  content::WebContents* inner_contents = GetContextualTasksInnerWebContents();

  // Wait for the first navigation to finish.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !inner_contents->IsLoading(); }));
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SubsequentRegionSelectionLoadsNewResult \
  DISABLED_SubsequentRegionSelectionLoadsNewResult
#else
#define MAYBE_SubsequentRegionSelectionLoadsNewResult \
  SubsequentRegionSelectionLoadsNewResult
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksLensInteractionBrowserTest,
                       MAYBE_SubsequentRegionSelectionLoadsNewResult) {
  // Wait for the page to be painted to prevent flakiness when screenshotting.
  WaitForPaint();

  auto* controller = GetLensSearchController();

  ASSERT_TRUE(controller);

  // Open Lens Overlay via App Menu.
  controller->OpenLensOverlay(lens::LensOverlayInvocationSource::kAppMenu);

  // Wait for the screenshot to be captured and overlay to be shown.
  WaitForOverlayToOpen(controller);
  ASSERT_TRUE(controller->IsShowingUI());

  // Simulate a region selection which calls IssueLensRegionRequest.
  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(0.5, 0.5, 0.1, 0.1);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kNormalized;
  controller->lens_overlay_controller()->IssueLensRegionRequestForTesting(
      region->Clone(), /*is_click=*/false);

  // Wait for the side panel to open and the inner WebContents to be created.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetContextualTasksInnerWebContents() != nullptr; }));
  content::WebContents* inner_contents = GetContextualTasksInnerWebContents();

  // Wait for the first navigation to finish (it will abort because
  // www.google.com is not resolvable in the test environment).
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !inner_contents->IsLoading(); }));

  // Create a navigation observer for the second region request
  content::TestNavigationObserver second_search_observer(inner_contents);

  // Issue the second region request
  controller->lens_overlay_controller()->IssueLensRegionRequestForTesting(
      region->Clone(), /*is_click=*/false);

  // Wait for the URL to update to reflect the second request
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return inner_contents->GetLastCommittedURL().spec().find("q=1") !=
           std::string::npos;
  }));
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SubsequentRegionSelectionFromComposeboxAddsVisualSelection \
  DISABLED_SubsequentRegionSelectionFromComposeboxAddsVisualSelection
#else
#define MAYBE_SubsequentRegionSelectionFromComposeboxAddsVisualSelection \
  SubsequentRegionSelectionFromComposeboxAddsVisualSelection
#endif
IN_PROC_BROWSER_TEST_F(
    ContextualTasksLensInteractionBrowserTest,
    MAYBE_SubsequentRegionSelectionFromComposeboxAddsVisualSelection) {
  // Wait for the page to be painted to prevent flakiness when screenshotting.
  WaitForPaint();

  auto* controller = GetLensSearchController();
  ASSERT_TRUE(controller);

  // Open Lens Overlay via App Menu.
  controller->OpenLensOverlay(lens::LensOverlayInvocationSource::kAppMenu);

  // Wait for the screenshot to be captured and overlay to be shown.
  WaitForOverlayToOpen(controller);
  ASSERT_TRUE(controller->IsShowingUI());

  // Simulate a region selection which calls IssueLensRegionRequest.
  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(0.5, 0.5, 0.1, 0.1);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kNormalized;
  controller->lens_overlay_controller()->IssueLensRegionRequestForTesting(
      region->Clone(), /*is_click=*/false);

  // Wait for the side panel to open and the inner WebContents to be created.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetContextualTasksInnerWebContents() != nullptr; }));
  content::WebContents* inner_contents = GetContextualTasksInnerWebContents();

  // Wait for the first navigation to finish (it will abort because
  // www.google.com is not resolvable in the test environment).
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !inner_contents->IsLoading(); }));

  // Simulate a click on the Lens icon in the composebox by calling
  // onLensClick_() directly.
  content::WebContents* panel_contents = GetContextualTasksWebContents();
  EXPECT_TRUE(content::ExecJs(
      panel_contents,
      "document.querySelector('contextual-tasks-app').shadowRoot."
      "querySelector('contextual-tasks-composebox').shadowRoot."
      "querySelector('cr-composebox').onLensClick_()"));

  // Wait for the invocation source to be updated to ContextualTasksComposebox.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return controller->invocation_source() ==
           lens::LensOverlayInvocationSource::kContextualTasksComposebox;
  }));

  // Issue the second region request
  controller->lens_overlay_controller()->IssueLensRegionRequestForTesting(
      region->Clone(), /*is_click=*/false);

  // Wait for the visual selection to be added to the composebox.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(
               panel_contents,
               "document.querySelector('contextual-tasks-app').shadowRoot."
               "querySelector('contextual-tasks-composebox').shadowRoot."
               "querySelector('cr-composebox').hasFiles()")
        .ExtractBool();
  }));

  // Verify that the inner contents did not navigate again. It should be the
  // same state as before the second region selection.
  EXPECT_FALSE(inner_contents->IsLoading());
  // The invocation source should remain ContextualTasksComposebox.
  EXPECT_EQ(controller->invocation_source(),
            lens::LensOverlayInvocationSource::kContextualTasksComposebox);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksLensInteractionBrowserTest,
                       OverlayClosesOnNavigation) {
  // Wait for the page to be painted to prevent flakiness when screenshotting.
  WaitForPaint();

  auto* controller = GetLensSearchController();
  ASSERT_TRUE(controller);

  // Open Lens Overlay via App Menu.
  controller->OpenLensOverlay(lens::LensOverlayInvocationSource::kAppMenu);

  // Wait for the screenshot to be captured and overlay to be shown.
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
  // remain open. It should also open the side panel.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsContextualTasksSidePanelOpen(); }));
  ASSERT_TRUE(controller->IsShowingUI());

  // Navigate to a new URL.
  const GURL new_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), new_url));

  // Verify Overlay state is kOff.
  auto* overlay_controller = controller->lens_overlay_controller();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return overlay_controller->state() == LensOverlayController::State::kOff;
  }));
  ASSERT_FALSE(controller->IsShowingUI());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksLensInteractionBrowserTest,
                       ProtectedPageTriggersErrorPage) {
  base::HistogramTester histogram_tester;
  // Wait for the page to be painted to prevent flakiness when screenshotting.
  WaitForPaint();

  auto* controller =
      static_cast<LensSearchControllerHelper*>(GetLensSearchController());
  ASSERT_TRUE(controller);
  controller->SetContextEligibility(false);

  // Open Lens Overlay via App Menu.
  controller->OpenLensOverlay(lens::LensOverlayInvocationSource::kAppMenu);

  // Wait for the screenshot to be captured and overlay to be shown.
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
  // remain open. It should also open the side panel.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsContextualTasksSidePanelOpen(); }));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsContextualTasksErrorPageOpen(); }));
  ASSERT_TRUE(controller->IsShowingUI());
  histogram_tester.ExpectUniqueSample(
      "ContextualSearch.ErrorPageShown.Lens",
      contextual_search::ContextualSearchErrorPage::kPageContextNotEligible, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksLensInteractionBrowserTest,
                       NonProtectedPageDoesNotTriggerErrorPage) {
  base::HistogramTester histogram_tester;
  // Wait for the page to be painted to prevent flakiness when screenshotting.
  WaitForPaint();

  auto* controller =
      static_cast<LensSearchControllerHelper*>(GetLensSearchController());
  ASSERT_TRUE(controller);
  controller->SetContextEligibility(true);

  // Open Lens Overlay via App Menu.
  controller->OpenLensOverlay(lens::LensOverlayInvocationSource::kAppMenu);

  // Wait for the screenshot to be captured and overlay to be shown.
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
  // remain open. It should also open the side panel.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsContextualTasksSidePanelOpen(); }));
  // The error page should not be open.
  EXPECT_FALSE(IsContextualTasksErrorPageOpen());
  ASSERT_TRUE(controller->IsShowingUI());
  histogram_tester.ExpectTotalCount("ContextualSearch.ErrorPageShown.Lens", 0);
}

class ContextualTasksRoutingEnabledTest
    : public ContextualTasksLensInteractionBrowserTestBase {
 public:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    feature_list_.InitWithFeaturesAndParameters(
        {{contextual_tasks::kContextualTasks,
          {{"ContextualTasksEnableLensInContextualTasks", "false"}}},
         {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}},
         {lens::features::kLensOverlay, {}},
         {lens::features::kLensOverlayContextualSearchbox, {}}},
        {});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksRoutingEnabledTest,
                       RoutingEnabledIfEligibleAndInvocationIsComposeBox) {
  auto* controller = GetLensSearchController();
  ASSERT_TRUE(controller);

  controller->OpenLensOverlay(
      lens::LensOverlayInvocationSource::kContextualTasksComposebox);

  EXPECT_TRUE(controller->should_route_to_contextual_tasks());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksRoutingEnabledTest,
                       RoutingDisabledIfInvocationIsNotComposeBox) {
  auto* controller = GetLensSearchController();
  ASSERT_TRUE(controller);

  controller->OpenLensOverlay(lens::LensOverlayInvocationSource::kAppMenu);

  EXPECT_FALSE(controller->should_route_to_contextual_tasks());
}

class ContextualTasksRoutingIneligibleTest
    : public ContextualTasksLensInteractionBrowserTestBase {
 public:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    feature_list_.InitWithFeatures(
        {contextual_tasks::kContextualTasks, lens::features::kLensOverlay,
         lens::features::kLensOverlayContextualSearchbox},
        {contextual_tasks::kContextualTasksForceEntryPointEligibility});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksRoutingIneligibleTest,
                       RoutingDisabledIfIneligibleEvenIfComposeBox) {
  auto* controller = GetLensSearchController();
  ASSERT_TRUE(controller);

  controller->OpenLensOverlay(
      lens::LensOverlayInvocationSource::kContextualTasksComposebox);

  EXPECT_FALSE(controller->should_route_to_contextual_tasks());
}
