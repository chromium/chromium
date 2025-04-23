// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/hats/survey_config_android.h"

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/hats/jni_headers/SurveyConfig_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace hats {

SurveyConfigHolder::SurveyConfigHolder(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       Profile* profile) {
  jobj_.Reset(env, obj);
  GetActiveSurveyConfigs(survey_configs_by_triggers_);
  InitJavaHolder(profile);
}

SurveyConfigHolder::~SurveyConfigHolder() = default;

// Initialize the holder on the java side.
void SurveyConfigHolder::InitJavaHolder(Profile* profile) {
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
    std::optional<base::TimeDelta> cooldown_period_override =
        survey_config.GetCooldownPeriodOverride(profile);
    jint jcooldown_period_override = cooldown_period_override.has_value()
                                         ? cooldown_period_override->InDays()
                                         : 0;
    jint requested_browser_type = survey_config.requested_browser_type;
    Java_SurveyConfig_addActiveSurveyConfigToHolder(
        env, jobj_, jtrigger, jtrigger_id, jprobability, juser_prompted,
        jpsd_bits_data_fields, jpsd_string_data_fields,
        jcooldown_period_override, requested_browser_type);
  }
}

void SurveyConfigHolder::Destroy(JNIEnv* env) {
  survey_configs_by_triggers_.clear();
  jobj_.Reset();
}

// static
jlong JNI_SurveyConfig_InitHolder(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const JavaParamRef<jobject>& profile) {
  SurveyConfigHolder* holder =
      new SurveyConfigHolder(env, caller, Profile::FromJavaObject(profile));
  return reinterpret_cast<intptr_t>(holder);
}

}  // namespace hats
