// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_ISOLATED_BROWSER_TEST_SUPPORT_H_
#define CHROME_BROWSER_WIN_ISOLATED_BROWSER_TEST_SUPPORT_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/process/process.h"
#include "base/test/multiprocess_test.h"

class ServiceEnvironment;

namespace chrome {

// Helper class for unit tests that need to test isolated browser behavior.
// This sets up the Elevation Service and provides a method to launch a
// multiprocess test inside the isolated environment.
class IsolatedBrowserTestEnvironment {
 public:
  // Installs the test Elevation Service. If the user does not have admin
  // rights, this constructor will call GTEST_SKIP().
  IsolatedBrowserTestEnvironment();
  IsolatedBrowserTestEnvironment(const IsolatedBrowserTestEnvironment&) =
      delete;
  IsolatedBrowserTestEnvironment& operator=(
      const IsolatedBrowserTestEnvironment&) = delete;
  ~IsolatedBrowserTestEnvironment();

  // Returns true if the environment was successfully initialized.
  bool is_valid() const;

 private:
  std::unique_ptr<ServiceEnvironment> service_environment_;
};

// Spawns a multiprocess test child, identified by `procname` (the name
// given to MULTIPROCESS_TEST_MAIN), running it isolated using the Elevation
// Service. `base_command_line` can be used to pass additional arguments or
// if not provided, base::GetMultiProcessTestChildBaseCommandLine() is used.
// Returns the launched process if successful.
base::Process SpawnIsolatedMultiProcessTestChild(
    const std::string& procname,
    const base::CommandLine& base_command_line =
        base::GetMultiProcessTestChildBaseCommandLine());

}  // namespace chrome

#endif  // CHROME_BROWSER_WIN_ISOLATED_BROWSER_TEST_SUPPORT_H_
