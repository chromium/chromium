// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_ANDROID_H_

#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "ui/base/accelerators/accelerator.h"

namespace content {
class BrowserContext;
}

namespace ui {
class KeyEventAndroid;
}

namespace extensions {

// This class handles keyboard accelerators for extensions on Android.
// Unlike other subclasses, it manages all the command types including action
// related commands.
class ExtensionKeybindingRegistryAndroid : public ExtensionKeybindingRegistry {
 public:
  explicit ExtensionKeybindingRegistryAndroid(content::BrowserContext* context);

  ExtensionKeybindingRegistryAndroid(
      const ExtensionKeybindingRegistryAndroid&) = delete;
  ExtensionKeybindingRegistryAndroid& operator=(
      const ExtensionKeybindingRegistryAndroid&) = delete;

  ~ExtensionKeybindingRegistryAndroid() override;

  // Handles the key down event. If the corresponding command is a regular
  // command (not extension action), it handles the command and returns true. If
  // the command is an extension action command, it returns the extension id. If
  // no command matches, it returns false.
  std::variant<bool, std::string> HandleKeyDownEvent(
      const ui::KeyEventAndroid& key_event);

 private:
  // Overridden from ExtensionKeybindingRegistry:
  bool PopulateCommands(const Extension* extension,
                        ui::CommandMap* commands) override;
  bool RegisterAccelerator(const ui::Accelerator& accelerator,
                           const ExtensionId& extension_id,
                           const std::string& command_name) override;
  void UnregisterAccelerator(const ui::Accelerator& accelerator) override;
  void OnShortcutHandlingSuspended(bool suspended) override;
  bool ShouldIgnoreCommand(const std::string& command) const override;

  std::set<ui::Accelerator> active_accelerators_;
  std::map<ui::Accelerator, ExtensionId> active_action_accelerators_;
  bool is_shortcut_handling_suspended_ = false;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_ANDROID_H_
