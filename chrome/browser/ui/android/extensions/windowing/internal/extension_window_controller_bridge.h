// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_WINDOWING_INTERNAL_EXTENSION_WINDOW_CONTROLLER_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_WINDOWING_INTERNAL_EXTENSION_WINDOW_CONTROLLER_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/window_controller.h"

class BrowserWindowInterface;
class WindowControllerListObserverForTesting;
enum class ExtensionInternalWindowEventForTesting;

// Native class for the Java |ExtensionWindowControllerBridge|.
//
// The primary purpose of this class is to own a cross-platform
// |extensions::WindowController| and allow the Java class to communicate with
// it.
class ExtensionWindowControllerBridge final {
 public:
  ExtensionWindowControllerBridge(JNIEnv* env,
                                  const base::android::JavaParamRef<jobject>&
                                      java_extension_window_controller_bridge,
                                  BrowserWindowInterface* browser_window);
  ExtensionWindowControllerBridge(const ExtensionWindowControllerBridge&) =
      delete;
  ExtensionWindowControllerBridge& operator=(
      const ExtensionWindowControllerBridge&) = delete;
  ~ExtensionWindowControllerBridge();

  // Implements Java |ExtensionWindowControllerBridgeImpl.Natives#destroy|.
  void Destroy(JNIEnv* env);

  // Implements Java
  // |ExtensionWindowControllerBridgeImpl.Natives#onTaskBoundsChanged|.
  void OnTaskBoundsChanged(JNIEnv* env);

  // Implements Java
  // |ExtensionWindowControllerBridgeImpl.Natives#onTaskFocusChanged|.
  void OnTaskFocusChanged(JNIEnv* env, bool has_focus);

  // Implements the Java |getExtensionWindowIdForTesting| method in
  // |ExtensionWindowControllerBridgeImpl.Natives|.
  int GetExtensionWindowIdForTesting(JNIEnv* env);

  const extensions::BrowserExtensionWindowController&
  GetExtensionWindowControllerForTesting();

 private:
  friend class WindowControllerListObserverForTesting;

  // Records window events received by |WindowControllerListObserverForTesting|
  // so that tests can verify the events.
  // |window_controller| is the |extensions::WindowController| that received
  // the |event|.
  static void RecordExtensionInternalEventForTesting(
      extensions::WindowController* window_controller,
      ExtensionInternalWindowEventForTesting event);

  base::android::ScopedJavaGlobalRef<jobject>
      java_extension_window_controller_bridge_;

  extensions::BrowserExtensionWindowController extension_window_controller_;

  raw_ptr<WindowControllerListObserverForTesting>
      window_controller_list_observer_for_testing_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_WINDOWING_INTERNAL_EXTENSION_WINDOW_CONTROLLER_BRIDGE_H_
