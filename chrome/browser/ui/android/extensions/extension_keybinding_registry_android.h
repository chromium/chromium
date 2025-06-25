// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_ANDROID_H_

#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/android/key_event_android.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// This class handles keyboard accelerators for extensions on Android.
class ExtensionKeybindingRegistryAndroid
    : public extensions::ExtensionKeybindingRegistry {
 public:
  explicit ExtensionKeybindingRegistryAndroid(content::BrowserContext* context);

  ExtensionKeybindingRegistryAndroid(
      const ExtensionKeybindingRegistryAndroid&) = delete;
  ExtensionKeybindingRegistryAndroid& operator=(
      const ExtensionKeybindingRegistryAndroid&) = delete;

  ~ExtensionKeybindingRegistryAndroid() override;

  // Handles the key down event. It returns whether the key event was handled.
  // It immediately returns false if the given key event should not intercept.
  bool HandleKeyDownEvent(const ui::KeyEventAndroid& key_event);

 private:
  // Overridden from ExtensionKeybindingRegistry:
  bool PopulateCommands(const extensions::Extension* extension,
                        ui::CommandMap* commands) override;
  bool RegisterAccelerator(const ui::Accelerator& accelerator) override;
  void UnregisterAccelerator(const ui::Accelerator& accelerator) override;
  void OnShortcutHandlingSuspended(bool suspended) override;

  std::set<ui::Accelerator> active_accelerators_;
  bool is_shortcut_handling_suspended_ = false;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_ANDROID_H_
