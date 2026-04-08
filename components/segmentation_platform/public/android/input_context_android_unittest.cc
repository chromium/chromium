// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/android/input_context_android.h"

#include <array>
#include <cstdint>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
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

  base::android::ScopedJavaLocalRef<JArray<jstring>> java_bool_keys =
      jni_zero::NewStringArray(jni_env, {"boolean_argument"});
  base::android::ScopedJavaLocalRef<JArray<bool>> java_bool_values =
      jni_zero::NewArray(jni_env, {true});

  base::android::ScopedJavaLocalRef<JArray<jstring>> java_int_keys =
      jni_zero::NewStringArray(jni_env,
                               {"int_argument", "negative_int", "large_int"});
  base::android::ScopedJavaLocalRef<JArray<int32_t>> java_int_values =
      jni_zero::NewArray(jni_env, {1234, -4, INT_MAX});

  base::android::ScopedJavaLocalRef<JArray<jstring>> java_float_keys =
      jni_zero::NewStringArray(jni_env, {"float_argument"});
  base::android::ScopedJavaLocalRef<JArray<float>> java_float_values =
      jni_zero::NewArray(jni_env, {13.37f});

  base::android::ScopedJavaLocalRef<JArray<jstring>> java_double_keys =
      jni_zero::NewStringArray(jni_env, {"double_argument"});
  base::android::ScopedJavaLocalRef<JArray<double>> java_double_values =
      jni_zero::NewArray(jni_env, {100.3});

  base::android::ScopedJavaLocalRef<JArray<jstring>> java_string_keys =
      jni_zero::NewStringArray(
          jni_env, {"string_argument", "second_string", "third_string"});
  base::android::ScopedJavaLocalRef<JArray<jstring>> java_string_values =
      jni_zero::NewStringArray(jni_env, {"Hello, World!", "Foo", "bar"});

  base::android::ScopedJavaLocalRef<JArray<jstring>> java_time_keys =
      jni_zero::NewStringArray(jni_env, {"time_argument"});
  base::android::ScopedJavaLocalRef<JArray<int64_t>> java_time_values =
      jni_zero::NewArray(jni_env, {time.InMillisecondsSinceUnixEpoch()});

  base::android::ScopedJavaLocalRef<JArray<jstring>> java_int64_keys =
      jni_zero::NewStringArray(jni_env, {"int64_argument"});
  base::android::ScopedJavaLocalRef<JArray<int64_t>> java_int64_values =
      jni_zero::NewArray(jni_env, {static_cast<int64_t>(123456)});

  base::android::ScopedJavaLocalRef<JArray<jstring>> java_url_keys =
      jni_zero::NewStringArray(jni_env, {"url_argument"});
  base::android::ScopedJavaLocalRef<JArray<jobject>> java_url_values =
      jni_zero::NewObjectArray(jni_env, {java_gurl});

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
