// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/register_self_contained_interrupt_scripts_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/mock_client.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service/mock_service_request_sender.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::WithArg;

class RegisterSelfContainedInterruptScriptsActionTest : public testing::Test {
 public:
  void SetUp() override {
    mock_service_request_sender_ =
        std::make_unique<NiceMock<MockServiceRequestSender>>();
    mock_service_request_sender_ptr_ = mock_service_request_sender_.get();

    ON_CALL(mock_action_delegate_, GetClient)
        .WillByDefault(Return(&mock_client_));
  }

  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_register_interrupt_scripts() = proto_;
    RegisterSelfContainedInterruptScriptsAction action(&mock_action_delegate_,
                                                       action_proto);
    action.service_request_sender_to_inject_ =
        std::move(mock_service_request_sender_);
    action.ProcessAction(callback_.Get());
  }

 protected:
  MockActionDelegate mock_action_delegate_;
  NiceMock<MockClient> mock_client_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  // Test expectations should be written against this raw pointer only.
  raw_ptr<NiceMock<MockServiceRequestSender>> mock_service_request_sender_ptr_;
  RegisterSelfContainedInterruptScripts proto_;

 private:
  std::unique_ptr<NiceMock<MockServiceRequestSender>>
      mock_service_request_sender_;
};

TEST_F(RegisterSelfContainedInterruptScriptsActionTest, SmokeTest) {
  proto_.mutable_match_info()
      ->mutable_supports_site_response()
      ->add_scripts()
      ->mutable_presentation()
      ->set_interrupt(true);
  proto_.mutable_match_info()->add_routine_scripts();
  EXPECT_CALL(mock_action_delegate_, AddInterruptScript);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(RegisterSelfContainedInterruptScriptsActionTest,
       FailsIfNumScriptsInconsistent) {
  proto_.mutable_match_info()
      ->mutable_supports_site_response()
      ->add_scripts()
      ->mutable_presentation()
      ->set_interrupt(true);
  proto_.mutable_match_info()->add_routine_scripts();
  proto_.mutable_match_info()->add_routine_scripts();
  EXPECT_CALL(mock_action_delegate_, AddInterruptScript).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(RegisterSelfContainedInterruptScriptsActionTest, FailsIfPathsMismatch) {
  auto* supports_site_script = proto_.mutable_match_info()
                                   ->mutable_supports_site_response()
                                   ->add_scripts();
  supports_site_script->mutable_presentation()->set_interrupt(true);
  supports_site_script->set_path("path_1");

  proto_.mutable_match_info()->add_routine_scripts()->set_script_path("path_2");
  EXPECT_CALL(mock_action_delegate_, AddInterruptScript).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(RegisterSelfContainedInterruptScriptsActionTest, SucceedsAllFieldsSet) {
  auto* supports_site_script = proto_.mutable_match_info()
                                   ->mutable_supports_site_response()
                                   ->add_scripts();
  supports_site_script->mutable_presentation()->set_interrupt(true);
  supports_site_script->set_path("interrupt_path");
  *supports_site_script->mutable_presentation()
       ->mutable_precondition()
       ->mutable_element_condition()
       ->mutable_match() = ToSelectorProto("foobar");

  auto* routine_script = proto_.mutable_match_info()->add_routine_scripts();
  routine_script->set_script_path("interrupt_path");
  routine_script->mutable_action_response()
      ->add_actions()
      ->mutable_tell()
      ->set_message("hello world");

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

  // No GetActions, GetNextActions, or SupportsSite request to the backend is
  // allowed.
  EXPECT_CALL(*mock_service_request_sender_ptr_, OnSendRequest).Times(0);
  base::MockCallback<ServiceRequestSender::ResponseCallback>
      mock_response_callback_;
  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_OK, _, _))
      .WillOnce(WithArg<1>([&](const std::string& response) {
        ActionsResponseProto actions;
        ASSERT_TRUE(actions.ParseFromString(response));
        EXPECT_EQ(actions, routine_script->action_response());
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
  EXPECT_CALL(mock_response_callback_, Run(Not(net::HTTP_OK), "", _));
  service_capture->GetScriptsForUrl(GURL("https://www.example.com"), {},
                                    mock_response_callback_.Get());

  // ReportProgress is allowed for MSBB users only.
  ON_CALL(mock_client_, GetMetricsReportingEnabled).WillByDefault(Return(true));
  EXPECT_CALL(mock_client_, GetMakeSearchesAndBrowsingBetterEnabled)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_service_request_sender_ptr_, OnSendRequest)
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "some_response",
                                   ServiceRequestSender::ResponseInfo{}));
  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_OK, "some_response", _));
  service_capture->ReportProgress({}, {}, mock_response_callback_.Get());

  // ReportProgress silently fails for non-MSBB users.
  EXPECT_CALL(mock_client_, GetMakeSearchesAndBrowsingBetterEnabled)
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_service_request_sender_ptr_, OnSendRequest).Times(0);
  // Expect HTTP_OK since ReportProgress is failing silently by default.
  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_OK, "", _));
  service_capture->ReportProgress({}, {}, mock_response_callback_.Get());
}

}  // namespace autofill_assistant
