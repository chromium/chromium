// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_WINDOWING_INTERNAL_EXTENSION_WINDOW_CONTROLLER_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_WINDOWING_INTERNAL_EXTENSION_WINDOW_CONTROLLER_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"

class BrowserWindowInterface;

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

  const extensions::BrowserExtensionWindowController&
  GetExtensionWindowControllerForTesting();

 private:
  base::android::ScopedJavaGlobalRef<jobject>
      java_extension_window_controller_bridge_;

  extensions::BrowserExtensionWindowController extension_window_controller_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_WINDOWING_INTERNAL_EXTENSION_WINDOW_CONTROLLER_BRIDGE_H_
