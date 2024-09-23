// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/android/input_context_android.h"

#include <array>
#include <cstdint>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/input_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/jni_zero/default_conversions.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

namespace segmentation_platform {

class InputContextAndroidTest : public testing::Test {
 public:
  InputContextAndroidTest() = default;
  ~InputContextAndroidTest() override = default;
};

TEST_F(InputContextAndroidTest, FromJavaParams) {
  raw_ptr<JNIEnv> jni_env = base::android::AttachCurrentThread();

  scoped_refptr<segmentation_platform::InputContext> native_input_context =
      base::MakeRefCounted<InputContext>();
  base::Time time = base::Time::Now();

  const GURL test_url = GURL("https://example.com");
  const base::android::ScopedJavaLocalRef<jobject> java_gurl =
      url::GURLAndroid::FromNativeGURL(jni_env, test_url);

  const std::vector<std::string> bool_keys({"boolean_argument"});
  bool bool_values[]{true};

  const std::vector<std::string> int_keys(
      {"int_argument", "negative_int", "large_int"});
  int int_values[]{1234, -4, INT_MAX};

  const std::vector<std::string> float_keys({"float_argument"});
  float float_values[]{13.37f};

  const std::vector<std::string> double_keys({"double_argument"});
  double double_values[]{100.3};

  const std::vector<std::string> string_keys(
      {"string_argument", "second_string", "third_string"});
  const std::vector<std::string> string_values({"Hello, World!", "Foo", "bar"});

  const std::vector<std::string> time_keys({"time_argument"});
  int64_t time_values[]{time.InMillisecondsSinceUnixEpoch()};

  const std::vector<std::string> int64_keys({"int64_argument"});
  int64_t int64_values[]{123456};

  const std::vector<std::string> url_keys({"url_argument"});
  std::vector<base::android::ScopedJavaLocalRef<jobject>> url_values(
      {java_gurl});

  base::android::ScopedJavaLocalRef<jobjectArray> java_bool_keys =
      base::android::ToJavaArrayOfStrings(jni_env, bool_keys);
  base::android::ScopedJavaLocalRef<jbooleanArray> java_bool_values =
      base::android::ToJavaBooleanArray(jni_env, bool_values);

  base::android::ScopedJavaLocalRef<jobjectArray> java_int_keys =
      base::android::ToJavaArrayOfStrings(jni_env, int_keys);
  base::android::ScopedJavaLocalRef<jintArray> java_int_values =
      base::android::ToJavaIntArray(jni_env, int_values);

  base::android::ScopedJavaLocalRef<jobjectArray> java_float_keys =
      base::android::ToJavaArrayOfStrings(jni_env, float_keys);
  base::android::ScopedJavaLocalRef<jfloatArray> java_float_values =
      base::android::ToJavaFloatArray(jni_env, float_values);

  base::android::ScopedJavaLocalRef<jobjectArray> java_double_keys =
      base::android::ToJavaArrayOfStrings(jni_env, double_keys);
  base::android::ScopedJavaLocalRef<jdoubleArray> java_double_values =
      base::android::ToJavaDoubleArray(jni_env, double_values);

  base::android::ScopedJavaLocalRef<jobjectArray> java_string_keys =
      base::android::ToJavaArrayOfStrings(jni_env, string_keys);

  base::android::ScopedJavaLocalRef<jobjectArray> java_string_values =
      base::android::ToJavaArrayOfStrings(jni_env, string_values);

  base::android::ScopedJavaLocalRef<jobjectArray> java_time_keys =
      base::android::ToJavaArrayOfStrings(jni_env, time_keys);
  base::android::ScopedJavaLocalRef<jlongArray> java_time_values =
      base::android::ToJavaLongArray(jni_env, time_values);

  base::android::ScopedJavaLocalRef<jobjectArray> java_int64_keys =
      base::android::ToJavaArrayOfStrings(jni_env, int64_keys);
  base::android::ScopedJavaLocalRef<jlongArray> java_int64_values =
      base::android::ToJavaLongArray(jni_env, int64_values);

  base::android::ScopedJavaLocalRef<jobjectArray> java_url_keys =
      base::android::ToJavaArrayOfStrings(jni_env, url_keys);
  base::android::ScopedJavaLocalRef<jobjectArray> java_url_values =
      jni_zero::ToJniArray(jni_env, url_values, jni_zero::g_object_class);

  segmentation_platform::InputContextAndroid::FromJavaParams(
      jni_env, reinterpret_cast<intptr_t>(native_input_context.get()),
      java_bool_keys, java_bool_values, java_int_keys, java_int_values,
      java_float_keys, java_float_values, java_double_keys, java_double_values,
      java_string_keys, java_string_values, java_time_keys, java_time_values,
      java_int64_keys, java_int64_values, java_url_keys, java_url_values);

  ASSERT_TRUE(native_input_context->metadata_args.contains("boolean_argument"));
  ASSERT_TRUE(native_input_context->metadata_args.find("boolean_argument")
                  ->second.bool_val);

  ASSERT_TRUE(native_input_context->metadata_args.contains("int_argument"));
  ASSERT_EQ(
      1234,
      native_input_context->metadata_args.find("int_argument")->second.int_val);
  ASSERT_TRUE(native_input_context->metadata_args.contains("negative_int"));
  ASSERT_EQ(
      -4,
      native_input_context->metadata_args.find("negative_int")->second.int_val);
  ASSERT_TRUE(native_input_context->metadata_args.contains("large_int"));
  ASSERT_EQ(
      INT_MAX,
      native_input_context->metadata_args.find("large_int")->second.int_val);

  ASSERT_TRUE(native_input_context->metadata_args.contains("float_argument"));
  ASSERT_EQ(13.37f, native_input_context->metadata_args.find("float_argument")
                        ->second.float_val);

  ASSERT_TRUE(native_input_context->metadata_args.contains("double_argument"));
  ASSERT_EQ(100.3, native_input_context->metadata_args.find("double_argument")
                       ->second.double_val);

  ASSERT_TRUE(native_input_context->metadata_args.contains("string_argument"));
  ASSERT_EQ("Hello, World!",
            native_input_context->metadata_args.find("string_argument")
                ->second.str_val);
  ASSERT_TRUE(native_input_context->metadata_args.contains("second_string"));
  ASSERT_EQ("Foo", native_input_context->metadata_args.find("second_string")
                       ->second.str_val);
  ASSERT_TRUE(native_input_context->metadata_args.contains("third_string"));
  ASSERT_EQ(
      "bar",
      native_input_context->metadata_args.find("third_string")->second.str_val);

  ASSERT_TRUE(native_input_context->metadata_args.contains("time_argument"));
  // InMillisecondsSinceUnixEpoch is a lossy operation, so we must compare java
  // times.
  ASSERT_EQ(time.InMillisecondsSinceUnixEpoch(),
            native_input_context->metadata_args.find("time_argument")
                ->second.time_val.InMillisecondsSinceUnixEpoch());

  ASSERT_TRUE(native_input_context->metadata_args.contains("int64_argument"));
  ASSERT_EQ(123456, native_input_context->metadata_args.find("int64_argument")
                        ->second.int64_val);

  ASSERT_TRUE(native_input_context->metadata_args.contains("url_argument"));
  ASSERT_EQ(
      test_url,
      *native_input_context->metadata_args.find("url_argument")->second.url);
}

}  // namespace segmentation_platform
