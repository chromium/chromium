// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/values.h"
#include "components/policy/core/common/android/android_combined_policy_provider.h"
#include "components/policy/core/common/android/policy_converter.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace policy {

namespace android {
class AndroidCombinedPolicyProviderTest : public ::testing::Test {
  void TearDown() override;
};

void AndroidCombinedPolicyProviderTest::TearDown() {
  AndroidCombinedPolicyProvider::SetShouldWaitForPolicy(false);
}

TEST_F(AndroidCombinedPolicyProviderTest, InitializationCompleted) {
  SchemaRegistry registry;
  AndroidCombinedPolicyProvider manager(&registry);
  EXPECT_TRUE(manager.IsInitializationComplete(POLICY_DOMAIN_CHROME));
  // If the manager is deleted (by going out of scope) without being shutdown
  // first it DCHECKs.
  manager.Shutdown();
}

TEST_F(AndroidCombinedPolicyProviderTest, SetShouldWaitForPolicy) {
  AndroidCombinedPolicyProvider::SetShouldWaitForPolicy(true);
  SchemaRegistry registry;
  AndroidCombinedPolicyProvider manager(&registry);
  EXPECT_FALSE(manager.IsInitializationComplete(POLICY_DOMAIN_CHROME));
  manager.FlushPolicies(nullptr, nullptr);
  EXPECT_TRUE(manager.IsInitializationComplete(POLICY_DOMAIN_CHROME));
  // If the manager is deleted (by going out of scope) without being shutdown
  // first it DCHECKs.
  manager.Shutdown();
}

TEST_F(AndroidCombinedPolicyProviderTest, FlushPolices) {
  const char kSchemaTemplate[] =
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "  }"
      "}";

  PolicyNamespace ns(POLICY_DOMAIN_CHROME, std::string());
  const auto schema = Schema::Parse(kSchemaTemplate);
  ASSERT_TRUE(schema.has_value());
  SchemaRegistry registry;
  registry.RegisterComponent(ns, *schema);
  AndroidCombinedPolicyProvider manager(&registry);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jpolicy =
      ConvertUTF8ToJavaString(env, "TestPolicy");
  ScopedJavaLocalRef<jstring> jvalue =
      ConvertUTF8ToJavaString(env, "TestValue");
  manager.GetPolicyConverterForTesting()->SetPolicyString(env, nullptr, jpolicy,
                                                          jvalue);
  manager.FlushPolicies(env, nullptr);
  const PolicyBundle& bundle = manager.policies();
  const PolicyMap& map = bundle.Get(ns);
  const base::Value* value =
      map.GetValue("TestPolicy", base::Value::Type::STRING);
  ASSERT_NE(nullptr, value);
  EXPECT_EQ(base::Value::Type::STRING, value->type());
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ("TestValue", value->GetString());
  // If the manager is deleted (by going out of scope) without being shutdown
  // first it DCHECKs.
  manager.Shutdown();
}

}  // namespace android

}  // namespace policy
