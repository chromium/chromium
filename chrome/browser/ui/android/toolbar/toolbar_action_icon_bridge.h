// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_ACTION_ICON_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_ACTION_ICON_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "extensions/browser/extension_action_icon_factory.h"

namespace extensions {
class Extension;
class ExtensionAction;
}  // namespace extensions

class ToolbarActionIconBridge
    : public extensions::ExtensionActionIconFactory::Observer {
 public:
  ToolbarActionIconBridge(
      const extensions::Extension& extension,
      extensions::ExtensionAction& action,
      const base::android::JavaParamRef<jobject>& java_object);
  ToolbarActionIconBridge(const ToolbarActionIconBridge&) = delete;
  ToolbarActionIconBridge& operator=(const ToolbarActionIconBridge&) = delete;
  ~ToolbarActionIconBridge() override;

  // JNI implementations.
  void Destroy(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetIcon(JNIEnv* env, jint tab_id);

  // extensions::ExtensionActionIconFactory::Observer:
  void OnIconUpdated() override;

 private:
  extensions::ExtensionActionIconFactory icon_factory_;
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_ACTION_ICON_BRIDGE_H_
