// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/android/policy_converter.h"

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/json/json_writer.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/policy/core/common/schema.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Value;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace policy {
namespace android {

class PolicyConverterTest : public testing::Test {
 public:
  void SetUp() override {
    const char kSchemaTemplate[] =
        R"({
        "type": "object",
        "properties": {
          "string": { "type": "string" },
          "int": { "type": "integer" },
          "bool": { "type": "boolean" },
          "double": { "type": "number" },
          "list": {
            "type": "array",
            "items": { "type": "string" }
          },
          "dict": { "type": "object" }
        }
      })";
    ASSIGN_OR_RETURN(schema_, Schema::Parse(kSchemaTemplate),
                     [](const auto& e) { ADD_FAILURE() << e; });
  }

 protected:
  // Converts the passed in value to the passed in schema, and serializes the
  // result to JSON, to make it easier to compare with EXPECT_EQ.
  std::string Convert(Value value, const Schema& value_schema) {
    std::optional<base::Value> converted_value =
        PolicyConverter::ConvertValueToSchema(std::move(value), value_schema);
    EXPECT_TRUE(converted_value.has_value());

    std::string json_string;
    EXPECT_TRUE(base::JSONWriter::Write(converted_value.value(), &json_string));
    return json_string;
  }

  // Uses `PolicyConverter::ConvertJavaStringArrayToListValue` to convert the
  // passed in java array and serializes the result to JSON, to make it easier
  // to compare with `EXPECT_EQ`.
  std::string ConvertJavaStringArrayToListValue(
      JNIEnv* env,
      const JavaRef<jobjectArray>& java_array) {
    base::Value::List list =
        PolicyConverter::ConvertJavaStringArrayToListValue(env, java_array);

    std::string json_string;
    EXPECT_TRUE(base::JSONWriter::Write(list, &json_string));

    return json_string;
  }

  // Converts the passed in values to a java string array
  ScopedJavaLocalRef<jobjectArray> MakeJavaStringArray(
      JNIEnv* env,
      std::vector<std::string> values) {
    jobjectArray java_array = (jobjectArray)env->NewObjectArray(
        values.size(), jni_zero::g_string_class, nullptr);
    for (size_t i = 0; i < values.size(); i++) {
      env->SetObjectArrayElement(
          java_array, i,
          base::android::ConvertUTF8ToJavaString(env, values[i]).obj());
    }

    return ScopedJavaLocalRef<jobjectArray>(env, java_array);
  }

  Schema schema_;
};

TEST_F(PolicyConverterTest, ConvertToBoolValue) {
  Schema bool_schema = schema_.GetKnownProperty("bool");
  ASSERT_TRUE(bool_schema.valid());

  EXPECT_EQ("true", Convert(Value(true), bool_schema));
  EXPECT_EQ("false", Convert(Value(false), bool_schema));
  EXPECT_EQ("true", Convert(Value("true"), bool_schema));
  EXPECT_EQ("false", Convert(Value("false"), bool_schema));
  EXPECT_EQ("\"narf\"", Convert(Value("narf"), bool_schema));
  EXPECT_EQ("false", Convert(Value(0), bool_schema));
  EXPECT_EQ("true", Convert(Value(1), bool_schema));
  EXPECT_EQ("true", Convert(Value(42), bool_schema));
  EXPECT_EQ("true", Convert(Value(-1), bool_schema));
  EXPECT_EQ("\"1\"", Convert(Value("1"), bool_schema));
  EXPECT_EQ("{}", Convert(Value(Value::Type::DICT), bool_schema));
}

TEST_F(PolicyConverterTest, ConvertToIntValue) {
  Schema int_schema = schema_.GetKnownProperty("int");
  ASSERT_TRUE(int_schema.valid());

  EXPECT_EQ("23", Convert(Value(23), int_schema));
  EXPECT_EQ("42", Convert(Value("42"), int_schema));
  EXPECT_EQ("-1", Convert(Value("-1"), int_schema));
  EXPECT_EQ("\"poit\"", Convert(Value("poit"), int_schema));
  EXPECT_EQ("false", Convert(Value(false), int_schema));
}

TEST_F(PolicyConverterTest, ConvertToDoubleValue) {
  Schema double_schema = schema_.GetKnownProperty("double");
  ASSERT_TRUE(double_schema.valid());

  EXPECT_EQ("3", Convert(Value(3), double_schema));
  EXPECT_EQ("3.14", Convert(Value(3.14), double_schema));
  EXPECT_EQ("2.71", Convert(Value("2.71"), double_schema));
  EXPECT_EQ("\"zort\"", Convert(Value("zort"), double_schema));
  EXPECT_EQ("true", Convert(Value(true), double_schema));
}

TEST_F(PolicyConverterTest, ConvertToStringValue) {
  Schema string_schema = schema_.GetKnownProperty("string");
  ASSERT_TRUE(string_schema.valid());

  EXPECT_EQ("\"troz\"", Convert(Value("troz"), string_schema));
  EXPECT_EQ("4711", Convert(Value(4711), string_schema));
}

TEST_F(PolicyConverterTest, ConvertToListValue) {
  Schema list_schema = schema_.GetKnownProperty("list");
  ASSERT_TRUE(list_schema.valid());

  Value::List list;
  list.Append("foo");
  list.Append("bar");
  EXPECT_EQ("[\"foo\",\"bar\"]", Convert(Value(std::move(list)), list_schema));
  EXPECT_EQ("[\"baz\",\"blurp\"]",
            Convert(Value("[\"baz\", \"blurp\"]"), list_schema));
  EXPECT_EQ("[\"hurz\"]", Convert(Value("hurz"), list_schema));
  EXPECT_EQ("[\"foo\",\"bar\"]", Convert(Value("foo,bar"), list_schema));
  EXPECT_EQ("[\"foo\",\"bar\"]", Convert(Value("foo, bar"), list_schema));
  EXPECT_EQ("19", Convert(Value(19), list_schema));

  EXPECT_FALSE(PolicyConverter::ConvertValueToSchema(Value(""), list_schema)
                   .has_value());
}

TEST_F(PolicyConverterTest, ConvertFromJavaListToListValue) {
  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_EQ("[\"foo\",\"bar\",\"baz\"]",
            ConvertJavaStringArrayToListValue(
                env, MakeJavaStringArray(env, {"foo", "bar", "baz"})));
  EXPECT_EQ("[]", ConvertJavaStringArrayToListValue(
                      env, MakeJavaStringArray(env, {})));
}

TEST_F(PolicyConverterTest, ConvertToDictValue) {
  Schema dict_schema = schema_.GetKnownProperty("dict");
  ASSERT_TRUE(dict_schema.valid());

  base::Value::Dict dict;
  dict.Set("thx", 1138);
  EXPECT_EQ("{\"thx\":1138}", Convert(Value(std::move(dict)), dict_schema));
  EXPECT_EQ("{\"moose\":true}",
            Convert(Value("{\"moose\": true}"), dict_schema));
  EXPECT_EQ("\"fnord\"", Convert(Value("fnord"), dict_schema));
  EXPECT_EQ("1729", Convert(Value(1729), dict_schema));

  EXPECT_FALSE(PolicyConverter::ConvertValueToSchema(Value(""), dict_schema)
                   .has_value());
}

}  // namespace android
}  // namespace policy
