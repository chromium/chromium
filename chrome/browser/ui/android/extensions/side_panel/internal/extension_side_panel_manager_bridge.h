// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_SIDE_PANEL_INTERNAL_EXTENSION_SIDE_PANEL_MANAGER_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_SIDE_PANEL_INTERNAL_EXTENSION_SIDE_PANEL_MANAGER_BRIDGE_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

class BrowserWindowInterface;

namespace extensions {
class ExtensionSidePanelManager;
}  // namespace extensions

// Native class for the Java |ExtensionSidePanelManagerBridge|.
//
// The primary purpose of this class is to own a window-scoped
// |extensions::ExtensionSidePanelManager| and manage its lifecycle with the
// window.
class ExtensionSidePanelManagerBridge final {
 public:
  ExtensionSidePanelManagerBridge(JNIEnv* env,
                                  const base::android::JavaRef<jobject>&
                                      java_extension_side_panel_manager_bridge,
                                  BrowserWindowInterface* browser_window);
  ExtensionSidePanelManagerBridge(const ExtensionSidePanelManagerBridge&) =
      delete;
  ExtensionSidePanelManagerBridge& operator=(
      const ExtensionSidePanelManagerBridge&) = delete;
  ~ExtensionSidePanelManagerBridge();

  // Implements Java |ExtensionSidePanelManagerBridgeImpl.Natives#destroy|.
  void Destroy(JNIEnv* env);

 private:
  JavaObjectWeakGlobalRef java_extension_side_panel_manager_bridge_;
  raw_ptr<BrowserWindowInterface> browser_window_;
  std::unique_ptr<extensions::ExtensionSidePanelManager>
      extension_side_panel_manager_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_SIDE_PANEL_INTERNAL_EXTENSION_SIDE_PANEL_MANAGER_BRIDGE_H_
