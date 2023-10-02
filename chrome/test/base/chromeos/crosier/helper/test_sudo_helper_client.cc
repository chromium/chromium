// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/helper/test_sudo_helper_client.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/test/base/chromeos/crosier/helper/switches.h"
#include "chrome/test/base/chromeos/crosier/helper/utils.h"

namespace {

inline constexpr char kKeyMethod[] = "method";

inline constexpr char kMethodRunCommand[] = "runCommand";
inline constexpr char kKeyCommand[] = "command";

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

TestSudoHelperClient::~TestSudoHelperClient() = default;

TestSudoHelperClient::Result TestSudoHelperClient::RunCommand(
    const std::string_view command) {
  base::Value::Dict dict;
  dict.Set(kKeyMethod, kMethodRunCommand);
  dict.Set(kKeyCommand, command);

  return SendDictAndGetResult(dict);
}

// static
TestSudoHelperClient::Result TestSudoHelperClient::ConnectAndRunCommand(
    const std::string_view command) {
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
  PCHECK(connect(client_sock.get(), reinterpret_cast<sockaddr*>(&addr),
                 addr_len) == 0)
      << "Unable to connect to test_sudo_helper.py's socket. This probably "
      << "means that the script didn't get started before the test or it "
      << "exited or crashed in the meantime.";
  return client_sock;
}

TestSudoHelperClient::Result TestSudoHelperClient::SendDictAndGetResult(
    const base::Value::Dict& dict) {
  std::string json_string;
  CHECK(base::JSONWriter::Write(dict, &json_string));

  base::FilePath client_path;
  CHECK(base::CreateTemporaryFile(&client_path));

  base::ScopedFD sock = ConnectToServer(client_path);

  // Sends the json string.
  crosier::SendString(sock, json_string);

  Result result;

  // Reads the 1 byte return code.
  signed char byte_buffer = 0;
  crosier::ReadBuffer(sock, &byte_buffer, 1);
  result.return_code = byte_buffer;

  // Reads the output.
  result.output = crosier::ReadString(sock);

  sock.reset();

  // Clean up the client socket path.
  unlink(client_path.value().c_str());

  LOG(INFO) << "Json sent: " << json_string;
  LOG(INFO) << "Return Code: " << result.return_code;
  LOG(INFO) << "Output: " << result.output;

  return result;
}
