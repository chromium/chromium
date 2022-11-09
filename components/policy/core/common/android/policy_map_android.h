// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_ANDROID_POLICY_MAP_ANDROID_H_
#define COMPONENTS_POLICY_CORE_COMMON_ANDROID_POLICY_MAP_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_export.h"

namespace base {
class Value;
}

namespace policy {
namespace android {

// PolicyMap bridge class that is used for Android.
class POLICY_EXPORT PolicyMapAndroid {
 public:
  explicit PolicyMapAndroid(const PolicyMap& policy_map);
  PolicyMapAndroid(const PolicyMapAndroid&) = delete;
  PolicyMapAndroid& operator=(const PolicyMapAndroid&) = delete;

  ~PolicyMapAndroid();

  jboolean HasValue(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& caller,
                    const base::android::JavaRef<jstring>& policy) const;

  jint GetIntValue(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& caller,
                   const base::android::JavaRef<jstring>& policy) const;

  jboolean GetBooleanValue(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& caller,
                           const base::android::JavaRef<jstring>& policy) const;

  base::android::ScopedJavaLocalRef<jstring> GetStringValue(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      const base::android::JavaRef<jstring>& policy) const;

  base::android::ScopedJavaLocalRef<jstring> GetListValue(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      const base::android::JavaRef<jstring>& policy) const;

  base::android::ScopedJavaLocalRef<jstring> GetDictValue(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      const base::android::JavaRef<jstring>& policy) const;

  jboolean Equals(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& caller,
                  jlong other) const;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  base::android::ScopedJavaLocalRef<jstring> GetListOrDictValue(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& policy,
      bool is_dict) const;

  const base::Value* GetValue(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& policy) const;

  const raw_ref<const PolicyMap> policy_map_;

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace android
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_ANDROID_POLICY_MAP_ANDROID_H_
