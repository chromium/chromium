// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/headless/headless_script_controller_impl.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill_assistant/browser/cud_condition.pb.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/fake_common_dependencies.h"
#include "components/autofill_assistant/browser/fake_script_executor_ui_delegate.h"
#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/headless/client_headless.h"
#include "components/autofill_assistant/browser/mock_autofill_assistant_tts_controller.h"
#include "components/autofill_assistant/browser/mock_client.h"
#include "components/autofill_assistant/browser/mock_controller_observer.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "components/autofill_assistant/browser/public/mock_external_action_delegate.h"
#include "components/autofill_assistant/browser/public/mock_runtime_manager.h"
#include "components/autofill_assistant/browser/service/mock_service.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/starter.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/ukm_test_util.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "components/password_manager/core/browser/mock_password_change_success_tracker.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Gt;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Sequence;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;
using ::testing::WithArgs;

const char kExampleDeeplink[] = "https://www.example.com";

class HeadlessScriptControllerImplTest : public testing::Test {
 public:
  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);
    mock_runtime_manager_ = std::make_unique<MockRuntimeManager>();

    starter_ = std::make_unique<Starter>(
        web_contents(), fake_platform_delegate_.GetWeakPtr(), &ukm_recorder_,
        mock_runtime_manager_->GetWeakPtr(),
        task_environment_.GetMockTickClock());

    mock_service_to_inject_ = std::make_unique<NiceMock<MockService>>();
    mock_service_ = mock_service_to_inject_.get();
    // Fetching scripts succeeds for all URLs, but return nothing.
    ON_CALL(*mock_service_, GetScriptsForUrl)
        .WillByDefault(RunOnceCallback<2>(
            net::HTTP_OK, "", ServiceRequestSender::ResponseInfo{}));

    // Scripts run, but have no actions.
    ON_CALL(*mock_service_, GetActions)
        .WillByDefault(RunOnceCallback<5>(
            net::HTTP_OK, "", ServiceRequestSender::ResponseInfo{}));
    ON_CALL(*mock_service_, GetNextActions)
        .WillByDefault(RunOnceCallback<6>(
            net::HTTP_OK, "", ServiceRequestSender::ResponseInfo{}));

    mock_web_controller_to_inject_ =
        std::make_unique<NiceMock<MockWebController>>();
    mock_web_controller_ = mock_web_controller_to_inject_.get();
    ON_CALL(*mock_web_controller_, FindElement)
        .WillByDefault(RunOnceCallback<2>(ClientStatus(), nullptr));

    mock_external_action_delegate_ =
        std::make_unique<MockExternalActionDelegate>();
    auto client = std::make_unique<ClientHeadless>(
        web_contents_.get(), starter_->GetCommonDependencies(),
        mock_external_action_delegate_.get(), nullptr,
        task_environment_.GetMockTickClock(),
        mock_runtime_manager_->GetWeakPtr(), &ukm_recorder_, nullptr);
    headless_script_controller_ =
        std::make_unique<HeadlessScriptControllerImpl>(
            web_contents_.get(), starter_.get(), std::move(client));
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

  // Note that calling this method moves |mock_service_to_inject_| and
  // |mock_web_controller_to_inject_| so it should not be called more than once
  // per test.
  void Start(const base::flat_map<std::string, std::string>& params,
             bool expect_success) {
    // Since the callback is often called in a PostTask, we use this to make
    // sure the test does not finish before the callback is called.
    base::RunLoop run_loop;

    EXPECT_CALL(mock_script_ended_callback_, Run)
        .WillOnce([&run_loop, &expect_success](
                      HeadlessScriptController::ScriptResult result) {
          EXPECT_EQ(result.success, expect_success);
          run_loop.Quit();
        });
    headless_script_controller_->StartScript(
        params, mock_script_ended_callback_.Get(),
        /* use_autofill_assistant_onboarding = */ false, base::DoNothing(),
        /* suppress_browsing_features = */ true,
        std::move(mock_service_to_inject_),
        std::move(mock_web_controller_to_inject_));
    run_loop.Run();
  }

  void SetupScripts(SupportsScriptResponseProto scripts) {
    EXPECT_CALL(*mock_service_, GetScriptsForUrl)
        .WillOnce(RunOnceCallback<2>(net::HTTP_OK, scripts.SerializeAsString(),
                                     ServiceRequestSender::ResponseInfo{}));
  }

  void SetupActionsForScript(const std::string& path,
                             ActionsResponseProto actions_response) {
    EXPECT_CALL(*mock_service_, GetActions(StrEq(path), _, _, _, _, _))
        .WillOnce(RunOnceCallback<5>(net::HTTP_OK,
                                     actions_response.SerializeAsString(),
                                     ServiceRequestSender::ResponseInfo{}));
  }

  static SupportedScriptProto* AddInterrupt(
      SupportsScriptResponseProto* response,
      const std::string& name_and_path,
      const std::string& precondition) {
    SupportedScriptProto* script = AddRunnableScript(response, name_and_path);
    script->mutable_presentation()->set_interrupt(true);
    *script->mutable_presentation()
         ->mutable_precondition()
         ->mutable_element_condition()
         ->mutable_match() = ToSelectorProto(precondition);
    return script;
  }

  static SupportedScriptProto* AddRunnableScript(
      SupportsScriptResponseProto* response,
      const std::string& name_and_path) {
    SupportedScriptProto* script = response->add_scripts();
    script->set_path(name_and_path);
    return script;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::MockCallback<
      base::OnceCallback<void(HeadlessScriptController::ScriptResult)>>
      mock_script_ended_callback_;
  FakeStarterPlatformDelegate fake_platform_delegate_ =
      FakeStarterPlatformDelegate(std::make_unique<FakeCommonDependencies>(
          /*identity_manager=*/nullptr));
  std::unique_ptr<MockRuntimeManager> mock_runtime_manager_;
  std::unique_ptr<Starter> starter_;
  std::unique_ptr<HeadlessScriptControllerImpl> headless_script_controller_;
  std::unique_ptr<MockExternalActionDelegate> mock_external_action_delegate_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  raw_ptr<MockService> mock_service_;
  raw_ptr<MockWebController> mock_web_controller_;

 private:
  // These will be moved when the |Start| method is called, so expectations
  // should be written using |mock_service_| and |mock_web_controller_| instead.
  std::unique_ptr<MockService> mock_service_to_inject_;
  std::unique_ptr<MockWebController> mock_web_controller_to_inject_;
};

TEST_F(HeadlessScriptControllerImplTest,
       StartFailsWithoutMandatoryScriptParameter) {
  // The startup will fail because we are missing the initial URL.
  base::flat_map<std::string, std::string> params = {
      {"ENABLED", "true"}, {"START_IMMEDIATELY", "true"}};
  Start(params, /* expect_success= */ false);
}

TEST_F(HeadlessScriptControllerImplTest, StartFailsIfNoScriptsAvailable) {
  EXPECT_CALL(*mock_service_, GetScriptsForUrl)
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{}));
  base::flat_map<std::string, std::string> params = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  Start(params, /* expect_success= */ false);
}

TEST_F(HeadlessScriptControllerImplTest, SuccessfulRun) {
  SupportsScriptResponseProto script_response;
  SupportedScriptProto* script = AddRunnableScript(&script_response, "script");
  script->mutable_presentation()->set_autostart(true);
  SetupScripts(script_response);

  ActionsResponseProto script_actions;
  script_actions.add_actions()->mutable_stop();
  SetupActionsForScript("script", script_actions);

  base::flat_map<std::string, std::string> params = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  Start(params, /* expect_success= */ true);
}

TEST_F(HeadlessScriptControllerImplTest, ScriptWithExternalActionSucceeds) {
  SupportsScriptResponseProto script_response;
  SupportedScriptProto* script = AddRunnableScript(&script_response, "script");
  script->mutable_presentation()->set_autostart(true);
  SetupScripts(script_response);

  ActionsResponseProto script_actions;
  script_actions.add_actions()->mutable_external_action()->mutable_info();
  script_actions.add_actions()->mutable_stop();

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(*mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{})));

  external::Result result;
  result.set_success(true);
  result.mutable_result_info();
  EXPECT_CALL(*mock_external_action_delegate_, OnActionRequested)
      .WillOnce(RunOnceCallback<3>(result));

  SetupActionsForScript("script", script_actions);

  base::flat_map<std::string, std::string> params = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  Start(params, /* expect_success= */ true);
  ASSERT_THAT(processed_actions_capture, SizeIs(2));
  EXPECT_EQ(processed_actions_capture[0].status(), ACTION_APPLIED);
  EXPECT_EQ(processed_actions_capture[1].status(), ACTION_APPLIED);
  EXPECT_TRUE(
      processed_actions_capture[0].external_action_result().has_result_info());
}

TEST_F(HeadlessScriptControllerImplTest,
       ReportMainActionFailureOnExternalActionFailure) {
  SupportsScriptResponseProto script_response;
  SupportedScriptProto* script = AddRunnableScript(&script_response, "script");
  script->mutable_presentation()->set_autostart(true);
  SetupScripts(script_response);

  ActionsResponseProto first_roundtrip_actions;
  first_roundtrip_actions.add_actions()
      ->mutable_external_action()
      ->mutable_info();
  SetupActionsForScript("script", first_roundtrip_actions);

  external::Result result;
  result.set_success(false);
  result.mutable_result_info();
  EXPECT_CALL(*mock_external_action_delegate_, OnActionRequested)
      .WillOnce(RunOnceCallback<3>(result));

  // An action failing causes all following actions to be ignored, so we need to
  // put the stop action in the following roundtrip.
  ActionsResponseProto second_roundtrip_actions;
  second_roundtrip_actions.add_actions()->mutable_stop();
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(*mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK,
                                   second_roundtrip_actions.SerializeAsString(),
                                   ServiceRequestSender::ResponseInfo{})))
      .WillOnce((RunOnceCallback<6>(net::HTTP_OK, "",
                                    ServiceRequestSender::ResponseInfo{})));

  base::flat_map<std::string, std::string> params = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  Start(params, /* expect_success= */ true);
  ASSERT_THAT(processed_actions_capture, SizeIs(1));
  EXPECT_EQ(processed_actions_capture[0].status(), UNKNOWN_ACTION_STATUS);
  EXPECT_TRUE(
      processed_actions_capture[0].external_action_result().has_result_info());
}

TEST_F(HeadlessScriptControllerImplTest,
       ExternalActionEndingDuringDomUpdateSuccessfullyEndsMainAction) {
  SupportsScriptResponseProto script_response;
  SupportedScriptProto* script = AddRunnableScript(&script_response, "script");
  script->mutable_presentation()->set_autostart(true);
  SetupScripts(script_response);

  ActionsResponseProto script_actions;
  auto* external_action =
      script_actions.add_actions()->mutable_external_action();
  external_action->mutable_info();
  auto* condition = external_action->add_conditions();
  condition->set_id(1);
  *condition->mutable_element_condition()->mutable_match() =
      ToSelectorProto("#element");
  EXPECT_CALL(*mock_web_controller_, FindElement(Selector({"#element"}), _, _))
      .WillRepeatedly(RunOnceCallback<2>(
          OkClientStatus(), std::make_unique<ElementFinderResult>()));
  script_actions.add_actions()->mutable_stop();

  base::MockCallback<
      base::RepeatingCallback<void(const external::ElementConditionsUpdate&)>>
      dom_updates_callback;
  base::OnceCallback<void(const external::Result&)> stored_end_action_callback;
  // The external action is requested but the external execution does not end it
  // right away.
  EXPECT_CALL(*mock_external_action_delegate_, OnActionRequested)
      .WillOnce([&stored_end_action_callback, &dom_updates_callback](
                    const external::Action& action_info, bool is_interrupt,
                    base::OnceCallback<void(
                        ExternalActionDelegate::DomUpdateCallback)>
                        start_dom_checks_callback,
                    base::OnceCallback<void(const external::Result&)>
                        end_action_callback) {
        stored_end_action_callback = std::move(end_action_callback);
        std::move(start_dom_checks_callback).Run(dom_updates_callback.Get());
      });

  external::Result result;
  result.set_success(true);
  result.mutable_result_info();
  // The action is ended as a result of a DOM update.
  EXPECT_CALL(dom_updates_callback, Run)
      .WillOnce([&result, &stored_end_action_callback](
                    const external::ElementConditionsUpdate& update) {
        ASSERT_THAT(update.results(), SizeIs(1));
        EXPECT_EQ(update.results(0).id(), 1);
        EXPECT_TRUE(update.results(0).satisfied());
        std::move(stored_end_action_callback).Run(result);
      });

  SetupActionsForScript("script", script_actions);

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(*mock_service_, GetNextActions)
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{})));

  base::flat_map<std::string, std::string> params = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  Start(params, /* expect_success= */ true);
  ASSERT_THAT(processed_actions_capture, SizeIs(2));
  EXPECT_EQ(processed_actions_capture[0].status(), ACTION_APPLIED);
  EXPECT_EQ(processed_actions_capture[1].status(), ACTION_APPLIED);
  EXPECT_TRUE(
      processed_actions_capture[0].external_action_result().has_result_info());
}

TEST_F(HeadlessScriptControllerImplTest,
       ExternalActionInInterruptScriptCorrectlyNotifiedAsInterrupt) {
  SupportsScriptResponseProto script_response;
  SupportedScriptProto* script = AddRunnableScript(&script_response, "script");
  script->mutable_presentation()->set_autostart(true);

  ActionsResponseProto script_actions;
  auto* external_action =
      script_actions.add_actions()->mutable_external_action();
  external_action->mutable_info();
  external_action->set_allow_interrupt(true);
  script_actions.add_actions()->mutable_stop();

  AddInterrupt(&script_response, "interrupt", "#element");
  EXPECT_CALL(*mock_web_controller_, FindElement(Selector({"#element"}), _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(),
                                   std::make_unique<ElementFinderResult>()));

  ActionsResponseProto interrupt_actions;
  interrupt_actions.add_actions()->mutable_external_action()->mutable_info();

  SetupScripts(script_response);
  SetupActionsForScript("script", script_actions);
  SetupActionsForScript("interrupt", interrupt_actions);

  base::MockCallback<
      base::RepeatingCallback<void(const external::ElementConditionsUpdate&)>>
      dom_updates_callback;
  base::OnceCallback<void(const external::Result&)> stored_end_action_callback;
  external::Result result;
  result.set_success(true);
  result.mutable_result_info();
  // The main ExternalAction is called first. We don't end it right away to give
  // time to the interrupt to trigger
  EXPECT_CALL(*mock_external_action_delegate_, OnActionRequested)
      .WillOnce([&stored_end_action_callback, &dom_updates_callback](
                    const external::Action& action_info, bool is_interrupt,
                    base::OnceCallback<void(
                        ExternalActionDelegate::DomUpdateCallback)>
                        start_dom_checks_callback,
                    base::OnceCallback<void(const external::Result&)>
                        end_action_callback) {
        EXPECT_FALSE(is_interrupt);
        stored_end_action_callback = std::move(end_action_callback);
        std::move(start_dom_checks_callback).Run(dom_updates_callback.Get());
      })
      // The interrupt ExternalAction is then called. We end it right away.
      .WillOnce([&result](const external::Action& action_info,
                          bool is_interrupt,
                          base::OnceCallback<void(
                              ExternalActionDelegate::DomUpdateCallback)>
                              start_dom_checks_callback,
                          base::OnceCallback<void(const external::Result&)>
                              end_action_callback) {
        EXPECT_TRUE(is_interrupt);

        std::move(end_action_callback).Run(result);
      });
  // No element check specified so no update should be sent.
  EXPECT_CALL(dom_updates_callback, Run).Times(0);

  EXPECT_CALL(*mock_external_action_delegate_, OnInterruptStarted).Times(1);

  // Once the interrupt script is finished, we finish the main action. This just
  // simulates the action finishing at some point after the end of the
  // interrupt.
  EXPECT_CALL(*mock_external_action_delegate_, OnInterruptFinished)
      .WillOnce([&result, &stored_end_action_callback]() {
        std::move(stored_end_action_callback).Run(result);
      });

  std::vector<ProcessedActionProto> interrupt_processed_actions_capture;
  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(*mock_service_, GetNextActions)
      .WillOnce(DoAll(SaveArg<3>(&interrupt_processed_actions_capture),
                      RunOnceCallback<6>(net::HTTP_OK, "",
                                         ServiceRequestSender::ResponseInfo{})))
      .WillOnce(
          DoAll(SaveArg<3>(&processed_actions_capture),
                RunOnceCallback<6>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{})));

  base::flat_map<std::string, std::string> params = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  Start(params, /* expect_success= */ true);

  // Check on the interrupt's result
  ASSERT_THAT(interrupt_processed_actions_capture, SizeIs(1));
  EXPECT_EQ(interrupt_processed_actions_capture[0].status(), ACTION_APPLIED);
  EXPECT_TRUE(interrupt_processed_actions_capture[0]
                  .external_action_result()
                  .has_result_info());

  // Check on the main script's result
  ASSERT_THAT(processed_actions_capture, SizeIs(2));
  EXPECT_EQ(processed_actions_capture[0].status(), ACTION_APPLIED);
  EXPECT_EQ(processed_actions_capture[1].status(), ACTION_APPLIED);
  EXPECT_TRUE(
      processed_actions_capture[0].external_action_result().has_result_info());
}

}  // namespace autofill_assistant
