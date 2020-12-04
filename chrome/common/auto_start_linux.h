// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_AUTO_START_LINUX_H_
#define CHROME_COMMON_AUTO_START_LINUX_H_

#include <string>

#include "base/macros.h"

namespace base {
class FilePath;
class Environment;
}  // namespace base

class AutoStart {
 public:
  // Registers an application to autostart on user login. |is_terminal_app|
  // specifies whether the app will run in a terminal window.
  static bool AddApplication(const std::string& autostart_filename,
                             const std::string& application_name,
                             const std::string& command_line,
                             bool is_terminal_app);
  // Removes an autostart file.
  static bool Remove(const std::string& autostart_filename);
  // Gets the entire contents of an autostart file.
  static bool GetAutostartFileContents(const std::string& autostart_filename,
                                       std::string* contents);
  // Gets a specific value from an autostart file.
  static bool GetAutostartFileValue(const std::string& autostart_filename,
                                    const std::string& value_name,
                                    std::string* value);
  // Gets the path to the autostart directory.
  static base::FilePath GetAutostartDirectory(base::Environment* environment);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AutoStart);
};

#endif  // CHROME_COMMON_AUTO_START_LINUX_H_
