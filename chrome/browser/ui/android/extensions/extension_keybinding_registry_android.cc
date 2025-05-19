// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_keybinding_registry_android.h"

#include <jni.h>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionKeybindingRegistryAndroid_jni.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/platform_event.h"

ExtensionKeybindingRegistryAndroid::ExtensionKeybindingRegistryAndroid(
    content::BrowserContext* context,
    ExtensionFilter extension_filter,
    Delegate* delegate)
    : ExtensionKeybindingRegistry(context, extension_filter, delegate) {}

ExtensionKeybindingRegistryAndroid::~ExtensionKeybindingRegistryAndroid() =
    default;

bool ExtensionKeybindingRegistryAndroid::PopulateCommands(
    const extensions::Extension* extension,
    ui::CommandMap* commands) {
  extensions::CommandService* command_service =
      extensions::CommandService::Get(browser_context());
  if (!command_service->GetNamedCommands(
          extension->id(), extensions::CommandService::ACTIVE,
          extensions::CommandService::REGULAR, commands)) {
    return false;
  }
  return true;
}

bool ExtensionKeybindingRegistryAndroid::RegisterAccelerator(
    const ui::Accelerator& accelerator) {
  active_accelerators_.insert(accelerator);
  return true;
}

void ExtensionKeybindingRegistryAndroid::UnregisterAccelerator(
    const ui::Accelerator& accelerator) {
  active_accelerators_.erase(accelerator);
}

void ExtensionKeybindingRegistryAndroid::OnShortcutHandlingSuspended(
    bool suspended) {
  is_shortcut_handling_suspended_ = suspended;
}

// JNI functions

static jlong JNI_ExtensionKeybindingRegistryAndroid_Init(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& profile_obj) {
  Profile* profile = Profile::FromJavaObject(profile_obj);
  ExtensionKeybindingRegistryAndroid* instance =
      new ExtensionKeybindingRegistryAndroid(
          profile, extensions::ExtensionKeybindingRegistry::ALL_EXTENSIONS,
          nullptr);
  return reinterpret_cast<jlong>(instance);
}

void ExtensionKeybindingRegistryAndroid::Destroy(JNIEnv* env) {
  delete this;
}

jboolean ExtensionKeybindingRegistryAndroid::HandleKeyEvent(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& java_key_event) {
  if (is_shortcut_handling_suspended_) {
    return false;
  }

  ui::PlatformEvent native_event((ui::KeyEventAndroid(env, java_key_event)));
  ui::Accelerator accelerator((ui::KeyEvent(native_event)));

  if (!active_accelerators_.contains(accelerator)) {
    return false;
  }

  return NotifyEventTargets(accelerator);
}
