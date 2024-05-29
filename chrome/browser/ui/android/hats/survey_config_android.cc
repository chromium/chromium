// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/hats/survey_config_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/hats/jni_headers/SurveyConfig_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace hats {

SurveyConfigHolder::SurveyConfigHolder(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  jobj_.Reset(env, obj);
  GetActiveSurveyConfigs(survey_configs_by_triggers_);
  InitJavaHolder();
}

SurveyConfigHolder::~SurveyConfigHolder() = default;

// Initialize the holder on the java side.
void SurveyConfigHolder::InitJavaHolder() {
  JNIEnv* env = AttachCurrentThread();

  for (auto entry : survey_configs_by_triggers_) {
    auto survey_config = entry.second;

    ScopedJavaLocalRef<jstring> jtrigger =
        ConvertUTF8ToJavaString(env, survey_config.trigger);
    ScopedJavaLocalRef<jstring> jtrigger_id =
        ConvertUTF8ToJavaString(env, survey_config.trigger_id);
    ScopedJavaLocalRef<jobjectArray> jpsd_bits_data_fields =
        base::android::ToJavaArrayOfStrings(
            env, survey_config.product_specific_bits_data_fields);
    ScopedJavaLocalRef<jobjectArray> jpsd_string_data_fields =
        base::android::ToJavaArrayOfStrings(
            env, survey_config.product_specific_string_data_fields);
    jboolean juser_prompted = survey_config.user_prompted;
    jdouble jprobability = survey_config.probability;
    Java_SurveyConfig_addActiveSurveyConfigToHolder(
        env, jobj_, jtrigger, jtrigger_id, jprobability, juser_prompted,
        jpsd_bits_data_fields, jpsd_string_data_fields);
  }
}

void SurveyConfigHolder::Destroy(JNIEnv* env) {
  survey_configs_by_triggers_.clear();
  jobj_.Reset();
}

// static
jlong JNI_SurveyConfig_InitHolder(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  SurveyConfigHolder* holder = new SurveyConfigHolder(env, caller);
  return reinterpret_cast<intptr_t>(holder);
}

}  // namespace hats
