// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "components/language/core/common/language_experiments.h"
#include "components/translate/content/android/jni_headers/TranslateFeatureList_jni.h"

namespace translate {
namespace android {
namespace {

// Array of translate features exposed through the Java TranslateFeatureList
// API. Entries in this array refer to features defined in
// components/language/core/common/language_experiments.h.
const base::Feature* const kFeaturesExposedToJava[] = {
    &language::kContentLanguagesInLanguagePicker,
};

// TODO(crbug.com/1060097): Remove/update this once a generalized FeatureList
// exists.
const base::Feature* FindFeatureExposedToJava(const std::string& feature_name) {
  for (const base::Feature* feature : kFeaturesExposedToJava) {
    if (feature->name == feature_name)
      return feature;
  }
  NOTREACHED() << "Queried feature cannot be found in TranslateFeatureList: "
               << feature_name;
  return nullptr;
}

}  // namespace

static jboolean JNI_TranslateFeatureList_IsEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature = FindFeatureExposedToJava(
      base::android::ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

static jboolean JNI_TranslateFeatureList_GetFieldTrialParamByFeatureAsBoolean(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jfeature_name,
    const base::android::JavaParamRef<jstring>& jparam_name,
    const jboolean jdefault_value) {
  const base::Feature* feature = FindFeatureExposedToJava(
      base::android::ConvertJavaStringToUTF8(env, jfeature_name));
  const std::string& param_name = ConvertJavaStringToUTF8(env, jparam_name);
  return base::GetFieldTrialParamByFeatureAsBool(*feature, param_name,
                                                 jdefault_value);
}

}  // namespace android
}  // namespace translate
