// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "chrome/browser/ui/side_panel/internal/android/jni_headers/SidePanelCoordinatorAndroidBridgeImpl_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

// Implements Java `SidePanelCoordinatorAndroidBridgeImpl.Natives#create`.
static int64_t JNI_SidePanelCoordinatorAndroidBridgeImpl_Create(
    JNIEnv* env,
    const JavaRef<jobject>& caller) {
  return reinterpret_cast<intptr_t>(
      new SidePanelCoordinatorAndroid(env, caller));
}

SidePanelCoordinatorAndroid::SidePanelCoordinatorAndroid(
    JNIEnv* env,
    const JavaRef<jobject>& java_bridge)
    : java_bridge_(env, java_bridge) {}

SidePanelCoordinatorAndroid::~SidePanelCoordinatorAndroid() {
  Java_SidePanelCoordinatorAndroidBridgeImpl_clearNativePtr(
      base::android::AttachCurrentThread(), java_bridge());
}

void SidePanelCoordinatorAndroid::Destroy(JNIEnv* env) {
  delete this;
}

ScopedJavaLocalRef<jobject> SidePanelCoordinatorAndroid::java_bridge() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> local_ref = java_bridge_.get(env);

  CHECK(local_ref)
      << "Java SidePanelCoordinatorAndroidBridge is the sole owner of "
         "SidePanelCoordinatorAndroid, so it shouldn't be destroyed before "
         "SidePanelCoordinatorAndroidBridge";
  return local_ref;
}

DEFINE_JNI(SidePanelCoordinatorAndroidBridgeImpl)
