// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/app_command.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/work_item_list.h"

namespace installer {

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

AppCommand::AppCommand(const std::wstring& command_line)
    : command_line_(command_line),
      sends_pings_(false),
      is_web_accessible_(false),
      is_auto_run_on_os_upgrade_(false),
      is_run_as_user_(false) {}

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

  for (size_t i = 0; i < base::size(kNameBoolVars); ++i) {
    DWORD value = 0;  // Set default to false.
    // Note: ReadValueDW only modifies out param on success.
    key.ReadValueDW(kNameBoolVars[i].name, &value);
    this->*(kNameBoolVars[i].data) = (value != 0);
  }

  return true;
}

void AppCommand::AddWorkItems(HKEY predefined_root,
                              const std::wstring& command_path,
                              WorkItemList* item_list) const {
  // Command_path is derived from GetRegCommandKey which always returns
  // value from GetClientsKeyPath() which should be 32-bit hive.
  item_list
      ->AddCreateRegKeyWorkItem(predefined_root, command_path, KEY_WOW64_32KEY)
      ->set_log_message("creating AppCommand registry key");
  item_list
      ->AddSetRegValueWorkItem(predefined_root, command_path, KEY_WOW64_32KEY,
                               google_update::kRegCommandLineField,
                               command_line_, true)
      ->set_log_message("setting AppCommand CommandLine registry value");

  for (size_t i = 0; i < base::size(kNameBoolVars); ++i) {
    const wchar_t* var_name = kNameBoolVars[i].name;
    bool var_data = this->*(kNameBoolVars[i].data);

    // Adds a work item to set |var_name| to DWORD 1 if |var_data| is true;
    // adds a work item to remove |var_name| otherwise.
    if (var_data) {
      item_list->AddSetRegValueWorkItem(predefined_root, command_path,
                                        KEY_WOW64_32KEY, var_name,
                                        static_cast<DWORD>(1), true);
    } else {
      item_list->AddDeleteRegValueWorkItem(predefined_root, command_path,
                                           KEY_WOW64_32KEY, var_name);
    }
  }
}

}  // namespace installer
