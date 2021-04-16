// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALLER_CONFIGURATION_H_
#define CHROME_UPDATER_WIN_INSTALLER_CONFIGURATION_H_

#include <windows.h>

namespace updater {

// A simple container of the updater's configuration, as defined by the
// command line used to invoke it.
class Configuration {
 public:
  enum Operation {
    INSTALL_PRODUCT,
    CLEANUP,
  };

  Configuration();
  ~Configuration();

  // Initializes this instance on the basis of the process's command line.
  bool Initialize(HMODULE module);

  // Returns the desired operation dictated by the command line options.
  Operation operation() const { return operation_; }

  // Returns true if --system-level is on the command line or if
  // GoogleUpdateIsMachine=1 is set in the process's environment.
  bool is_system_level() const { return is_system_level_; }

  // Returns true if any invalid switch is found on the command line.
  bool has_invalid_switch() const { return has_invalid_switch_; }

 protected:
  void Clear();
  bool ParseCommandLine(const wchar_t* command_line);

  wchar_t** args_ = nullptr;
  const wchar_t* command_line_ = nullptr;
  int argument_count_ = 0;
  Operation operation_ = INSTALL_PRODUCT;
  bool is_system_level_ = false;
  bool has_invalid_switch_ = false;

 private:
  Configuration(const Configuration&) = delete;
  Configuration& operator=(const Configuration&) = delete;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_INSTALLER_CONFIGURATION_H_
