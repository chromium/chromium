// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_APP_COMMAND_H_
#define CHROME_INSTALLER_UTIL_APP_COMMAND_H_

#include <windows.h>

#include <string>

class WorkItemList;

namespace base {
namespace win {
class RegKey;
}
}  // namespace base

namespace installer {

// A description of a command registered by setup.exe that can be invoked by
// Google Update.  This class is CopyConstructible and Assignable for use in
// STL containers.
class AppCommand {
 public:
  AppCommand();

  // Constructs a new command with the given `command_name` and `command_line`.
  // All other properties default to false.
  AppCommand(const std::wstring& command_name,
             const std::wstring& command_line);

  // The default copy ctors, dtor, and assignment operators are desired.
  AppCommand(AppCommand&&);
  AppCommand(const AppCommand&);
  ~AppCommand();
  AppCommand& operator=(AppCommand&&) = default;
  AppCommand& operator=(const AppCommand&) = default;

  // Initializes an instance from the command in
  // `root_key`\Google\Update\Clients\{`app_id`}\Commands\`command_name_`
  bool Initialize(HKEY root_key);

  // Initializes an instance from the command in |key|.
  bool Initialize(const base::win::RegKey& key);

  // Adds to `item_list` work items to write the command under `root_key`.
  void AddCreateAppCommandWorkItems(const HKEY root_key,
                                    WorkItemList* item_list) const;

  // Adds to `item_list` work items to delete the command under `root_key`.
  void AddDeleteAppCommandWorkItems(const HKEY root_key,
                                    WorkItemList* item_list) const;

  // Returns the command-line for the app command as it is represented in the
  // registry.  Use CommandLine::FromString() on this value to check arguments
  // or to launch the command.
  const std::wstring& command_line() const { return command_line_; }
  void set_command_line(const std::wstring& command_line) {
    command_line_ = command_line;
  }

  bool is_auto_run_on_os_upgrade() const { return is_auto_run_on_os_upgrade_; }
  void set_is_auto_run_on_os_upgrade(bool is_auto_run_on_os_upgrade) {
    is_auto_run_on_os_upgrade_ = is_auto_run_on_os_upgrade;
  }

 protected:
  std::wstring command_name_;
  std::wstring command_line_;
  bool is_auto_run_on_os_upgrade_ = false;

 private:
  struct NamedBoolVar {
    bool AppCommand::* data;
    const wchar_t* name;
  };

  static const NamedBoolVar kNameBoolVars[];
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_APP_COMMAND_H_
