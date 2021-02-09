// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/trigger_script_coordinator.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/service/mock_service_request_sender.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "components/autofill_assistant/browser/trigger_scripts/mock_dynamic_trigger_conditions.h"
#include "components/autofill_assistant/browser/trigger_scripts/mock_static_trigger_conditions.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/version_info/version_info.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::NaggyMock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::UnorderedElementsAreArray;

std::unique_ptr<base::test::ScopedFeatureList> CreateScopedFeatureList(
    bool dialog_onboarding) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeatureState(
      autofill_assistant::features::kAutofillAssistantDialogOnboarding,
      dialog_onboarding);
  return scoped_feature_list;
}

class MockObserver : public TriggerScriptCoordinator::Observer {
 public:
  MOCK_METHOD1(OnTriggerScriptShown, void(const TriggerScriptUIProto& proto));
  MOCK_METHOD0(OnTriggerScriptHidden, void());
  MOCK_METHOD1(OnTriggerScriptFinished,
               void(Metrics::LiteScriptFinishedState state));
  MOCK_METHOD1(OnVisibilityChanged, void(bool visible));
  MOCK_METHOD1(OnOnboardingRequested, void(bool use_dialog_onboarding));
};

const char kFakeDeepLink[] = "https://example.com/q?data=test";
const char kFakeServerUrl[] =
    "https://www.fake.backend.com/trigger_script_server";

class TriggerScriptCoordinatorTest : public content::RenderViewHostTestHarness {
 public:
  TriggerScriptCoordinatorTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~TriggerScriptCoordinatorTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
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

    ON_CALL(*mock_static_trigger_conditions, has_results)
        .WillByDefault(Return(true));
    ON_CALL(*mock_dynamic_trigger_conditions, HasResults)
        .WillByDefault(Return(true));

    coordinator_ = std::make_unique<TriggerScriptCoordinator>(
        web_contents(), &mock_website_login_manager_,
        mock_is_first_time_user_callback_.Get(), std::move(mock_web_controller),
        std::move(mock_request_sender), GURL(kFakeServerUrl),
        std::move(mock_static_trigger_conditions),
        std::move(mock_dynamic_trigger_conditions), &ukm_recorder_);
    coordinator_->AddObserver(&mock_observer_);

    SimulateNavigateToUrl(GURL(kFakeDeepLink));
  }

  void TearDown() override { coordinator_->RemoveObserver(&mock_observer_); }

  void SimulateWebContentsVisibilityChanged(content::Visibility visibility) {
    coordinator_->OnVisibilityChanged(visibility);
  }

  void SimulateWebContentsInteractabilityChanged(bool interactable) {
    coordinator_->OnTabInteractabilityChanged(interactable);
  }

  void SimulateNavigateToUrl(const GURL& url) {
    content::WebContentsTester::For(web_contents())->SetLastCommittedURL(url);
    content::NavigationSimulator::NavigateAndCommitFromDocument(
        url, web_contents()->GetMainFrame());
    content::WebContentsTester::For(web_contents())->TestSetIsLoading(false);
  }

  void AssertRecordedFinishedState(Metrics::LiteScriptFinishedState state) {
    auto entries =
        ukm_recorder_.GetEntriesByName("AutofillAssistant.LiteScriptFinished");
    ASSERT_THAT(entries.size(), Eq(1u));
    ukm_recorder_.ExpectEntrySourceHasUrl(
        entries[0], web_contents()->GetLastCommittedURL());
    EXPECT_EQ(*ukm_recorder_.GetEntryMetric(entries[0], "LiteScriptFinished"),
              static_cast<int64_t>(state));
  }

  void AssertRecordedShownToUserState(Metrics::LiteScriptShownToUser state,
                                      int expected_times) {
    auto entries = ukm_recorder_.GetEntriesByName(
        "AutofillAssistant.LiteScriptShownToUser");
    ukm_recorder_.ExpectEntrySourceHasUrl(
        entries[0], web_contents()->GetLastCommittedURL());
    int actual_times = 0;
    for (const auto* entry : entries) {
      if (*ukm_recorder_.GetEntryMetric(entry, "LiteScriptShownToUser") ==
          static_cast<int64_t>(state)) {
        actual_times++;
      }
    }
    EXPECT_EQ(expected_times, actual_times);
  }

  void AssertRecordedLiteScriptOnboardingState(
      Metrics::LiteScriptOnboarding state,
      int expected_times) {
    auto entries = ukm_recorder_.GetEntriesByName(
        "AutofillAssistant.LiteScriptOnboarding");
    ukm_recorder_.ExpectEntrySourceHasUrl(
        entries[0], web_contents()->GetLastCommittedURL());
    int actual_times = 0;
    for (const auto* entry : entries) {
      if (*ukm_recorder_.GetEntryMetric(entry, "LiteScriptOnboarding") ==
          static_cast<int64_t>(state)) {
        actual_times++;
      }
    }
    EXPECT_EQ(expected_times, actual_times);
  }

 protected:
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  NiceMock<MockServiceRequestSender>* mock_request_sender_;
  NiceMock<MockWebController>* mock_web_controller_;
  NiceMock<MockWebsiteLoginManager> mock_website_login_manager_;
  base::MockCallback<base::RepeatingCallback<bool(void)>>
      mock_is_first_time_user_callback_;
  NaggyMock<MockObserver> mock_observer_;
  std::unique_ptr<TriggerScriptCoordinator> coordinator_;
  NiceMock<MockStaticTriggerConditions>* mock_static_trigger_conditions_;
  NiceMock<MockDynamicTriggerConditions>* mock_dynamic_trigger_conditions_;
};

TEST_F(TriggerScriptCoordinatorTest, StartSendsOnlyApprovedFields) {
  std::map<std::string, std::string> input_script_params{
      {"keyA", "valueA"},
      {"DEBUG_BUNDLE_ID", "bundle_id"},
      {"DEBUG_SOCKET_ID", "socket_id"},
      {"keyB", "valueB"},
      {"DEBUG_BUNDLE_VERSION", "socket_version"}};

  std::map<std::string, std::string> expected_script_params{
      {"DEBUG_BUNDLE_ID", "bundle_id"},
      {"DEBUG_SOCKET_ID", "socket_id"},
      {"DEBUG_BUNDLE_VERSION", "socket_version"}};

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce([&](const GURL& url, const std::string& request_body,
                    ServiceRequestSender::ResponseCallback& callback) {
        GetTriggerScriptsRequestProto request;
        ASSERT_TRUE(request.ParseFromString(request_body));
        EXPECT_THAT(request.url(), Eq(kFakeDeepLink));

        std::map<std::string, std::string> params;
        for (const auto& param : request.debug_script_parameters()) {
          params[param.name()] = param.value();
        }
        EXPECT_THAT(params, UnorderedElementsAreArray(expected_script_params));

        ClientContextProto expected_client_context;
        expected_client_context.mutable_chrome()->set_chrome_version(
            version_info::GetProductNameAndVersionForUserAgent());
        EXPECT_THAT(request.client_context(), Eq(expected_client_context));
      });

  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>(
                          /* params = */ input_script_params,
                          /* exp = */ "1,2,4"));
}

TEST_F(TriggerScriptCoordinatorTest, StopOnBackendRequestFailed) {
  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_FORBIDDEN, ""));
  EXPECT_CALL(
      mock_observer_,
      OnTriggerScriptFinished(
          Metrics::LiteScriptFinishedState::LITE_SCRIPT_GET_ACTIONS_FAILED));
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());
  AssertRecordedFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_GET_ACTIONS_FAILED);
}

TEST_F(TriggerScriptCoordinatorTest, StopOnParsingError) {
  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "invalid"));
  EXPECT_CALL(mock_observer_,
              OnTriggerScriptFinished(Metrics::LiteScriptFinishedState::
                                          LITE_SCRIPT_GET_ACTIONS_PARSE_ERROR));
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());
  AssertRecordedFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_GET_ACTIONS_PARSE_ERROR);
}

TEST_F(TriggerScriptCoordinatorTest, StopOnNoTriggerScriptsAvailable) {
  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, ""));
  EXPECT_CALL(mock_observer_, OnTriggerScriptFinished(
                                  Metrics::LiteScriptFinishedState::
                                      LITE_SCRIPT_NO_TRIGGER_SCRIPT_AVAILABLE));
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());
  AssertRecordedFinishedState(Metrics::LiteScriptFinishedState::
                                  LITE_SCRIPT_NO_TRIGGER_SCRIPT_AVAILABLE);
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

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, ClearSelectors).Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              AddSelectorsFromTriggerScript(response.trigger_scripts(0)))
      .Times(1);
  ON_CALL(*mock_dynamic_trigger_conditions_, OnUpdate(mock_web_controller_, _))
      .WillByDefault(RunOnceCallback<1>());
  ON_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillByDefault(Return(false));
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .WillOnce(RunOnceCallback<1>());
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_F(TriggerScriptCoordinatorTest, ShowAndHideTriggerScript) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  ON_CALL(*mock_dynamic_trigger_conditions_, OnUpdate(mock_web_controller_, _))
      .WillByDefault(RunOnceCallback<1>());
  ON_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillByDefault(Return(true));
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

  // Condition stays true, no further notification should be sent to observers.
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Condition turns false, trigger script is hidden.
  ON_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillByDefault(Return(false));
  EXPECT_CALL(mock_observer_, OnTriggerScriptHidden).Times(1);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Condition is true again, trigger script is shown again.
  ON_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillByDefault(Return(true));
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_F(TriggerScriptCoordinatorTest, PauseAndResumeOnTabVisibilityChange) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .WillOnce(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

  // When a tab becomes invisible, the trigger script is hidden and trigger
  // condition evaluation is suspended.
  EXPECT_CALL(mock_observer_, OnTriggerScriptHidden).Times(1);
  EXPECT_CALL(mock_observer_, OnVisibilityChanged(false)).Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .Times(0);
  SimulateWebContentsVisibilityChanged(content::Visibility::HIDDEN);

  // When a hidden tab becomes visible again, the trigger scripts must be
  // fetched again.
  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .WillOnce(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  EXPECT_CALL(mock_observer_, OnVisibilityChanged(true)).Times(1);
  SimulateWebContentsVisibilityChanged(content::Visibility::VISIBLE);
}

TEST_F(TriggerScriptCoordinatorTest, PerformTriggerScriptActionNotNow) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  ON_CALL(*mock_dynamic_trigger_conditions_, OnUpdate(mock_web_controller_, _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

  EXPECT_CALL(mock_observer_, OnTriggerScriptHidden).Times(1);
  coordinator_->PerformTriggerScriptAction(TriggerScriptProto::NOT_NOW);

  // Despite the trigger condition still being true, the trigger script is not
  // shown again until the condition has become first false and then true again.
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(0);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(false));
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_F(TriggerScriptCoordinatorTest, PerformTriggerScriptActionCancelSession) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  ON_CALL(*mock_dynamic_trigger_conditions_, OnUpdate(mock_web_controller_, _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

  EXPECT_CALL(mock_observer_, OnTriggerScriptFinished(
                                  Metrics::LiteScriptFinishedState::
                                      LITE_SCRIPT_PROMPT_FAILED_CANCEL_SESSION))
      .Times(1);
  EXPECT_CALL(mock_observer_, OnTriggerScriptHidden).Times(1);
  coordinator_->PerformTriggerScriptAction(TriggerScriptProto::CANCEL_SESSION);
  AssertRecordedFinishedState(Metrics::LiteScriptFinishedState::
                                  LITE_SCRIPT_PROMPT_FAILED_CANCEL_SESSION);
}

TEST_F(TriggerScriptCoordinatorTest, PerformTriggerScriptActionCancelForever) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  ON_CALL(*mock_dynamic_trigger_conditions_, OnUpdate(mock_web_controller_, _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

  EXPECT_CALL(mock_observer_, OnTriggerScriptFinished(
                                  Metrics::LiteScriptFinishedState::
                                      LITE_SCRIPT_PROMPT_FAILED_CANCEL_FOREVER))
      .Times(1);
  EXPECT_CALL(mock_observer_, OnTriggerScriptHidden).Times(1);
  coordinator_->PerformTriggerScriptAction(TriggerScriptProto::CANCEL_FOREVER);
  AssertRecordedFinishedState(Metrics::LiteScriptFinishedState::
                                  LITE_SCRIPT_PROMPT_FAILED_CANCEL_FOREVER);
}

TEST_F(TriggerScriptCoordinatorTest, PerformTriggerScriptActionAccept) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  ON_CALL(*mock_dynamic_trigger_conditions_, OnUpdate(mock_web_controller_, _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

  coordinator_->PerformTriggerScriptAction(TriggerScriptProto::ACCEPT);
}

TEST_F(TriggerScriptCoordinatorTest, CancelOnNavigateAway) {
  GetTriggerScriptsResponseProto response;
  response.add_additional_allowed_domains("other-example.com");
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  ON_CALL(*mock_dynamic_trigger_conditions_, OnUpdate(mock_web_controller_, _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

  // Same-domain navigation is ok.
  EXPECT_CALL(mock_observer_, OnTriggerScriptFinished(_)).Times(0);
  SimulateNavigateToUrl(GURL("https://example.com/cart"));

  // Navigating to sub-domain of original domain is ok.
  SimulateNavigateToUrl(GURL("https://subdomain.example.com/test"));

  // Navigating to whitelisted domain is ok.
  SimulateNavigateToUrl(GURL("https://other-example.com/page"));

  // Navigating to subdomain of whitelisted domain is ok.
  SimulateNavigateToUrl(GURL("https://other-example.com/page"));

  // Navigating to non-whitelisted domain is not ok.
  EXPECT_CALL(
      mock_observer_,
      OnTriggerScriptFinished(
          Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_FAILED_NAVIGATE))
      .Times(1);
  SimulateNavigateToUrl(GURL("https://example.different.com/page"));
  AssertRecordedFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_FAILED_NAVIGATE);
}

TEST_F(TriggerScriptCoordinatorTest, IgnoreNavigationEventsWhileNotStarted) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .WillOnce(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

  // When a tab becomes invisible, navigation events are disregarded.
  EXPECT_CALL(mock_observer_, OnTriggerScriptHidden).Times(1);
  EXPECT_CALL(mock_observer_, OnTriggerScriptFinished).Times(0);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .Times(0);
  SimulateWebContentsVisibilityChanged(content::Visibility::HIDDEN);
  // Note: in reality, it should be impossible to navigate on hidden tabs.
  SimulateNavigateToUrl(GURL("https://example.different.com"));
  SimulateNavigateToUrl(GURL("https://also-not-supported.com"));

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, /* response = */ ""));
  // However, when the tab becomes visible again, the trigger script is
  // restarted and thus fails if the tab is still on an unsupported domain.
  EXPECT_CALL(mock_observer_, OnTriggerScriptFinished(
                                  Metrics::LiteScriptFinishedState::
                                      LITE_SCRIPT_NO_TRIGGER_SCRIPT_AVAILABLE))
      .Times(1);
  SimulateWebContentsVisibilityChanged(content::Visibility::VISIBLE);
  AssertRecordedFinishedState(Metrics::LiteScriptFinishedState::
                                  LITE_SCRIPT_NO_TRIGGER_SCRIPT_AVAILABLE);
}

TEST_F(TriggerScriptCoordinatorTest, BottomSheetClosedWithSwipe) {
  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts()->set_on_swipe_to_dismiss(
      TriggerScriptProto::NOT_NOW);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  ON_CALL(*mock_dynamic_trigger_conditions_, OnUpdate(mock_web_controller_, _))
      .WillByDefault(RunOnceCallback<1>());
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

  EXPECT_CALL(mock_observer_, OnTriggerScriptHidden).Times(1);
  coordinator_->OnBottomSheetClosedWithSwipe();
  AssertRecordedShownToUserState(
      Metrics::LiteScriptShownToUser::LITE_SCRIPT_SWIPE_DISMISSED, 1);
}

TEST_F(TriggerScriptCoordinatorTest, TimeoutAfterInvisibleForTooLong) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  response.set_timeout_ms(3000);
  response.set_trigger_condition_check_interval_ms(1000);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());

  // Note: expect 4 calls: 1 initial plus 3 until timeout.
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .Times(4)
      .WillRepeatedly(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .Times(4)
      .WillRepeatedly(Return(false));
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  EXPECT_CALL(mock_observer_, OnTriggerScriptFinished(
                                  Metrics::LiteScriptFinishedState::
                                      LITE_SCRIPT_TRIGGER_CONDITION_TIMEOUT));
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  AssertRecordedFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_TRIGGER_CONDITION_TIMEOUT);
}

TEST_F(TriggerScriptCoordinatorTest, TimeoutResetsAfterTriggerScriptShown) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  response.set_timeout_ms(3000);
  response.set_trigger_condition_check_interval_ms(1000);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .WillRepeatedly(RunOnceCallback<1>());

  // While the trigger script is shown, the timeout is ignored.
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillRepeatedly(Return(true));
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  // When the trigger script is hidden, the timeout resets.
  EXPECT_CALL(mock_observer_, OnTriggerScriptHidden).Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillRepeatedly(Return(false));
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_CALL(mock_observer_, OnTriggerScriptFinished(
                                  Metrics::LiteScriptFinishedState::
                                      LITE_SCRIPT_TRIGGER_CONDITION_TIMEOUT));
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  AssertRecordedFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_TRIGGER_CONDITION_TIMEOUT);
}

TEST_F(TriggerScriptCoordinatorTest, NoTimeoutByDefault) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .WillRepeatedly(RunOnceCallback<1>());

  EXPECT_CALL(mock_observer_, OnTriggerScriptFinished).Times(0);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillRepeatedly(Return(false));
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());
  for (int i = 0; i < 10; ++i) {
    task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  }
}

TEST_F(TriggerScriptCoordinatorTest, KeyboardEventTriggersOutOfScheduleCheck) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  response.set_timeout_ms(3000);
  response.set_trigger_condition_check_interval_ms(1000);
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .WillOnce(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(false));
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

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
              OnUpdate(mock_web_controller_, _))
      .Times(3)
      .WillRepeatedly(RunOnceCallback<1>());
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  EXPECT_CALL(mock_observer_, OnTriggerScriptFinished(
                                  Metrics::LiteScriptFinishedState::
                                      LITE_SCRIPT_TRIGGER_CONDITION_TIMEOUT));
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  AssertRecordedFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_TRIGGER_CONDITION_TIMEOUT);
}

TEST_F(TriggerScriptCoordinatorTest, OnTriggerScriptFailedToShow) {
  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts();
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .WillRepeatedly(RunOnceCallback<1>());

  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).WillOnce([&]() {
    coordinator_->OnTriggerScriptShown(/* success = */ false);
  });
  EXPECT_CALL(
      mock_observer_,
      OnTriggerScriptFinished(
          Metrics::LiteScriptFinishedState::LITE_SCRIPT_FAILED_TO_SHOW));
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());
  AssertRecordedFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_FAILED_TO_SHOW);
}

TEST_F(TriggerScriptCoordinatorTest, OnProactiveHelpSettingDisabled) {
  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts();
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .WillRepeatedly(RunOnceCallback<1>());

  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

  EXPECT_CALL(
      mock_observer_,
      OnTriggerScriptFinished(Metrics::LiteScriptFinishedState::
                                  LITE_SCRIPT_DISABLED_PROACTIVE_HELP_SETTING));
  coordinator_->OnProactiveHelpSettingChanged(
      /* proactive_help_enabled = */ false);
  AssertRecordedFinishedState(Metrics::LiteScriptFinishedState::
                                  LITE_SCRIPT_DISABLED_PROACTIVE_HELP_SETTING);
}

TEST_F(TriggerScriptCoordinatorTest, PauseAndResumeOnTabSwitch) {
  GetTriggerScriptsResponseProto response;
  *response.add_trigger_scripts()
       ->mutable_trigger_condition()
       ->mutable_selector() = ToSelectorProto("#selector");
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .WillOnce(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

  // During tab switching, the tab becomes non-interactive. In this test, the
  // same tab is then re-selected (otherwise, the original tab's visibility
  // would change).
  EXPECT_CALL(mock_observer_, OnTriggerScriptHidden).Times(1);
  EXPECT_CALL(mock_observer_, OnVisibilityChanged(false)).Times(1);
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .Times(0);
  SimulateWebContentsInteractabilityChanged(/* interactable = */ false);

  // When a non-interactable tab becomes interactable again, the trigger scripts
  // must be fetched again.
  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .WillOnce(RunOnceCallback<1>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_, GetSelectorMatches)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  EXPECT_CALL(mock_observer_, OnVisibilityChanged(true)).Times(1);
  SimulateWebContentsInteractabilityChanged(/* interactable = */ true);
}

TEST_F(TriggerScriptCoordinatorTest, OnboardingShownAndAccepted) {
  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts();
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .WillRepeatedly(RunOnceCallback<1>());
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

  EXPECT_CALL(
      mock_observer_,
      OnTriggerScriptFinished(
          Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_SUCCEEDED))
      .Times(1);
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(0);
  coordinator_->OnOnboardingFinished(/* onboardingShown= */ true,
                                     /* result= */ OnboardingResult::ACCEPTED);

  AssertRecordedLiteScriptOnboardingState(
      Metrics::LiteScriptOnboarding::LITE_SCRIPT_ONBOARDING_SEEN_AND_ACCEPTED,
      1);
  AssertRecordedFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_SUCCEEDED);
}

TEST_F(TriggerScriptCoordinatorTest,
       CancellingDialogOnboardingDoesNotStopTriggerScript) {
  auto feature_list = CreateScopedFeatureList(/* dialog_onboarding= */ true);

  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts();
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .WillRepeatedly(RunOnceCallback<1>());
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

  EXPECT_CALL(mock_observer_, OnTriggerScriptFinished).Times(0);
  coordinator_->OnOnboardingFinished(/* onboardingShown= */ true,
                                     /* result= */ OnboardingResult::REJECTED);
  coordinator_->OnOnboardingFinished(/* onboardingShown= */ true,
                                     /* result= */ OnboardingResult::DISMISSED);
  coordinator_->OnOnboardingFinished(
      /* onboardingShown= */ true,
      /* result= */ OnboardingResult::NAVIGATION);

  EXPECT_CALL(
      mock_observer_,
      OnTriggerScriptFinished(
          Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_SUCCEEDED))
      .Times(1);
  EXPECT_CALL(mock_observer_, OnTriggerScriptHidden).Times(0);
  coordinator_->OnOnboardingFinished(/* onboardingShown= */ true,
                                     /* result= */ OnboardingResult::ACCEPTED);

  AssertRecordedLiteScriptOnboardingState(
      Metrics::LiteScriptOnboarding::LITE_SCRIPT_ONBOARDING_SEEN_AND_REJECTED,
      1);
  AssertRecordedLiteScriptOnboardingState(
      Metrics::LiteScriptOnboarding::LITE_SCRIPT_ONBOARDING_SEEN_AND_ACCEPTED,
      1);
  AssertRecordedLiteScriptOnboardingState(
      Metrics::LiteScriptOnboarding::
          LITE_SCRIPT_ONBOARDING_SEEN_AND_INTERRUPTED_BY_NAVIGATION,
      1);
  AssertRecordedFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_SUCCEEDED);
}

TEST_F(TriggerScriptCoordinatorTest,
       RejectingBottomSheetOnboardingStopsTriggerScript) {
  auto feature_list = CreateScopedFeatureList(/* dialog_onboarding= */ false);

  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts();
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .WillRepeatedly(RunOnceCallback<1>());
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

  EXPECT_CALL(
      mock_observer_,
      OnTriggerScriptFinished(Metrics::LiteScriptFinishedState::
                                  LITE_SCRIPT_BOTTOMSHEET_ONBOARDING_REJECTED))
      .Times(1);
  EXPECT_CALL(mock_observer_, OnTriggerScriptHidden).Times(1);
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(0);
  coordinator_->OnOnboardingFinished(/* onboardingShown= */ true,
                                     /* result= */ OnboardingResult::REJECTED);

  AssertRecordedLiteScriptOnboardingState(
      Metrics::LiteScriptOnboarding::LITE_SCRIPT_ONBOARDING_SEEN_AND_REJECTED,
      1);
  AssertRecordedFinishedState(Metrics::LiteScriptFinishedState::
                                  LITE_SCRIPT_BOTTOMSHEET_ONBOARDING_REJECTED);
}

TEST_F(TriggerScriptCoordinatorTest, OnboardingNotShown) {
  GetTriggerScriptsResponseProto response;
  response.add_trigger_scripts();
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  EXPECT_CALL(*mock_request_sender_, OnSendRequest(GURL(kFakeServerUrl), _, _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, serialized_response));
  EXPECT_CALL(*mock_static_trigger_conditions_, Init)
      .WillOnce(RunOnceCallback<4>());
  EXPECT_CALL(*mock_dynamic_trigger_conditions_,
              OnUpdate(mock_web_controller_, _))
      .WillRepeatedly(RunOnceCallback<1>());
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(1);
  coordinator_->Start(GURL(kFakeDeepLink),
                      std::make_unique<TriggerContextImpl>());

  EXPECT_CALL(
      mock_observer_,
      OnTriggerScriptFinished(
          Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_SUCCEEDED))
      .Times(1);
  EXPECT_CALL(mock_observer_, OnTriggerScriptShown).Times(0);
  coordinator_->OnOnboardingFinished(/* onboardingShown= */ false,
                                     /* result= */ OnboardingResult::ACCEPTED);

  AssertRecordedLiteScriptOnboardingState(
      Metrics::LiteScriptOnboarding::LITE_SCRIPT_ONBOARDING_ALREADY_ACCEPTED,
      1);
  AssertRecordedFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_SUCCEEDED);
}

}  // namespace autofill_assistant
