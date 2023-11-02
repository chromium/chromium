// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_android.h"
#include "chrome/android/features/start_surface/jni_headers/StartSurfaceConfiguration_jni.h"
#include "chrome/browser/ui/android/start_surface/start_surface_android.h"

using base::android::JavaParamRef;

bool IsStartSurfaceBehaviouralTargetingEnabled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_StartSurfaceConfiguration_isBehaviouralTargetingEnabled(env);
}
