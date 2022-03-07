// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/js_flow_util.h"

#include <vector>

#include "base/json/json_reader.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace js_flow {
namespace {

TEST(JsFlowUtilTest, SimpleValues) {
  std::string ignored_error_message;
  EXPECT_TRUE(js_flow::ContainsOnlyPrimitveValues(base::Value(),
                                                  ignored_error_message));
  EXPECT_TRUE(js_flow::ContainsOnlyPrimitveValues(base::Value(3),
                                                  ignored_error_message));
  EXPECT_TRUE(js_flow::ContainsOnlyPrimitveValues(base::Value(2.0),
                                                  ignored_error_message));
  EXPECT_TRUE(js_flow::ContainsOnlyPrimitveValues(base::Value(true),
                                                  ignored_error_message));
  EXPECT_FALSE(js_flow::ContainsOnlyPrimitveValues(base::Value(std::string()),
                                                   ignored_error_message));
  EXPECT_FALSE(js_flow::ContainsOnlyPrimitveValues(
      base::Value(std::string("test")), ignored_error_message));
  EXPECT_FALSE(js_flow::ContainsOnlyPrimitveValues(
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
  EXPECT_TRUE(js_flow::ContainsOnlyPrimitveValues(CreateTestValue(),
                                                  ignored_error_message));
}

TEST(JsFlowUtilTest, DictContainingStringNotAllowed) {
  std::string ignored_error_message;
  base::Value key_a_is_string = CreateTestValue();
  key_a_is_string.GetIfDict()->Set("keyA", "not allowed");
  EXPECT_FALSE(js_flow::ContainsOnlyPrimitveValues(key_a_is_string,
                                                   ignored_error_message));
}

TEST(JsFlowUtilTest, DictContainingBinaryNotAllowed) {
  std::string ignored_error_message;
  base::Value key_a_is_binary = CreateTestValue();
  key_a_is_binary.GetIfDict()->Set("keyA",
                                   std::vector<uint8_t>{'t', 'e', 's', 't'});
  EXPECT_FALSE(js_flow::ContainsOnlyPrimitveValues(key_a_is_binary,
                                                   ignored_error_message));
}

TEST(JsFlowUtilTest, NestedDictContainingStringNotAllowed) {
  std::string ignored_error_message;
  base::Value nested_key_is_string = CreateTestValue();
  nested_key_is_string.GetIfDict()->SetByDottedPath("keyE.nestedA",
                                                    "not allowed");
  EXPECT_FALSE(js_flow::ContainsOnlyPrimitveValues(nested_key_is_string,
                                                   ignored_error_message));
}

TEST(JsFlowUtilTest, NestedDictContainingBinaryNotAllowed) {
  std::string ignored_error_message;
  base::Value nested_key_is_binary = CreateTestValue();
  nested_key_is_binary.GetIfDict()->SetByDottedPath(
      "keyE.nestedA", std::vector<uint8_t>{'t', 'e', 's', 't'});
  EXPECT_FALSE(js_flow::ContainsOnlyPrimitveValues(nested_key_is_binary,
                                                   ignored_error_message));
}

TEST(JsFlowUtilTest, NestedListContainingStringsNotAllowed) {
  std::string ignored_error_message;
  base::Value nested_list_contains_strings = CreateTestValue();
  base::Value::List list_containing_strings;
  list_containing_strings.Append("not allowed");
  nested_list_contains_strings.GetIfDict()->SetByDottedPath(
      "keyE.nestedE", std::move(list_containing_strings));
  EXPECT_FALSE(js_flow::ContainsOnlyPrimitveValues(nested_list_contains_strings,
                                                   ignored_error_message));
}

TEST(JsFlowUtilTest, NestedListContainingBinaryNotAllowed) {
  std::string ignored_error_message;
  base::Value nested_list_contains_binary = CreateTestValue();
  base::Value::List list_containing_binary;
  list_containing_binary.Append(std::vector<uint8_t>{'t', 'e', 's', 't'});
  nested_list_contains_binary.GetIfDict()->SetByDottedPath(
      "keyE.nestedE", std::move(list_containing_binary));
  EXPECT_FALSE(js_flow::ContainsOnlyPrimitveValues(nested_list_contains_binary,
                                                   ignored_error_message));
}

TEST(JsFlowUtilTest, NestedListContainingDictWithStringsNotAllowed) {
  std::string ignored_error_message;
  base::Value list_contains_dict_with_strings = CreateTestValue();
  base::Value::List* key_g_list =
      list_contains_dict_with_strings.GetIfDict()->FindList("keyG");
  ASSERT_TRUE(key_g_list != nullptr);
  key_g_list->front().GetIfDict()->Set("key", "not allowed");
  EXPECT_FALSE(js_flow::ContainsOnlyPrimitveValues(
      list_contains_dict_with_strings, ignored_error_message));
}

}  // namespace
}  // namespace js_flow
}  // namespace autofill_assistant
