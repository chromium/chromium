// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distilled_page_prefs_android.h"

#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/jni_headers/DistilledPagePrefs_jni.h"

using base::android::JavaParamRef;

namespace dom_distiller {

namespace android {

DistilledPagePrefsAndroid::DistilledPagePrefsAndroid(
    JNIEnv* env,
    jobject obj,
    DistilledPagePrefs* distilled_page_prefs_ptr)
    : distilled_page_prefs_(distilled_page_prefs_ptr) {}

DistilledPagePrefsAndroid::~DistilledPagePrefsAndroid() {}

void DistilledPagePrefsAndroid::SetFontFamily(JNIEnv* env,
                                              const JavaParamRef<jobject>& obj,
                                              jint font_family) {
  distilled_page_prefs_->SetFontFamily(
      static_cast<mojom::FontFamily>(font_family));
}

jint DistilledPagePrefsAndroid::GetFontFamily(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return (int)distilled_page_prefs_->GetFontFamily();
}

void DistilledPagePrefsAndroid::SetTheme(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         jint theme) {
  distilled_page_prefs_->SetTheme(static_cast<mojom::Theme>(theme));
}

jint DistilledPagePrefsAndroid::GetTheme(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  return (int)distilled_page_prefs_->GetTheme();
}

void DistilledPagePrefsAndroid::SetFontScaling(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               jfloat scaling) {
  distilled_page_prefs_->SetFontScaling(static_cast<float>(scaling));
}

jfloat DistilledPagePrefsAndroid::GetFontScaling(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return distilled_page_prefs_->GetFontScaling();
}

jlong JNI_DistilledPagePrefs_Init(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jlong distilled_page_prefs_ptr) {
  DistilledPagePrefs* distilled_page_prefs =
      reinterpret_cast<DistilledPagePrefs*>(distilled_page_prefs_ptr);
  DistilledPagePrefsAndroid* distilled_page_prefs_android =
      new DistilledPagePrefsAndroid(env, obj, distilled_page_prefs);
  return reinterpret_cast<intptr_t>(distilled_page_prefs_android);
}

void DistilledPagePrefsAndroid::AddObserver(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj,
                                            jlong observer_ptr) {
  DistilledPagePrefsObserverAndroid* distilled_page_prefs_observer_wrapper =
      reinterpret_cast<DistilledPagePrefsObserverAndroid*>(observer_ptr);
  distilled_page_prefs_->AddObserver(distilled_page_prefs_observer_wrapper);
}

void DistilledPagePrefsAndroid::RemoveObserver(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               jlong observer_ptr) {
  DistilledPagePrefsObserverAndroid* distilled_page_prefs_observer_wrapper =
      reinterpret_cast<DistilledPagePrefsObserverAndroid*>(observer_ptr);
  distilled_page_prefs_->RemoveObserver(distilled_page_prefs_observer_wrapper);
}

DistilledPagePrefsObserverAndroid::DistilledPagePrefsObserverAndroid(
    JNIEnv* env,
    jobject obj) {
  java_ref_.Reset(env, obj);
}

DistilledPagePrefsObserverAndroid::~DistilledPagePrefsObserverAndroid() {}

void DistilledPagePrefsObserverAndroid::DestroyObserverAndroid(JNIEnv* env) {
  delete this;
}

void DistilledPagePrefsObserverAndroid::OnChangeFontFamily(
    mojom::FontFamily new_font_family) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DistilledPagePrefsObserverWrapper_onChangeFontFamily(
      env, java_ref_, (int)new_font_family);
}

void DistilledPagePrefsObserverAndroid::OnChangeTheme(mojom::Theme new_theme) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DistilledPagePrefsObserverWrapper_onChangeTheme(env, java_ref_,
                                                       (int)new_theme);
}

void DistilledPagePrefsObserverAndroid::OnChangeFontScaling(float scaling) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DistilledPagePrefsObserverWrapper_onChangeFontScaling(env, java_ref_,
                                                             scaling);
}

jlong JNI_DistilledPagePrefs_InitObserverAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DistilledPagePrefsObserverAndroid* observer_android =
      new DistilledPagePrefsObserverAndroid(env, obj);
  return reinterpret_cast<intptr_t>(observer_android);
}

}  // namespace android

}  // namespace dom_distiller
