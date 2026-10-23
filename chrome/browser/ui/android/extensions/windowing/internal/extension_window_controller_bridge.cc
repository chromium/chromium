// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/windowing/internal/extension_window_controller_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/check.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/ui/android/extensions/windowing/internal/jni/ExtensionWindowControllerBridgeImpl_jni.h"
#include "chrome/browser/ui/android/extensions/windowing/internal/window_controller_list_observer_for_testing.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "third_party/jni_zero/jni_zero.h"

namespace {
using base::android::AttachCurrentThread;
using extensions::BrowserExtensionWindowController;
using extensions::WindowController;
using extensions::WindowControllerList;
}  // namespace

// Implements Java |ExtensionWindowControllerBridgeImpl.Natives#create|
static jni_zero::JniUniquePtr<ExtensionWindowControllerBridge>
JNI_ExtensionWindowControllerBridgeImpl_Create(
    JNIEnv* env,
    BrowserWindowInterface* browser_window) {
  return jni_zero::MakeUnique<ExtensionWindowControllerBridge>(browser_window);
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
    BrowserWindowInterface* browser_window)
    : extension_window_controller_(
          BrowserExtensionWindowController(browser_window)) {}

ExtensionWindowControllerBridge::~ExtensionWindowControllerBridge() = default;

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
