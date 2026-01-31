// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MAC_INSTALL_TEST_UTIL_H_
#define CHROME_INSTALLER_MAC_INSTALL_TEST_UTIL_H_

#include <string>

#include "base/environment.h"

namespace base {
class CommandLine;
class FilePath;
class TimeDelta;
}  // namespace base

namespace installer::mac::test {

// Run an executable with the provided command line, environment, and working
// directory, asserting that it finishes within a specified timeout. stdout
// and stderr are combined and written into `output`.
void AssertExecutableCompletes(const base::CommandLine& cmd,
                               const base::EnvironmentMap& env,
                               const base::FilePath& working_dir,
                               const base::TimeDelta& timeout,
                               std::string* output,
                               int* exit_code);

}  // namespace installer::mac::test

#endif  // CHROME_INSTALLER_MAC_INSTALL_TEST_UTIL_H_
