// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_string.h"
#include "base/base64.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "components/variations/net/variations_command_line.h"
#include "components/variations/variations_ids_provider.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/variations/android/variations_data_jni/VariationsAssociatedData_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace variations {
namespace android {

static ScopedJavaLocalRef<jstring>
JNI_VariationsAssociatedData_GetVariationParamValue(
    JNIEnv* env,
    const JavaParamRef<jstring>& jtrial_name,
    const JavaParamRef<jstring>& jparam_name) {
  std::string trial_name(ConvertJavaStringToUTF8(env, jtrial_name));
  std::string param_name(ConvertJavaStringToUTF8(env, jparam_name));
  std::string param_value =
      base::GetFieldTrialParamValue(trial_name, param_name);
  return ConvertUTF8ToJavaString(env, param_value);
}

static ScopedJavaLocalRef<jstring>
JNI_VariationsAssociatedData_GetFeedbackVariations(JNIEnv* env) {
  const std::string values =
      VariationsIdsProvider::GetInstance()->GetVariationsString();
  return ConvertUTF8ToJavaString(env, values);
}

static ScopedJavaLocalRef<jstring>
JNI_VariationsAssociatedData_GetVariationsState(JNIEnv* env) {
  if (!base::FeatureList::IsEnabled(variations::kFeedbackIncludeVariations)) {
    return nullptr;
  }
  std::vector<uint8_t> ciphertext;
  const auto status =
      variations::VariationsCommandLine::GetForCurrentProcess().EncryptToString(
          &ciphertext);
  base::UmaHistogramEnumeration("Variations.VariationsStateEncryptionStatus",
                                status);
  if (status != variations::VariationsStateEncryptionStatus::kSuccess) {
    return nullptr;
  }
  std::string value = base::Base64Encode(ciphertext);
  return ConvertUTF8ToJavaString(env, value);
}

static ScopedJavaLocalRef<jstring>
JNI_VariationsAssociatedData_GetGoogleAppVariations(JNIEnv* env) {
  const std::string values =
      VariationsIdsProvider::GetInstance()->GetGoogleAppVariationsString();
  return ConvertUTF8ToJavaString(env, values);
}

}  // namespace android
}  // namespace variations

DEFINE_JNI(VariationsAssociatedData)
