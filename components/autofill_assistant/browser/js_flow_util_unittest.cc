// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/js_flow_util.h"

#include <vector>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/test/values_test_util.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace js_flow_util {
namespace {

using ::base::test::IsJson;

TEST(JsFlowUtilTest, SimpleValues) {
  std::string ignored_error_message;
  EXPECT_TRUE(ContainsOnlyAllowedValues(base::Value(), ignored_error_message));
  EXPECT_TRUE(ContainsOnlyAllowedValues(base::Value(3), ignored_error_message));
  EXPECT_TRUE(
      ContainsOnlyAllowedValues(base::Value(2.0), ignored_error_message));
  EXPECT_TRUE(
      ContainsOnlyAllowedValues(base::Value(true), ignored_error_message));
  EXPECT_FALSE(ContainsOnlyAllowedValues(base::Value(std::string()),
                                         ignored_error_message));
  EXPECT_FALSE(
      ContainsOnlyAllowedValues(base::Value("test"), ignored_error_message));
  EXPECT_FALSE(ContainsOnlyAllowedValues(
      base::Value(std::vector<uint8_t>{'b', 'i', 'n', 'a', 'r', 'y'}),
      ignored_error_message));
}

// Creates a complex value that contains only primitive data types.
base::Value CreateTestValue() {
  return *base::JSONReader::Read(
      R"(
      {
        "keyA":null,
        "keyB":3,
        "keyC": 2.0,
        "keyD": true,
        "keyE": {
          "nestedA": null,
          "nestedB": 4,
          "nestedC": 3.0,
          "nestedD": false,
          "nestedE": [false, false, true]
        },
        "keyF": [1,2,3,4,5],
        "keyG": [{"key":true}, {"key":false}]
      }
  )");
}

TEST(JsFlowUtilTest, ComplexAllowedValue) {
  std::string ignored_error_message;
  EXPECT_TRUE(
      ContainsOnlyAllowedValues(CreateTestValue(), ignored_error_message));
}

TEST(JsFlowUtilTest, DictContainingStringNotAllowed) {
  std::string ignored_error_message;
  base::Value key_a_is_string = CreateTestValue();
  key_a_is_string.GetIfDict()->Set("keyA", "not allowed");
  EXPECT_FALSE(
      ContainsOnlyAllowedValues(key_a_is_string, ignored_error_message));
}

TEST(JsFlowUtilTest, DictContainingBinaryNotAllowed) {
  std::string ignored_error_message;
  base::Value key_a_is_binary = CreateTestValue();
  key_a_is_binary.GetIfDict()->Set("keyA",
                                   std::vector<uint8_t>{'t', 'e', 's', 't'});
  EXPECT_FALSE(
      ContainsOnlyAllowedValues(key_a_is_binary, ignored_error_message));
}

TEST(JsFlowUtilTest, NestedDictContainingStringNotAllowed) {
  std::string ignored_error_message;
  base::Value nested_key_is_string = CreateTestValue();
  nested_key_is_string.GetIfDict()->SetByDottedPath("keyE.nestedA",
                                                    "not allowed");
  EXPECT_FALSE(
      ContainsOnlyAllowedValues(nested_key_is_string, ignored_error_message));
}

TEST(JsFlowUtilTest, NestedDictContainingBinaryNotAllowed) {
  std::string ignored_error_message;
  base::Value nested_key_is_binary = CreateTestValue();
  nested_key_is_binary.GetIfDict()->SetByDottedPath(
      "keyE.nestedA", std::vector<uint8_t>{'t', 'e', 's', 't'});
  EXPECT_FALSE(
      ContainsOnlyAllowedValues(nested_key_is_binary, ignored_error_message));
}

TEST(JsFlowUtilTest, NestedListContainingStringsNotAllowed) {
  std::string ignored_error_message;
  base::Value nested_list_contains_strings = CreateTestValue();
  base::Value::List list_containing_strings;
  list_containing_strings.Append("not allowed");
  nested_list_contains_strings.GetIfDict()->SetByDottedPath(
      "keyE.nestedE", std::move(list_containing_strings));
  EXPECT_FALSE(ContainsOnlyAllowedValues(nested_list_contains_strings,
                                         ignored_error_message));
}

TEST(JsFlowUtilTest, NestedListContainingBinaryNotAllowed) {
  std::string ignored_error_message;
  base::Value nested_list_contains_binary = CreateTestValue();
  base::Value::List list_containing_binary;
  list_containing_binary.Append(std::vector<uint8_t>{'t', 'e', 's', 't'});
  nested_list_contains_binary.GetIfDict()->SetByDottedPath(
      "keyE.nestedE", std::move(list_containing_binary));
  EXPECT_FALSE(ContainsOnlyAllowedValues(nested_list_contains_binary,
                                         ignored_error_message));
}

TEST(JsFlowUtilTest, NestedListContainingDictWithStringsNotAllowed) {
  std::string ignored_error_message;
  base::Value list_contains_dict_with_strings = CreateTestValue();
  base::Value::List* key_g_list =
      list_contains_dict_with_strings.GetIfDict()->FindList("keyG");
  ASSERT_TRUE(key_g_list != nullptr);
  key_g_list->front().GetIfDict()->Set("key", "not allowed");
  EXPECT_FALSE(ContainsOnlyAllowedValues(list_contains_dict_with_strings,
                                         ignored_error_message));
}

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
  ClientStatus status = ExtractFlowReturnValue(
      devtools_status, devtools_result.get(), out_flow_value,
      /* js_line_offset= */ 0, /* num_stack_entries_to_drop= */ 0);
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

TEST(JsFlowUtilTest, NativeActionResultToResultValueHasEmptyActionResult) {
  ProcessedActionProto processed_action;

  EXPECT_THAT(NativeActionResultToResultValue(processed_action),
              Pointee(IsJson(R"(
        { "navigationStarted": false }
  )")));
}

}  // namespace
}  // namespace js_flow_util
}  // namespace autofill_assistant
