// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_distiller/core/distilled_page_prefs_android.h"

#include "components/web_distiller/core/distilled_page_prefs.h"
#include "components/web_distiller/core/web_distiller_service.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/web_distiller/core/android/jni_headers/DistilledPagePrefs_jni.h"

using base::android::JavaRef;

namespace web_distiller {

namespace android {

DistilledPagePrefsAndroid::DistilledPagePrefsAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    DistilledPagePrefs* distilled_page_prefs_ptr)
    : distilled_page_prefs_(distilled_page_prefs_ptr) {}

DistilledPagePrefsAndroid::~DistilledPagePrefsAndroid() = default;

void DistilledPagePrefsAndroid::SetFontFamily(JNIEnv* env,
                                              int32_t font_family) {
  distilled_page_prefs_->SetFontFamily(
      static_cast<mojom::FontFamily>(font_family));
}

int32_t DistilledPagePrefsAndroid::GetFontFamily(JNIEnv* env) {
  return (int)distilled_page_prefs_->GetFontFamily();
}

void DistilledPagePrefsAndroid::SetUserPrefTheme(JNIEnv* env, int32_t theme) {
  distilled_page_prefs_->SetUserPrefTheme(static_cast<mojom::Theme>(theme));
}

void DistilledPagePrefsAndroid::SetDefaultTheme(JNIEnv* env, int32_t theme) {
  distilled_page_prefs_->SetDefaultTheme(static_cast<mojom::Theme>(theme));
}

int32_t DistilledPagePrefsAndroid::GetTheme(JNIEnv* env) {
  return (int)distilled_page_prefs_->GetTheme();
}

void DistilledPagePrefsAndroid::SetUserPrefFontScaling(JNIEnv* env,
                                                       float scaling) {
  distilled_page_prefs_->SetUserPrefFontScaling(scaling);
}

void DistilledPagePrefsAndroid::SetDefaultFontScaling(JNIEnv* env,
                                                      float scaling) {
  distilled_page_prefs_->SetDefaultFontScaling(scaling);
}

float DistilledPagePrefsAndroid::GetFontScaling(JNIEnv* env) {
  return distilled_page_prefs_->GetFontScaling();
}

void DistilledPagePrefsAndroid::SetLinksEnabled(JNIEnv* env, bool enabled) {
  distilled_page_prefs_->SetLinksEnabled(enabled);
}

bool DistilledPagePrefsAndroid::GetLinksEnabled(JNIEnv* env) {
  return distilled_page_prefs_->GetLinksEnabled();
}

static int64_t JNI_DistilledPagePrefs_Init(JNIEnv* env,
                                           const JavaRef<jobject>& obj,
                                           int64_t distilled_page_prefs_ptr) {
  DistilledPagePrefs* distilled_page_prefs =
      reinterpret_cast<DistilledPagePrefs*>(distilled_page_prefs_ptr);
  DistilledPagePrefsAndroid* distilled_page_prefs_android =
      new DistilledPagePrefsAndroid(env, obj, distilled_page_prefs);
  return reinterpret_cast<intptr_t>(distilled_page_prefs_android);
}

void DistilledPagePrefsAndroid::AddObserver(
    JNIEnv* env,
    DistilledPagePrefsObserverAndroid* observer) {
  distilled_page_prefs_->AddObserver(observer);
}

void DistilledPagePrefsAndroid::RemoveObserver(
    JNIEnv* env,
    DistilledPagePrefsObserverAndroid* observer) {
  distilled_page_prefs_->RemoveObserver(observer);
}

DistilledPagePrefsObserverAndroid::DistilledPagePrefsObserverAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj) {
  java_ref_.Reset(env, obj);
}

DistilledPagePrefsObserverAndroid::~DistilledPagePrefsObserverAndroid() =
    default;

void DistilledPagePrefsObserverAndroid::OnChangeFontFamily(
    mojom::FontFamily new_font_family) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_DistilledPagePrefsObserverWrapper_onChangeFontFamily(
      env, java_ref_, (int)new_font_family);
}

void DistilledPagePrefsObserverAndroid::OnChangeTheme(
    mojom::Theme new_theme,
    ThemeSettingsUpdateSource source) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_DistilledPagePrefsObserverWrapper_onChangeTheme(env, java_ref_,
                                                       (int)new_theme);
}

void DistilledPagePrefsObserverAndroid::OnChangeFontScaling(float scaling) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_DistilledPagePrefsObserverWrapper_onChangeFontScaling(env, java_ref_,
                                                             scaling);
}

void DistilledPagePrefsObserverAndroid::OnChangeLinksEnabled(bool enabled) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_DistilledPagePrefsObserverWrapper_onChangeLinksEnabled(env, java_ref_,
                                                              enabled);
}

static jni_zero::JniUniquePtr<DistilledPagePrefsObserverAndroid>
JNI_DistilledPagePrefs_InitObserverAndroid(JNIEnv* env,
                                           const JavaRef<jobject>& obj) {
  return jni_zero::MakeUnique<DistilledPagePrefsObserverAndroid>(env, obj);
}

}  // namespace android

}  // namespace web_distiller

DEFINE_JNI(DistilledPagePrefs)
