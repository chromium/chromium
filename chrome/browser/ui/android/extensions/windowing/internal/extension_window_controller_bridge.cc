// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/windowing/internal/extension_window_controller_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/ui/android/extensions/windowing/internal/jni/ExtensionWindowControllerBridgeImpl_jni.h"
#include "chrome/browser/ui/android/extensions/windowing/internal/window_controller_list_observer_for_testing.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using extensions::BrowserExtensionWindowController;
using extensions::WindowController;
using extensions::WindowControllerList;
}  // namespace

// Implements Java |ExtensionWindowControllerBridgeImpl.Natives#create|
static jlong JNI_ExtensionWindowControllerBridgeImpl_Create(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    jlong native_browser_window_ptr) {
  BrowserWindowInterface* browser_window =
      reinterpret_cast<BrowserWindowInterface*>(native_browser_window_ptr);

  return reinterpret_cast<intptr_t>(
      new ExtensionWindowControllerBridge(env, caller, browser_window));
}

ExtensionWindowControllerBridge::ExtensionWindowControllerBridge(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&
        java_extension_window_controller_bridge,
    BrowserWindowInterface* browser_window)
    : extension_window_controller_(
          BrowserExtensionWindowController(browser_window)) {
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

void ExtensionWindowControllerBridge::OnTaskBoundsChanged(JNIEnv* env) {
  extension_window_controller_.NotifyWindowBoundsChanged();
}

void ExtensionWindowControllerBridge::AddWindowControllerListObserverForTesting(
    JNIEnv* env) {
  CHECK(window_controller_list_observer_for_testing_ == nullptr)
      << "WindowControllerListObserverForTesting is already added.";

  window_controller_list_observer_for_testing_ =
      new WindowControllerListObserverForTesting(this);  // IN-TEST
  WindowControllerList::GetInstance()->AddObserver(
      window_controller_list_observer_for_testing_);
}

void ExtensionWindowControllerBridge::
    RemoveWindowControllerListObserverForTesting(JNIEnv* env) {
  if (window_controller_list_observer_for_testing_ != nullptr) {
    WindowControllerList::GetInstance()->RemoveObserver(
        window_controller_list_observer_for_testing_);

    delete window_controller_list_observer_for_testing_;
    window_controller_list_observer_for_testing_ = nullptr;
  }
}

const BrowserExtensionWindowController&
ExtensionWindowControllerBridge::GetExtensionWindowControllerForTesting() {
  return extension_window_controller_;
}

void ExtensionWindowControllerBridge::RecordExtensionInternalEventForTesting(
    ExtensionInternalWindowEventForTesting event) {
  Java_ExtensionWindowControllerBridgeImpl_recordExtensionInternalEventForTesting(  // IN-TEST
      AttachCurrentThread(), java_extension_window_controller_bridge_,
      static_cast<int>(event));
}
