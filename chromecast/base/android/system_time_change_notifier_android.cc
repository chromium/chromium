// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/android/system_time_change_notifier_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chromecast/base/jni_headers/SystemTimeChangeNotifierAndroid_jni.h"

using jni_zero::JavaParamRef;

namespace chromecast {

SystemTimeChangeNotifierAndroid::SystemTimeChangeNotifierAndroid() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  java_notifier_.Reset(Java_SystemTimeChangeNotifierAndroid_create(env));
  Java_SystemTimeChangeNotifierAndroid_initializeFromNative(
      env, java_notifier_, reinterpret_cast<jlong>(this));
}

SystemTimeChangeNotifierAndroid::~SystemTimeChangeNotifierAndroid() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_SystemTimeChangeNotifierAndroid_finalizeFromNative(env, java_notifier_);
}

void SystemTimeChangeNotifierAndroid::OnTimeChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  NotifySystemTimeChanged();
}

}  // namespace chromecast
