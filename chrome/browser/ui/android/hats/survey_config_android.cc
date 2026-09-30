// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/hats/survey_config_android.h"

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/jni_zero/default_conversions.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ui/android/hats/jni_headers/SurveyConfig_jni.h"

using base::android::AttachCurrentThread;
using jni_zero::JavaRef;

namespace hats {

SurveyConfigHolder::SurveyConfigHolder(JNIEnv* env,
                                       const JavaRef<jobject>& obj,
                                       Profile* profile) {
  jobj_.Reset(env, obj);
  GetActiveSurveyConfigs(survey_configs_by_triggers_);
  InitJavaHolder(profile);
}

SurveyConfigHolder::~SurveyConfigHolder() = default;

// Initialize the holder on the java side.
void SurveyConfigHolder::InitJavaHolder(Profile* profile) {
  JNIEnv* env = AttachCurrentThread();

  for (const auto& [trigger, survey_config] : survey_configs_by_triggers_) {
    bool user_prompted = survey_config.user_prompted;
    double probability = survey_config.probability;
    int32_t requested_browser_type = survey_config.requested_browser_type;
    int32_t profile_age_requirement =
        static_cast<int32_t>(survey_config.profile_age_requirement);

    Java_SurveyConfig_addActiveSurveyConfigToHolder(
        env, jobj_, survey_config.trigger, survey_config.trigger_id,
        probability, user_prompted,
        survey_config.product_specific_bits_data_fields,
        survey_config.product_specific_string_data_fields,
        requested_browser_type, profile_age_requirement);
  }
}

void SurveyConfigHolder::Destroy() {
  survey_configs_by_triggers_.clear();
  jobj_.Reset();
}

// static
static int64_t JNI_SurveyConfig_InitHolder(JNIEnv* env,
                                           const JavaRef<jobject>& caller,
                                           Profile* profile) {
  SurveyConfigHolder* holder = new SurveyConfigHolder(env, caller, profile);
  return reinterpret_cast<intptr_t>(holder);
}

}  // namespace hats

DEFINE_JNI(SurveyConfig)
