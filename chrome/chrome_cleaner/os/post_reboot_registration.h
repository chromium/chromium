// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_POST_REBOOT_REGISTRATION_H_
#define CHROME_CHROME_CLEANER_OS_POST_REBOOT_REGISTRATION_H_

#include <set>
#include <string>

#include "base/command_line.h"

namespace chrome_cleaner {

// Registers the application to execute again after a reboot.
class PostRebootRegistration {
 public:
  explicit PostRebootRegistration(const std::wstring& product_shortname);

  // Register the current running app to be called post reboot with the provided
  // command line switches. Returns false if failed.
  bool RegisterRunOnceOnRestart(const std::string& cleanup_id,
                                const base::CommandLine& switches);

  // Unregisters the current running app from RunOnce, to prevent running it
  // again.
  void UnregisterRunOnceOnRestart();

  // Returns the value stored by |RegisterRunOnceOnRestart|, or an empty string
  // if there is none.
  std::wstring RunOnceOnRestartRegisteredValue();

  // Fills |out_command_line| with the switches of the post-reboot run by
  // reading from a cleanup-id dependent registry key. The registry key is then
  // deleted. Returns false if reading or deleting the registry key failed.
  bool ReadRunOncePostRebootCommandLine(const std::string& cleanup_id,
                                        base::CommandLine* out_command_line);

 private:
  // Test functions should have access to GetPostRebootSwitchKeyPath.
  friend bool RunOnceOverrideCommandLineContains(const std::string& cleanup_id,
                                                 const wchar_t* sub_string);

  // Returns the registry key path in which the full post-reboot command line
  // switches can be found.
  static std::wstring GetPostRebootSwitchKeyPath();

  std::wstring product_shortname_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_POST_REBOOT_REGISTRATION_H_
