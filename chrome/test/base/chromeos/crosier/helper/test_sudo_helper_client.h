// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_HELPER_TEST_SUDO_HELPER_CLIENT_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_HELPER_TEST_SUDO_HELPER_CLIENT_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
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

  // Waits for server socket to be ready. Returns true if a connection is made
  // successfully within the time out. Otherwise, returns false.
  bool WaitForServer(base::TimeDelta max_wait);

  // Runs the given command line via `test_sudo_helper`. Returns true if the
  // command exit with 0. Otherwise, returns false.
  Result RunCommand(std::string_view command);

  // Connects using the server path on the default switch, runs one command, and
  // disconnects. Fails if the server path switch is not found.
  static Result ConnectAndRunCommand(std::string_view command);

  // Starts the session_manager daemon. `stopped_callback` will be invoked
  // when session_manager daemon terminates.
  Result StartSessionManager(base::OnceClosure stopped_callback);

  // Stops the session manager daemon.
  Result StopSessionManager();

  // Ensures that session manager daemon is not left running from this client.
  void EnsureSessionManagerStopped();

 private:
  base::ScopedFD ConnectToServer(const base::FilePath& client_path);

  Result SendDictAndGetResult(const base::Value::Dict& dict,
                              base::ScopedFD* out_sock = nullptr,
                              bool fatal_on_connection_error = true);

  void ReadSessionManagerEventOnWatcherThread(base::ScopedFD sock);

  // Socket path where `test_sudo_helper` server is listening. By default,
  // it is `kTestSudoHelperServerSocketPath`.
  std::string server_path_;

  std::unique_ptr<base::Thread> session_manager_watcher_thread_;
  base::OnceClosure session_manager_stopped_callback_;
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_HELPER_TEST_SUDO_HELPER_CLIENT_H_
