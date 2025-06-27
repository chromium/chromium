// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_keybinding_registry_android.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/platform_event.h"

namespace extensions {

ExtensionKeybindingRegistryAndroid::ExtensionKeybindingRegistryAndroid(
    content::BrowserContext* context)
    : ExtensionKeybindingRegistry(context,
                                  ExtensionFilter::ALL_EXTENSIONS,
                                  nullptr) {}

ExtensionKeybindingRegistryAndroid::~ExtensionKeybindingRegistryAndroid() =
    default;

bool ExtensionKeybindingRegistryAndroid::PopulateCommands(
    const Extension* extension,
    ui::CommandMap* commands) {
  CommandService* command_service = CommandService::Get(browser_context());
  bool populated_named_commands =
      command_service->GetNamedCommands(extension->id(), CommandService::ACTIVE,
                                        CommandService::REGULAR, commands);

  Command cmd;
  bool populated_action_command = command_service->GetExtensionActionCommand(
      extension->id(), ActionInfo::Type::kAction,
      CommandService::QueryType::ACTIVE, &cmd, /*active=*/nullptr);
  if (populated_action_command) {
    (*commands)[cmd.command_name()] = cmd;
  }

  return populated_named_commands || populated_action_command;
}

bool ExtensionKeybindingRegistryAndroid::RegisterAccelerator(
    const ui::Accelerator& accelerator,
    const ExtensionId& extension_id,
    const std::string& command_name) {
  active_accelerators_.insert(accelerator);
  if (Command::IsActionRelatedCommand(command_name)) {
    active_action_accelerators_[accelerator] = extension_id;
  }
  return true;
}

void ExtensionKeybindingRegistryAndroid::UnregisterAccelerator(
    const ui::Accelerator& accelerator) {
  active_accelerators_.erase(accelerator);
  active_action_accelerators_.erase(accelerator);
}

void ExtensionKeybindingRegistryAndroid::OnShortcutHandlingSuspended(
    bool suspended) {
  is_shortcut_handling_suspended_ = suspended;
}

bool ExtensionKeybindingRegistryAndroid::ShouldIgnoreCommand(
    const std::string& command) const {
  // This class supports action related commands, so ignore nothing.
  return false;
}

std::variant<bool, std::string>
ExtensionKeybindingRegistryAndroid::HandleKeyDownEvent(
    const ui::KeyEventAndroid& key_event) {
  if (is_shortcut_handling_suspended_) {
    return false;
  }

  ui::PlatformEvent native_event(key_event);
  ui::Accelerator accelerator((ui::KeyEvent(native_event)));

  if (!active_accelerators_.contains(accelerator)) {
    return false;
  }
  auto it = active_action_accelerators_.find(accelerator);
  if (it != active_action_accelerators_.end()) {
    return it->second;
  }

  return NotifyEventTargets(accelerator);
}

}  // namespace extensions
