// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/hats/survey_client_android.h"

#include <string_view>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/heap_array.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/hats/survey_config_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/hats/internal/jni_headers/SurveyClientBridge_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace hats {

// static
SurveyClientAndroid::SurveyClientAndroid(
    const std::string& trigger,
    SurveyUiDelegateAndroid* ui_delegate,
    Profile* profile,
    const std::optional<std::string>& supplied_trigger_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_trigger =
      ConvertUTF8ToJavaString(env, trigger);
  ScopedJavaLocalRef<jstring> java_supplied_trigger_id =
      ConvertUTF8ToJavaString(env, supplied_trigger_id.has_value()
                                       ? supplied_trigger_id.value()
                                       : std::string_view());
  jobj_ = Java_SurveyClientBridge_create(
      env, reinterpret_cast<int64_t>(this), java_trigger,
      ui_delegate->GetJavaObject(env), profile->GetJavaObject(),
      java_supplied_trigger_id);
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
  auto bits_values =
      base::HeapArray<bool>::WithSize(product_specific_bits_data.size());
  size_t value_iterator = 0u;
  base::ranges::for_each(
      product_specific_bits_data.begin(), product_specific_bits_data.end(),
      [&bits_fields, &bits_values,
       &value_iterator](const SurveyBitsData::value_type& pair) {
        bits_fields.push_back(pair.first);
        bits_values[value_iterator++] = pair.second;
      });
  ScopedJavaLocalRef<jobjectArray> jpsd_bits_data_fields =
      base::android::ToJavaArrayOfStrings(env, bits_fields);
  ScopedJavaLocalRef<jbooleanArray> jpsd_bits_data_vals =
      base::android::ToJavaBooleanArray(env, bits_values);

  // Parse string PSDs.
  std::vector<std::string> string_fields;
  std::vector<std::string> string_values;
  base::ranges::for_each(
      product_specific_string_data.begin(), product_specific_string_data.end(),
      [&string_fields,
       &string_values](const SurveyStringData::value_type& pair) {
        string_fields.push_back(pair.first);
        string_values.push_back(pair.second);
      });
  ScopedJavaLocalRef<jobjectArray> jpsd_string_data_fields =
      base::android::ToJavaArrayOfStrings(env, string_fields);
  ScopedJavaLocalRef<jobjectArray> jpsd_string_data_vals =
      base::android::ToJavaArrayOfStrings(env, string_values);

  Java_SurveyClientBridge_showSurvey(
      env, jobj_, window->GetJavaObject(), jpsd_bits_data_fields,
      jpsd_bits_data_vals, jpsd_string_data_fields, jpsd_string_data_vals);
}

void SurveyClientAndroid::Destroy() {
  jobj_.Reset();
}

}  // namespace hats
