// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/js_flow_util.h"

#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/test/values_test_util.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/switches.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant::js_flow_util {
namespace {

using ::base::test::IsJson;

TEST(JsFlowUtilTest, ExtractFlowReturnValue) {
  // Note: this is tested much more extensively in the JsFlowExecutorImpl.
  DevtoolsClient::ReplyStatus devtools_status;
  std::unique_ptr<runtime::EvaluateResult> devtools_result =
      runtime::EvaluateResult::Builder()
          .SetResult(runtime::RemoteObject::Builder()
                         .SetType(runtime::RemoteObjectType::NUMBER)
                         .SetValue(std::make_unique<base::Value>(12345))
                         .Build())
          .Build();

  std::unique_ptr<base::Value> out_flow_value;
  ClientStatus status =
      ExtractFlowReturnValue(devtools_status, devtools_result.get(),
                             out_flow_value, /* js_line_offsets= */ {});
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(*out_flow_value, base::Value(12345));
}

TEST(JsFlowUtilTest, ExtractJsFlowActionReturnValueAllowsNullValue) {
  std::unique_ptr<base::Value> out_result_value;
  EXPECT_EQ(ExtractJsFlowActionReturnValue(base::Value(), out_result_value)
                .proto_status(),
            ACTION_APPLIED);
  EXPECT_EQ(out_result_value, nullptr);
}

TEST(JsFlowUtilTest, ExtractJsFlowActionReturnValueDisallowsNonDictValues) {
  std::unique_ptr<base::Value> out_result_value;
  EXPECT_EQ(ExtractJsFlowActionReturnValue(base::Value(1), out_result_value)
                .proto_status(),
            INVALID_ACTION);
  EXPECT_EQ(out_result_value, nullptr);
}

TEST(JsFlowUtilTest, ExtractJsFlowActionReturnValueDisallowsInvalidDict) {
  std::unique_ptr<base::Value> out_result_value;

  // Empty dict.
  EXPECT_EQ(ExtractJsFlowActionReturnValue(base::Value(base::Value::Dict()),
                                           out_result_value)
                .proto_status(),
            INVALID_ACTION);
  EXPECT_EQ(out_result_value, nullptr);

  // Invalid dict (does not contain 'status' field).
  base::Value::Dict dict;
  dict.Set("foo", 12345);
  EXPECT_EQ(ExtractJsFlowActionReturnValue(base::Value(std::move(dict)),
                                           out_result_value)
                .proto_status(),
            INVALID_ACTION);
  EXPECT_EQ(out_result_value, nullptr);
}

TEST(JsFlowUtilTest,
     ExtractJsFlowActionReturnValueDisallowsResultWithoutStatus) {
  std::unique_ptr<base::Value> out_result_value;
  base::Value::Dict dict;
  dict.Set("result", 12345);
  EXPECT_EQ(ExtractJsFlowActionReturnValue(base::Value(std::move(dict)),
                                           out_result_value)
                .proto_status(),
            INVALID_ACTION);
  EXPECT_EQ(out_result_value, nullptr);
}

TEST(JsFlowUtilTest, ExtractJsFlowActionReturnValueDisallowsInvalidStatus) {
  std::unique_ptr<base::Value> out_result_value;
  base::Value::Dict dict;
  dict.Set("status", -1);
  EXPECT_EQ(ExtractJsFlowActionReturnValue(base::Value(std::move(dict)),
                                           out_result_value)
                .proto_status(),
            INVALID_ACTION);
  EXPECT_EQ(out_result_value, nullptr);
}

TEST(JsFlowUtilTest, ExtractJsFlowActionReturnValueAllowsStatusWithoutResult) {
  std::unique_ptr<base::Value> out_result_value;
  base::Value::Dict dict;
  dict.Set("status", 3);
  EXPECT_EQ(ExtractJsFlowActionReturnValue(base::Value(std::move(dict)),
                                           out_result_value)
                .proto_status(),
            OTHER_ACTION_STATUS);
  EXPECT_EQ(out_result_value, nullptr);
}

TEST(JsFlowUtilTest, ExtractJsFlowActionReturnValueAllowsStatusWithResult) {
  std::unique_ptr<base::Value> out_result_value;
  base::Value::Dict dict;
  dict.Set("status", 3);
  dict.Set("result", *base::JSONReader::Read(R"([[1, 2], null, {"enum": 5}])"));
  EXPECT_EQ(ExtractJsFlowActionReturnValue(base::Value(std::move(dict)),
                                           out_result_value)
                .proto_status(),
            OTHER_ACTION_STATUS);
  EXPECT_EQ(*out_result_value,
            *base::JSONReader::Read(R"([[1, 2], null, {"enum": 5}])"));
}

TEST(JsFlowUtilTest, NativeActionResultToResultValueHasSerializedActionResult) {
  ProcessedActionProto processed_action;
  WaitForDomProto::Result* wait_for_dom_result =
      processed_action.mutable_wait_for_dom_result();
  wait_for_dom_result->add_matching_condition_tags("1");
  wait_for_dom_result->add_matching_condition_tags("2");
  std::string wait_for_dom_result_base64;
  base::Base64Encode(wait_for_dom_result->SerializeAsString(),
                     &wait_for_dom_result_base64);

  EXPECT_THAT(
      NativeActionResultToResultValue(processed_action), Pointee(IsJson(R"(
        {
          "navigationStarted": false,
          "actionSpecificResult": ")" + wait_for_dom_result_base64 + "\"}")));
}

TEST(JsFlowUtilTest, NativeActionResultToResultValueHasAutofillErrorInfo) {
  ProcessedActionProto processed_action;
  AutofillErrorInfoProto* autofill_error_info =
      processed_action.mutable_status_details()->mutable_autofill_error_info();
  autofill_error_info->set_client_memory_address_key_names("key_names");

  std::string autofill_error_info_base64;
  base::Base64Encode(autofill_error_info->SerializeAsString(),
                     &autofill_error_info_base64);

  EXPECT_THAT(
      NativeActionResultToResultValue(processed_action), Pointee(IsJson(R"(
        {
          "navigationStarted": false,
          "autofillErrorInfo": ")" + autofill_error_info_base64 + "\"}")));
}

TEST(JsFlowUtilTest, NativeActionResultToResultValueHasEmptyActionResult) {
  ProcessedActionProto processed_action;

  EXPECT_THAT(NativeActionResultToResultValue(processed_action),
              Pointee(IsJson(R"(
        { "navigationStarted": false }
  )")));
}

TEST(JsFlowUtilTest, ExceptionLocationToDevtoolsUrlMapping) {
  const std::string url =
      GetDevtoolsSourceUrl(UnexpectedErrorInfoProto::JS_FLOW);
  EXPECT_THAT(GetExceptionLocation(url), UnexpectedErrorInfoProto::JS_FLOW);
}

TEST(JsFlowUtilTest, UnknownUrl) {
  EXPECT_THAT(GetExceptionLocation("SOME_STRING"),
              UnexpectedErrorInfoProto::UNKNOWN);
}

TEST(JsFlowUtilTest, GetDevtoolsSourceUrlCommentToAppend) {
  EXPECT_THAT(GetDevtoolsSourceUrlCommentToAppend(
                  UnexpectedErrorInfoProto::JS_FLOW_LIBRARY),
              "\n//# sourceURL=JS_FLOW_LIBRARY");
}

TEST(JsFlowUtilTest, ExpectsDebugModeSetToTrue) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kAutofillAssistantDebugMode);
  command_line->AppendSwitchASCII(switches::kAutofillAssistantDebugMode,
                                  "true");
  EXPECT_EQ(IsDebugMode(), true);
}

TEST(JsFlowUtilTest, ExpectsDebugModeSetToFalse) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kAutofillAssistantDebugMode);
  command_line->AppendSwitchASCII(switches::kAutofillAssistantDebugMode,
                                  "false");
  EXPECT_EQ(IsDebugMode(), false);
}

TEST(JsFlowUtilTest, ExpectsDebugModeDefaultIsFalse) {
  EXPECT_EQ(IsDebugMode(), false);
}

}  // namespace
}  // namespace autofill_assistant::js_flow_util
