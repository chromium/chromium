// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/js_flow_action.h"
#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/js_flow_executor.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::IsJson;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Property;
using ::testing::SaveArgPointee;
using ::testing::WithArg;

// Parses |json| as a base::Value. No error handling - this will crash for
// invalid json inputs.
std::unique_ptr<base::Value> UniqueValueFromJson(const std::string& json) {
  return std::make_unique<base::Value>(
      std::move(*base::JSONReader::Read(json)));
}

}  // namespace

class MockJsFlowExecutor : public JsFlowExecutor {
 public:
  MockJsFlowExecutor() = default;
  ~MockJsFlowExecutor() override = default;

  MOCK_METHOD(
      void,
      Start,
      (const std::string& js_flow,
       base::OnceCallback<void(const ClientStatus&,
                               std::unique_ptr<base::Value>)> result_callback),
      (override));
};

class JsFlowActionTest : public testing::Test {
 protected:
  std::unique_ptr<JsFlowAction> CreateAction(
      std::unique_ptr<JsFlowExecutor> js_flow_executor) {
    ActionProto action_proto;
    *action_proto.mutable_js_flow() = proto_;
    std::unique_ptr<JsFlowAction> action = std::make_unique<JsFlowAction>(
        &mock_action_delegate_, action_proto, std::move(js_flow_executor));
    return action;
  }

  ProcessedActionProto* GetInnerProcessedAction(JsFlowAction& js_flow_action) {
    return js_flow_action.current_native_action_->processed_action_proto_.get();
  }

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  base::MockCallback<base::OnceCallback<
      void(const ClientStatus&, std::unique_ptr<base::Value> result_value)>>
      native_action_callback_;
  JsFlowProto proto_;
};

TEST_F(JsFlowActionTest, SmokeTest) {
  auto mock_js_flow_executor = std::make_unique<MockJsFlowExecutor>();

  EXPECT_CALL(*mock_js_flow_executor, Start("", _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(ACTION_APPLIED),
                                   /* return_value = */ nullptr));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CreateAction(std::move(mock_js_flow_executor))
      ->ProcessAction(callback_.Get());
}

TEST_F(JsFlowActionTest, FailsIfNativeActionIsInvalid) {
  auto mock_js_flow_executor = std::make_unique<MockJsFlowExecutor>();
  auto* mock_js_flow_executor_ptr = mock_js_flow_executor.get();
  auto action = CreateAction(std::move(mock_js_flow_executor));

  EXPECT_CALL(*mock_js_flow_executor_ptr, Start).WillOnce([&]() {
    // invalid action_id
    EXPECT_CALL(
        native_action_callback_,
        Run(Property(&ClientStatus::proto_status, UNSUPPORTED_ACTION), _));
    action->RunNativeAction(
        /* action_id = */ -1,
        /* action = */ std::string(), native_action_callback_.Get());
  });
  action->ProcessAction(callback_.Get());
}

TEST_F(JsFlowActionTest, FailsForInvalidNativeActionProto) {
  auto mock_js_flow_executor = std::make_unique<MockJsFlowExecutor>();
  auto* mock_js_flow_executor_ptr = mock_js_flow_executor.get();
  auto action = CreateAction(std::move(mock_js_flow_executor));

  EXPECT_CALL(*mock_js_flow_executor_ptr, Start).WillOnce([&]() {
    // invalid proto.
    EXPECT_CALL(native_action_callback_,
                Run(Property(&ClientStatus::proto_status, INVALID_ACTION), _));
    action->RunNativeAction(
        /* action_id = */ 11,
        /* action = */ "\xff\xff\xff", native_action_callback_.Get());
  });
  action->ProcessAction(callback_.Get());
}

TEST_F(JsFlowActionTest, NestedFlowActionsAreDisallowed) {
  auto mock_js_flow_executor = std::make_unique<MockJsFlowExecutor>();
  auto* mock_js_flow_executor_ptr = mock_js_flow_executor.get();
  auto action = CreateAction(std::move(mock_js_flow_executor));

  EXPECT_CALL(*mock_js_flow_executor_ptr, Start).WillOnce([&]() {
    // nested flows are disallowed. 92 == js_flow action.
    EXPECT_CALL(native_action_callback_,
                Run(Property(&ClientStatus::proto_status, INVALID_ACTION), _));
    action->RunNativeAction(
        /* action_id = */ 92,
        /* action = */ "", native_action_callback_.Get());
  });
  action->ProcessAction(callback_.Get());
}

TEST_F(JsFlowActionTest, NativeActionSucceeds) {
  auto mock_js_flow_executor = std::make_unique<MockJsFlowExecutor>();
  auto* mock_js_flow_executor_ptr = mock_js_flow_executor.get();
  auto action = CreateAction(std::move(mock_js_flow_executor));

  ActionProto native_action;
  native_action.mutable_tell()->set_message("Hello World!");
  EXPECT_CALL(*mock_js_flow_executor_ptr, Start)
      .WillOnce(WithArg<1>([&](auto finished_callback) {
        std::string serialized_native_action;
        native_action.tell().SerializeToString(&serialized_native_action);

        EXPECT_CALL(mock_action_delegate_, SetStatusMessage("Hello World!"));
        EXPECT_CALL(
            native_action_callback_,
            Run(Property(&ClientStatus::proto_status, ACTION_APPLIED), _));
        action->RunNativeAction(
            /* action_id = */ static_cast<int>(
                native_action.action_info_case()),
            /* action = */
            serialized_native_action, native_action_callback_.Get());
        std::move(finished_callback).Run(ClientStatus(ACTION_APPLIED), nullptr);
      }));
  ProcessedActionProto processed_action_capture;
  EXPECT_CALL(callback_, Run)
      .WillOnce(SaveArgPointee<0>(&processed_action_capture));
  action->ProcessAction(callback_.Get());

  EXPECT_THAT(processed_action_capture.status(), Eq(ACTION_APPLIED));
}

TEST_F(JsFlowActionTest, NativeActionFails) {
  auto mock_js_flow_executor = std::make_unique<MockJsFlowExecutor>();
  auto* mock_js_flow_executor_ptr = mock_js_flow_executor.get();
  auto action = CreateAction(std::move(mock_js_flow_executor));

  ActionProto native_action;
  // ShowCast without a selector is invalid.
  native_action.mutable_show_cast();
  EXPECT_CALL(*mock_js_flow_executor_ptr, Start)
      .WillOnce(WithArg<1>([&](auto finished_callback) {
        std::string serialized_native_action;
        native_action.show_cast().SerializeToString(&serialized_native_action);

        EXPECT_CALL(
            native_action_callback_,
            Run(Property(&ClientStatus::proto_status, INVALID_SELECTOR), _));
        action->RunNativeAction(
            /* action_id = */ static_cast<int>(
                native_action.action_info_case()),
            /* action = */
            serialized_native_action, native_action_callback_.Get());

        // Note: it is possible for the flow to succeed even if a native action
        // fails.
        std::move(finished_callback).Run(ClientStatus(ACTION_APPLIED), nullptr);
      }));
  ProcessedActionProto processed_action_capture;
  EXPECT_CALL(callback_, Run)
      .WillOnce(SaveArgPointee<0>(&processed_action_capture));
  action->ProcessAction(callback_.Get());
  EXPECT_THAT(processed_action_capture.status(), Eq(ACTION_APPLIED));
}

TEST_F(JsFlowActionTest, WritesFlowResultAsJsonDictToActionResult) {
  auto mock_js_flow_executor = std::make_unique<MockJsFlowExecutor>();

  EXPECT_CALL(*mock_js_flow_executor, Start("", _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(ACTION_APPLIED),
                                   /* return_value = */ UniqueValueFromJson(R"(
        {
          "status": 3,
          "result": [[1, 2], null, {"enum": 5}]
        }
  )")));

  ProcessedActionProto processed_action_capture;
  EXPECT_CALL(callback_, Run)
      .WillOnce(SaveArgPointee<0>(&processed_action_capture));
  CreateAction(std::move(mock_js_flow_executor))
      ->ProcessAction(callback_.Get());

  // OTHER_ACTION_STATUS == 3
  EXPECT_THAT(processed_action_capture.status(), Eq(OTHER_ACTION_STATUS));
  EXPECT_EQ(base::JSONReader::Read(
                processed_action_capture.js_flow_result().result_json()),
            base::JSONReader::Read(R"([[1, 2], null, {"enum": 5}])"));
}

TEST_F(JsFlowActionTest, AllowsFlowsWithoutReturnValue) {
  auto mock_js_flow_executor = std::make_unique<MockJsFlowExecutor>();

  EXPECT_CALL(*mock_js_flow_executor, Start("", _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(ACTION_APPLIED),
                                   /* return_value = */ nullptr));

  ProcessedActionProto processed_action_capture;
  EXPECT_CALL(callback_, Run)
      .WillOnce(SaveArgPointee<0>(&processed_action_capture));
  CreateAction(std::move(mock_js_flow_executor))
      ->ProcessAction(callback_.Get());

  EXPECT_THAT(processed_action_capture.status(), Eq(ACTION_APPLIED));
  EXPECT_FALSE(processed_action_capture.js_flow_result().has_result_json());
}

TEST_F(JsFlowActionTest, AllowsFlowsReturningStatusWithoutResult) {
  auto mock_js_flow_executor = std::make_unique<MockJsFlowExecutor>();

  EXPECT_CALL(*mock_js_flow_executor, Start("", _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(ACTION_APPLIED),
                                   /* return_value = */ UniqueValueFromJson(R"(
        {
          "status": 3
        }
  )")));

  ProcessedActionProto processed_action_capture;
  EXPECT_CALL(callback_, Run)
      .WillOnce(SaveArgPointee<0>(&processed_action_capture));
  CreateAction(std::move(mock_js_flow_executor))
      ->ProcessAction(callback_.Get());

  EXPECT_THAT(processed_action_capture.status(), Eq(OTHER_ACTION_STATUS));
  EXPECT_FALSE(processed_action_capture.js_flow_result().has_result_json());
}

TEST_F(JsFlowActionTest, RemovesOriginalProtoFromTheProcessedAction) {
  auto mock_js_flow_executor = std::make_unique<MockJsFlowExecutor>();

  EXPECT_CALL(*mock_js_flow_executor, Start)
      .WillOnce(RunOnceCallback<1>(ClientStatus(ACTION_APPLIED),
                                   /* return_value = */ nullptr));

  ProcessedActionProto processed_action_capture;
  EXPECT_CALL(callback_, Run)
      .WillOnce(SaveArgPointee<0>(&processed_action_capture));
  proto_.set_js_flow(
      "console.log('Large blob that should not be sent back to the backend');");
  CreateAction(std::move(mock_js_flow_executor))
      ->ProcessAction(callback_.Get());

  EXPECT_THAT(processed_action_capture.status(), Eq(ACTION_APPLIED));
  EXPECT_FALSE(processed_action_capture.action().js_flow().has_js_flow());
}

TEST_F(JsFlowActionTest, NativeActionReturnsNavigationStarted) {
  auto mock_js_flow_executor = std::make_unique<MockJsFlowExecutor>();
  auto* mock_js_flow_executor_ptr = mock_js_flow_executor.get();
  auto action = CreateAction(std::move(mock_js_flow_executor));

  EXPECT_CALL(*mock_js_flow_executor_ptr, Start)
      .WillOnce(WithArg<1>([&](auto finished_callback) {
        ActionProto native_action;
        native_action.mutable_wait_for_dom()->mutable_wait_condition();

        action->RunNativeAction(
            /* action_id = */ static_cast<int>(
                native_action.action_info_case()),
            /* action = */
            native_action.wait_for_dom().SerializeAsString(),
            native_action_callback_.Get());

        std::move(finished_callback).Run(ClientStatus(ACTION_APPLIED), nullptr);
      }));

  EXPECT_CALL(mock_action_delegate_, WaitForDomWithSlowWarning)
      .WillOnce(WithArg<4>([&](auto dom_finished_callback) {
        GetInnerProcessedAction(*action)
            ->mutable_navigation_info()
            ->set_started(true);

        std::move(dom_finished_callback)
            .Run(ClientStatus(ACTION_APPLIED), base::Seconds(0));
      }));

  EXPECT_CALL(native_action_callback_, Run(_, Pointee(IsJson(R"(
        { "navigationStarted": true }
  )"))));

  action->ProcessAction(callback_.Get());
}

TEST_F(JsFlowActionTest, NativeActionReturnsActionResult) {
  auto mock_js_flow_executor = std::make_unique<MockJsFlowExecutor>();
  auto* mock_js_flow_executor_ptr = mock_js_flow_executor.get();
  auto action = CreateAction(std::move(mock_js_flow_executor));

  EXPECT_CALL(*mock_js_flow_executor_ptr, Start)
      .WillOnce(WithArg<1>([&](auto finished_callback) {
        ActionProto native_action;
        native_action.mutable_wait_for_dom()->mutable_wait_condition();

        action->RunNativeAction(
            /* action_id = */ static_cast<int>(
                native_action.action_info_case()),
            /* action = */
            native_action.wait_for_dom().SerializeAsString(),
            native_action_callback_.Get());

        std::move(finished_callback).Run(ClientStatus(ACTION_APPLIED), nullptr);
      }));

  WaitForDomProto::Result wait_for_dom_result;
  wait_for_dom_result.add_matching_condition_tags("1");
  wait_for_dom_result.add_matching_condition_tags("2");
  std::string wait_for_dom_result_base64;
  base::Base64Encode(wait_for_dom_result.SerializeAsString(),
                     &wait_for_dom_result_base64);

  EXPECT_CALL(mock_action_delegate_, WaitForDomWithSlowWarning)
      .WillOnce(WithArg<4>([&](auto dom_finished_callback) {
        *GetInnerProcessedAction(*action)->mutable_wait_for_dom_result() =
            wait_for_dom_result;
        std::move(dom_finished_callback)
            .Run(ClientStatus(ACTION_APPLIED), base::Seconds(0));
      }));

  EXPECT_CALL(native_action_callback_, Run(_, Pointee(IsJson(R"(
        {
          "navigationStarted": false,
          "actionSpecificResult": ")" + wait_for_dom_result_base64 +
                                                             "\"}"))));

  action->ProcessAction(callback_.Get());
}

TEST_F(JsFlowActionTest, NativeActionReturnsAutofillErrorInfo) {
  auto mock_js_flow_executor = std::make_unique<MockJsFlowExecutor>();
  auto* mock_js_flow_executor_ptr = mock_js_flow_executor.get();
  auto action = CreateAction(std::move(mock_js_flow_executor));

  EXPECT_CALL(*mock_js_flow_executor_ptr, Start)
      .WillOnce(WithArg<1>([&](auto finished_callback) {
        ActionProto native_action;
        native_action.mutable_wait_for_dom()->mutable_wait_condition();

        action->RunNativeAction(
            /* action_id = */ static_cast<int>(
                native_action.action_info_case()),
            /* action = */
            native_action.wait_for_dom().SerializeAsString(),
            native_action_callback_.Get());

        std::move(finished_callback).Run(ClientStatus(ACTION_APPLIED), nullptr);
      }));

  AutofillErrorInfoProto autofill_error_info;
  autofill_error_info.set_client_memory_address_key_names("key_names");
  std::string autofill_error_info_base64;
  base::Base64Encode(autofill_error_info.SerializeAsString(),
                     &autofill_error_info_base64);

  EXPECT_CALL(mock_action_delegate_, WaitForDomWithSlowWarning)
      .WillOnce(WithArg<4>([&](auto dom_finished_callback) {
        *GetInnerProcessedAction(*action)
             ->mutable_status_details()
             ->mutable_autofill_error_info() = autofill_error_info;
        std::move(dom_finished_callback)
            .Run(ClientStatus(ACTION_APPLIED), base::Seconds(0));
      }));

  EXPECT_CALL(native_action_callback_, Run(_, Pointee(IsJson(R"(
        {
          "navigationStarted": false,
          "autofillErrorInfo": ")" + autofill_error_info_base64 +
                                                             "\"}"))));

  action->ProcessAction(callback_.Get());
}

}  // namespace autofill_assistant
