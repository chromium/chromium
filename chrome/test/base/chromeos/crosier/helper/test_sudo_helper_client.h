// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_HELPER_TEST_SUDO_HELPER_CLIENT_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_HELPER_TEST_SUDO_HELPER_CLIENT_H_

#include <string>
#include <string_view>

#include "base/files/scoped_file.h"
#include "base/values.h"

namespace base {
class FilePath;
}

// Client class to send requests to `test_sudo_helper`. Crosier tests uses this
// class to run tasks requiring user `root`, such as start/stop session_manager
// daemon.
class TestSudoHelperClient {
 public:
  struct Result {
    Result() = default;
    Result(int rc, std::string out) : return_code(rc), output(std::move(out)) {}

    int return_code = -1;
    std::string output;  // stdout and stderr combined.
  };

  TestSudoHelperClient();
  TestSudoHelperClient(const TestSudoHelperClient&) = delete;
  TestSudoHelperClient& operator=(const TestSudoHelperClient&) = delete;
  ~TestSudoHelperClient();

  // Runs the given command line via `test_sudo_helper`. Returns true if the
  // command exit with 0. Otherwise, returns false.
  Result RunCommand(const std::string_view command);

  // Connects using the server path on the default switch, runs one command, and
  // disconnects. Fails if the server path switch is not found.
  static Result ConnectAndRunCommand(const std::string_view command);

 private:
  base::ScopedFD ConnectToServer(const base::FilePath& client_path);

  Result SendDictAndGetResult(const base::Value::Dict& dict);

  // Socket path where `test_sudo_helper` server is listening. By default,
  // it is `kTestSudoHelperServerSocketPath`.
  std::string server_path_;
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_HELPER_TEST_SUDO_HELPER_CLIENT_H_
