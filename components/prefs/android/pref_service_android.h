// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_ANDROID_PREF_SERVICE_ANDROID_H_
#define COMPONENTS_PREFS_ANDROID_PREF_SERVICE_ANDROID_H_

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"

class PrefService;

// The native side of the PrefServiceAndroid is created and destroyed by the
// Java.
class PrefServiceAndroid {
 public:
  explicit PrefServiceAndroid(PrefService* pref_service);
  PrefServiceAndroid(const PrefServiceAndroid& other) = delete;
  PrefServiceAndroid& operator=(const PrefServiceAndroid& other) = delete;
  ~PrefServiceAndroid();

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  void ClearPref(JNIEnv* env,
                 const base::android::JavaParamRef<jstring>& j_preference);
  jboolean GetBoolean(JNIEnv* env,
                      const base::android::JavaParamRef<jstring>& j_preference);
  void SetBoolean(JNIEnv* env,
                  const base::android::JavaParamRef<jstring>& j_preference,
                  const jboolean j_value);
  jint GetInteger(JNIEnv* env,
                  const base::android::JavaParamRef<jstring>& j_preference);
  void SetInteger(JNIEnv* env,
                  const base::android::JavaParamRef<jstring>& j_preference,
                  const jint j_value);
  base::android::ScopedJavaLocalRef<jstring> GetString(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_preference);
  void SetString(JNIEnv* env,
                 const base::android::JavaParamRef<jstring>& j_preference,
                 const base::android::JavaParamRef<jstring>& j_value);
  jboolean IsManagedPreference(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_preference);

 private:
  PrefService* pref_service_;
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

#endif  // COMPONENTS_PREFS_ANDROID_PREF_SERVICE_ANDROID_H_
