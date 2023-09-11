// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_HELPER_TEST_SUDO_HELPER_CLIENT_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_HELPER_TEST_SUDO_HELPER_CLIENT_H_

#include <string>
#include <string_view>

#include "base/files/scoped_file.h"

namespace base {
class FilePath;
}

// Client class to send requests to `test_sudo_helper`. Crosier tests uses this
// class to run tasks requiring user `root`, such as start/stop session_manager
// daemon.
class TestSudoHelperClient {
 public:
  explicit TestSudoHelperClient(const std::string_view server_path);
  TestSudoHelperClient(const TestSudoHelperClient&) = delete;
  TestSudoHelperClient& operator=(const TestSudoHelperClient&) = delete;
  ~TestSudoHelperClient();

  // Runs the given command line via `test_sudo_helper`. Returns true if the
  // command exit with 0. Otherwise, returns false.
  bool RunCommand(const std::string_view command);

 private:
  base::ScopedFD ConnectToServer(const base::FilePath& client_path);

  // Socket path where `test_sudo_helper` server is listening. By default,
  // it is `kTestSudoHelperServerSocketPath`.
  std::string server_path_;
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_HELPER_TEST_SUDO_HELPER_CLIENT_H_
