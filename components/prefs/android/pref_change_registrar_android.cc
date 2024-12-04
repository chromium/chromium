// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/android/pref_change_registrar_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "components/prefs/android/pref_service_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/prefs/android/jni_headers/PrefChangeRegistrar_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

PrefChangeRegistrarAndroid::PrefChangeRegistrarAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    PrefService* prefs) {
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_jobject_.Reset(env, obj);
}

PrefChangeRegistrarAndroid::~PrefChangeRegistrarAndroid() = default;

void PrefChangeRegistrarAndroid::Destroy(JNIEnv*,
                                         const JavaParamRef<jobject>&) {
  delete this;
}

void PrefChangeRegistrarAndroid::Add(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_preference) {
  std::string preference =
      base::android::ConvertJavaStringToUTF8(env, j_preference);
  pref_change_registrar_.Add(
      preference,
      base::BindRepeating(&PrefChangeRegistrarAndroid::OnPreferenceChange,
                          base::Unretained(this), preference));
}

void PrefChangeRegistrarAndroid::Remove(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_preference) {
  pref_change_registrar_.Remove(
      base::android::ConvertJavaStringToUTF8(env, j_preference));
}

void PrefChangeRegistrarAndroid::OnPreferenceChange(std::string preference) {
  JNIEnv* env = AttachCurrentThread();
  Java_PrefChangeRegistrar_onPreferenceChange(
      env, pref_change_registrar_jobject_,
      base::android::ConvertUTF8ToJavaString(env, preference));
}

jlong JNI_PrefChangeRegistrar_Init(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   PrefService* prefs) {
  return reinterpret_cast<intptr_t>(
      new PrefChangeRegistrarAndroid(env, obj, prefs));
}
