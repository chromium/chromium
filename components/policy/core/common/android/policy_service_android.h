// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_ANDROID_POLICY_SERVICE_ANDROID_H_
#define COMPONENTS_POLICY_CORE_COMMON_ANDROID_POLICY_SERVICE_ANDROID_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/policy/core/common/android/policy_map_android.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_export.h"

namespace policy {
namespace android {

// PolicyService bridge class that is used for Android. It's owned by the main
// PolicyService instance.
// Note that it only support the Chrome policy domain but not any extension
// policy.
class POLICY_EXPORT PolicyServiceAndroid : public PolicyService::Observer {
 public:
  explicit PolicyServiceAndroid(PolicyService* policy_service);
  PolicyServiceAndroid(const PolicyServiceAndroid&) = delete;
  PolicyServiceAndroid& operator=(const PolicyServiceAndroid&) = delete;

  ~PolicyServiceAndroid() override;

  void AddObserver(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& caller);

  void RemoveObserver(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& caller);

  bool IsInitializationComplete(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller) const;

  base::android::ScopedJavaLocalRef<jobject> GetPolicies(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);

  // PolicyService::Observer implementation.
  // Pass the event to the Java observers.
  void OnPolicyServiceInitialized(PolicyDomain domain) override;
  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  raw_ptr<PolicyService> policy_service_;

  // Contains all Chrome policies. The PolicyBundle is not used as there is only
  // one policy namespace supported on Android.
  PolicyMapAndroid policy_map_;

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace android

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_ANDROID_POLICY_SERVICE_ANDROID_H_
