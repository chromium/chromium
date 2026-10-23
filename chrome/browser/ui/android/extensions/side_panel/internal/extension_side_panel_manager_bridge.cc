// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/side_panel/internal/extension_side_panel_manager_bridge.h"

#include <jni.h>

#include "base/check.h"
#include "chrome/browser/ui/android/extensions/side_panel/internal/jni/ExtensionSidePanelManagerBridgeImpl_jni.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "third_party/jni_zero/jni_zero.h"

// Implements Java |ExtensionSidePanelManagerBridgeImpl.Natives#create|.
static jni_zero::JniUniquePtr<ExtensionSidePanelManagerBridge>
JNI_ExtensionSidePanelManagerBridgeImpl_Create(
    JNIEnv* env,
    BrowserWindowInterface* browser_window) {
  return jni_zero::MakeUnique<ExtensionSidePanelManagerBridge>(browser_window);
}

ExtensionSidePanelManagerBridge::ExtensionSidePanelManagerBridge(
    BrowserWindowInterface* browser_window)
    : browser_window_(browser_window) {
  CHECK(browser_window_);

  SidePanelRegistry* registry = SidePanelRegistry::From(browser_window_);
  CHECK(registry);

  extension_side_panel_manager_ =
      std::make_unique<extensions::ExtensionSidePanelManager>(browser_window_,
                                                              registry);
}

ExtensionSidePanelManagerBridge::~ExtensionSidePanelManagerBridge() = default;

DEFINE_JNI(ExtensionSidePanelManagerBridgeImpl)
