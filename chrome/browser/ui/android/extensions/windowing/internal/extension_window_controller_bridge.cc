// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/windowing/internal/extension_window_controller_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/ui/android/extensions/windowing/internal/jni/ExtensionWindowControllerBridgeImpl_jni.h"
#include "chrome/browser/ui/android/extensions/windowing/internal/window_controller_list_observer_for_testing.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using extensions::BrowserExtensionWindowController;
using extensions::WindowController;
using extensions::WindowControllerList;
}  // namespace

// Implements Java |ExtensionWindowControllerBridgeImpl.Natives#create|
static int64_t JNI_ExtensionWindowControllerBridgeImpl_Create(
    JNIEnv* env,
    const JavaRef<jobject>& caller,
    int64_t native_browser_window_ptr) {
  BrowserWindowInterface* browser_window =
      reinterpret_cast<BrowserWindowInterface*>(native_browser_window_ptr);

  return reinterpret_cast<intptr_t>(
      new ExtensionWindowControllerBridge(env, caller, browser_window));
}

// Implements the Java |addWindowControllerListObserverForTesting()| method in
// |ExtensionWindowControllerBridgeImpl.Natives|.
static void
JNI_ExtensionWindowControllerBridgeImpl_AddWindowControllerListObserverForTesting(  // IN-TEST
    JNIEnv* env) {
  WindowControllerList::GetInstance()->AddObserver(
      WindowControllerListObserverForTesting::GetInstance());
}

// Implements the Java |removeWindowControllerListObserverForTesting()| method
// in |ExtensionWindowControllerBridgeImpl.Natives|.
static void
JNI_ExtensionWindowControllerBridgeImpl_RemoveWindowControllerListObserverForTesting(  // IN-TEST
    JNIEnv* env) {
  WindowControllerList::GetInstance()->RemoveObserver(
      WindowControllerListObserverForTesting::GetInstance());
}

// static
void ExtensionWindowControllerBridge::RecordExtensionInternalEventForTesting(
    extensions::WindowController* window_controller,
    ExtensionInternalWindowEventForTesting event) {
  Java_ExtensionWindowControllerBridgeImpl_recordExtensionInternalEventForTesting(  // IN-TEST
      AttachCurrentThread(), window_controller->GetWindowId(),
      static_cast<int>(event));
}

ExtensionWindowControllerBridge::ExtensionWindowControllerBridge(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_bridge,
    BrowserWindowInterface* browser_window)
    : java_bridge_(env, java_bridge),
      extension_window_controller_(
          BrowserExtensionWindowController(browser_window)) {}

ExtensionWindowControllerBridge::~ExtensionWindowControllerBridge() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_bridge = java_bridge_.get(env);
  CHECK(java_bridge)
      << "Java ExtensionWindowControllerBridge is the sole owner of "
         "C++ ExtensionWindowControllerBridge, so the Java object "
         "shouldn't be destroyed before the C++ object";
  Java_ExtensionWindowControllerBridgeImpl_clearNativePtr(env, java_bridge);
}

void ExtensionWindowControllerBridge::Destroy(JNIEnv* env) {
  delete this;
}

void ExtensionWindowControllerBridge::OnTaskBoundsChanged(JNIEnv* env) {
  extension_window_controller_.NotifyWindowBoundsChanged();
}

void ExtensionWindowControllerBridge::OnTaskFocusChanged(JNIEnv* env,
                                                         bool has_focus) {
  extension_window_controller_.NotifyWindowFocusChanged(has_focus);
}

int ExtensionWindowControllerBridge::GetExtensionWindowIdForTesting(
    JNIEnv* env) {
  return extension_window_controller_.GetWindowId();
}

const BrowserExtensionWindowController&
ExtensionWindowControllerBridge::GetExtensionWindowControllerForTesting() {
  return extension_window_controller_;
}

DEFINE_JNI(ExtensionWindowControllerBridgeImpl)
