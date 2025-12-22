// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extensions_toolbar_bridge.h"

#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionsToolbarBridge_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace extensions {

ExtensionsToolbarBridge::ExtensionsToolbarBridge(
    BrowserWindowInterface* browser,
    const base::android::JavaRef<jobject>& java_object)
    : browser_(browser), java_object_(java_object) {}

ExtensionsToolbarBridge::~ExtensionsToolbarBridge() = default;

void ExtensionsToolbarBridge::Destroy(JNIEnv* env) {
  delete this;
}

static jlong JNI_ExtensionsToolbarBridge_Init(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_object,
    jlong j_browser_window_interface) {
  BrowserWindowInterface* browser =
      reinterpret_cast<BrowserWindowInterface*>(j_browser_window_interface);
  return reinterpret_cast<jlong>(
      new ExtensionsToolbarBridge(browser, java_object));
}

}  // namespace extensions

DEFINE_JNI(ExtensionsToolbarBridge)
