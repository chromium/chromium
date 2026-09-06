// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_VIEWS_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class Profile;
class TabListInterface;

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
  DECLARE_USER_DATA(ExtensionKeybindingRegistryViews);

  ExtensionKeybindingRegistryViews(Profile* profile,
                                   TabListInterface* tab_list_interface,
                                   ExtensionFilter extension_filter,
                                   views::FocusManager* focus_manager);
  // Constructs a window-scoped registry, reachable through From(`browser`).
  ExtensionKeybindingRegistryViews(BrowserWindowInterface* browser,
                                   ExtensionFilter extension_filter,
                                   views::FocusManager* focus_manager);

  ExtensionKeybindingRegistryViews(const ExtensionKeybindingRegistryViews&) =
      delete;
  ExtensionKeybindingRegistryViews& operator=(
      const ExtensionKeybindingRegistryViews&) = delete;

  ~ExtensionKeybindingRegistryViews() override;

  // Returns the window-scoped registry for `browser`, or null if there is
  // none - BrowserWindowFeatures only creates one when the window has a focus
  // manager.
  static ExtensionKeybindingRegistryViews* From(
      BrowserWindowInterface* browser);

  // Overridden from ui::AcceleratorTarget.
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  void OnHostActivationChanged(bool active);

 private:
  // Overridden from ExtensionKeybindingRegistry:
  bool PopulateCommands(const extensions::Extension* extension,
                        ui::CommandMap* commands) override;
  bool RegisterAccelerator(const ui::Accelerator& accelerator,
                           const extensions::ExtensionId& extension_id,
                           const std::string& command_name) override;
  void UnregisterAccelerator(const ui::Accelerator& accelerator) override;
  void OnShortcutHandlingSuspended(bool suspended) override;

  // Weak pointer to the our profile. Not owned by us.
  raw_ptr<Profile> profile_;

  // Weak pointer back to the focus manager to use to register and unregister
  // accelerators with. Not owned by us.
  raw_ptr<views::FocusManager> focus_manager_;

  // Only set for the window-scoped instance created by
  // BrowserWindowFeatures.
  std::optional<ui::ScopedUnownedUserData<ExtensionKeybindingRegistryViews>>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_VIEWS_H_
