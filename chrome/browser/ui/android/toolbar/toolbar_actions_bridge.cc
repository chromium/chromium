// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/toolbar_actions_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/toolbar/toolbar_actions_bridge_factory.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/toolbar/jni_headers/ToolbarAction_jni.h"
#include "chrome/browser/ui/android/toolbar/jni_headers/ToolbarActionsBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using extensions::Extension;
using extensions::ExtensionAction;
using extensions::ExtensionActionManager;
using extensions::ExtensionRegistry;

ToolbarActionsBridge::ToolbarActionsBridge(Profile* profile)
    : profile_(profile), model_(ToolbarActionsModel::Get(profile)) {
  java_object_ = Java_ToolbarActionsBridge_Constructor(
      AttachCurrentThread(), reinterpret_cast<jlong>(this));
  model_observation_.Observe(model_);
}

ToolbarActionsBridge::~ToolbarActionsBridge() {
  Java_ToolbarActionsBridge_destroy(AttachCurrentThread(), java_object_);
}

// static
ToolbarActionsBridge* ToolbarActionsBridge::Get(Profile* profile) {
  return ToolbarActionsBridgeFactory::GetForProfile(profile);
}

ScopedJavaLocalRef<jobject> ToolbarActionsBridge::GetJavaObject() {
  return java_object_.AsLocalRef(AttachCurrentThread());
}

jboolean ToolbarActionsBridge::AreActionsInitialized(JNIEnv* env) {
  return static_cast<jboolean>(model_->actions_initialized());
}

std::vector<std::string> ToolbarActionsBridge::GetActionIds(JNIEnv* env) {
  const auto& ids = model_->action_ids();
  return std::vector(ids.begin(), ids.end());
}

ScopedJavaLocalRef<jobject> ToolbarActionsBridge::GetAction(
    JNIEnv* env,
    const std::string& action_id,
    jint tab_id) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  DCHECK(registry);
  ExtensionActionManager* manager = ExtensionActionManager::Get(profile_);
  DCHECK(manager);

  const Extension* extension =
      registry->enabled_extensions().GetByID(action_id);
  if (extension == nullptr) {
    return nullptr;
  }

  ExtensionAction* action = manager->GetExtensionAction(*extension);
  if (action == nullptr) {
    return nullptr;
  }

  return Java_ToolbarAction_Constructor(
      env, action_id, action->GetTitle(static_cast<int>(tab_id)));
}

void ToolbarActionsBridge::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& id) {
  Java_ToolbarActionsBridge_onToolbarActionAdded(AttachCurrentThread(),
                                                 java_object_, id);
}

void ToolbarActionsBridge::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& id) {
  Java_ToolbarActionsBridge_onToolbarActionRemoved(AttachCurrentThread(),
                                                   java_object_, id);
}

void ToolbarActionsBridge::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& id) {
  Java_ToolbarActionsBridge_onToolbarActionUpdated(AttachCurrentThread(),
                                                   java_object_, id);
}

void ToolbarActionsBridge::OnToolbarModelInitialized() {
  Java_ToolbarActionsBridge_onToolbarModelInitialized(AttachCurrentThread(),
                                                      java_object_);
}

void ToolbarActionsBridge::OnToolbarPinnedActionsChanged() {
  Java_ToolbarActionsBridge_onToolbarPinnedActionsChanged(AttachCurrentThread(),
                                                          java_object_);
}

static ScopedJavaLocalRef<jobject> JNI_ToolbarActionsBridge_Get(
    JNIEnv* env,
    Profile* profile) {
  ToolbarActionsBridge* bridge = ToolbarActionsBridge::Get(profile);
  DCHECK(bridge);
  return bridge->GetJavaObject();
}
