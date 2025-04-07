// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/toolbar_action_icon_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/toolbar/jni_headers/ToolbarActionIconBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using extensions::Extension;
using extensions::ExtensionAction;
using extensions::ExtensionActionManager;
using extensions::ExtensionRegistry;

ToolbarActionIconBridge::ToolbarActionIconBridge(
    const extensions::Extension& extension,
    extensions::ExtensionAction& action,
    const JavaParamRef<jobject>& java_object)
    : icon_factory_(&extension, &action, this), java_object_(java_object) {}

ToolbarActionIconBridge::~ToolbarActionIconBridge() = default;

void ToolbarActionIconBridge::Destroy(JNIEnv* env) {
  delete this;
}

base::android::ScopedJavaLocalRef<jobject> ToolbarActionIconBridge::GetIcon(
    JNIEnv* env,
    jint tab_id) {
  gfx::Image image = icon_factory_.GetIcon(static_cast<int>(tab_id));
  return gfx::ConvertToJavaBitmap(*image.ToSkBitmap());
}

void ToolbarActionIconBridge::OnIconUpdated() {
  Java_ToolbarActionIconBridge_onIconUpdated(AttachCurrentThread(),
                                             java_object_);
}

jlong JNI_ToolbarActionIconBridge_Init(JNIEnv* env,
                                       const JavaParamRef<jobject>& java_object,
                                       Profile* profile,
                                       std::string& action_id) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  DCHECK(registry);

  ExtensionActionManager* manager = ExtensionActionManager::Get(profile);
  DCHECK(manager);

  const Extension* extension =
      registry->enabled_extensions().GetByID(action_id);
  if (extension == nullptr) {
    return 0;
  }

  ExtensionAction* action = manager->GetExtensionAction(*extension);
  if (action == nullptr) {
    return 0;
  }

  return reinterpret_cast<jlong>(
      new ToolbarActionIconBridge(*extension, *action, java_object));
}
