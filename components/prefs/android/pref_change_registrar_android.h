// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_ANDROID_PREF_CHANGE_REGISTRAR_ANDROID_H_
#define COMPONENTS_PREFS_ANDROID_PREF_CHANGE_REGISTRAR_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;

class PrefService;

// This class contains a PrefChangeRegistrar that observes PrefService changes
// for Android.
class PrefChangeRegistrarAndroid {
 public:
  PrefChangeRegistrarAndroid(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             PrefService* prefs);
  void Destroy(JNIEnv*, const JavaParamRef<jobject>&);

  PrefChangeRegistrarAndroid(const PrefChangeRegistrarAndroid&) = delete;
  PrefChangeRegistrarAndroid& operator=(const PrefChangeRegistrarAndroid&) =
      delete;

  void Add(JNIEnv* env,
           const JavaParamRef<jobject>& obj,
           const JavaParamRef<jstring>& j_preference);
  void Remove(JNIEnv* env,
              const JavaParamRef<jobject>& obj,
              const JavaParamRef<jstring>& j_preference);

 private:
  ~PrefChangeRegistrarAndroid();
  void OnPreferenceChange(std::string preference);

  PrefChangeRegistrar pref_change_registrar_;
  ScopedJavaGlobalRef<jobject> pref_change_registrar_jobject_;
};

#endif  // COMPONENTS_PREFS_ANDROID_PREF_CHANGE_REGISTRAR_ANDROID_H_
