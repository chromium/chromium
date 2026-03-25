// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/tab_scoped_side_panel_registry_bridge.h"

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/side_panel/internal/android/jni_headers/TabScopedSidePanelRegistryBridgeImpl_jni.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

// Implements Java `TabScopedSidePanelRegistryBridgeImpl.Natives#create`.
static int64_t JNI_TabScopedSidePanelRegistryBridgeImpl_Create(
    JNIEnv* env,
    const JavaRef<jobject>& caller,
    const JavaRef<jobject>& j_tab) {
  return reinterpret_cast<intptr_t>(new TabScopedSidePanelRegistryBridge(
      env, caller, TabAndroid::GetNativeTab(env, j_tab)));
}

// Implements Java
// `TabScopedSidePanelRegistryBridgeImpl.Natives#createForTesting`.
static int64_t
JNI_TabScopedSidePanelRegistryBridgeImpl_CreateForTesting(  // IN-TEST
    JNIEnv* env,
    const JavaRef<jobject>& caller,
    int64_t nativeMockTabInterfacePtr) {
  return reinterpret_cast<intptr_t>(new TabScopedSidePanelRegistryBridge(
      env, caller,
      reinterpret_cast<tabs::TabInterface*>(nativeMockTabInterfacePtr)));
}

TabScopedSidePanelRegistryBridge ::TabScopedSidePanelRegistryBridge(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_bridge,
    tabs::TabInterface* tab)
    : java_bridge_(env, java_bridge),
      side_panel_registry_(std::make_unique<SidePanelRegistry>(tab)) {}

TabScopedSidePanelRegistryBridge::~TabScopedSidePanelRegistryBridge() {
  Java_TabScopedSidePanelRegistryBridgeImpl_clearNativePtr(
      base::android::AttachCurrentThread(), java_bridge());
}

void TabScopedSidePanelRegistryBridge::Destroy(JNIEnv* env) {
  delete this;
}

SidePanelRegistry*
TabScopedSidePanelRegistryBridge::GetSidePanelRegistryForTesting() const {
  return side_panel_registry_.get();
}

ScopedJavaLocalRef<jobject> TabScopedSidePanelRegistryBridge::java_bridge()
    const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> local_ref = java_bridge_.get(env);

  CHECK(local_ref) << "Java TabScopedSidePanelRegistryBridge is the sole owner "
                      "of its C++ counterpart, so the Java object shouldn't be "
                      "destroyed before the C++ object";
  return local_ref;
}

DEFINE_JNI(TabScopedSidePanelRegistryBridgeImpl)
