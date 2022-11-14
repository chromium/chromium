// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/register_js_interrupt_for_parent_js_flow_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/js_flow_action.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/actions/tell_action.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::WithArg;

class RegisterJsInterruptForParentJsFlowActionTest : public testing::Test {
 public:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_register_js_interrupt_for_flow() = proto_;
    RegisterJsInterruptForParentJsFlowAction action(&mock_action_delegate_,
                                                    action_proto);
    action.ProcessAction(callback_.Get());
  }

 protected:
  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  RegisterJsInterruptForParentJsFlow proto_;
};

TEST_F(RegisterJsInterruptForParentJsFlowActionTest, FailsIfNoParentAction) {
  EXPECT_CALL(mock_action_delegate_, GetCurrentRootAction)
      .WillOnce(Return(nullptr));
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              OTHER_ACTION_STATUS))));
  Run();
}

TEST_F(RegisterJsInterruptForParentJsFlowActionTest,
       FailsIfParentActionIsNotJsFlow) {
  ActionProto tell_proto;
  tell_proto.mutable_tell();
  TellAction parent_action(&mock_action_delegate_, tell_proto);
  EXPECT_CALL(mock_action_delegate_, GetCurrentRootAction)
      .WillOnce(Return(&parent_action));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(RegisterJsInterruptForParentJsFlowActionTest, SucceedsAllFieldsSet) {
  proto_.set_path("interrupt_path");
  *proto_.mutable_precondition()->mutable_match() =
      ToSelectorProto("interrupt_condition");
  proto_.set_js_startup_variable_name("js_variable_name");
  proto_.set_js_startup_variable_value("js_variable_value");

  ActionProto parent_flow_proto;
  parent_flow_proto.mutable_js_flow()->set_js_flow("parent JS flow");
  JsFlowAction parent_action(&mock_action_delegate_, parent_flow_proto);
  EXPECT_CALL(mock_action_delegate_, GetCurrentRootAction)
      .WillOnce(Return(&parent_action));

  std::unique_ptr<Script> script_capture;
  std::unique_ptr<Service> service_capture;
  EXPECT_CALL(mock_action_delegate_, AddInterruptScript)
      .WillOnce([&](std::unique_ptr<Script> interrupt_script,
                    std::unique_ptr<Service> optional_service) {
        script_capture = std::move(interrupt_script);
        service_capture = std::move(optional_service);
      });
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();

  ASSERT_NE(script_capture, nullptr);
  ASSERT_NE(service_capture, nullptr);
  EXPECT_NE(script_capture->precondition, nullptr);
  EXPECT_EQ(script_capture->handle.path, "interrupt_path");

  ActionProto expected_interrupt_js_flow_action;
  expected_interrupt_js_flow_action.mutable_js_flow()->set_js_flow(
      "parent JS flow");
  expected_interrupt_js_flow_action.mutable_js_flow()->set_startup_param_name(
      "js_variable_name");
  expected_interrupt_js_flow_action.mutable_js_flow()->set_startup_param_value(
      "js_variable_value");

  base::MockCallback<ServiceRequestSender::ResponseCallback>
      mock_response_callback_;
  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_OK, _, _))
      .WillOnce(WithArg<1>([&](const std::string& response) {
        ActionsResponseProto actions;
        ASSERT_TRUE(actions.ParseFromString(response));
        EXPECT_THAT(actions.actions(),
                    ElementsAre(expected_interrupt_js_flow_action));
      }));
  service_capture->GetActions("interrupt_path", {}, {}, {}, {},
                              mock_response_callback_.Get());

  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_OK, _, _))
      .WillOnce(WithArg<1>([&](const std::string& response) {
        ActionsResponseProto actions;
        ASSERT_TRUE(actions.ParseFromString(response));
        EXPECT_THAT(actions.actions(), IsEmpty());
      }));
  service_capture->GetNextActions({}, {}, {}, {}, {}, {},
                                  mock_response_callback_.Get());
}

}  // namespace
}  // namespace autofill_assistant
