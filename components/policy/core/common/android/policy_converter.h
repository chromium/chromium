// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_ANDROID_POLICY_CONVERTER_H_
#define COMPONENTS_POLICY_CORE_COMMON_ANDROID_POLICY_CONVERTER_H_

#include <jni.h>

#include <optional>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/policy_export.h"

namespace base {

class Value;

}  // namespace base

namespace policy {

class Schema;

namespace android {

// Populates natives policies from Java key/value pairs. With its associated
// java classes, allows transforming Android |Bundle|s into |PolicyBundle|s.
class POLICY_EXPORT PolicyConverter {
 public:
  explicit PolicyConverter(const Schema* policy_schema);
  PolicyConverter(const PolicyConverter&) = delete;
  PolicyConverter& operator=(const PolicyConverter&) = delete;
  ~PolicyConverter();

  // Returns a policy bundle containing all policies collected since the last
  // call to this method.
  PolicyBundle GetPolicyBundle();

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
  static std::optional<base::Value> ConvertValueToSchema(base::Value value,
                                                         const Schema& schema);

  // Public for testing.
  static base::Value::List ConvertJavaStringArrayToListValue(
      JNIEnv* env,
      const base::android::JavaRef<jobjectArray>& array);

  // Exposes `SetPolicyValue` for testing purposes.
  void SetPolicyValueForTesting(const std::string& key, base::Value raw_value);

 private:
  const raw_ptr<const Schema> policy_schema_;

  PolicyBundle policy_bundle_;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  void SetPolicyValue(const std::string& key, base::Value raw_value);
};

}  // namespace android
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_ANDROID_POLICY_CONVERTER_H_
