// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_WINDOW_SCOPED_SIDE_PANEL_REGISTRY_BRIDGE_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_WINDOW_SCOPED_SIDE_PANEL_REGISTRY_BRIDGE_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"

class BrowserWindowInterface;

// JNI bridge that owns a window-scoped `SidePanelRegistry` and is owned by the
// Java `WindowScopedSidePanelRegistryBridge`.
class WindowScopedSidePanelRegistryBridge final {
 public:
  WindowScopedSidePanelRegistryBridge(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& java_bridge,
      BrowserWindowInterface* browser);
  WindowScopedSidePanelRegistryBridge(
      const WindowScopedSidePanelRegistryBridge&) = delete;
  WindowScopedSidePanelRegistryBridge& operator=(
      const WindowScopedSidePanelRegistryBridge&) = delete;

  ~WindowScopedSidePanelRegistryBridge();

  // Implements Java `WindowScopedSidePanelRegistryBridgeImpl.Natives#destroy`.
  void Destroy(JNIEnv* env);

  SidePanelRegistry* GetSidePanelRegistryForTesting() const;

 private:
  base::android::ScopedJavaLocalRef<jobject> java_bridge() const;

  JavaObjectWeakGlobalRef java_bridge_;
  std::unique_ptr<SidePanelRegistry> side_panel_registry_;
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_WINDOW_SCOPED_SIDE_PANEL_REGISTRY_BRIDGE_H_
