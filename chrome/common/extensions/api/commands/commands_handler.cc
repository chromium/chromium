// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/commands/commands_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/command.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"

namespace extensions {

namespace keys = manifest_keys;

namespace {
// The maximum number of commands (including page action/browser actions) with a
// keybinding an extension can have.
const int kMaxCommandsWithKeybindingPerExtension = 4;
}  // namespace

CommandsInfo::CommandsInfo() {
}

CommandsInfo::~CommandsInfo() {
}

// static
const Command* CommandsInfo::GetBrowserActionCommand(
    const Extension* extension) {
  CommandsInfo* info = static_cast<CommandsInfo*>(
      extension->GetManifestData(keys::kCommands));
  return info ? info->browser_action_command.get() : NULL;
}

// static
const Command* CommandsInfo::GetPageActionCommand(const Extension* extension) {
  CommandsInfo* info = static_cast<CommandsInfo*>(
      extension->GetManifestData(keys::kCommands));
  return info ? info->page_action_command.get() : NULL;
}

// static
const Command* CommandsInfo::GetActionCommand(const Extension* extension) {
  CommandsInfo* info =
      static_cast<CommandsInfo*>(extension->GetManifestData(keys::kCommands));
  return info ? info->action_command.get() : nullptr;
}

// static
const CommandMap* CommandsInfo::GetNamedCommands(const Extension* extension) {
  CommandsInfo* info = static_cast<CommandsInfo*>(
      extension->GetManifestData(keys::kCommands));
  return info ? &info->named_commands : NULL;
}

CommandsHandler::CommandsHandler() {
}

CommandsHandler::~CommandsHandler() {
}

bool CommandsHandler::Parse(Extension* extension, std::u16string* error) {
  if (!extension->manifest()->HasKey(keys::kCommands)) {
    std::unique_ptr<CommandsInfo> commands_info(new CommandsInfo);
    MaybeSetBrowserActionDefault(extension, commands_info.get());
    extension->SetManifestData(keys::kCommands, std::move(commands_info));
    return true;
  }

  const base::DictionaryValue* dict = NULL;
  if (!extension->manifest()->GetDictionary(keys::kCommands, &dict)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidCommandsKey);
    return false;
  }

  std::unique_ptr<CommandsInfo> commands_info(new CommandsInfo);

  int command_index = 0;
  int keybindings_found = 0;
  for (base::DictionaryValue::Iterator iter(*dict); !iter.IsAtEnd();
       iter.Advance()) {
    ++command_index;

    const base::DictionaryValue* command = NULL;
    if (!iter.value().GetAsDictionary(&command)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          manifest_errors::kInvalidKeyBindingDictionary,
          base::NumberToString(command_index));
      return false;
    }

    std::unique_ptr<extensions::Command> binding(new Command());
    if (!binding->Parse(command, iter.key(), command_index, error))
      return false;  // |error| already set.

    if (binding->accelerator().key_code() != ui::VKEY_UNKNOWN) {
      // Only media keys are allowed to work without modifiers, and because
      // media keys aren't registered exclusively they should not count towards
      // the max of four shortcuts per extension.
      if (!Command::IsMediaKey(binding->accelerator()))
        ++keybindings_found;

      if (keybindings_found > kMaxCommandsWithKeybindingPerExtension &&
          !PermissionsParser::HasAPIPermission(
              extension, mojom::APIPermissionID::kCommandsAccessibility)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            manifest_errors::kInvalidKeyBindingTooMany,
            base::NumberToString(kMaxCommandsWithKeybindingPerExtension));
        return false;
      }
    }

    std::string command_name = binding->command_name();
    if (command_name == manifest_values::kBrowserActionCommandEvent) {
      commands_info->browser_action_command = std::move(binding);
    } else if (command_name ==
                   manifest_values::kPageActionCommandEvent) {
      commands_info->page_action_command = std::move(binding);
    } else if (command_name == manifest_values::kActionCommandEvent) {
      commands_info->action_command = std::move(binding);
    } else {
      if (command_name[0] != '_')  // All commands w/underscore are reserved.
        commands_info->named_commands[command_name] = *binding;
    }
  }

  MaybeSetBrowserActionDefault(extension, commands_info.get());

  extension->SetManifestData(keys::kCommands, std::move(commands_info));
  return true;
}

bool CommandsHandler::AlwaysParseForType(Manifest::Type type) const {
  return type == Manifest::TYPE_EXTENSION ||
      type == Manifest::TYPE_LEGACY_PACKAGED_APP ||
      type == Manifest::TYPE_PLATFORM_APP;
}

void CommandsHandler::MaybeSetBrowserActionDefault(const Extension* extension,
                                                   CommandsInfo* info) {
  // TODO(devlin): Synthesize a command for the "action" key, too?
  if (extension->manifest()->HasKey(keys::kBrowserAction) &&
      !info->browser_action_command.get()) {
    info->browser_action_command =
        std::make_unique<Command>(manifest_values::kBrowserActionCommandEvent,
                                  std::u16string(), std::string(), false);
  }
}

base::span<const char* const> CommandsHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kCommands};
  return kKeys;
}

}  // namespace extensions
