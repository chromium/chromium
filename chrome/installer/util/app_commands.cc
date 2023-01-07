// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/app_commands.h"

#include "base/logging.h"
#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/work_item_list.h"

using base::win::RegKey;

namespace installer {

AppCommands::AppCommands() {}

AppCommands::~AppCommands() {}

bool AppCommands::Initialize(const base::win::RegKey& key, REGSAM wow64access) {
  if (!key.Valid()) {
    LOG(DFATAL) << "Cannot initialize AppCommands from an invalid key.";
    return false;
  }

  if (wow64access != 0 && wow64access != KEY_WOW64_32KEY &&
      wow64access != KEY_WOW64_64KEY) {
    LOG(DFATAL) << "Invalid wow64access supplied to AppCommands.";
    return false;
  }

  using base::win::RegistryKeyIterator;
  static const wchar_t kEmptyString[] = L"";

  commands_.clear();

  RegKey cmd_key;
  LONG result;
  AppCommand command;
  for (RegistryKeyIterator key_iterator(key.Handle(), kEmptyString,
                                        wow64access);
       key_iterator.Valid(); ++key_iterator) {
    const wchar_t* name = key_iterator.Name();
    result = cmd_key.Open(key.Handle(), name, KEY_QUERY_VALUE);
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to open key \"" << name
                 << "\" with last-error code " << result;
    } else if (command.Initialize(cmd_key)) {
      commands_[name] = command;
    } else {
      VLOG(1) << "Skipping over key \"" << name
              << "\" as it does not appear to hold a product command.";
    }
  }

  return true;
}

AppCommands& AppCommands::CopyFrom(const AppCommands& other) {
  commands_ = other.commands_;

  return *this;
}

void AppCommands::Clear() {
  commands_.clear();
}

bool AppCommands::Get(const std::wstring& command_id,
                      AppCommand* command) const {
  DCHECK(command);
  CommandMap::const_iterator it(commands_.find(command_id));
  if (it == commands_.end())
    return false;
  *command = it->second;
  return true;
}

bool AppCommands::Set(const std::wstring& command_id,
                      const AppCommand& command) {
  std::pair<CommandMap::iterator, bool> result(
      commands_.insert(std::make_pair(command_id, command)));
  if (!result.second)
    result.first->second = command;
  return result.second;
}

bool AppCommands::Remove(const std::wstring& command_id) {
  return commands_.erase(command_id) != 0;
}

AppCommands::CommandMapRange AppCommands::GetIterators() const {
  return std::make_pair(commands_.begin(), commands_.end());
}

}  // namespace installer
