// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/android/system_time_change_notifier_android.h"

#include "chromecast/base/jni_headers/SystemTimeChangeNotifierAndroid_jni.h"

using base::android::JavaParamRef;

namespace chromecast {

SystemTimeChangeNotifierAndroid::SystemTimeChangeNotifierAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_notifier_.Reset(Java_SystemTimeChangeNotifierAndroid_create(env));
  Java_SystemTimeChangeNotifierAndroid_initializeFromNative(
      env, java_notifier_, reinterpret_cast<jlong>(this));
}

SystemTimeChangeNotifierAndroid::~SystemTimeChangeNotifierAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SystemTimeChangeNotifierAndroid_finalizeFromNative(env, java_notifier_);
}

void SystemTimeChangeNotifierAndroid::OnTimeChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  NotifySystemTimeChanged();
}

}  // namespace chromecast
