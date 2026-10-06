// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/browser_window_helper/internal/extension_browser_window_helper_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/browser/extensions/extension_browser_window_helper.h"
#include "chrome/browser/ui/android/extensions/browser_window_helper/internal/jni/ExtensionBrowserWindowHelperBridgeImpl_jni.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
}  // namespace

// Implements Java |ExtensionBrowserWindowHelperBridgeImpl.Natives#create|.
static int64_t JNI_ExtensionBrowserWindowHelperBridgeImpl_Create(
    JNIEnv* env,
    const JavaRef<jobject>& caller,
    int64_t native_browser_window_ptr) {
  BrowserWindowInterface* browser_window =
      reinterpret_cast<BrowserWindowInterface*>(native_browser_window_ptr);

  return reinterpret_cast<intptr_t>(
      new ExtensionBrowserWindowHelperBridge(env, caller, browser_window));
}

ExtensionBrowserWindowHelperBridge::ExtensionBrowserWindowHelperBridge(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_bridge,
    BrowserWindowInterface* browser_window)
    : java_bridge_(env, java_bridge),
      extension_browser_window_helper_(
          browser_window,
          CHECK_DEREF(browser_window).GetProfile()) {}

ExtensionBrowserWindowHelperBridge::~ExtensionBrowserWindowHelperBridge() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_bridge = java_bridge_.get(env);
  CHECK(java_bridge)
      << "Java ExtensionBrowserWindowHelperBridge is the sole owner of "
         "C++ ExtensionBrowserWindowHelperBridge, so the Java object "
         "shouldn't be destroyed before the C++ object";
  Java_ExtensionBrowserWindowHelperBridgeImpl_clearNativePtr(env, java_bridge);
}

void ExtensionBrowserWindowHelperBridge::Destroy(JNIEnv* env) {
  delete this;
}

DEFINE_JNI(ExtensionBrowserWindowHelperBridgeImpl)
