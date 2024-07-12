// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_LAUNCHER_UTILS_H_
#define CHROME_TEST_BASE_TEST_LAUNCHER_UTILS_H_

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

// Creates and overrides the current process' user data dir.
[[nodiscard]] bool CreateUserDataDir(base::ScopedTempDir* temp_dir);

// Overrides the current process' user data dir.
[[nodiscard]] bool OverrideUserDataDir(const base::FilePath& user_data_dir);

}  // namespace test_launcher_utils

#endif  // CHROME_TEST_BASE_TEST_LAUNCHER_UTILS_H_
