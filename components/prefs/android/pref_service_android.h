// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_ANDROID_PREF_SERVICE_ANDROID_H_
#define COMPONENTS_PREFS_ANDROID_PREF_SERVICE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/prefs_export.h"

class PrefService;

// The native side of the PrefServiceAndroid is created and destroyed by the
// Java.
class COMPONENTS_PREFS_EXPORT PrefServiceAndroid {
 public:
  explicit PrefServiceAndroid(PrefService* pref_service);
  PrefServiceAndroid(const PrefServiceAndroid& other) = delete;
  PrefServiceAndroid& operator=(const PrefServiceAndroid& other) = delete;
  ~PrefServiceAndroid();

  // Returns the native counterpart of a Java `PrefService`.
  static PrefService* FromPrefServiceAndroid(
      const base::android::JavaRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  void ClearPref(JNIEnv* env,
                 const base::android::JavaParamRef<jstring>& j_preference);
  jboolean HasPrefPath(
      JNIEnv* env,
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
  jdouble GetDouble(JNIEnv* env,
                    const base::android::JavaParamRef<jstring>& j_preference);
  void SetDouble(JNIEnv* env,
                 const base::android::JavaParamRef<jstring>& j_preference,
                 const jdouble j_value);
  jlong GetLong(JNIEnv* env,
                const base::android::JavaParamRef<jstring>& j_preference);
  void SetLong(JNIEnv* env,
               const base::android::JavaParamRef<jstring>& j_preference,
               const jlong j_value);
  base::android::ScopedJavaLocalRef<jstring> GetString(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_preference);
  void SetString(JNIEnv* env,
                 const base::android::JavaParamRef<jstring>& j_preference,
                 const base::android::JavaParamRef<jstring>& j_value);
  jboolean IsManagedPreference(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_preference);
  jboolean IsDefaultValuePreference(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_preference);

 private:
  raw_ptr<PrefService> pref_service_;
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

namespace jni_zero {
template <>
inline PrefService* FromJniType<PrefService*>(JNIEnv* env,
                                              const JavaRef<jobject>& obj) {
  return PrefServiceAndroid::FromPrefServiceAndroid(obj);
}
}  // namespace jni_zero

#endif  // COMPONENTS_PREFS_ANDROID_PREF_SERVICE_ANDROID_H_
