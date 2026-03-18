// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/android/side_panel_ui_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "chrome/browser/ui/side_panel/android/jni_headers/SidePanelUIAndroidBridge_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

// Implements Java `SidePanelUIAndroidBridge.Natives#create`.
static int64_t JNI_SidePanelUIAndroidBridge_Create(
    JNIEnv* env,
    const JavaRef<jobject>& caller) {
  return reinterpret_cast<intptr_t>(new SidePanelUIAndroid(env, caller));
}

SidePanelUIAndroid::SidePanelUIAndroid(JNIEnv* env,
                                       const JavaRef<jobject>& java_bridge)
    : java_bridge_(env, java_bridge) {}

SidePanelUIAndroid::~SidePanelUIAndroid() {
  Java_SidePanelUIAndroidBridge_clearNativePtr(
      base::android::AttachCurrentThread(), java_bridge());
}

void SidePanelUIAndroid::Destroy(JNIEnv* env) {
  delete this;
}

ScopedJavaLocalRef<jobject> SidePanelUIAndroid::java_bridge() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> local_ref = java_bridge_.get(env);

  CHECK(local_ref) << "Java SidePanelUIAndroidBridge is the sole owner of "
                      "SidePanelUIAndroid, so it shouldn't be destroyed before "
                      "SidePanelUIAndroidBridge";
  return local_ref;
}

DEFINE_JNI(SidePanelUIAndroidBridge)
