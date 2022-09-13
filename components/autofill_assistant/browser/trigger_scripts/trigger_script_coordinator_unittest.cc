// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/trigger_script_coordinator.h"

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill_assistant/browser/fake_common_dependencies.h"
#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/mock_assistant_field_trial_util.h"
#include "components/autofill_assistant/browser/public/password_change/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/service/mock_service_request_sender.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "components/autofill_assistant/browser/trigger_scripts/mock_dynamic_trigger_conditions.h"
#include "components/autofill_assistant/browser/trigger_scripts/mock_static_trigger_conditions.h"
#include "components/autofill_assistant/browser/trigger_scripts/mock_trigger_script_ui_delegate.h"
#include "components/autofill_assistant/browser/ukm_test_util.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/version_info/version_info.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"

namespace autofill_assistant {

using autofill_assistant::features::kAutofillAssistantDialogOnboarding;
using autofill_assistant::features::
    kAutofillAssistantGetTriggerScriptsByHashPrefix;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;
using ::testing::ValuesIn;
using ::testing::WithArg;

std::unique_ptr<base::test::ScopedFeatureList> CreateScopedFeatureList(
    base::Feature feature,
    bool feature_enabled) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeatureState(feature, feature_enabled);
  return scoped_feature_list;
}

const char kFakeDeepLink[] = "https://example.com/q?data=test";
const char kFakeServerUrl[] =
    "https://www.fake.backend.com/trigger_script_server";
const char kFakePrivacySensitiveServerUrl[] =
    "https://www.fake.backend.com/trigger_script_by_hash_prefix_server";

class TriggerScriptCoordinatorTest : public testing::Test {
 public:
  TriggerScriptCoordinatorTest() = default;

  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());

    auto mock_request_sender =
        std::make_unique<NiceMock<MockServiceRequestSender>>();
    mock_request_sender_ = mock_request_sender.get();

    auto mock_web_controller = std::make_unique<NiceMock<MockWebController>>();
    mock_web_controller_ = mock_web_controller.get();

    auto mock_static_trigger_conditions =
        std::make_unique<NiceMock<MockStaticTriggerConditions>>();
    mock_static_trigger_conditions_ = mock_static_trigger_conditions.get();
    auto mock_dynamic_trigger_conditions =
        std::make_unique<NiceMock<MockDynamicTriggerConditions>>();
    mock_dynamic_trigger_conditions_ = mock_dynamic_trigger_conditions.get();
    auto mock_ui_delegate =
        std::make_unique<NiceMock<MockTriggerScriptUiDelegate>>();
    mock_ui_delegate_ = mock_ui_delegate.get();
    fake_platform_delegate_.trigger_script_ui_delegate_ =
        std::move(mock_ui_delegate);

    ON_CALL(*mock_static_trigger_conditions, has_results)
        .WillByDefault(Return(true));
    ON_CALL(*mock_static_trigger_conditions_, Update)
        .WillByDefault(RunOnceCallback<0>());
    ON_CALL(*mock_dynamic_trigger_conditions, HasResults)
        .WillByDefault(Return(true));
    ON_CALL(*mock_ui_delegate_, ShowTriggerScript).WillByDefault([&]() {
      coordinator_->OnTriggerScriptShown(true);
    });

    SimulateNavigateToUrl(GURL(kFakeDeepLink));
    coordinator_ = std::make_unique<TriggerScriptCoordinator>(
        fake_platform_delegate_.GetWeakPtr(), web_contents(),
        std::move(mock_web_controller), std::move(mock_request_sender),
        GURL(kFakeServerUrl), GURL(kFakePrivacySensitiveServerUrl),
        std::move(mock_static_trigger_conditions),
        std::move(mock_dynamic_trigger_conditions), &ukm_recorder_,
        web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
  }

  void TearDown() override { coordinator_.reset(); }

  content::WebContents* web_contents() { return web_contents_.get(); }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

  void SimulateWebContentsVisibilityChanged(content::Visibility visibility) {
    coordinator_->OnVisibilityChanged(visibility);
  }

  void SimulateWebContentsInteractabilityChanged(bool interactable) {
    coordinator_->OnTabInteractabilityChanged(interactable);
  }

  void SimulateNavigateToUrl(const GURL& url) {
    content::WebContentsTester::For(web_contents())->SetLastCommittedURL(url);
    content::NavigationSimulator::NavigateAndCommitFromDocument(
        url, web_contents()->GetPrimaryMainFrame());
    content::WebContentsTester::For(web_contents())->TestSetIsLoading(false);
    navigation_ids_.emplace_back(
        web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  raw_ptr<NiceMock<MockServiceRequestSender>> mock_request_sender_;
  raw_ptr<NiceMock<MockWebController>> mock_web_controller_;
  base::MockCallback<base::OnceCallback<void(
      Metrics::TriggerScriptFinishedState result,
      std::unique_ptr<TriggerContext> trigger_context,
      absl::optional<TriggerScriptProto> trigger_script)>>
      mock_callback_;
  FakeStarterPlatformDelegate fake_platform_delegate_ =
      FakeStarterPlatformDelegate(std::make_unique<FakeCommonDependencies>(
          /*identity_manager=*/nullptr));
  raw_ptr<NiceMock<MockTriggerScriptUiDelegate>> mock_ui_delegate_;
  std::unique_ptr<TriggerScriptCoordinator> coordinator_;
  raw_ptr<NiceMock<MockStaticTriggerConditions>>
      mock_static_trigger_conditions_;
  raw_ptr<NiceMock<MockDynamicTriggerConditions>>
      mock_dynamic_trigger_conditions_;
  std::vector<ukm::SourceId> navigation_ids_;
};

TEST_F(TriggerScriptCoordinatorTest, StartSendsOnlyApprovedFields) {
  base::flat_map<std::string, std::string> input_script_params{
      {"USER_EMAIL", "should.not.be.sent@chromium.org"},
      {"keyA", "valueA"},
      {"DEBUG_BUNDLE_ID", "bundle_id"},
      {"DEBUG_SOCKET_ID", "socket_id"},
      {"keyB", "valueB"},
      {"DEBUG_BUNDLE_VERSION", "socket_version"},
      {"FALLBACK_BUNDLE_ID", "fallback_id"},
      {"FALLBACK_BUNDLE_VERSION", "fallback_version"}};

  base::flat_map<std::string, std::string> expected_script_params{
      {"DEBUG_BUNDLE_ID", "bundle_id"},
      {"DEBUG_SOCKET_ID", "socket_id"},
      {"DEBUG_BUNDLE_VERSION", "socket_version"},
      {"FALLBACK_BUNDLE_ID", "fallback_id"},
      {"FALLBACK_BUNDLE_VERSION", "fallback_version"}};

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce([&](const GURL& url, const std::string& request_body,
                    ServiceRequestSender::ResponseCallback& callback,
                    RpcType rpc_type) {
        GetTriggerScriptsRequestProto request;
        ASSERT_TRUE(request.ParseFromString(request_body));
        EXPECT_THAT(request.url(), Eq(kFakeDeepLink));

        base::flat_map<std::string, std::string> params;
        for (const auto& param : request.script_parameters()) {
          params[param.name()] = param.value();
        }
        EXPECT_THAT(params, UnorderedElementsAreArray(expected_script_params));

        // Note that the all other fields are expected to be removed!
        ClientContextProto expected_client_context;
        expected_client_context.mutable_chrome()->set_chrome_version(
            version_info::GetProductNameAndVersionForUserAgent());
        expected_client_context.set_is_in_chrome_triggered(true);
        expected_client_context.set_locale("fr-CH");
        expected_client_context.set_country("CH");
        EXPECT_THAT(request.client_context(), Eq(expected_client_context));
      });

  fake_platform_delegate_.fake_common_dependencies_->locale_.assign("fr-CH");
  fake_platform_delegate_.fake_common_dependencies_->country_code_.assign("CH");
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContext>(
                          /* params = */ std::make_unique<ScriptParameters>(
                              input_script_params),
                          TriggerContext::Options(
                              /* experiment_ids = */ "1,2,4",
                              /* is_cct = */ true,
                              /* onboarding_shown = */ true,
                              /* is_direct_action = */ true,
                              /* initial_url = */ "https://www.example.com",
                              /* is_in_chrome_triggered = */ true,
                              /* is_externally_triggered = */ false,
                              /* skip_autofill_assistant_onboarding = */ false,
                              /* suppress_browsing_features = */ true)),
                      mock_callback_.Get());
}

TEST_F(TriggerScriptCoordinatorTest, StopOnBackendRequestFailed) {
  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_FORBIDDEN, "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(
      mock_callback_,
      Run(Metrics::TriggerScriptFinishedState::GET_ACTIONS_FAILED, _, _));
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptFinishedState::GET_ACTIONS_FAILED,
                     TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}}})));
}

TEST_F(TriggerScriptCoordinatorTest, StopOnParsingError) {
  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "invalid",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(
      mock_callback_,
      Run(Metrics::TriggerScriptFinishedState::GET_ACTIONS_PARSE_ERROR, _, _));
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());
  EXPECT_THAT(
      GetUkmTriggerScriptFinished(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptFinishedState::GET_ACTIONS_PARSE_ERROR,
             TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}}})));
}

TEST_F(TriggerScriptCoordinatorTest,
       StopOnParsingError_GetScriptsByHashPrefix) {
  // Disable MSBB and enable the feature that allows fetching by hash prefix
  auto feature_list = CreateScopedFeatureList(
      kAutofillAssistantGetTriggerScriptsByHashPrefix, true);
  fake_platform_delegate_.fake_common_dependencies_->msbb_enabled_ = false;

  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kFakePrivacySensitiveServerUrl), _, _,
                            RpcType::GET_TRIGGER_SCRIPTS_BY_HASH_PREFIX))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "invalid",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(
      mock_callback_,
      Run(Metrics::TriggerScriptFinishedState::GET_ACTIONS_PARSE_ERROR, _, _));

  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());
  EXPECT_THAT(
      GetUkmTriggerScriptFinished(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptFinishedState::GET_ACTIONS_PARSE_ERROR,
             TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}}})));
}

TEST_F(TriggerScriptCoordinatorTest, StopOnNoTriggerScriptsAvailable) {
  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(
      mock_callback_,
      Run(Metrics::TriggerScriptFinishedState::NO_TRIGGER_SCRIPT_AVAILABLE, _,
          _));
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());
  EXPECT_THAT(
      GetUkmTriggerScriptFinished(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptFinishedState::NO_TRIGGER_SCRIPT_AVAILABLE,
             TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}}})));
}

TEST_F(TriggerScriptCoordinatorTest, UseRegularGetTriggerScriptsIfMsbbEnabled) {
  // Even though the feature that allows fetching by hash prefix is enabled
  auto feature_list = CreateScopedFeatureList(
      kAutofillAssistantGetTriggerScriptsByHashPrefix, true);
  // MSBB is also enabled
  fake_platform_delegate_.fake_common_dependencies_->msbb_enabled_ = true;
  // so make sure that we use the GetTriggerScripts endpoint
  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .Times(1);
  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kFakeServerUrl), _, _,
                            RpcType::GET_TRIGGER_SCRIPTS_BY_HASH_PREFIX))
      .Times(0);

  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());
}

TEST_F(TriggerScriptCoordinatorTest, StartChecksStaticAndDynamicConditions) {
  GetTriggerScriptsResponseProto response;
  auto* trigger_condition_all_of = response.add_trigger_scripts()
                                       ->mutable_trigger_condition()
                                       ->mutable_all_of();
  *trigger_condition_all_of->add_conditions()->mutable_selector() =
      ToSelectorProto("#selector");
  trigger_condition_all_of->add_conditions()->mutable_is_first_time_user();
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, ClearConditions).Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              AddConditionsFromTriggerScript(response.trigger_scripts(0)))
      .Times(1);
  ON_CALL(*mock_dynamic_trigger_conditions_,
          OnUpdate(mock_web_controller_.get(), _))
      .WillByDefault(RunOnceCallback<1>());
  ON_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillByDefault(Return(false));
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillOnce(RunOnceCallback<1>());
  task_environment()->FastForwardBy(base::Seconds(1));
}

TEST_F(TriggerScriptCoordinatorTest, ShowAndHideTriggerScript) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));

  ON_CALL(*mock_dynamic_trigger_conditions_,
          OnUpdate(mock_web_controller_.get(), _))
      .WillByDefault(RunOnceCallback<1>());
  ON_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillByDefault(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  // Condition stays true, trigger script should not be hidden.
  task_environment()->FastForwardBy(base::Seconds(1));

  // Condition turns false, trigger script should be hidden.
  ON_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillByDefault(Return(false));
  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(1);
  task_environment()->FastForwardBy(base::Seconds(1));

  // Condition is true again, trigger script should be shown again.
  ON_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillByDefault(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  task_environment()->FastForwardBy(base::Seconds(1));
}

TEST_F(TriggerScriptCoordinatorTest, PauseAndResumeOnTabVisibilityChange) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillOnce(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  // When a tab becomes invisible, the trigger script is hidden and trigger
  // condition evaluation is suspended.
  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .Times(0);
  SimulateWebContentsVisibilityChanged(content::Visibility::HIDDEN);

  // When a hidden tab becomes visible again, the trigger scripts must be
  // fetched again.
  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillOnce(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  SimulateWebContentsVisibilityChanged(content::Visibility::VISIBLE);
}

TEST_F(TriggerScriptCoordinatorTest, PerformTriggerScriptActionNotNow) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  ON_CALL(*mock_dynamic_trigger_conditions_,
          OnUpdate(mock_web_controller_.get(), _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(1);
  coordinator_->PerformTriggerScriptAction(TriggerScriptProto::NOT_NOW);

  // Despite the trigger condition still being true, the trigger script is not
  // shown again until the condition has become first false and then true again.
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(0);
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(false));
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  task_environment()->FastForwardBy(base::Seconds(1));
}

TEST_F(TriggerScriptCoordinatorTest, PerformTriggerScriptActionCancelSession) {
  GetTriggerScriptsResponseProto response;
  TriggerScriptProto* script = response.add_trigger_scripts();
  *script->mutable_trigger_condition()->mutable_selector() =
      ToSelectorProto("#selector");
  script->set_trigger_ui_type(TriggerScriptProto::SHOPPING_CART_RETURNING_USER);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  ON_CALL(*mock_dynamic_trigger_conditions_,
          OnUpdate(mock_web_controller_.get(), _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  EXPECT_CALL(
      mock_callback_,
      Run(Metrics::TriggerScriptFinishedState::PROMPT_FAILED_CANCEL_SESSION, _,
          _));
  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(1);
  coordinator_->PerformTriggerScriptAction(TriggerScriptProto::CANCEL_SESSION);
  EXPECT_THAT(
      GetUkmTriggerScriptFinished(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptFinishedState::PROMPT_FAILED_CANCEL_SESSION,
             TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
}

TEST_F(TriggerScriptCoordinatorTest, PerformTriggerScriptActionCancelForever) {
  GetTriggerScriptsResponseProto response;
  TriggerScriptProto* script = response.add_trigger_scripts();
  *script->mutable_trigger_condition()->mutable_selector() =
      ToSelectorProto("#selector");
  script->set_trigger_ui_type(TriggerScriptProto::SHOPPING_CART_RETURNING_USER);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  ON_CALL(*mock_dynamic_trigger_conditions_,
          OnUpdate(mock_web_controller_.get(), _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  EXPECT_CALL(
      mock_callback_,
      Run(Metrics::TriggerScriptFinishedState::PROMPT_FAILED_CANCEL_FOREVER, _,
          _));
  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(1);
  coordinator_->PerformTriggerScriptAction(TriggerScriptProto::CANCEL_FOREVER);
  EXPECT_THAT(
      GetUkmTriggerScriptFinished(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptFinishedState::PROMPT_FAILED_CANCEL_FOREVER,
             TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
}

TEST_F(TriggerScriptCoordinatorTest, PerformTriggerScriptActionAccept) {
  GetTriggerScriptsResponseProto response;
  TriggerScriptProto* script = response.add_trigger_scripts();
  *script->mutable_trigger_condition()->mutable_selector() =
      ToSelectorProto("#selector");
  script->set_trigger_ui_type(
      TriggerScriptProto::SHOPPING_CHECKOUT_RETURNING_USER);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  ON_CALL(*mock_dynamic_trigger_conditions_,
          OnUpdate(mock_web_controller_.get(), _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  coordinator_->PerformTriggerScriptAction(TriggerScriptProto::ACCEPT);
}

TEST_F(TriggerScriptCoordinatorTest, CancelOnNavigateAway) {
  GetTriggerScriptsResponseProto response;
  response.add_additional_allowed_domains("other-example.com");
  TriggerScriptProto* script = response.add_trigger_scripts();
  *script->mutable_trigger_condition()->mutable_selector() =
      ToSelectorProto("#selector");
  script->set_trigger_ui_type(TriggerScriptProto::SHOPPING_CART_RETURNING_USER);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  ON_CALL(*mock_dynamic_trigger_conditions_,
          OnUpdate(mock_web_controller_.get(), _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  // Same-domain navigation is ok.
  EXPECT_CALL(mock_callback_, Run).Times(0);
  SimulateNavigateToUrl(GURL("https://example.com/cart"));

  // Navigating to sub-domain of original domain is ok.
  SimulateNavigateToUrl(GURL("https://subdomain.example.com/test"));

  // Navigating to whitelisted domain is ok.
  SimulateNavigateToUrl(GURL("https://other-example.com/page"));

  // Navigating to subdomain of whitelisted domain is ok.
  SimulateNavigateToUrl(GURL("https://subdomain.other-example.com/page"));

  // Navigating to non-whitelisted domain is not ok.
  EXPECT_CALL(
      mock_callback_,
      Run(Metrics::TriggerScriptFinishedState::PROMPT_FAILED_NAVIGATE, _, _));
  SimulateNavigateToUrl(GURL("https://example.different.com/page"));
  // UKM is recorded for the last seen URL that was still on a supported domain.
  EXPECT_THAT(
      GetUkmTriggerScriptFinished(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[4],
            {Metrics::TriggerScriptFinishedState::PROMPT_FAILED_NAVIGATE,
             TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
}

TEST_F(TriggerScriptCoordinatorTest, IgnoreNavigationEventsWhileNotStarted) {
  GetTriggerScriptsResponseProto response;
  TriggerScriptProto* script = response.add_trigger_scripts();
  *script->mutable_trigger_condition()->mutable_selector() =
      ToSelectorProto("#selector");
  script->set_trigger_ui_type(TriggerScriptProto::SHOPPING_CART_RETURNING_USER);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillOnce(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  // When a tab becomes invisible, navigation events are disregarded.
  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(1);
  EXPECT_CALL(mock_callback_, Run).Times(0);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .Times(0);
  SimulateWebContentsVisibilityChanged(content::Visibility::HIDDEN);
  // Note: in reality, it should be impossible to navigate on hidden tabs.
  SimulateNavigateToUrl(GURL("https://example.different.com"));
  SimulateNavigateToUrl(GURL("https://also-not-supported.com"));

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, /* response = */ "",
                                   ServiceRequestSender::ResponseInfo{}));
  // However, when the tab becomes visible again, the trigger script is
  // restarted and thus fails if the tab is still on an unsupported domain.
  EXPECT_CALL(
      mock_callback_,
      Run(Metrics::TriggerScriptFinishedState::NO_TRIGGER_SCRIPT_AVAILABLE, _,
          _));
  SimulateWebContentsVisibilityChanged(content::Visibility::VISIBLE);
  EXPECT_THAT(
      GetUkmTriggerScriptFinished(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptFinishedState::NO_TRIGGER_SCRIPT_AVAILABLE,
             TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}}})));
}

TEST_F(TriggerScriptCoordinatorTest, BottomSheetClosedWithSwipe) {
  GetTriggerScriptsResponseProto response;
  TriggerScriptProto* script = response.add_trigger_scripts();
  script->set_on_swipe_to_dismiss(TriggerScriptProto::NOT_NOW);
  script->set_trigger_ui_type(TriggerScriptProto::SHOPPING_CART_RETURNING_USER);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  ON_CALL(*mock_dynamic_trigger_conditions_,
          OnUpdate(mock_web_controller_.get(), _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(1);
  coordinator_->OnBottomSheetClosedWithSwipe();
  EXPECT_THAT(GetUkmTriggerScriptShownToUsers(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptShownToUser::RUNNING,
                     TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}},
                   {navigation_ids_[0],
                    {Metrics::TriggerScriptShownToUser::SHOWN_TO_USER,
                     TriggerScriptProto::SHOPPING_CART_RETURNING_USER}},
                   {navigation_ids_[0],
                    {Metrics::TriggerScriptShownToUser::SWIPE_DISMISSED,
                     TriggerScriptProto::SHOPPING_CART_RETURNING_USER}},
                   {navigation_ids_[0],
                    {Metrics::TriggerScriptShownToUser::NOT_NOW,
                     TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
}

TEST_F(TriggerScriptCoordinatorTest, TimeoutAfterInvisibleForTooLong) {
  GetTriggerScriptsResponseProto response;
  TriggerScriptProto* script = response.add_trigger_scripts();
  *script->mutable_trigger_condition()->mutable_selector() =
      ToSelectorProto("#selector");
  script->set_trigger_ui_type(
      TriggerScriptProto::SHOPPING_CHECKOUT_RETURNING_USER);
  response.set_trigger_condition_timeout_ms(3000);
  response.set_trigger_condition_check_interval_ms(1000);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));

  // Note: expect 4 calls: 1 initial plus 3 until timeout.
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .Times(4)
      .WillRepeatedly(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .Times(4)
      .WillRepeatedly(Return(false));
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());
  task_environment()->FastForwardBy(base::Seconds(1));
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_CALL(
      mock_callback_,
      Run(Metrics::TriggerScriptFinishedState::TRIGGER_CONDITION_TIMEOUT, _,
          _));
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_THAT(
      GetUkmTriggerScriptFinished(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptFinishedState::TRIGGER_CONDITION_TIMEOUT,
             TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}}})));
}

TEST_F(TriggerScriptCoordinatorTest, TimeoutResetsAfterTriggerScriptShown) {
  GetTriggerScriptsResponseProto response;
  TriggerScriptProto* script = response.add_trigger_scripts();
  *script->mutable_trigger_condition()->mutable_selector() =
      ToSelectorProto("#selector");
  script->set_trigger_ui_type(TriggerScriptProto::SHOPPING_CART_RETURNING_USER);
  response.set_trigger_condition_timeout_ms(3000);
  response.set_trigger_condition_check_interval_ms(1000);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillRepeatedly(RunOnceCallback<1>());

  // While the trigger script is shown, the timeout is ignored.
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillRepeatedly(Return(true));
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());
  task_environment()->FastForwardBy(base::Seconds(1));
  task_environment()->FastForwardBy(base::Seconds(1));
  task_environment()->FastForwardBy(base::Seconds(1));
  task_environment()->FastForwardBy(base::Seconds(1));

  // When the trigger script is hidden, the timeout resets.
  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillRepeatedly(Return(false));
  task_environment()->FastForwardBy(base::Seconds(1));

  task_environment()->FastForwardBy(base::Seconds(1));
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_CALL(
      mock_callback_,
      Run(Metrics::TriggerScriptFinishedState::TRIGGER_CONDITION_TIMEOUT, _,
          _));
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_THAT(
      GetUkmTriggerScriptFinished(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptFinishedState::TRIGGER_CONDITION_TIMEOUT,
             TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}}})));
}

TEST_F(TriggerScriptCoordinatorTest, NoTimeoutByDefault) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillRepeatedly(RunOnceCallback<1>());

  EXPECT_CALL(mock_callback_, Run).Times(0);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillRepeatedly(Return(false));
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());
  for (int i = 0; i < 10; ++i) {
    task_environment()->FastForwardBy(base::Seconds(1));
  }
}

TEST_F(TriggerScriptCoordinatorTest, KeyboardEventTriggersOutOfScheduleCheck) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  response.set_trigger_condition_timeout_ms(3000);
  response.set_trigger_condition_check_interval_ms(1000);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillOnce(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(false));
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  // While the next call to Update is pending, a keyboard visibility event will
  // immediately trigger an out-of-schedule update (which does not count towards
  // the timeout).
  for (int i = 0; i < 3; ++i) {
    EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
        .WillOnce(Return(false));
    EXPECT_CALL(*mock_dynamic_trigger_conditions_, OnUpdate).Times(0);
    coordinator_->OnKeyboardVisibilityChanged(true);
  }

  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .Times(3)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .Times(3)
      .WillRepeatedly(RunOnceCallback<1>());
  task_environment()->FastForwardBy(base::Seconds(1));
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_CALL(
      mock_callback_,
      Run(Metrics::TriggerScriptFinishedState::TRIGGER_CONDITION_TIMEOUT, _,
          _));
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_THAT(
      GetUkmTriggerScriptFinished(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptFinishedState::TRIGGER_CONDITION_TIMEOUT,
             TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}}})));
}

TEST_F(TriggerScriptCoordinatorTest, UrlChangeOutOfScheduleCheckPathMatch) {
  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts()->mutable_trigger_condition()->set_path_pattern(
      ".*trigger_page.*");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillOnce(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, SetURL).Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetPathPatternMatches)
      .WillOnce(Return(false));
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              SetURL(GURL("https://example.com/trigger_page")))
      .Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              GetPathPatternMatches(".*trigger_page.*"))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  SimulateNavigateToUrl(GURL("https://example.com/trigger_page"));
}

TEST_F(TriggerScriptCoordinatorTest, UrlChangeOutOfScheduleCheckDomainMatch) {
  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts()
      ->mutable_trigger_condition()
      ->set_domain_with_scheme("https://example.com");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillOnce(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, SetURL).Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetDomainAndSchemeMatches)
      .WillOnce(Return(false));
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              SetURL(GURL("https://example.com/trigger_page")))
      .Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              GetDomainAndSchemeMatches(GURL("https://example.com")))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  SimulateNavigateToUrl(GURL("https://example.com/trigger_page"));
}

TEST_F(TriggerScriptCoordinatorTest,
       UrlChangeToAnUnsupportedDomainDoesNotUpdateUrl) {
  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts()->mutable_trigger_condition()->set_path_pattern(
      ".*trigger_page.*");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillOnce(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, SetURL).Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetPathPatternMatches)
      .WillOnce(Return(false));
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  EXPECT_CALL(*mock_dynamic_trigger_conditions_, SetURL).Times(0);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetPathPatternMatches)
      .Times(0);

  EXPECT_CALL(
      mock_callback_,
      Run(Metrics::TriggerScriptFinishedState::PROMPT_FAILED_NAVIGATE, _, _));
  SimulateNavigateToUrl(GURL("http://example.different.com/page"));
}

TEST_F(TriggerScriptCoordinatorTest, OnTriggerScriptFailedToShow) {
  GetTriggerScriptsResponseProto response;
  TriggerScriptProto* script = response.add_trigger_scripts();
  script->set_trigger_ui_type(TriggerScriptProto::SHOPPING_CART_RETURNING_USER);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillRepeatedly(RunOnceCallback<1>());

  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).WillOnce([&]() {
    coordinator_->OnTriggerScriptShown(/* success = */ false);
  });
  EXPECT_CALL(mock_callback_,
              Run(Metrics::TriggerScriptFinishedState::FAILED_TO_SHOW, _, _));
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptFinishedState::FAILED_TO_SHOW,
                     TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
}

TEST_F(TriggerScriptCoordinatorTest, OnProactiveHelpSettingDisabled) {
  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts();
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillRepeatedly(RunOnceCallback<1>());

  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  EXPECT_CALL(
      mock_callback_,
      Run(Metrics::TriggerScriptFinishedState::DISABLED_PROACTIVE_HELP_SETTING,
          _, _));
  fake_platform_delegate_.proactive_help_enabled_ = false;
  SimulateWebContentsInteractabilityChanged(false);
  SimulateWebContentsInteractabilityChanged(true);
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptFinishedState::
                         DISABLED_PROACTIVE_HELP_SETTING,
                     TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}}})));
}

TEST_F(TriggerScriptCoordinatorTest, PauseAndResumeOnTabSwitch) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillOnce(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  // During tab switching, the tab becomes non-interactive. In this test, the
  // same tab is then re-selected (otherwise, the original tab's visibility
  // would change).
  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .Times(0);
  SimulateWebContentsInteractabilityChanged(/* interactable = */ false);

  // When a non-interactable tab becomes interactable again, the trigger scripts
  // must be fetched again.
  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillOnce(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, SetURL).Times(1);
  SimulateWebContentsInteractabilityChanged(/* interactable = */ true);
}

TEST_F(TriggerScriptCoordinatorTest, OnboardingShownAndAccepted) {
  GetTriggerScriptsResponseProto response;
  auto* script = response.add_trigger_scripts();
  script->set_trigger_ui_type(TriggerScriptProto::SHOPPING_CART_RETURNING_USER);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillRepeatedly(RunOnceCallback<1>());
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  fake_platform_delegate_.is_first_time_user_ = false;
  fake_platform_delegate_.show_onboarding_result_ = OnboardingResult::ACCEPTED;
  fake_platform_delegate_.show_onboarding_result_shown_ = true;
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  EXPECT_CALL(mock_callback_,
              Run(Metrics::TriggerScriptFinishedState::PROMPT_SUCCEEDED, _,
                  testing::Optional(response.trigger_scripts(0))));
  coordinator_->PerformTriggerScriptAction(TriggerScriptProto::ACCEPT);

  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called_, Eq(1));
  EXPECT_THAT(
      GetUkmTriggerScriptOnboarding(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptOnboarding::ONBOARDING_SEEN_AND_ACCEPTED,
             TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptFinishedState::PROMPT_SUCCEEDED,
                     TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
}

TEST_F(TriggerScriptCoordinatorTest,
       CancellingDialogOnboardingDoesNotStopTriggerScript) {
  auto feature_list =
      CreateScopedFeatureList(kAutofillAssistantDialogOnboarding, true);

  GetTriggerScriptsResponseProto response;
  auto* script = response.add_trigger_scripts();
  script->set_trigger_ui_type(TriggerScriptProto::SHOPPING_CART_RETURNING_USER);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillRepeatedly(RunOnceCallback<1>());
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  fake_platform_delegate_.is_first_time_user_ = false;
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  EXPECT_CALL(mock_callback_, Run).Times(0);
  fake_platform_delegate_.show_onboarding_result_ = OnboardingResult::REJECTED;
  fake_platform_delegate_.show_onboarding_result_shown_ = true;
  coordinator_->PerformTriggerScriptAction(TriggerScriptProto::ACCEPT);

  fake_platform_delegate_.show_onboarding_result_ = OnboardingResult::DISMISSED;
  fake_platform_delegate_.show_onboarding_result_shown_ = true;
  coordinator_->PerformTriggerScriptAction(TriggerScriptProto::ACCEPT);

  fake_platform_delegate_.show_onboarding_result_ =
      OnboardingResult::NAVIGATION;
  fake_platform_delegate_.show_onboarding_result_shown_ = true;
  coordinator_->PerformTriggerScriptAction(TriggerScriptProto::ACCEPT);

  EXPECT_CALL(mock_callback_,
              Run(Metrics::TriggerScriptFinishedState::PROMPT_SUCCEEDED, _,
                  testing::Optional(response.trigger_scripts(0))));
  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(0);
  fake_platform_delegate_.show_onboarding_result_ = OnboardingResult::ACCEPTED;
  fake_platform_delegate_.show_onboarding_result_shown_ = true;
  coordinator_->PerformTriggerScriptAction(TriggerScriptProto::ACCEPT);

  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called_, Eq(4));
  EXPECT_THAT(
      GetUkmTriggerScriptOnboarding(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptOnboarding::ONBOARDING_SEEN_AND_REJECTED,
             TriggerScriptProto::SHOPPING_CART_RETURNING_USER}},
           {navigation_ids_[0],
            {Metrics::TriggerScriptOnboarding::ONBOARDING_SEEN_AND_DISMISSED,
             TriggerScriptProto::SHOPPING_CART_RETURNING_USER}},
           {navigation_ids_[0],
            {Metrics::TriggerScriptOnboarding::
                 ONBOARDING_SEEN_AND_INTERRUPTED_BY_NAVIGATION,
             TriggerScriptProto::SHOPPING_CART_RETURNING_USER}},
           {navigation_ids_[0],
            {Metrics::TriggerScriptOnboarding::ONBOARDING_SEEN_AND_ACCEPTED,
             TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptFinishedState::PROMPT_SUCCEEDED,
                     TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
}

TEST_F(TriggerScriptCoordinatorTest,
       RejectingBottomSheetOnboardingStopsTriggerScript) {
  auto feature_list =
      CreateScopedFeatureList(kAutofillAssistantDialogOnboarding, false);

  GetTriggerScriptsResponseProto response;
  auto* script = response.add_trigger_scripts();
  script->set_trigger_ui_type(TriggerScriptProto::SHOPPING_CART_RETURNING_USER);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillRepeatedly(RunOnceCallback<1>());
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  EXPECT_CALL(
      mock_callback_,
      Run(Metrics::TriggerScriptFinishedState::BOTTOMSHEET_ONBOARDING_REJECTED,
          _, _));
  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(1);
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(0);
  fake_platform_delegate_.show_onboarding_result_ = OnboardingResult::REJECTED;
  fake_platform_delegate_.show_onboarding_result_shown_ = true;
  coordinator_->PerformTriggerScriptAction(TriggerScriptProto::ACCEPT);

  EXPECT_THAT(fake_platform_delegate_.num_show_onboarding_called_, Eq(1));
  EXPECT_THAT(
      GetUkmTriggerScriptOnboarding(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptOnboarding::ONBOARDING_SEEN_AND_REJECTED,
             TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptFinishedState::
                         BOTTOMSHEET_ONBOARDING_REJECTED,
                     TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
}

TEST_F(TriggerScriptCoordinatorTest, OnboardingNotShown) {
  GetTriggerScriptsResponseProto response;
  auto* script = response.add_trigger_scripts();
  script->set_trigger_ui_type(TriggerScriptProto::SHOPPING_CART_RETURNING_USER);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_.get(), _))
      .WillRepeatedly(RunOnceCallback<1>());
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  EXPECT_CALL(mock_callback_,
              Run(Metrics::TriggerScriptFinishedState::PROMPT_SUCCEEDED, _, _));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(0);
  fake_platform_delegate_.onboarding_accepted_ = true;
  fake_platform_delegate_.show_onboarding_result_shown_ = false;
  coordinator_->PerformTriggerScriptAction(TriggerScriptProto::ACCEPT);

  EXPECT_THAT(
      GetUkmTriggerScriptOnboarding(ukm_recorder_),
      ElementsAreArray(ToHumanReadableMetrics(
          {{navigation_ids_[0],
            {Metrics::TriggerScriptOnboarding::ONBOARDING_ALREADY_ACCEPTED,
             TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptFinishedState::PROMPT_SUCCEEDED,
                     TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
}

TEST_F(TriggerScriptCoordinatorTest, RecordUkmsForCurrentUrlIfPossible) {
  GetTriggerScriptsResponseProto response;
  response.add_additional_allowed_domains("other-example.com");
  TriggerScriptProto* script = response.add_trigger_scripts();
  script->mutable_trigger_condition()->set_path_pattern(".*cart.*");
  script->set_trigger_ui_type(TriggerScriptProto::SHOPPING_CART_RETURNING_USER);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  ON_CALL(*mock_dynamic_trigger_conditions_,
          OnUpdate(mock_web_controller_.get(), _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(0);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  // Navigating to cart page should trigger.
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              GetPathPatternMatches(".*cart.*"))
      .WillOnce(Return(true));
  SimulateNavigateToUrl(GURL("https://example.com/cart"));

  EXPECT_THAT(GetUkmTriggerScriptShownToUsers(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptShownToUser::RUNNING,
                     TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}},
                   {navigation_ids_[1],
                    {Metrics::TriggerScriptShownToUser::SHOWN_TO_USER,
                     TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
}

TEST_F(TriggerScriptCoordinatorTest, BackendCanOverrideScriptParameters) {
  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts();
  auto* param_1 = response.add_script_parameters();
  param_1->set_name("name_1");
  param_1->set_value("new_value_1");
  auto* param_2 = response.add_script_parameters();
  param_2->set_name("name_2");
  param_2->set_value("new_value_2");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));

  coordinator_->Start(
      GURL(kFakeDeepLink),
      std::make_unique<TriggerContext>(
          std::make_unique<ScriptParameters>(
              base::flat_map<std::string, std::string>{
                  {"name_1", "old_value_1"}, {"name_3", "value_3"}}),
          TriggerContext::Options()),
      mock_callback_.Get());
  EXPECT_THAT(coordinator_->GetTriggerContext().GetScriptParameters().ToProto(),
              UnorderedElementsAre(std::make_pair("name_1", "new_value_1"),
                                   std::make_pair("name_2", "new_value_2"),
                                   std::make_pair("name_3", "value_3")));
}

TEST_F(TriggerScriptCoordinatorTest, RegisterSyntheticFieldTrial) {
  auto mock_field_trial_util =
      std::make_unique<NiceMock<MockAssistantFieldTrialUtil>>();
  const auto* mock_field_trial_util_ptr = mock_field_trial_util.get();
  fake_platform_delegate_.field_trial_util_ = std::move(mock_field_trial_util);

  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts();
  auto* param_1 = response.add_script_parameters();
  param_1->set_name("EXPERIMENT_IDS");
  param_1->set_value("1337,1002,1001");
  auto* trial_1 = response.add_script_parameters();
  trial_1->set_name("FIELD_TRIAL_1");
  trial_1->set_value("1001");
  auto* trial_2 = response.add_script_parameters();
  trial_2->set_name("FIELD_TRIAL_2");
  trial_2->set_value("1002");

  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_field_trial_util_ptr,
              RegisterSyntheticFieldTrial(
                  base::StringPiece("AutofillAssistantTriggered"),
                  base::StringPiece("Enabled")));
  EXPECT_CALL(*mock_field_trial_util_ptr,
              RegisterSyntheticFieldTrial(
                  base::StringPiece("AutofillAssistantExperimentsTrial-1"),
                  base::StringPiece("1001")));
  EXPECT_CALL(*mock_field_trial_util_ptr,
              RegisterSyntheticFieldTrial(
                  base::StringPiece("AutofillAssistantExperimentsTrial-2"),
                  base::StringPiece("1002")));

  // Backwards compatibility.
  // TODO(b/242171397): Remove
  EXPECT_CALL(*mock_field_trial_util_ptr,
              RegisterSyntheticFieldTrial(
                  base::StringPiece("AutofillAssistantExperimentsTrial"),
                  base::StringPiece("1001")));

  coordinator_->Start(
      GURL(kFakeDeepLink),
      std::make_unique<TriggerContext>(std::make_unique<ScriptParameters>(),
                                       TriggerContext::Options()),
      mock_callback_.Get());
}

TEST_F(TriggerScriptCoordinatorTest, UiTimeoutWhileShown) {
  GetTriggerScriptsResponseProto response;
  TriggerScriptProto* script = response.add_trigger_scripts();
  script->set_trigger_ui_type(TriggerScriptProto::SHOPPING_CART_RETURNING_USER);
  script->mutable_user_interface()->set_ui_timeout_ms(2000);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));

  ON_CALL(*mock_dynamic_trigger_conditions_,
          OnUpdate(mock_web_controller_.get(), _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(0);
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(1);
  task_environment()->FastForwardBy(base::Seconds(1));

  // Reloading the page should show the prompt again, resetting the timer.
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  content::NavigationSimulator::Reload(web_contents());
  navigation_ids_.emplace_back(
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());

  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(0);
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(1);
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_THAT(GetUkmTriggerScriptShownToUsers(ukm_recorder_),
              UnorderedElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptShownToUser::RUNNING,
                     TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}},
                   {navigation_ids_[0],
                    {Metrics::TriggerScriptShownToUser::SHOWN_TO_USER,
                     TriggerScriptProto::SHOPPING_CART_RETURNING_USER}},
                   {navigation_ids_[0],
                    {Metrics::TriggerScriptShownToUser::UI_TIMEOUT,
                     TriggerScriptProto::SHOPPING_CART_RETURNING_USER}},
                   {navigation_ids_[1],
                    {Metrics::TriggerScriptShownToUser::SHOWN_TO_USER,
                     TriggerScriptProto::SHOPPING_CART_RETURNING_USER}},
                   {navigation_ids_[1],
                    {Metrics::TriggerScriptShownToUser::UI_TIMEOUT,
                     TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
}

TEST_F(TriggerScriptCoordinatorTest, UiTimeoutInterruptedByCancelPopup) {
  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts()->mutable_user_interface()->set_ui_timeout_ms(
      2000);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));

  ON_CALL(*mock_dynamic_trigger_conditions_,
          OnUpdate(mock_web_controller_.get(), _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).WillOnce([&]() {
    coordinator_->OnTriggerScriptShown(true);
    coordinator_->PerformTriggerScriptAction(
        TriggerScriptProto::SHOW_CANCEL_POPUP);
  });
  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(0);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  // Showing the cancel popup should have disabled the timer.
  task_environment()->FastForwardBy(base::Seconds(5));

  // As long as the prompt is not hidden, the timer continues to be disabled.
  content::NavigationSimulator::Reload(web_contents());
  task_environment()->FastForwardBy(base::Seconds(1));
  task_environment()->FastForwardBy(base::Seconds(1));
}

TEST_F(TriggerScriptCoordinatorTest, UiTimeoutInterruptedByOnboarding) {
  // Specify a mock onboarding callback to simulate that the onboarding is shown
  // for a longer period of time.
  base::MockCallback<base::OnceCallback<void(
      base::OnceCallback<void(bool, OnboardingResult)>)>>
      mock_onboarding_callback;
  fake_platform_delegate_.on_show_onboarding_callback_ =
      mock_onboarding_callback.Get();

  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts()->mutable_user_interface()->set_ui_timeout_ms(
      2000);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));

  ON_CALL(*mock_dynamic_trigger_conditions_,
          OnUpdate(mock_web_controller_.get(), _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).WillOnce([&]() {
    coordinator_->OnTriggerScriptShown(true);
    coordinator_->PerformTriggerScriptAction(TriggerScriptProto::ACCEPT);
  });
  EXPECT_CALL(mock_onboarding_callback, Run);
  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(0);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  // Showing the onboarding should have disabled the timer.
  task_environment()->FastForwardBy(base::Seconds(5));
}

TEST_F(TriggerScriptCoordinatorTest, UiTimeoutInterruptedBySkipSession) {
  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts()->mutable_user_interface()->set_ui_timeout_ms(
      2000);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));

  ON_CALL(*mock_dynamic_trigger_conditions_,
          OnUpdate(mock_web_controller_.get(), _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).WillOnce([&]() {
    coordinator_->OnTriggerScriptShown(true);
    coordinator_->PerformTriggerScriptAction(
        TriggerScriptProto::CANCEL_SESSION);
  });
  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(1);
  EXPECT_CALL(
      mock_callback_,
      Run(Metrics::TriggerScriptFinishedState::PROMPT_FAILED_CANCEL_SESSION, _,
          _));
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  // Just to check that the timer has gone properly out-of-scope along with the
  // coordinator and nothing blows up.
  coordinator_.reset();
  task_environment()->FastForwardBy(base::Seconds(5));
}

TEST_F(TriggerScriptCoordinatorTest, UiTimeoutInterruptedByNotNow) {
  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts()->mutable_user_interface()->set_ui_timeout_ms(
      2000);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));

  ON_CALL(*mock_dynamic_trigger_conditions_,
          OnUpdate(mock_web_controller_.get(), _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).WillOnce([&]() {
    coordinator_->OnTriggerScriptShown(true);
    coordinator_->PerformTriggerScriptAction(TriggerScriptProto::NOT_NOW);
  });
  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  // Time that passes while the prompt is hidden is irrelevant
  // (HideTriggerScript is not called again).
  task_environment()->FastForwardBy(base::Seconds(5));

  // Reloading the page will show the prompt again and reset the timeout.
  EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).WillOnce([&]() {
    coordinator_->OnTriggerScriptShown(true);
  });
  content::NavigationSimulator::Reload(web_contents());
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(1);
  task_environment()->FastForwardBy(base::Seconds(1));
}

TEST_F(TriggerScriptCoordinatorTest, StoppingTwiceDoesNotCrash) {
  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_FORBIDDEN, "",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(*mock_ui_delegate_, Detach).Times(2);
  EXPECT_CALL(*mock_ui_delegate_, HideTriggerScript).Times(0);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  // Stopping coordinator after it was already stopped by a failed request.
  coordinator_->Stop(
      Metrics::TriggerScriptFinishedState::CCT_TO_TAB_NOT_SUPPORTED);

  // Only the first event is logged (and nothing crashed).
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptFinishedState::GET_ACTIONS_FAILED,
                     TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}}})));
}

TEST_F(TriggerScriptCoordinatorTest, RecordTriggerConditionEvaluationTime) {
  GetTriggerScriptsResponseProto response;
  response.set_trigger_condition_check_interval_ms(1000);
  response.add_trigger_scripts();
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(*mock_dynamic_trigger_conditions_, OnUpdate)
      .WillOnce(WithArg<1>([&](auto& callback) {
        task_environment()->FastForwardBy(base::Milliseconds(700));
        std::move(callback).Run();
      }))
      .WillOnce(WithArg<1>([&](auto& callback) {
        task_environment()->FastForwardBy(base::Milliseconds(300));
        std::move(callback).Run();
      }));

  // Start will immediately run the first trigger condition evaluation.
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  // Run the second scheduled trigger condition evaluation.
  task_environment()->FastForwardBy(base::Seconds(1));

  // Run out-of-schedule trigger condition evaluation (should not be recorded).
  coordinator_->OnKeyboardVisibilityChanged(true);

  EXPECT_THAT(
      GetUkmTriggerConditionEvaluationTime(ukm_recorder_),
      ElementsAreArray(std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>{
          {navigation_ids_[0], {{kTriggerConditionTimingMs, 700}}},
          {navigation_ids_[0], {{kTriggerConditionTimingMs, 300}}}}));
}

TEST_F(TriggerScriptCoordinatorTest, RecordIfPrimaryPageFailed) {
  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts();
  std::string serialized_response;
  response.SerializeToString(&serialized_response);
  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));

  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  // Navigate to the primary page.
  EXPECT_CALL(mock_callback_,
              Run(Metrics::TriggerScriptFinishedState::NAVIGATION_ERROR, _, _))
      .Times(1);
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kFakeDeepLink), web_contents());
  simulator->Fail(net::ERR_TIMED_OUT);
  simulator->CommitErrorPage();

  // UKM should be recorded by the primary page's fail response.
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptFinishedState::NAVIGATION_ERROR,
                     TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}}})));
}

class TriggerScriptCoordinatorPrerenderTest
    : public TriggerScriptCoordinatorTest {
 public:
  TriggerScriptCoordinatorPrerenderTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kPrerender2},
        // Disable the memory requirement of Prerender2 so the test can run on
        // any bot.
        {blink::features::kPrerender2MemoryControls});
  }

  ~TriggerScriptCoordinatorPrerenderTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TriggerScriptCoordinatorPrerenderTest, DoNotRecordIfPrerenderingFailed) {
  content::test::ScopedPrerenderWebContentsDelegate web_contents_delegate(
      *web_contents());

  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts();
  std::string serialized_response;
  response.SerializeToString(&serialized_response);
  EXPECT_CALL(
      *mock_request_sender_,
      OnSendRequest(GURL(kFakeServerUrl), _, _, RpcType::GET_TRIGGER_SCRIPTS))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(mock_callback_, Run).Times(0);
  coordinator_->Start(GURL(kFakeDeepLink), std::make_unique<TriggerContext>(),
                      mock_callback_.Get());

  // Start prerendering a page.
  auto simulator = content::WebContentsTester::For(web_contents())
                       ->AddPrerenderAndStartNavigation(GURL(kFakeDeepLink));
  simulator->Fail(net::ERR_TIMED_OUT);
  simulator->CommitErrorPage();

  // UKM should not be recorded by the prerendering's fail response.
  EXPECT_THAT(GetUkmTriggerScriptFinished(ukm_recorder_), IsEmpty());
}

class TriggerScriptCoordinatorParameterizedTest
    : public TriggerScriptCoordinatorTest,
      public testing::WithParamInterface<std::pair<std::string, bool>> {
 public:
  void SetUp() override {
    TriggerScriptCoordinatorTest::SetUp();
    current_parameterized_url_ = GetParam().first;
    is_matching_url = GetParam().second;
  }

  void TearDown() override { TriggerScriptCoordinatorTest::TearDown(); }

 protected:
  std::string current_parameterized_url_;
  bool is_matching_url;
};

TEST_P(TriggerScriptCoordinatorParameterizedTest,
       GetScriptsByHashPrefix_NonMsbbMatchCurrentDomainByUrlHost) {
  // Disable MSBB and enable the feature that allows fetching by hash prefix
  auto feature_list = CreateScopedFeatureList(
      kAutofillAssistantGetTriggerScriptsByHashPrefix, true);
  fake_platform_delegate_.fake_common_dependencies_->msbb_enabled_ = false;

  // Create the GetTriggerScriptsByHashPrefixProtoResponse
  GetTriggerScriptsByHashPrefixResponseProto response;
  // nike.com, first time user, will *not* get matched
  auto* nikeMatchInfo = response.add_match_info();
  nikeMatchInfo->set_domain("https://nike.com/");
  auto* nikeTriggerScript =
      nikeMatchInfo->mutable_trigger_scripts_response()->add_trigger_scripts();
  nikeTriggerScript->set_trigger_ui_type(
      TriggerScriptProto::SHOPPING_CART_FIRST_TIME_USER);

  // example.com, returning user, will get matched
  auto* exampleMatchInfo = response.add_match_info();
  exampleMatchInfo->set_domain(current_parameterized_url_);
  auto* exampleTriggerScript =
      exampleMatchInfo->mutable_trigger_scripts_response()
          ->add_trigger_scripts();
  exampleTriggerScript->set_trigger_ui_type(
      TriggerScriptProto::SHOPPING_CART_RETURNING_USER);

  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_,
              OnSendRequest(GURL(kFakePrivacySensitiveServerUrl), _, _,
                            RpcType::GET_TRIGGER_SCRIPTS_BY_HASH_PREFIX))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response,
                                   ServiceRequestSender::ResponseInfo{}));

  if (is_matching_url) {
    EXPECT_CALL(*mock_dynamic_trigger_conditions_,
                OnUpdate(mock_web_controller_.get(), _))
        .WillRepeatedly(RunOnceCallback<1>());
    EXPECT_CALL(*mock_ui_delegate_, ShowTriggerScript).Times(1);
  }

  fake_platform_delegate_.is_first_time_user_ = false;
  fake_platform_delegate_.show_onboarding_result_ = OnboardingResult::ACCEPTED;
  fake_platform_delegate_.show_onboarding_result_shown_ = true;

  base::flat_map<std::string, std::string> input_script_params{
      {"DEBUG_BUNDLE_ID", "bundle_id"},
      {"DEBUG_SOCKET_ID", "socket_id"},
      {"DEBUG_BUNDLE_VERSION", "socket_version"},
      {"FALLBACK_BUNDLE_ID", "fallback_id"},
      {"FALLBACK_BUNDLE_VERSION", "fallback_version"}};

  // The initial URL from which we initiate the check
  std::string deep_link = "https://example.com/q?data=test";
  coordinator_->Start(GURL(deep_link),
                      std::make_unique<TriggerContext>(
                          /* params = */ std::make_unique<ScriptParameters>(
                              input_script_params),
                          TriggerContext::Options(
                              /* experiment_ids = */ "1,2,4",
                              /* is_cct = */ true,
                              /* onboarding_shown = */ true,
                              /* is_direct_action = */ true,
                              /* initial_url = */ "https://not-example.com/",
                              /* is_in_chrome_triggered = */ true,
                              /* is_externally_triggered = */ false,
                              /* skip_autofill_assistant_onboarding = */ false,
                              /* suppress_browsing_features = */ true)),
                      mock_callback_.Get());

  if (!is_matching_url) {
    EXPECT_THAT(GetUkmTriggerScriptShownToUsers(ukm_recorder_), SizeIs(0));
    EXPECT_THAT(
        GetUkmTriggerScriptFinished(ukm_recorder_),
        ElementsAreArray(ToHumanReadableMetrics(
            {{navigation_ids_[0],
              {Metrics::TriggerScriptFinishedState::NO_TRIGGER_SCRIPT_AVAILABLE,
               TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}}})));
    return;
  }
  EXPECT_THAT(GetUkmTriggerScriptShownToUsers(ukm_recorder_),
              ElementsAreArray(ToHumanReadableMetrics(
                  {{navigation_ids_[0],
                    {Metrics::TriggerScriptShownToUser::RUNNING,
                     TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE}},
                   {navigation_ids_[0],
                    {Metrics::TriggerScriptShownToUser::SHOWN_TO_USER,
                     TriggerScriptProto::SHOPPING_CART_RETURNING_USER}}})));
}

const std::vector<std::pair<std::string, bool>> domains(
    {{"https://example.com", true},
     {"https://example.com/", true},
     {"https://example.com/cart", true},
     {"http://example.com", false},  // different scheme (http vs https)
     {"https://adidas.com", false},
     {"https://not-example.com", false}});

INSTANTIATE_TEST_SUITE_P(TriggerScriptCoordinatorParameterizedTestSuite,
                         TriggerScriptCoordinatorParameterizedTest,
                         ValuesIn(domains));

}  // namespace autofill_assistant
