// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_string.h"
#include "base/metrics/field_trial_params.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_ids_provider.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/variations/android/variations_jni/VariationsAssociatedData_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace variations {
namespace android {

ScopedJavaLocalRef<jstring> JNI_VariationsAssociatedData_GetVariationParamValue(
    JNIEnv* env,
    const JavaParamRef<jstring>& jtrial_name,
    const JavaParamRef<jstring>& jparam_name) {
  std::string trial_name(ConvertJavaStringToUTF8(env, jtrial_name));
  std::string param_name(ConvertJavaStringToUTF8(env, jparam_name));
  std::string param_value =
      base::GetFieldTrialParamValue(trial_name, param_name);
  return ConvertUTF8ToJavaString(env, param_value);
}

ScopedJavaLocalRef<jstring> JNI_VariationsAssociatedData_GetFeedbackVariations(
    JNIEnv* env) {
  const std::string values =
      VariationsIdsProvider::GetInstance()->GetVariationsString();
  return ConvertUTF8ToJavaString(env, values);
}

ScopedJavaLocalRef<jstring> JNI_VariationsAssociatedData_GetGoogleAppVariations(
    JNIEnv* env) {
  const std::string values =
      VariationsIdsProvider::GetInstance()->GetGoogleAppVariationsString();
  return ConvertUTF8ToJavaString(env, values);
}

}  // namespace android
}  // namespace variations
