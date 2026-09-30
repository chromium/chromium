// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/hats/survey_client_android.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/hats/survey_config_android.h"
#include "third_party/jni_zero/default_conversions.h"
#include "ui/android/window_android.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ui/android/hats/internal/jni_headers/SurveyClientBridge_jni.h"

namespace hats {

// static
SurveyClientAndroid::SurveyClientAndroid(
    const std::string& trigger,
    SurveyUiDelegateAndroid* ui_delegate,
    Profile* profile,
    const std::optional<std::string>& supplied_trigger_id,
    ui::WindowAndroid* window) {
  JNIEnv* env = base::android::AttachCurrentThread();
  jobj_ = Java_SurveyClientBridge_create(
      env, trigger, ui_delegate->GetJavaObject(env), profile,
      supplied_trigger_id.value_or(std::string()), window);
}

SurveyClientAndroid::~SurveyClientAndroid() = default;

void SurveyClientAndroid::LaunchSurvey(
    ui::WindowAndroid* window,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Ignore the call if the java object is null.
  if (!jobj_) {
    return;
  }

  // Parse bit PSDs.
  std::vector<std::string> bits_fields;
  std::vector<bool> bits_values;
  bits_fields.reserve(product_specific_bits_data.size());
  bits_values.reserve(product_specific_bits_data.size());
  for (const auto& [field, value] : product_specific_bits_data) {
    bits_fields.push_back(field);
    bits_values.push_back(value);
  }

  // Parse string PSDs.
  std::vector<std::string> string_fields;
  std::vector<std::string> string_values;
  string_fields.reserve(product_specific_string_data.size());
  string_values.reserve(product_specific_string_data.size());
  for (const auto& [field, value] : product_specific_string_data) {
    string_fields.push_back(field);
    string_values.push_back(value);
  }

  Java_SurveyClientBridge_showSurvey(env, jobj_, window, bits_fields,
                                     bits_values, string_fields, string_values);
}

void SurveyClientAndroid::Destroy() {
  jobj_.Reset();
}

}  // namespace hats

DEFINE_JNI(SurveyClientBridge)
