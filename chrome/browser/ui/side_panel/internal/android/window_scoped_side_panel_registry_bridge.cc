// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/window_scoped_side_panel_registry_bridge.h"

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/internal/android/jni_headers/WindowScopedSidePanelRegistryBridgeImpl_jni.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

// Implements Java `WindowScopedSidePanelRegistryBridgeImpl.Natives#create`.
static int64_t JNI_WindowScopedSidePanelRegistryBridgeImpl_Create(
    JNIEnv* env,
    const JavaRef<jobject>& caller,
    int64_t nativeBrowserWindowPtr) {
  return reinterpret_cast<intptr_t>(new WindowScopedSidePanelRegistryBridge(
      env, caller,
      reinterpret_cast<BrowserWindowInterface*>(nativeBrowserWindowPtr)));
}

WindowScopedSidePanelRegistryBridge ::WindowScopedSidePanelRegistryBridge(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_bridge,
    BrowserWindowInterface* browser)
    : java_bridge_(env, java_bridge),
      side_panel_registry_(std::make_unique<SidePanelRegistry>(browser)) {}

WindowScopedSidePanelRegistryBridge::~WindowScopedSidePanelRegistryBridge() {
  Java_WindowScopedSidePanelRegistryBridgeImpl_clearNativePtr(
      base::android::AttachCurrentThread(), java_bridge());
}

void WindowScopedSidePanelRegistryBridge::Destroy(JNIEnv* env) {
  delete this;
}

SidePanelRegistry*
WindowScopedSidePanelRegistryBridge::GetSidePanelRegistryForTesting() const {
  return side_panel_registry_.get();
}

ScopedJavaLocalRef<jobject> WindowScopedSidePanelRegistryBridge::java_bridge()
    const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> local_ref = java_bridge_.get(env);

  CHECK(local_ref) << "Java WindowScopedSidePanelRegistryBridge is the sole "
                      "owner of its C++ counterpart, so the Java object "
                      "shouldn't be destroyed before the C++ object";
  return local_ref;
}

DEFINE_JNI(WindowScopedSidePanelRegistryBridgeImpl)
