// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base64.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/base_browsertest.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"
#include "components/autofill_assistant/browser/fake_script_executor_ui_delegate.h"
#include "components/autofill_assistant/browser/js_flow_util.h"
#include "components/autofill_assistant/browser/model.pb.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/mock_service.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "content/public/test/browser_test.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::StrictMock;
using ::testing::WithArg;
using ::testing::WithArgs;

class ScriptExecutorBrowserTest : public BaseBrowserTest {
 public:
  void SetUpOnMainThread() override {
    BaseBrowserTest::SetUpOnMainThread();

    web_controller_ = WebController::CreateForWebContents(
        shell()->web_contents(), &user_data_, &log_info_, nullptr,
        /*enable_full_stack_traces= */ true);

    fake_script_executor_delegate_.SetService(&mock_service_);
    fake_script_executor_delegate_.SetWebController(web_controller_.get());
    fake_script_executor_delegate_.SetCurrentURL(GURL("http://example.com/"));
    fake_script_executor_delegate_.SetWebContents(shell()->web_contents());
  }

 protected:
  void Run(const ActionsResponseProto& actions_response) {
    EXPECT_CALL(mock_service_, GetActions)
        .WillOnce(RunOnceCallback<5>(net::HTTP_OK,
                                     actions_response.SerializeAsString(),
                                     ServiceRequestSender::ResponseInfo{}));

    ON_CALL(mock_service_, GetNextActions)
        .WillByDefault(RunOnceCallback<6>(
            net::HTTP_OK, ActionsResponseProto().SerializeAsString(),
            ServiceRequestSender::ResponseInfo{}));

    base::RunLoop run_loop;

    script_executor_ = std::make_unique<ScriptExecutor>(
        /* script_path= */ "",
        /* additional_context= */ std::make_unique<TriggerContext>(),
        /* global_payload= */ "",
        /* script_payload= */ "",
        /* listener= */ nullptr, &ordered_interrupts_,
        &fake_script_executor_delegate_, &fake_script_executor_ui_delegate_);

    script_executor_->Run(
        &user_data_, executor_callback_.Get().Then(run_loop.QuitClosure()));
    run_loop.Run();
  }

  void RunJsFlow(const std::string& js_flow,
                 const ProcessedActionStatusProto& result) {
    EXPECT_CALL(executor_callback_,
                Run(Field(&ScriptExecutor::Result::success, true)));

    EXPECT_CALL(mock_service_, GetNextActions)
        .WillOnce(DoAll(
            WithArgs<3>([&result](const std::vector<ProcessedActionProto>&
                                      processed_actions) {
              EXPECT_THAT(
                  processed_actions,
                  ElementsAre(Property(&ProcessedActionProto::status, result)));
            }),
            RunOnceCallback<6>(net::HTTP_OK,
                               ActionsResponseProto().SerializeAsString(),
                               ServiceRequestSender::ResponseInfo{})));

    ActionsResponseProto actions_response;
    actions_response.add_actions()->mutable_js_flow()->set_js_flow(js_flow);
    Run(actions_response);
  }

  std::unique_ptr<ScriptExecutor> script_executor_;

  std::vector<std::unique_ptr<Script>> ordered_interrupts_;

  ProcessedActionStatusDetailsProto log_info_;
  std::unique_ptr<WebController> web_controller_;

  FakeScriptExecutorDelegate fake_script_executor_delegate_;
  FakeScriptExecutorUiDelegate fake_script_executor_ui_delegate_;
  UserData user_data_;

  NiceMock<MockService> mock_service_;

  StrictMock<base::MockCallback<ScriptExecutor::RunScriptCallback>>
      executor_callback_;
};

std::string CreateRunNativeActionCall(
    const google::protobuf::MessageLite* proto,
    const ActionProto::ActionInfoCase action_info_case) {
  return base::StrCat({"const [status, value] = await runNativeAction(",
                       base::NumberToString(action_info_case), ", '",
                       js_flow_util::SerializeToBase64(proto), "');"});
}

std::string CreateRunNativeActionCallReturn(
    const google::protobuf::MessageLite* proto,
    const ActionProto::ActionInfoCase action_info_case) {
  return base::StrCat({"{", CreateRunNativeActionCall(proto, action_info_case),
                       "return {status}}"});
}

std::string CreateRunNativeActionCallReturnIfError(
    const google::protobuf::MessageLite* proto,
    const ActionProto::ActionInfoCase action_info_case) {
  return base::StrCat({"{", CreateRunNativeActionCall(proto, action_info_case),
                       "if(status != 2) return {status}}"});
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest, ShutdownAfter_Stop) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_stop();

  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::SHUTDOWN))));

  Run(actions_response);
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest,
                       ShutdownGracefullyAfter_Tell_Stop) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("message");
  actions_response.add_actions()->mutable_stop();

  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::SHUTDOWN_GRACEFULLY))));

  Run(actions_response);
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest,
                       ShutdownGracefullyAfter_Tell_EmptyJsFlow_Stop) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("message");
  actions_response.add_actions()->mutable_js_flow();
  actions_response.add_actions()->mutable_stop();

  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::SHUTDOWN_GRACEFULLY))));

  Run(actions_response);
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest,
                       ShutdownGracefullyAfter_JsFlowTell_Stop) {
  TellProto tell;
  tell.set_message("message");

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_js_flow()->set_js_flow(
      CreateRunNativeActionCallReturn(&tell, ActionProto::kTell));
  actions_response.add_actions()->mutable_stop();

  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::SHUTDOWN_GRACEFULLY))));

  Run(actions_response);
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest,
                       ShutdownGracefullyAfter_JsFlowTellAndStop) {
  TellProto tell;
  tell.set_message("message");
  StopProto stop;

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_js_flow()->set_js_flow(
      CreateRunNativeActionCallReturnIfError(&tell, ActionProto::kTell) +
      CreateRunNativeActionCallReturn(&stop, ActionProto::kStop));

  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::SHUTDOWN_GRACEFULLY))));

  Run(actions_response);
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest,
                       ShutdownGracefullyAfter_JsFlowTell_JsFlowStop) {
  TellProto tell;
  tell.set_message("message");
  StopProto stop;

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_js_flow()->set_js_flow(
      CreateRunNativeActionCallReturnIfError(&tell, ActionProto::kTell));
  actions_response.add_actions()->mutable_js_flow()->set_js_flow(
      CreateRunNativeActionCallReturn(&stop, ActionProto::kStop));

  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::SHUTDOWN_GRACEFULLY))));

  Run(actions_response);
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest,
                       ShutdownGracefullyAfter_Tell_JsFlowStop) {
  StopProto stop;

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("message");
  actions_response.add_actions()->mutable_js_flow()->set_js_flow(
      CreateRunNativeActionCallReturn(&stop, ActionProto::kStop));

  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::SHUTDOWN_GRACEFULLY))));

  Run(actions_response);
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest, JsLibraryIsUsed) {
  ActionsResponseProto actions_response;
  actions_response.set_js_flow_library("const status = 2;");
  actions_response.add_actions()->mutable_js_flow()->set_js_flow(
      "return {status};");

  EXPECT_CALL(mock_service_,
              GetNextActions(_, _, _,
                             ElementsAre(Property(&ProcessedActionProto::status,
                                                  ACTION_APPLIED)),
                             _, _, _))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK,
                                   ActionsResponseProto().SerializeAsString(),
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  Run(actions_response);
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest, JsLibraryIsReused) {
  ActionsResponseProto actions_response;
  actions_response.set_js_flow_library("const status = 2;");
  actions_response.add_actions()->mutable_js_flow()->set_js_flow(
      "return {status};");
  actions_response.add_actions()->mutable_js_flow()->set_js_flow(
      "return {status};");

  EXPECT_CALL(
      mock_service_,
      GetNextActions(
          _, _, _,
          ElementsAre(Property(&ProcessedActionProto::status, ACTION_APPLIED),
                      Property(&ProcessedActionProto::status, ACTION_APPLIED)),
          _, _, _))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK,
                                   ActionsResponseProto().SerializeAsString(),
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  Run(actions_response);
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest, JsLibraryIsUsedAccrossCalls) {
  ActionsResponseProto actions_response_1;
  actions_response_1.set_js_flow_library("const st = 2;");
  actions_response_1.add_actions()->mutable_js_flow()->set_js_flow(
      "return {status: st};");

  ActionsResponseProto actions_response_2;
  actions_response_2.add_actions()->mutable_js_flow()->set_js_flow(
      "return {status: st};");

  EXPECT_CALL(mock_service_,
              GetNextActions(_, _, _,
                             ElementsAre(Property(&ProcessedActionProto::status,
                                                  ACTION_APPLIED)),
                             _, _, _))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK,
                                   actions_response_2.SerializeAsString(),
                                   ServiceRequestSender::ResponseInfo{}))
      .WillOnce(RunOnceCallback<6>(net::HTTP_OK,
                                   ActionsResponseProto().SerializeAsString(),
                                   ServiceRequestSender::ResponseInfo{}));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  Run(actions_response_1);
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest, JsFlowErrorInJsLibraryFlow) {
  ActionsResponseProto actions_response_1;
  actions_response_1.set_js_flow_library("throw new Error();");
  actions_response_1.add_actions()->mutable_js_flow()->set_js_flow(
      "return {status: 2};");

  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(DoAll(
          WithArgs<3>(
              [](const std::vector<ProcessedActionProto>& processed_actions) {
                ASSERT_THAT(processed_actions, SizeIs(1));

                const auto& processed_action = processed_actions[0];
                EXPECT_THAT(processed_action,
                            Property(&ProcessedActionProto::status,
                                     UNEXPECTED_JS_ERROR));

                const auto& unexpected_error_info =
                    processed_action.status_details().unexpected_error_info();

                EXPECT_THAT(
                    unexpected_error_info.js_exception_locations(),
                    ElementsAre(UnexpectedErrorInfoProto::JS_FLOW_LIBRARY));
                EXPECT_THAT(unexpected_error_info.js_exception_line_numbers(),
                            ElementsAre(0));
                EXPECT_THAT(unexpected_error_info.js_exception_column_numbers(),
                            ElementsAre(6));
              }),
          RunOnceCallback<6>(net::HTTP_OK,
                             ActionsResponseProto().SerializeAsString(),
                             ServiceRequestSender::ResponseInfo{})));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  Run(actions_response_1);
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest, JsFlowErrorInJsFlow) {
  ActionsResponseProto actions_response_1;
  actions_response_1.set_js_flow_library("const st = 2;\n");
  actions_response_1.add_actions()->mutable_js_flow()->set_js_flow(
      "throw new Error();");

  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(DoAll(
          WithArgs<3>(
              [](const std::vector<ProcessedActionProto>& processed_actions) {
                ASSERT_THAT(processed_actions, SizeIs(1));

                const auto& processed_action = processed_actions[0];
                EXPECT_THAT(processed_action,
                            Property(&ProcessedActionProto::status,
                                     UNEXPECTED_JS_ERROR));

                const auto& unexpected_error_info =
                    processed_action.status_details().unexpected_error_info();

                EXPECT_THAT(unexpected_error_info.js_exception_locations(),
                            ElementsAre(UnexpectedErrorInfoProto::JS_FLOW));
                EXPECT_THAT(unexpected_error_info.js_exception_line_numbers(),
                            ElementsAre(0));
                EXPECT_THAT(unexpected_error_info.js_exception_column_numbers(),
                            ElementsAre(6));
              }),
          RunOnceCallback<6>(net::HTTP_OK,
                             ActionsResponseProto().SerializeAsString(),
                             ServiceRequestSender::ResponseInfo{})));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  Run(actions_response_1);
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest, JsFlowNestedError) {
  ActionsResponseProto actions_response_1;
  actions_response_1.set_js_flow_library(
      "function throwError() {throw new Error();}");
  actions_response_1.add_actions()->mutable_js_flow()->set_js_flow(
      "throwError();");

  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(DoAll(
          WithArgs<3>(
              [](const std::vector<ProcessedActionProto>& processed_actions) {
                ASSERT_THAT(processed_actions, SizeIs(1));

                const auto& processed_action = processed_actions[0];
                EXPECT_THAT(processed_action,
                            Property(&ProcessedActionProto::status,
                                     UNEXPECTED_JS_ERROR));

                const auto& unexpected_error_info =
                    processed_action.status_details().unexpected_error_info();

                EXPECT_THAT(
                    unexpected_error_info.js_exception_locations(),
                    ElementsAre(UnexpectedErrorInfoProto::JS_FLOW_LIBRARY,
                                UnexpectedErrorInfoProto::JS_FLOW));
                EXPECT_THAT(unexpected_error_info.js_exception_line_numbers(),
                            ElementsAre(0, 0));
                EXPECT_THAT(unexpected_error_info.js_exception_column_numbers(),
                            ElementsAre(29, 0));
              }),
          RunOnceCallback<6>(net::HTTP_OK,
                             ActionsResponseProto().SerializeAsString(),
                             ServiceRequestSender::ResponseInfo{})));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  Run(actions_response_1);
}

// Stack traces in async functions do not contain the call added by the wrapper.
// We need to make sure that we do not remove an entry.
IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest, JsFlowAsyncError) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_js_flow()->set_js_flow(
      "async function throwError() {\n"
      "  await new Promise(r => setTimeout(r, 1));\n"
      "  throw new Error();"
      "}"
      "await throwError();");

  EXPECT_CALL(mock_service_, GetNextActions)
      .WillOnce(DoAll(
          WithArgs<3>(
              [](const std::vector<ProcessedActionProto>& processed_actions) {
                ASSERT_THAT(processed_actions, SizeIs(1));

                const auto& processed_action = processed_actions[0];
                EXPECT_THAT(processed_action,
                            Property(&ProcessedActionProto::status,
                                     UNEXPECTED_JS_ERROR));

                const auto& unexpected_error_info =
                    processed_action.status_details().unexpected_error_info();

                EXPECT_THAT(unexpected_error_info.js_exception_locations(),
                            ElementsAre(UnexpectedErrorInfoProto::JS_FLOW));
                EXPECT_THAT(unexpected_error_info.js_exception_line_numbers(),
                            ElementsAre(2));
                EXPECT_THAT(unexpected_error_info.js_exception_column_numbers(),
                            ElementsAre(8));
              }),
          RunOnceCallback<6>(net::HTTP_OK,
                             ActionsResponseProto().SerializeAsString(),
                             ServiceRequestSender::ResponseInfo{})));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  Run(actions_response);
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest, WaitForDomSucceeds) {
  WaitForDomProto wait_for_dom;
  wait_for_dom.mutable_wait_condition()
      ->mutable_match()
      ->add_filters()
      ->set_css_selector("#button");

  RunJsFlow(
      CreateRunNativeActionCallReturn(&wait_for_dom, ActionProto::kWaitForDom),
      ACTION_APPLIED);
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest, WaitForDomFails) {
  WaitForDomProto wait_for_dom;
  wait_for_dom.mutable_wait_condition()
      ->mutable_match()
      ->add_filters()
      ->set_css_selector("#not-found");

  RunJsFlow(
      CreateRunNativeActionCallReturn(&wait_for_dom, ActionProto::kWaitForDom),
      ELEMENT_RESOLUTION_FAILED);
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest, ReportProgress) {
  ActionsResponseProto actions_response;
  *actions_response.add_actions()
       ->mutable_report_progress()
       ->mutable_payload() = "payload";

  EXPECT_CALL(mock_service_, ReportProgress(_, "payload", _))
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, std::string(),
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(mock_service_, GetNextActions)
      .WillRepeatedly(RunOnceCallback<6>(net::HTTP_OK, /* response= */ "",
                                         ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  Run(actions_response);
}
}  // namespace
}  // namespace autofill_assistant
