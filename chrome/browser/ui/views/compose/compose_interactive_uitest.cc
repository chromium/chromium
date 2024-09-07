// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>

#include "base/test/bind.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/compose/compose_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/config.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/interaction/element_tracker_views.h"

using testing::_;
using testing::An;
using testing::Return;
using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kContentPageTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kComposeWebContents);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementReadyEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementChangedEvent);

const char kTestPageDomain[] = "a.test";
const char kTestPageUrl[] = "/compose/compose_happy_path.html";

const DeepQuery kFirstRunOkButton = {"compose-app", "#firstRunOkButton"};
const DeepQuery kSubmitButton = {"compose-app", "#submitButton"};
const DeepQuery kAcceptButton = {"compose-app", "#acceptButton"};
const DeepQuery kComposeTextArea = {"compose-app", "compose-textarea"};
const DeepQuery kTextarea = {"#elem1"};

}  // namespace

// TODO(b/319449485): Enable this test on Mac.  In development it timed out for
// unknown reasons (possibly related to the Mac handling of context menus.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ComposeInteractiveUiTest DISABLED_ComposeInteractiveUiTest
#else
#define MAYBE_ComposeInteractiveUiTest ComposeInteractiveUiTest
#endif

class MAYBE_ComposeInteractiveUiTest : public InteractiveBrowserTest {
 public:
  MAYBE_ComposeInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        {compose::features::kEnableCompose,
         optimization_guide::features::kOptimizationGuideModelExecution},
        {});
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&MAYBE_ComposeInteractiveUiTest::
                                        OnWillCreateBrowserContextServices));
  }
  ~MAYBE_ComposeInteractiveUiTest() override = default;

  /////////////////////////////////////////
  // Compose interactive UI test step helpers.
  InteractiveTestApi::StepBuilder PressJsButton(
      const ui::ElementIdentifier web_contents_id,
      const DeepQuery& button_query) {
    // This can close/navigate the current page, so don't wait for success.
    return ExecuteJsAt(web_contents_id, button_query, "(btn) => btn.click()",
                       ExecuteJsMode::kFireAndForget);
  }

  InteractiveTestApi::MultiStep WaitForElementToLoad(
      const DeepQuery& element_query) {
    StateChange element_loaded;
    element_loaded.event = kElementReadyEvent;
    element_loaded.type = StateChange::Type::kExists;
    element_loaded.where = element_query;
    return WaitForStateChange(kContentPageTabId, std::move(element_loaded));
  }

  InteractiveTestApi::MultiStep WaitForElementValueToIncludeCucumbers(
      const DeepQuery& element_query) {
    StateChange element_changed;
    element_changed.event = kElementChangedEvent;
    element_changed.type = StateChange::Type::kExistsAndConditionTrue;
    element_changed.where = element_query;
    element_changed.test_function =
        R"((el) => (el.value.includes("Cucumbers")))";
    return WaitForStateChange(kContentPageTabId, std::move(element_changed));
  }

  InteractiveTestApi::MultiStep OpenCompose() {
    return Steps(
        WaitForElementToLoad(kTextarea),
        MoveMouseTo(kContentPageTabId, kTextarea),
        ClickMouse(ui_controls::RIGHT),
        WaitForShow(RenderViewContextMenu::kComposeMenuItem),
        // Required to fully render the menu before selection.
        SelectMenuItem(RenderViewContextMenu::kComposeMenuItem),
        WaitForShow(ComposeDialogView::kComposeDialogId),
        InstrumentNonTabWebView(kComposeWebContents, kComposeWebviewElementId));
  }

  InteractiveTestApi::MultiStep AcceptFRE() {
    return Steps(PressJsButton(kComposeWebContents, kFirstRunOkButton));
  }

  InteractiveTestApi::MultiStep RequestCompose() {
    return Steps(ExecuteJsAt(kComposeWebContents, kComposeTextArea,
                             "(ct) => { ct.value = 'Abra Cadabra 1,2,3'; }"),
                 PressJsButton(kComposeWebContents, kSubmitButton));
  }

  InteractiveTestApi::MultiStep AcceptComposeResult() {
    return Steps(PressJsButton(kComposeWebContents, kAcceptButton));
  }

  InteractiveTestApi::StepBuilder MakePrimaryAccountAvailable() {
    return Do([this]() {
      identity_test_environment_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable("test@example.com",
                                        signin::ConsentLevel::kSync);
    });
  }

  /////////////////////////////////////////
  // Setup tasks
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    compose::ResetConfigForTesting();
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data for cross_site_iframe_factory.html
    https_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    content::SetupCrossSiteRedirector(https_server());
    net::test_server::RegisterDefaultHandlers(https_server());

    ASSERT_TRUE(https_server()->Start());

    SetUpOptimizationGuide();
    SetUpAccount();
  }

  void TearDownOnMainThread() override {
    base::RunLoop().RunUntilIdle();
    mock_optimization_guide_keyed_service_ = nullptr;
    testing::Mock::VerifyAndClear(&session());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  // Sets up the guide to not make a decision about Compose per-page, and
  // to return a response of "Cucumbers" whenever ExecuteModel is called.
  void SetUpOptimizationGuide() {
    mock_optimization_guide_keyed_service_ =
        static_cast<testing::NiceMock<MockOptimizationGuideKeyedService>*>(
            OptimizationGuideKeyedServiceFactory::GetForProfile(
                browser()->profile()));
    ASSERT_TRUE(mock_optimization_guide_keyed_service_);
    ON_CALL(
        *mock_optimization_guide_keyed_service_,
        CanApplyOptimization(
            _, _, An<optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillByDefault(testing::Invoke(
            [](const GURL&, optimization_guide::proto::OptimizationType type,
               optimization_guide::OptimizationGuideDecisionCallback callback) {
              std::move(callback).Run(
                  optimization_guide::OptimizationGuideDecision::kTrue,
                  optimization_guide::OptimizationMetadata());
            }));
    ON_CALL(*mock_optimization_guide_keyed_service_,
            CanApplyOptimization(
                _, _, An<optimization_guide::OptimizationMetadata*>()))
        .WillByDefault(
            Return(optimization_guide::OptimizationGuideDecision::kTrue));
    ON_CALL(*mock_optimization_guide_keyed_service_,
            ShouldFeatureBeCurrentlyEnabledForUser)
        .WillByDefault(Return(true));
    ON_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
        .WillByDefault([&] {
          return std::make_unique<optimization_guide::MockSessionWrapper>(
              &session());
        });
    ON_CALL(session(), ExecuteModel(_, _))
        .WillByDefault(testing::WithArg<1>(testing::Invoke(
            [&](optimization_guide::
                    OptimizationGuideModelExecutionResultStreamingCallback
                        callback) {
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      std::move(callback),
                      OptimizationGuideStreamingResult(
                          ComposeResponse(true, "Cucumbers"), true, false,
                          std::make_unique<
                              optimization_guide::ModelQualityLogEntry>(
                              std::make_unique<optimization_guide::proto::
                                                   LogAiDataRequest>(),
                              nullptr))));
            })));
  }

  void SetUpAccount() {
    // Turn on MSBB.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  }

  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  net::EmbeddedTestServer* https_server() { return &https_server_; }
  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }
  optimization_guide::MockSession& session() { return session_; }

 private:
  static void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              testing::NiceMock<MockOptimizationGuideKeyedService>>();
        }));
  }

  optimization_guide::StreamingResponse OptimizationGuideResponse(
      const optimization_guide::proto::ComposeResponse compose_response,
      bool is_complete = true) {
    constexpr char kTypeURL[] =
        "type.googleapis.com/optimization_guide.proto.ComposeResponse";
    optimization_guide::proto::Any any;
    any.set_type_url(kTypeURL);
    compose_response.SerializeToString(any.mutable_value());
    return optimization_guide::StreamingResponse{
        .response = any,
        .is_complete = is_complete,
    };
  }

  optimization_guide::OptimizationGuideModelStreamingExecutionResult
  OptimizationGuideStreamingResult(
      const optimization_guide::proto::ComposeResponse compose_response,
      bool is_complete = true,
      bool provided_by_on_device = false,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry =
          nullptr) {
    return optimization_guide::OptimizationGuideModelStreamingExecutionResult(
        base::ok(OptimizationGuideResponse(compose_response, is_complete)),
        provided_by_on_device, std::move(log_entry));
  }

  optimization_guide::proto::ComposeResponse ComposeResponse(
      bool ok,
      std::string output) {
    optimization_guide::proto::ComposeResponse response;
    response.set_output(ok ? output : "");
    return response;
  }

  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::CallbackListSubscription subscription_;
  raw_ptr<testing::NiceMock<MockOptimizationGuideKeyedService>>
      mock_optimization_guide_keyed_service_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  testing::NiceMock<optimization_guide::MockSession> session_;
};

// Flaky on all platforms: https://crbug.com/1517430
IN_PROC_BROWSER_TEST_F(MAYBE_ComposeInteractiveUiTest,
                       DISABLED_OpenAndCloseCompose) {
  RunTestSequence(
      MakePrimaryAccountAvailable(), InstrumentTab(kContentPageTabId),
      NavigateWebContents(
          kContentPageTabId,
          https_server()->GetURL(kTestPageDomain, kTestPageUrl)),
      WaitForWebContentsReady(
          kContentPageTabId,
          https_server()->GetURL(kTestPageDomain, kTestPageUrl)),
      OpenCompose(), AcceptFRE(), RequestCompose(), AcceptComposeResult(),
      WaitForElementValueToIncludeCucumbers(kTextarea));
}
