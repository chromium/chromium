// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/side_panel/internal/extension_side_panel_manager_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "chrome/browser/ui/android/extensions/side_panel/internal/jni/ExtensionSidePanelManagerBridgeImpl_jni.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
}  // namespace

// Implements Java |ExtensionSidePanelManagerBridgeImpl.Natives#create|.
static int64_t JNI_ExtensionSidePanelManagerBridgeImpl_Create(
    JNIEnv* env,
    const JavaRef<jobject>& caller,
    int64_t native_browser_window_ptr) {
  BrowserWindowInterface* browser_window =
      reinterpret_cast<BrowserWindowInterface*>(native_browser_window_ptr);

  return reinterpret_cast<intptr_t>(
      new ExtensionSidePanelManagerBridge(env, caller, browser_window));
}

ExtensionSidePanelManagerBridge::ExtensionSidePanelManagerBridge(
    JNIEnv* env,
    const base::android::JavaRef<jobject>&
        java_extension_side_panel_manager_bridge,
    BrowserWindowInterface* browser_window)
    : java_extension_side_panel_manager_bridge_(
          env,
          java_extension_side_panel_manager_bridge),
      browser_window_(browser_window) {
  CHECK(browser_window_);

  SidePanelRegistry* registry = SidePanelRegistry::From(browser_window_);
  CHECK(registry);

  extension_side_panel_manager_ =
      std::make_unique<extensions::ExtensionSidePanelManager>(browser_window_,
                                                              registry);
}

ExtensionSidePanelManagerBridge::~ExtensionSidePanelManagerBridge() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_bridge =
      java_extension_side_panel_manager_bridge_.get(env);
  if (java_bridge) {
    Java_ExtensionSidePanelManagerBridgeImpl_clearNativePtr(env, java_bridge);
  }
}

void ExtensionSidePanelManagerBridge::Destroy(JNIEnv* env) {
  delete this;
}

DEFINE_JNI(ExtensionSidePanelManagerBridgeImpl)
