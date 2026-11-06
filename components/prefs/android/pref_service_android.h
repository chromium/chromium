// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_ANDROID_PREF_SERVICE_ANDROID_H_
#define COMPONENTS_PREFS_ANDROID_PREF_SERVICE_ANDROID_H_

#include <stdint.h>

#include <string>

#include "base/android/jni_android.h"
#include "base/component_export.h"
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

  void ClearPref(const std::string& preference);
  bool HasPrefPath(const std::string& preference);
  bool GetBoolean(const std::string& preference);
  void SetBoolean(const std::string& preference, bool value);
  int32_t GetInteger(const std::string& preference);
  void SetInteger(const std::string& preference, int32_t value);
  double GetDouble(const std::string& preference);
  void SetDouble(const std::string& preference, double value);
  int64_t GetLong(const std::string& preference);
  void SetLong(const std::string& preference, int64_t value);
  std::string GetString(const std::string& preference);
  void SetString(const std::string& preference, const std::string& value);
  bool IsManagedPreference(const std::string& preference);
  bool HasRecommendation(const std::string& preference);
  bool IsFollowingRecommendation(const std::string& preference);
  bool IsRecommendedPreference(const std::string& preference);
  bool IsDefaultValuePreference(const std::string& preference);

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

template <>
COMPONENTS_PREFS_EXPORT ScopedJavaLocalRef<jobject> ToJniType<PrefService>(
    JNIEnv* env,
    PrefService* pref_service);
}  // namespace jni_zero

#endif  // COMPONENTS_PREFS_ANDROID_PREF_SERVICE_ANDROID_H_
