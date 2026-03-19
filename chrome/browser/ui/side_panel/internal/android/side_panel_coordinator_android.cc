// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "chrome/browser/ui/side_panel/internal/android/jni_headers/SidePanelCoordinatorAndroidImpl_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

// Implements Java `SidePanelCoordinatorAndroidImpl.Natives#create`.
static int64_t JNI_SidePanelCoordinatorAndroidImpl_Create(
    JNIEnv* env,
    const JavaRef<jobject>& caller) {
  return reinterpret_cast<intptr_t>(
      new SidePanelCoordinatorAndroid(env, caller));
}

SidePanelCoordinatorAndroid::SidePanelCoordinatorAndroid(
    JNIEnv* env,
    const JavaRef<jobject>& java_coordinator)
    : java_coordinator_(env, java_coordinator) {}

SidePanelCoordinatorAndroid::~SidePanelCoordinatorAndroid() {
  Java_SidePanelCoordinatorAndroidImpl_clearNativePtr(
      base::android::AttachCurrentThread(), java_coordinator());
}

void SidePanelCoordinatorAndroid::Destroy(JNIEnv* env) {
  delete this;
}

ScopedJavaLocalRef<jobject> SidePanelCoordinatorAndroid::java_coordinator()
    const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> local_ref = java_coordinator_.get(env);

  CHECK(local_ref) << "Java SidePanelCoordinatorAndroid is the sole owner of "
                      "C++ SidePanelCoordinatorAndroid, so the Java object "
                      "shouldn't be destroyed before the C++ object";
  return local_ref;
}

DEFINE_JNI(SidePanelCoordinatorAndroidImpl)
