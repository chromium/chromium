// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_APP_COMMANDS_H_
#define CHROME_INSTALLER_UTIL_APP_COMMANDS_H_

#include <windows.h>

#include <map>
#include <string>
#include <utility>

#include "chrome/installer/util/app_command.h"

namespace base {
namespace win {
class RegKey;
}
}  // namespace base

namespace installer {

// A collection of AppCommand objects.
class AppCommands {
 public:
  typedef std::map<std::wstring, AppCommand> CommandMap;
  typedef std::pair<CommandMap::const_iterator, CommandMap::const_iterator>
      CommandMapRange;

  AppCommands();

  AppCommands(const AppCommands&) = delete;
  AppCommands& operator=(const AppCommands&) = delete;

  ~AppCommands();

  // Initialize an instance from the set of commands in a given registry key
  // (typically the "Commands" subkey of an app's Clients key). |key| must have
  // been opened with at least KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE access
  // rights. |wow64access| must be one of 0, KEY_WOW64_32KEY or KEY_WOW64_64KEY
  // and must match the original WOW64 access used to open |key| previously.
  bool Initialize(const base::win::RegKey& key, REGSAM wow64access);

  // Replaces the contents of this object with that of |other|.
  AppCommands& CopyFrom(const AppCommands& other);

  // Clears this instance.
  void Clear();

  // Retrieves the command identified by |command_id| from the set, copying it
  // into |command| and returning true if present.
  bool Get(const std::wstring& command_id, AppCommand* command) const;

  // Sets a command in the collection, adding it if it doesn't already exist.
  // Returns true if a new command is added; false if |command_id| was already
  // present and has been replaced with |command|.
  bool Set(const std::wstring& command_id, const AppCommand& command);

  // Removes a command from the collection.  Returns false if |command_id| was
  // not found.
  bool Remove(const std::wstring& command_id);

  // Returns a pair of STL iterators defining the range of objects in the
  // collection.
  CommandMapRange GetIterators() const;

 protected:
  CommandMap commands_;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_APP_COMMANDS_H_
