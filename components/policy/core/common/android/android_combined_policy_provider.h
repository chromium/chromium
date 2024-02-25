// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_ANDROID_ANDROID_COMBINED_POLICY_PROVIDER_H_
#define COMPONENTS_POLICY_CORE_COMMON_ANDROID_ANDROID_COMBINED_POLICY_PROVIDER_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_export.h"

namespace policy {

class SchemaRegistry;

namespace android {

class PolicyConverter;

class POLICY_EXPORT AndroidCombinedPolicyProvider
    : public ConfigurationPolicyProvider {
 public:
  explicit AndroidCombinedPolicyProvider(SchemaRegistry* registry);
  AndroidCombinedPolicyProvider(const AndroidCombinedPolicyProvider&) = delete;
  AndroidCombinedPolicyProvider& operator=(
      const AndroidCombinedPolicyProvider&) = delete;

  ~AndroidCombinedPolicyProvider() override;

  // Push the polices updated by the Java policy providers to the core policy
  // system
  void FlushPolicies(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);

  // Call this method to tell the policy system whether it should wait for
  // policies to be loaded by this provider. If this method is called,
  // IsInitializationComplete() will only return true after SetPolicies() has
  // been called at least once, otherwise it will return true immediately.
  static void SetShouldWaitForPolicy(bool should_wait_for_policy);

  // ConfigurationPolicyProvider:
  bool IsInitializationComplete(PolicyDomain domain) const override;
  bool IsFirstPolicyLoadComplete(PolicyDomain domain) const override;
  void RefreshPolicies(PolicyFetchReason reason) override;

  // For testing
  PolicyConverter* GetPolicyConverterForTesting() {
    return policy_converter_.get();
  }

 private:
  bool initialized_;
  std::unique_ptr<policy::android::PolicyConverter> policy_converter_;
  base::android::ScopedJavaGlobalRef<jobject> java_combined_policy_provider_;
};

}  // namespace android

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_ANDROID_ANDROID_COMBINED_POLICY_PROVIDER_H_
