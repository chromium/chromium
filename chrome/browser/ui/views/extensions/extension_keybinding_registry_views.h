// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_VIEWS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "ui/base/accelerators/accelerator.h"

class Profile;

namespace extensions {
class Extension;
}

namespace views {
class FocusManager;
}

// ExtensionKeybindingRegistryViews is a class that handles Views-specific
// implementation of the Extension Keybinding shortcuts (keyboard accelerators).
// Note: It handles regular extension commands (not browserAction and pageAction
// popups, which are handled elsewhere). This class registers the accelerators
// on behalf of the extensions and routes the commands to them via the
// BrowserEventRouter.
class ExtensionKeybindingRegistryViews
    : public extensions::ExtensionKeybindingRegistry,
      public ui::AcceleratorTarget {
 public:
  ExtensionKeybindingRegistryViews(Profile* profile,
                                   views::FocusManager* focus_manager,
                                   ExtensionFilter extension_filter,
                                   Delegate* delegate);

  ExtensionKeybindingRegistryViews(const ExtensionKeybindingRegistryViews&) =
      delete;
  ExtensionKeybindingRegistryViews& operator=(
      const ExtensionKeybindingRegistryViews&) = delete;

  ~ExtensionKeybindingRegistryViews() override;

  // Overridden from ui::AcceleratorTarget.
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

 private:
  // Overridden from ExtensionKeybindingRegistry:
  void AddExtensionKeybindings(const extensions::Extension* extension,
                               const std::string& command_name) override;
  void RemoveExtensionKeybindingImpl(const ui::Accelerator& accelerator,
                                     const std::string& command_name) override;
  void OnShortcutHandlingSuspended(bool suspended) override;

  // Weak pointer to the our profile. Not owned by us.
  raw_ptr<Profile> profile_;

  // Weak pointer back to the focus manager to use to register and unregister
  // accelerators with. Not owned by us.
  raw_ptr<views::FocusManager> focus_manager_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_VIEWS_H_
