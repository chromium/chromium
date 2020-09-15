// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_ANDROID_POLICY_CONVERTER_H_
#define COMPONENTS_POLICY_CORE_COMMON_ANDROID_POLICY_CONVERTER_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/policy/policy_export.h"

namespace base {

class Value;

}  // namespace base

namespace policy {

class PolicyBundle;
class Schema;

namespace android {

// Populates natives policies from Java key/value pairs. With its associated
// java classes, allows transforming Android |Bundle|s into |PolicyBundle|s.
class POLICY_EXPORT PolicyConverter {
 public:
  explicit PolicyConverter(const Schema* policy_schema);
  ~PolicyConverter();

  // Returns a policy bundle containing all policies collected since the last
  // call to this method.
  std::unique_ptr<PolicyBundle> GetPolicyBundle();

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // To be called from Java:
  void SetPolicyBoolean(JNIEnv* env,
                        const base::android::JavaRef<jobject>& obj,
                        const base::android::JavaRef<jstring>& policyKey,
                        jboolean value);
  void SetPolicyInteger(JNIEnv* env,
                        const base::android::JavaRef<jobject>& obj,
                        const base::android::JavaRef<jstring>& policyKey,
                        jint value);
  void SetPolicyString(JNIEnv* env,
                       const base::android::JavaRef<jobject>& obj,
                       const base::android::JavaRef<jstring>& policyKey,
                       const base::android::JavaRef<jstring>& value);
  void SetPolicyStringArray(JNIEnv* env,
                            const base::android::JavaRef<jobject>& obj,
                            const base::android::JavaRef<jstring>& policyKey,
                            const base::android::JavaRef<jobjectArray>& value);

  // Converts the passed in value to the type desired by the schema. If the
  // value is not convertible, it is returned unchanged, so the policy system
  // can report the error.
  // Note that this method will only look at the type of the schema, not at any
  // additional restrictions, or the schema for value's items or properties in
  // the case of a list or dictionary value.
  // Public for testing.
  static base::Optional<base::Value> ConvertValueToSchema(base::Value value,
                                                          const Schema& schema);

  // Public for testing.
  static base::Value ConvertJavaStringArrayToListValue(
      JNIEnv* env,
      const base::android::JavaRef<jobjectArray>& array);

 private:
  const Schema* const policy_schema_;

  std::unique_ptr<PolicyBundle> policy_bundle_;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  void SetPolicyValue(const std::string& key, base::Value raw_value);

  DISALLOW_COPY_AND_ASSIGN(PolicyConverter);
};

}  // namespace android
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_ANDROID_POLICY_CONVERTER_H_
