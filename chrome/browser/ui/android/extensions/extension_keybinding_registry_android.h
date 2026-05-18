// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_ANDROID_H_

#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"
#include "ui/base/accelerators/accelerator.h"

class TabListInterface;

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
  explicit ExtensionKeybindingRegistryAndroid(
      content::BrowserContext* context,
      TabListInterface* tab_list_interface,
      ExtensionsToolbarViewModel* toolbar_view_model);

  ExtensionKeybindingRegistryAndroid(
      const ExtensionKeybindingRegistryAndroid&) = delete;
  ExtensionKeybindingRegistryAndroid& operator=(
      const ExtensionKeybindingRegistryAndroid&) = delete;

  ~ExtensionKeybindingRegistryAndroid() override;

  // Handles the key down event, and returns whether the event was handled.
  bool HandleKeyDownEvent(const ui::KeyEventAndroid& key_event);

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

  const raw_ptr<ExtensionsToolbarViewModel> toolbar_view_model_;
  std::set<ui::Accelerator> active_accelerators_;
  std::map<ui::Accelerator, ExtensionId> active_action_accelerators_;
  bool is_shortcut_handling_suspended_ = false;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_ANDROID_H_
