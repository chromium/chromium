// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/android/policy_map_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/json/json_writer.h"
#include "base/values.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/policy/android/jni_headers/PolicyMap_jni.h"

namespace policy {
namespace android {

PolicyMapAndroid::PolicyMapAndroid(const PolicyMap& policy_map)
    : policy_map_(policy_map) {}

PolicyMapAndroid::~PolicyMapAndroid() = default;

jboolean PolicyMapAndroid::HasValue(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const base::android::JavaRef<jstring>& policy) const {
  return GetValue(env, policy) != nullptr;
}

jint PolicyMapAndroid::GetIntValue(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const base::android::JavaRef<jstring>& policy) const {
  const base::Value* value = GetValue(env, policy);
  DCHECK(value && value->is_int())
      << "The policy must exist and be stored as integer.";
  return value->GetInt();
}

jboolean PolicyMapAndroid::GetBooleanValue(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const base::android::JavaRef<jstring>& policy) const {
  const base::Value* value = GetValue(env, policy);
  DCHECK(value && value->is_bool())
      << "The policy must exist and be stored as boolean.";
  return value->GetBool();
}

base::android::ScopedJavaLocalRef<jstring> PolicyMapAndroid::GetStringValue(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const base::android::JavaRef<jstring>& policy) const {
  const base::Value* value = GetValue(env, policy);
  if (!value)
    return nullptr;
  DCHECK(value->is_string()) << "The policy must be stored as string.";
  return base::android::ConvertUTF8ToJavaString(env, value->GetString());
}

base::android::ScopedJavaLocalRef<jstring> PolicyMapAndroid::GetListValue(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const base::android::JavaRef<jstring>& policy) const {
  return GetListOrDictValue(env, policy, /* is_dict */ false);
}

base::android::ScopedJavaLocalRef<jstring> PolicyMapAndroid::GetDictValue(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const base::android::JavaRef<jstring>& policy) const {
  return GetListOrDictValue(env, policy, /* is_dict */ true);
}

jboolean PolicyMapAndroid::Equals(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    jlong other) const {
  return policy_map_->Equals(
      *reinterpret_cast<PolicyMapAndroid*>(other)->policy_map_);
}

base::android::ScopedJavaLocalRef<jobject> PolicyMapAndroid::GetJavaObject() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_ref_) {
    java_ref_.Reset(
        Java_PolicyMap_Constructor(env, reinterpret_cast<intptr_t>(this)));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_ref_);
}

base::android::ScopedJavaLocalRef<jstring> PolicyMapAndroid::GetListOrDictValue(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& policy,
    bool is_dict) const {
  const base::Value* value = GetValue(env, policy);
  if (!value)
    return nullptr;
#if DCHECK_IS_ON()
  if (is_dict)
    DCHECK(value->is_dict()) << "The policy must be stored as dictionary.";
  else
    DCHECK(value->is_list()) << "The policy must be stored as list.";
#endif  // DCHECK_IS_ON()
  std::string json_string;
  base::JSONWriter::Write(*value, &json_string);
  return base::android::ConvertUTF8ToJavaString(env, json_string);
}

const base::Value* PolicyMapAndroid::GetValue(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& policy) const {
  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  return policy_map_->GetValueUnsafe(
      base::android::ConvertJavaStringToUTF8(env, policy));
}

}  // namespace android
}  // namespace policy
