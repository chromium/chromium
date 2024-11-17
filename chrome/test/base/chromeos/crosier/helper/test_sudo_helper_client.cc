// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/helper/test_sudo_helper_client.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "chrome/test/base/chromeos/crosier/helper/switches.h"
#include "chrome/test/base/chromeos/crosier/helper/utils.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Tracks the start session manager calls. It should never go above one since
// there could only be one instance of session manager daemon running.
int g_start_session_manager_count = 0;

inline constexpr char kKeyMethod[] = "method";

inline constexpr char kMethodRunCommand[] = "runCommand";
inline constexpr char kKeyCommand[] = "command";

inline constexpr char kMethodStartSessionManager[] = "startSessionManager";
inline constexpr char kMethodStopSessionManager[] = "stopSessionManager";

std::string GetServerSocketPath() {
  base::CommandLine* command = base::CommandLine::ForCurrentProcess();
  CHECK(command->HasSwitch(crosier::kSwitchSocketPath))
      << "Switch " << crosier::kSwitchSocketPath
      << " not specified, can't connect to the test_sudo_helper server.";
  return command->GetSwitchValueASCII(crosier::kSwitchSocketPath);
}

}  // namespace

TestSudoHelperClient::TestSudoHelperClient()
    : server_path_(GetServerSocketPath()) {
  CHECK_LT(server_path_.size(), sizeof(sockaddr_un::sun_path));
}

TestSudoHelperClient::~TestSudoHelperClient() {
  if (session_manager_watcher_thread_ &&
      session_manager_watcher_thread_->IsRunning()) {
    session_manager_watcher_thread_->FlushForTesting();
    session_manager_watcher_thread_->Stop();
  }
}

bool TestSudoHelperClient::WaitForServer(base::TimeDelta max_wait) {
  base::ElapsedTimer elapsed;

  base::Value::Dict dict;
  dict.Set(kKeyMethod, kMethodRunCommand);
  dict.Set(kKeyCommand, "true");

  while (true) {
    Result result = SendDictAndGetResult(dict, /*out_sock=*/nullptr,
                                         /*fatal_on_connection_error=*/false);
    if (result.return_code == 0) {
      break;
    }

    if (elapsed.Elapsed() >= max_wait) {
      LOG(ERROR) << "Failed to wait for server.";
      return false;
    }

    constexpr base::TimeDelta kInterval = base::Seconds(5);
    base::PlatformThread::Sleep(kInterval);
  }
  return true;
}

TestSudoHelperClient::Result TestSudoHelperClient::RunCommand(
    std::string_view command) {
  // This is a test-only function that does a blocking call to the test helper
  // process that should already be running. Synchronuos blocking operation is
  // expected in this testing context.
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::Value::Dict dict;
  dict.Set(kKeyMethod, kMethodRunCommand);
  dict.Set(kKeyCommand, command);

  return SendDictAndGetResult(dict);
}

TestSudoHelperClient::Result TestSudoHelperClient::StartSessionManager(
    base::OnceClosure stopped_callback) {
  CHECK_EQ(g_start_session_manager_count, 0)
      << "Starting more than one session manager instance is not supported.";

  ++g_start_session_manager_count;

  session_manager_stopped_callback_ = std::move(stopped_callback);

  base::Value::Dict dict;
  dict.Set(kKeyMethod, kMethodStartSessionManager);

  base::ScopedFD sock;
  Result result = SendDictAndGetResult(dict, &sock);

  session_manager_watcher_thread_ =
      std::make_unique<base::Thread>("SessionManagerWatcherThread");
  session_manager_watcher_thread_->Start();

  session_manager_watcher_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TestSudoHelperClient::ReadSessionManagerEventOnWatcherThread,
          base::Unretained(this), std::move(sock)));

  return result;
}

TestSudoHelperClient::Result TestSudoHelperClient::StopSessionManager() {
  CHECK_EQ(g_start_session_manager_count, 1)
      << "No stop since session manager is not requested to start.";
  CHECK(session_manager_watcher_thread_)
      << "Unsupported because session manager is not started from this client.";

  base::Value::Dict dict;
  dict.Set(kKeyMethod, kMethodStopSessionManager);

  return SendDictAndGetResult(dict);
}

void TestSudoHelperClient::EnsureSessionManagerStopped() {
  if (g_start_session_manager_count == 0) {
    return;
  }

  CHECK_EQ(StopSessionManager().return_code, 0);
}

// static
TestSudoHelperClient::Result TestSudoHelperClient::ConnectAndRunCommand(
    std::string_view command) {
  return TestSudoHelperClient().RunCommand(command);
}

base::ScopedFD TestSudoHelperClient::ConnectToServer(
    const base::FilePath& client_path) {
  base::ScopedFD client_sock = crosier::CreateSocketAndBind(client_path);

  struct sockaddr_un addr = {0};
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, server_path_.c_str(), sizeof(sockaddr_un::sun_path));

  socklen_t addr_len =
      offsetof(struct sockaddr_un, sun_path) + server_path_.size();
  if (connect(client_sock.get(), reinterpret_cast<sockaddr*>(&addr),
              addr_len) == 0) {
    return client_sock;
  }
  return base::ScopedFD();
}

TestSudoHelperClient::Result TestSudoHelperClient::SendDictAndGetResult(
    const base::Value::Dict& dict,
    base::ScopedFD* out_sock,
    bool fatal_on_connection_error) {
  Result result;

  std::string json_string;
  CHECK(base::JSONWriter::Write(dict, &json_string));

  base::FilePath client_path;
  CHECK(base::CreateTemporaryFile(&client_path));

  base::ScopedFD sock = ConnectToServer(client_path);
  if (!sock.is_valid()) {
    LOG_IF(FATAL, fatal_on_connection_error)
        << "Unable to connect to test_sudo_helper.py's socket. This probably "
        << "means that the script didn't get started before the test or it "
        << "exited or crashed in the meantime.";

    unlink(client_path.value().c_str());

    // Mark `result` as failure.
    result.return_code = -1;
    return result;
  }

  // Sends the json string.
  crosier::SendString(sock, json_string);

  // Reads the 1 byte return code.
  signed char byte_buffer = 0;
  crosier::ReadBuffer(sock, &byte_buffer, 1);
  result.return_code = byte_buffer;

  // Reads the output.
  result.output = crosier::ReadString(sock);

  if (out_sock) {
    *out_sock = std::move(sock);
  } else {
    sock.reset();
  }

  // Clean up the client socket path.
  unlink(client_path.value().c_str());

  LOG(INFO) << "Json sent: " << json_string;
  LOG(INFO) << "Return Code: " << result.return_code;
  LOG(INFO) << "Output: " << result.output;

  return result;
}

void TestSudoHelperClient::ReadSessionManagerEventOnWatcherThread(
    base::ScopedFD sock) {
  CHECK_EQ(session_manager_watcher_thread_->GetThreadId(),
           base::PlatformThread::CurrentId());

  signed char byte_buffer = 0;
  crosier::ReadBuffer(sock, &byte_buffer, 1);
  CHECK_EQ(byte_buffer, 0);

  std::string output = crosier::ReadString(sock);
  CHECK_EQ(output, "stopped");

  if (session_manager_stopped_callback_) {
    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
        ->PostTask(FROM_HERE, std::move(session_manager_stopped_callback_));
  }
}
