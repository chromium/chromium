// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_WINDOWING_INTERNAL_EXTENSION_WINDOW_CONTROLLER_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_WINDOWING_INTERNAL_EXTENSION_WINDOW_CONTROLLER_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

// Native class for the Java |ExtensionWindowControllerBridge|.
//
// The primary purpose of this class is to own a cross-platform
// |extensions::WindowController| and allow the Java class to communicate with
// it.
class ExtensionWindowControllerBridge final {
 public:
  ExtensionWindowControllerBridge(JNIEnv* env,
                                  const base::android::JavaParamRef<jobject>&
                                      java_extension_window_controller_bridge);
  ExtensionWindowControllerBridge(const ExtensionWindowControllerBridge&) =
      delete;
  ExtensionWindowControllerBridge& operator=(
      const ExtensionWindowControllerBridge&) = delete;
  ~ExtensionWindowControllerBridge();

  // Implements Java |ExtensionWindowControllerBridgeImpl.Natives#destroy|.
  void Destroy(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject>
      java_extension_window_controller_bridge_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_WINDOWING_INTERNAL_EXTENSION_WINDOW_CONTROLLER_BRIDGE_H_
