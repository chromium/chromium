// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSION_TAB_CONTEXT_MENU_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSION_TAB_CONTEXT_MENU_BRIDGE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "extensions/buildflags/buildflags.h"

namespace content {
class WebContents;
}

namespace ui {
class MenuModelBridge;
}

namespace extensions {

class ExtensionMenuModel;

// Manages the C++ ExtensionMenuModel and MenuModelBridge for a tab context
// menu on Android.
//
// Lifetime management:
// An instance of this C++ class is created when its Java counterpart
// (ExtensionTabContextMenuBridge.java) requests it via JNI. The C++ object's
// lifetime is tied to its Java peer; when the Java object is no longer needed,
// its destroy() method calls Destroy() on this C++ object, which calls
// delete this.
class ExtensionTabContextMenuBridge {
 public:
  explicit ExtensionTabContextMenuBridge(content::WebContents* web_contents);
  ExtensionTabContextMenuBridge(const ExtensionTabContextMenuBridge&) = delete;
  ExtensionTabContextMenuBridge& operator=(
      const ExtensionTabContextMenuBridge&) = delete;
  ~ExtensionTabContextMenuBridge();

  base::android::ScopedJavaLocalRef<jobject> GetMenuModelBridge(JNIEnv* env);
  void Destroy(JNIEnv* env);

  bool IsEmpty() const;

 private:
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  std::unique_ptr<ExtensionMenuModel> menu_model_;
  std::unique_ptr<ui::MenuModelBridge> menu_model_bridge_;
#endif
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSION_TAB_CONTEXT_MENU_BRIDGE_H_
