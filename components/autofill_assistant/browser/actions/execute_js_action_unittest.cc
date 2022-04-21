// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/execute_js_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/dom_action.pb.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

const char kClientId[] = "1";
const char kSnippet[] = "return 2;";  // ACTION_APPLIED

class ExecuteJsActionTest : public testing::Test {
 public:
  ExecuteJsActionTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetWebController)
        .WillByDefault(Return(&mock_web_controller_));
  }

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_execute_js() = proto_;
    // Keep action alive so the timeout has a chance to expire.
    action_ =
        std::make_unique<ExecuteJsAction>(&mock_action_delegate_, action_proto);
    action_->ProcessAction(callback_.Get());
  }

  base::test::TaskEnvironment task_environment_;
  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ExecuteJsProto proto_;
  std::unique_ptr<ExecuteJsAction> action_;
};

TEST_F(ExecuteJsActionTest, EmptyClientIdFails) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));

  Run();
}

TEST_F(ExecuteJsActionTest, FailsIfElementDoesNotExist) {
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              CLIENT_ID_RESOLUTION_FAILED))));

  proto_.mutable_client_id()->set_identifier(kClientId);
  Run();
}

TEST_F(ExecuteJsActionTest, ExecutesSnippetAndReturns) {
  ElementFinderResult element;
  element.SetObjectId("id");
  mock_action_delegate_.GetElementStore()->AddElement(kClientId,
                                                      element.dom_object());

  EXPECT_CALL(mock_web_controller_,
              ExecuteJS(kSnippet, EqualsElement(element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  proto_.set_js_snippet(kSnippet);
  proto_.mutable_client_id()->set_identifier(kClientId);
  Run();
}

TEST_F(ExecuteJsActionTest, TimesOut) {
  ElementFinderResult element;
  element.SetObjectId("id");
  mock_action_delegate_.GetElementStore()->AddElement(kClientId,
                                                      element.dom_object());

  // Swallow the call and don't return to let the timeout trigger.
  base::OnceCallback<void(const ClientStatus&)> captured_callback;
  EXPECT_CALL(mock_web_controller_,
              ExecuteJS(kSnippet, EqualsElement(element), _))
      .WillOnce([&captured_callback](
                    const std::string& snippet,
                    const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&)> callback) {
        captured_callback = std::move(callback);
      });

  proto_.set_js_snippet(kSnippet);
  proto_.mutable_client_id()->set_identifier(kClientId);
  proto_.set_timeout_ms(1000);
  Run();

  EXPECT_CALL(callback_,
              Run(Pointee(Property(&ProcessedActionProto::status, TIMED_OUT))));
  task_environment_.FastForwardBy(base::Milliseconds(2000));

  // This callback should be ignored, it's too late. This should not report a
  // success or crash.
  std::move(captured_callback).Run(OkClientStatus());
}

TEST_F(ExecuteJsActionTest, DoesNotTimeOut) {
  ElementFinderResult element;
  element.SetObjectId("id");
  mock_action_delegate_.GetElementStore()->AddElement(kClientId,
                                                      element.dom_object());

  EXPECT_CALL(mock_web_controller_,
              ExecuteJS(kSnippet, EqualsElement(element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  proto_.set_js_snippet(kSnippet);
  proto_.mutable_client_id()->set_identifier(kClientId);
  proto_.set_timeout_ms(1000);
  Run();

  // Moving forward in time causes the timer to expire. This should not report
  // a failure or crash.
  task_environment_.FastForwardBy(base::Milliseconds(2000));
}

}  // namespace
}  // namespace autofill_assistant
