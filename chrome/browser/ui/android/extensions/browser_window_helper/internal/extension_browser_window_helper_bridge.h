// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_BROWSER_WINDOW_HELPER_INTERNAL_EXTENSION_BROWSER_WINDOW_HELPER_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_BROWSER_WINDOW_HELPER_INTERNAL_EXTENSION_BROWSER_WINDOW_HELPER_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/extensions/extension_browser_window_helper.h"

class BrowserWindowInterface;

// Native class for the Java |ExtensionBrowserWindowHelperBridge|.
//
// The primary purpose of this class is to own a cross-platform
// |extensions::ExtensionBrowserWindowHelper| and manage its lifecycle with the
// window.
class ExtensionBrowserWindowHelperBridge final {
 public:
  ExtensionBrowserWindowHelperBridge(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& java_bridge,
      BrowserWindowInterface* browser_window);
  ExtensionBrowserWindowHelperBridge(
      const ExtensionBrowserWindowHelperBridge&) = delete;
  ExtensionBrowserWindowHelperBridge& operator=(
      const ExtensionBrowserWindowHelperBridge&) = delete;
  ~ExtensionBrowserWindowHelperBridge();

  // Implements Java |ExtensionBrowserWindowHelperBridgeImpl.Natives#destroy|.
  void Destroy(JNIEnv* env);

 private:
  JavaObjectWeakGlobalRef java_bridge_;

  extensions::ExtensionBrowserWindowHelper extension_browser_window_helper_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_BROWSER_WINDOW_HELPER_INTERNAL_EXTENSION_BROWSER_WINDOW_HELPER_BRIDGE_H_
