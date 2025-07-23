// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/windowing/internal/extension_window_controller_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/android/extensions/windowing/internal/jni/ExtensionWindowControllerBridgeImpl_jni.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
}  // namespace

// Implements Java |ExtensionWindowControllerBridgeImpl.Natives#create|
static jlong JNI_ExtensionWindowControllerBridgeImpl_Create(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  return reinterpret_cast<intptr_t>(
      new ExtensionWindowControllerBridge(env, caller));
}

ExtensionWindowControllerBridge::ExtensionWindowControllerBridge(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&
        java_extension_window_controller_bridge) {
  java_extension_window_controller_bridge_.Reset(
      env, java_extension_window_controller_bridge);
}

ExtensionWindowControllerBridge::~ExtensionWindowControllerBridge() {
  Java_ExtensionWindowControllerBridgeImpl_clearNativePtr(
      AttachCurrentThread(), java_extension_window_controller_bridge_);
}

void ExtensionWindowControllerBridge::Destroy(JNIEnv* env) {
  delete this;
}
