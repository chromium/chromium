// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_actions_bridge.h"

#include <memory>
#include <variant>

#include "base/android/jni_string.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/events/android/key_event_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionAction_jni.h"
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionActionsBridge_jni.h"

namespace extensions {

ExtensionActionsBridge::ExtensionActionsBridge(
    BrowserWindowInterface* browser,
    const base::android::JavaRef<jobject>& java_object)
    : browser_(browser),
      profile_(browser->GetProfile()),
      java_object_(java_object) {}

ExtensionActionsBridge::~ExtensionActionsBridge() = default;

void ExtensionActionsBridge::Destroy(JNIEnv* env) {
  delete this;
}

static int64_t JNI_ExtensionActionsBridge_Init(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_object,
    int64_t j_browser_window_interface) {
  BrowserWindowInterface* browser =
      reinterpret_cast<BrowserWindowInterface*>(j_browser_window_interface);
  return reinterpret_cast<int64_t>(
      new ExtensionActionsBridge(browser, java_object));
}

static bool JNI_ExtensionActionsBridge_ExtensionsEnabled(JNIEnv* env,
                                                         Profile* profile) {
  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(profile);
  return extension_management->ExtensionsEnabledForDesktopAndroid();
}

}  // namespace extensions

DEFINE_JNI(ExtensionAction)
DEFINE_JNI(ExtensionActionsBridge)
