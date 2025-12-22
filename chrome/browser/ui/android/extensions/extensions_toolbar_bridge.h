// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSIONS_TOOLBAR_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSIONS_TOOLBAR_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "third_party/jni_zero/jni_zero.h"

class BrowserWindowInterface;

namespace extensions {

// The JNI bridge for the extensions UI.
// This bridge is created and owned by Java UI code.
class ExtensionsToolbarBridge {
 public:
  ExtensionsToolbarBridge(BrowserWindowInterface* browser,
                          const base::android::JavaRef<jobject>& java_object);
  ExtensionsToolbarBridge(const ExtensionsToolbarBridge&) = delete;
  ExtensionsToolbarBridge& operator=(const ExtensionsToolbarBridge&) = delete;
  ~ExtensionsToolbarBridge();

  // JNI implementations.
  void Destroy(JNIEnv* env);

 private:
  const raw_ptr<BrowserWindowInterface> browser_;
  const base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSIONS_TOOLBAR_BRIDGE_H_
