// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"

#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/extension_commands_global_registry.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/accelerator_priority.h"
#include "extensions/common/extension.h"
#include "ui/base/accelerators/command.h"
#include "ui/views/focus/focus_manager.h"

ExtensionKeybindingRegistryViews::ExtensionKeybindingRegistryViews(
    Profile* profile,
    views::FocusManager* focus_manager,
    ExtensionFilter extension_filter,
    Delegate* delegate)
    : ExtensionKeybindingRegistry(profile, extension_filter, delegate),
      profile_(profile),
      focus_manager_(focus_manager) {
  Init();
}

ExtensionKeybindingRegistryViews::~ExtensionKeybindingRegistryViews() {
  if (extensions::ExtensionCommandsGlobalRegistry* const global_registry =
          extensions::ExtensionCommandsGlobalRegistry::Get(profile_);
      global_registry->registry_for_active_window() == this) {
    global_registry->set_registry_for_active_window(nullptr);
  }

  focus_manager_->UnregisterAccelerators(this);
}

bool ExtensionKeybindingRegistryViews::PopulateCommands(
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

bool ExtensionKeybindingRegistryViews::RegisterAccelerator(
    const ui::Accelerator& accelerator,
    const extensions::ExtensionId& extension_id,
    const std::string& command_name) {
  focus_manager_->RegisterAccelerator(accelerator,
                                      kExtensionAcceleratorPriority, this);
  return true;
}

void ExtensionKeybindingRegistryViews::UnregisterAccelerator(
    const ui::Accelerator& accelerator) {
  focus_manager_->UnregisterAccelerator(accelerator, this);
}

void ExtensionKeybindingRegistryViews::OnShortcutHandlingSuspended(
    bool suspended) {
  focus_manager_->set_shortcut_handling_suspended(suspended);
}

bool ExtensionKeybindingRegistryViews::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  return ExtensionKeybindingRegistry::NotifyEventTargets(accelerator);
}

bool ExtensionKeybindingRegistryViews::CanHandleAccelerators() const {
  return true;
}

void ExtensionKeybindingRegistryViews::OnHostActivationChanged(bool active) {
  if (extensions::ExtensionCommandsGlobalRegistry* const global_registry =
          extensions::ExtensionCommandsGlobalRegistry::Get(profile_)) {
    if (active) {
      global_registry->set_registry_for_active_window(this);
    } else if (global_registry->registry_for_active_window() == this) {
      global_registry->set_registry_for_active_window(nullptr);
    }
  }
}
