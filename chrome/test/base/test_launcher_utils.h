// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_LAUNCHER_UTILS_H_
#define CHROME_TEST_BASE_TEST_LAUNCHER_UTILS_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"

namespace base {
class CommandLine;
class ScopedTempDir;
}

// A set of utilities for test code that launches separate processes.
namespace test_launcher_utils {

// Appends browser switches to provided |command_line| to be used
// when running under tests.
void PrepareBrowserCommandLineForTests(base::CommandLine* command_line);

// Appends browser switches to provided |command_line| to be used
// when running under browser tests. These are in addition to the flags from
// PrepareBrowserCommandLineForTests().
void PrepareBrowserCommandLineForBrowserTests(base::CommandLine* command_line,
                                              bool open_about_blank_on_launch);

// Appends all switches from |in_command_line| to |out_command_line| except for
// |switch_to_remove|.
// TODO(xhwang): Add CommandLine::RemoveSwitch() so we don't need this hack.
void RemoveCommandLineSwitch(const base::CommandLine& in_command_line,
                             const std::string& switch_to_remove,
                             base::CommandLine* out_command_line);

// Creates and overrides the current process' user data dir.
bool CreateUserDataDir(base::ScopedTempDir* temp_dir) WARN_UNUSED_RESULT;

// Overrides the current process' user data dir.
bool OverrideUserDataDir(const base::FilePath& user_data_dir)
    WARN_UNUSED_RESULT;

}  // namespace test_launcher_utils

#endif  // CHROME_TEST_BASE_TEST_LAUNCHER_UTILS_H_
