// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_keybinding_registry_android.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/ui/extensions/accelerator_priority.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/command.h"
#include "extensions/common/extension.h"
#include "ui/android/accelerator_manager_android.h"
#include "ui/android/ui_android_features.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/event.h"
#include "ui/events/platform_event.h"

namespace extensions {

ExtensionKeybindingRegistryAndroid::ExtensionKeybindingRegistryAndroid(
    content::BrowserContext* context,
    ui::WindowAndroid* window,
    TabListInterface* tab_list_interface,
    ExtensionsToolbarViewModel* toolbar_view_model)
    : ExtensionKeybindingRegistry(context,
                                  tab_list_interface,
                                  ExtensionFilter::ALL_EXTENSIONS),
      window_(base::FeatureList::IsEnabled(ui::kAndroidUnifiedKeyboardShortcuts)
                  ? window
                  : nullptr),
      toolbar_view_model_(toolbar_view_model) {
  CHECK(window);
  Init();
}

ExtensionKeybindingRegistryAndroid::~ExtensionKeybindingRegistryAndroid() {
  if (ui::AcceleratorManagerAndroid* accelerator_manager =
          ui::AcceleratorManagerAndroid::FromWindow(window_)) {
    for (const ui::Accelerator& accelerator : active_accelerators_) {
      accelerator_manager->UnregisterAccelerator(accelerator, this);
    }
  }
}

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
  if (ui::AcceleratorManagerAndroid* accelerator_manager =
          ui::AcceleratorManagerAndroid::FromWindow(window_)) {
    accelerator_manager->RegisterAccelerator(
        accelerator, kExtensionAcceleratorPriority, this);
  }
  return true;
}

void ExtensionKeybindingRegistryAndroid::UnregisterAccelerator(
    const ui::Accelerator& accelerator) {
  active_accelerators_.erase(accelerator);
  active_action_accelerators_.erase(accelerator);
  if (ui::AcceleratorManagerAndroid* accelerator_manager =
          ui::AcceleratorManagerAndroid::FromWindow(window_)) {
    accelerator_manager->UnregisterAccelerator(accelerator, this);
  }
}

bool ExtensionKeybindingRegistryAndroid::ShouldIgnoreCommand(
    const std::string& command) const {
  // This class supports action related commands, so ignore nothing.
  return false;
}

bool ExtensionKeybindingRegistryAndroid::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (!active_accelerators_.contains(accelerator)) {
    return false;
  }
  auto it = active_action_accelerators_.find(accelerator);
  if (it != active_action_accelerators_.end()) {
    ToolbarActionViewModel* model =
        toolbar_view_model_->GetActionModelForId(it->second);
    if (model == nullptr || !model->CanHandleAccelerators()) {
      return false;
    }

    return model->TryHandleAcceleratorPress();
  }

  return NotifyEventTargets(accelerator);
}

bool ExtensionKeybindingRegistryAndroid::CanHandleAccelerators() const {
  return !shortcut_handling_suspended();
}

bool ExtensionKeybindingRegistryAndroid::HandleKeyDownEvent(
    const ui::KeyEventAndroid& key_event) {
  if (!CanHandleAccelerators()) {
    return false;
  }

  ui::PlatformEvent native_event(key_event);
  ui::Accelerator accelerator((ui::KeyEvent(native_event)));
  return AcceleratorPressed(accelerator);
}

}  // namespace extensions
