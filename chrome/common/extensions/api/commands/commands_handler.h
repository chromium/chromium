// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_COMMANDS_COMMANDS_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_COMMANDS_COMMANDS_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/common/extensions/command.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct CommandsInfo : public Extension::ManifestData {
  CommandsInfo();
  ~CommandsInfo() override;

  // Optional list of commands (keyboard shortcuts).
  // These commands are the commands which the extension wants to use, which are
  // not necessarily the ones it can use, as it might be inactive (see also
  // Get*Command[s] in CommandService).
  std::unique_ptr<Command> browser_action_command;
  std::unique_ptr<Command> page_action_command;
  std::unique_ptr<Command> action_command;
  CommandMap named_commands;

  static const Command* GetBrowserActionCommand(const Extension* extension);
  static const Command* GetPageActionCommand(const Extension* extension);
  static const Command* GetActionCommand(const Extension* extension);
  static const CommandMap* GetNamedCommands(const Extension* extension);
};

// Parses the "commands" manifest key.
class CommandsHandler : public ManifestHandler {
 public:
  CommandsHandler();
  ~CommandsHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;
  bool AlwaysParseForType(Manifest::Type type) const override;

 private:
  // If the extension defines a browser action, but no command for it, then
  // we synthesize a generic one, so the user can configure a shortcut for it.
  // No keyboard shortcut will be assigned to it, until the user selects one.
  void MaybeSetBrowserActionDefault(const Extension* extension,
                                    CommandsInfo* info);

  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(CommandsHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_COMMANDS_COMMANDS_HANDLER_H_
