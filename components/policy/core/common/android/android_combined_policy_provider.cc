// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/android/android_combined_policy_provider.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/policy/core/common/android/policy_converter.h"
#include "components/policy/core/common/policy_logger.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/policy/android/jni_headers/CombinedPolicyProvider_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace {

bool g_wait_for_policies = false;

}  // namespace

namespace policy {
namespace android {

AndroidCombinedPolicyProvider::AndroidCombinedPolicyProvider(
    SchemaRegistry* registry)
    : initialized_(!g_wait_for_policies) {
  PolicyNamespace ns(POLICY_DOMAIN_CHROME, std::string());
  const Schema* schema = registry->schema_map()->GetSchema(ns);
  policy_converter_ =
      std::make_unique<policy::android::PolicyConverter>(schema);
  java_combined_policy_provider_.Reset(Java_CombinedPolicyProvider_linkNative(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      policy_converter_->GetJavaObject()));
}

AndroidCombinedPolicyProvider::~AndroidCombinedPolicyProvider() {
  Java_CombinedPolicyProvider_linkNative(AttachCurrentThread(), 0, nullptr);
  java_combined_policy_provider_.Reset();
}

void AndroidCombinedPolicyProvider::RefreshPolicies(PolicyFetchReason reason) {
  JNIEnv* env = AttachCurrentThread();
  Java_CombinedPolicyProvider_refreshPolicies(env,
                                              java_combined_policy_provider_);
}

void AndroidCombinedPolicyProvider::FlushPolicies(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  initialized_ = true;
  UpdatePolicy(policy_converter_->GetPolicyBundle());
}

// static
void AndroidCombinedPolicyProvider::SetShouldWaitForPolicy(
    bool should_wait_for_policy) {
  VLOG_POLICY(2, POLICY_PROCESSING)
      << "SetShouldWaitForPolicy: " << should_wait_for_policy;
  g_wait_for_policies = should_wait_for_policy;
}

bool AndroidCombinedPolicyProvider::IsInitializationComplete(
    PolicyDomain domain) const {
  return initialized_;
}

bool AndroidCombinedPolicyProvider::IsFirstPolicyLoadComplete(
    PolicyDomain domain) const {
  return IsInitializationComplete(domain);
}

}  // namespace android
}  // namespace policy
