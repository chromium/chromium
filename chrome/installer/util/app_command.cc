// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/util/app_command.h"

#include <windows.h>

#include <stddef.h>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/work_item_list.h"

namespace installer {

namespace {

// Returns the "`GetClientsKeyPath()`\\Commands\\`name`" registry key path used
// to register AppCommands with the updater.
std::wstring GetCommandKey(const std::wstring& name) {
  return base::StrCat({install_static::GetClientsKeyPath(), L"\\",
                       google_update::kRegCommandsKey, L"\\", name});
}

}  // namespace

// static
// Associate bool member variables with registry entries.
const AppCommand::NamedBoolVar AppCommand::kNameBoolVars[] = {
    {&AppCommand::sends_pings_, google_update::kRegSendsPingsField},
    {&AppCommand::is_web_accessible_, google_update::kRegWebAccessibleField},
    {&AppCommand::is_auto_run_on_os_upgrade_,
     google_update::kRegAutoRunOnOSUpgradeField},
    {&AppCommand::is_run_as_user_, google_update::kRegRunAsUserField},
};

AppCommand::AppCommand()
    : sends_pings_(false),
      is_web_accessible_(false),
      is_auto_run_on_os_upgrade_(false),
      is_run_as_user_(false) {}

AppCommand::AppCommand(const std::wstring& command_name,
                       const std::wstring& command_line)
    : command_name_(command_name),
      command_line_(command_line),
      sends_pings_(false),
      is_web_accessible_(false),
      is_auto_run_on_os_upgrade_(false),
      is_run_as_user_(false) {}

AppCommand::AppCommand(AppCommand&&) = default;
AppCommand::AppCommand(const AppCommand&) = default;
AppCommand::~AppCommand() = default;

bool AppCommand::Initialize(HKEY root_key) {
  DCHECK(!command_name_.empty());

  base::win::RegKey key;
  auto result = key.Open(root_key, GetCommandKey(command_name_).c_str(),
                         KEY_QUERY_VALUE | KEY_WOW64_32KEY);
  if (result != ERROR_SUCCESS) {
    ::SetLastError(result);
    PLOG(DFATAL) << "Error opening AppCommand: " << command_name_;
    return false;
  }

  return Initialize(key);
}

bool AppCommand::Initialize(const base::win::RegKey& key) {
  if (!key.Valid()) {
    LOG(DFATAL) << "Cannot initialize an AppCommand from an invalid key.";
    return false;
  }

  LONG result = ERROR_SUCCESS;
  std::wstring cmd_line;

  result = key.ReadValue(google_update::kRegCommandLineField, &cmd_line);
  if (result != ERROR_SUCCESS) {
    LOG(WARNING) << "Error reading " << google_update::kRegCommandLineField
                 << " value from registry: " << result;
    return false;
  }

  command_line_.swap(cmd_line);

  for (size_t i = 0; i < std::size(kNameBoolVars); ++i) {
    DWORD value = 0;  // Set default to false.
    // Note: ReadValueDW only modifies out param on success.
    key.ReadValueDW(kNameBoolVars[i].name, &value);
    this->*(kNameBoolVars[i].data) = (value != 0);
  }

  return true;
}

void AppCommand::AddCreateAppCommandWorkItems(const HKEY root_key,
                                              WorkItemList* item_list) const {
  DCHECK(!command_name_.empty());
  DCHECK(!command_line_.empty());

  const std::wstring command_path = GetCommandKey(command_name_);
  item_list->AddCreateRegKeyWorkItem(root_key, command_path, KEY_WOW64_32KEY)
      ->set_log_message("creating AppCommand registry key");
  item_list
      ->AddSetRegValueWorkItem(root_key, command_path, KEY_WOW64_32KEY,
                               google_update::kRegCommandLineField,
                               command_line_, true)
      ->set_log_message("setting AppCommand CommandLine registry value");

  for (size_t i = 0; i < std::size(kNameBoolVars); ++i) {
    const wchar_t* var_name = kNameBoolVars[i].name;
    bool var_data = this->*(kNameBoolVars[i].data);

    // Adds a work item to set |var_name| to DWORD 1 if |var_data| is true;
    // adds a work item to remove |var_name| otherwise.
    if (var_data) {
      item_list->AddSetRegValueWorkItem(root_key, command_path, KEY_WOW64_32KEY,
                                        var_name, static_cast<DWORD>(1), true);
    } else {
      item_list->AddDeleteRegValueWorkItem(root_key, command_path,
                                           KEY_WOW64_32KEY, var_name);
    }
  }
}

void AppCommand::AddDeleteAppCommandWorkItems(const HKEY root_key,
                                              WorkItemList* item_list) const {
  DCHECK(!command_name_.empty());

  item_list->AddDeleteRegKeyWorkItem(root_key, GetCommandKey(command_name_),
                                     KEY_WOW64_32KEY);
}

}  // namespace installer
