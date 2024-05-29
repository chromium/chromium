// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/android/policy_map_android.h"

#include <vector>

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/policy/android/test_jni_headers/PolicyMapTestSupporter_jni.h"

namespace policy {
namespace android {
namespace {

constexpr char kPolicyName[] = "policy-name";

}  // namespace

class PolicyMapAndroidTest : public ::testing::Test {
 public:
  PolicyMapAndroidTest() = default;
  ~PolicyMapAndroidTest() override = default;

  void SetPolicy(base::Value&& value) {
    policy_map_.Set(kPolicyName, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_PLATFORM, std::move(value), nullptr);
  }

  raw_ptr<JNIEnv> env_ = base::android::AttachCurrentThread();
  PolicyMap policy_map_;
  PolicyMapAndroid policy_map_android_{policy_map_};
  base::android::ScopedJavaLocalRef<jobject> j_support_ =
      Java_PolicyMapTestSupporter_Constructor(
          env_,
          policy_map_android_.GetJavaObject());
  base::android::ScopedJavaLocalRef<jstring> policy_name_android_ =
      base::android::ConvertUTF8ToJavaString(env_, kPolicyName);
};

TEST_F(PolicyMapAndroidTest, IntPolicy) {
  Java_PolicyMapTestSupporter_verifyIntPolicy(env_, j_support_,
                                              policy_name_android_, false, 0);
  int value = 42;
  SetPolicy(base::Value(value));
  Java_PolicyMapTestSupporter_verifyIntPolicy(
      env_, j_support_, policy_name_android_, true, value);

  value = -42;
  SetPolicy(base::Value(value));
  Java_PolicyMapTestSupporter_verifyIntPolicy(
      env_, j_support_, policy_name_android_, true, value);
}

TEST_F(PolicyMapAndroidTest, BooleanPolicy) {
  Java_PolicyMapTestSupporter_verifyBooleanPolicy(
      env_, j_support_, policy_name_android_, false, false);
  bool value = true;
  SetPolicy(base::Value(value));
  Java_PolicyMapTestSupporter_verifyBooleanPolicy(
      env_, j_support_, policy_name_android_, true, value);
}

TEST_F(PolicyMapAndroidTest, StringPolicy) {
  Java_PolicyMapTestSupporter_verifyStringPolicy(env_, j_support_,
                                                 policy_name_android_, nullptr);
  std::string value = "policy-value";
  SetPolicy(base::Value(value));
  Java_PolicyMapTestSupporter_verifyStringPolicy(
      env_, j_support_, policy_name_android_,
      base::android::ConvertUTF8ToJavaString(env_, value));
}

TEST_F(PolicyMapAndroidTest, DictPolicy) {
  Java_PolicyMapTestSupporter_verifyDictPolicy(env_, j_support_,
                                               policy_name_android_, nullptr);
  base::Value::Dict value;
  value.Set("key", 42);
  SetPolicy(base::Value(std::move(value)));
  Java_PolicyMapTestSupporter_verifyDictPolicy(
      env_, j_support_, policy_name_android_,
      base::android::ConvertUTF8ToJavaString(env_, R"({"key":42})"));
}

TEST_F(PolicyMapAndroidTest, ListPolicy) {
  Java_PolicyMapTestSupporter_verifyListPolicy(env_, j_support_,
                                               policy_name_android_, nullptr);
  base::Value::List value;
  value.Append("value-1");
  value.Append("value-2");
  SetPolicy(base::Value(std::move(value)));
  Java_PolicyMapTestSupporter_verifyListPolicy(
      env_, j_support_, policy_name_android_,
      base::android::ConvertUTF8ToJavaString(env_, R"(["value-1","value-2"])"));
}

}  // namespace android
}  // namespace policy
