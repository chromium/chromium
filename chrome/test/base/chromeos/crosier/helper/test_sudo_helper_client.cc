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
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/test/base/chromeos/crosier/helper/utils.h"

namespace {

inline constexpr char kKeyMethod[] = "method";

inline constexpr char kMethodRunCommand[] = "runCommand";
inline constexpr char kKeyCommand[] = "command";

// Returns a JSON string that wraps a remote command line.
std::string ToJsonRunCommand(const std::string_view command) {
  base::Value::Dict dict;
  dict.Set(kKeyMethod, kMethodRunCommand);
  dict.Set(kKeyCommand, command);

  std::string json_string;
  CHECK(base::JSONWriter::Write(dict, &json_string));
  return json_string;
}

}  // namespace

TestSudoHelperClient::TestSudoHelperClient(const std::string_view server_path)
    : server_path_(server_path) {
  CHECK_LT(server_path_.size(), sizeof(sockaddr_un::sun_path));
}

TestSudoHelperClient::~TestSudoHelperClient() = default;

bool TestSudoHelperClient::RunCommand(const std::string_view command) {
  base::FilePath client_path;
  CHECK(base::CreateTemporaryFile(&client_path));

  base::ScopedFD sock = ConnectToServer(client_path);

  // Sends the command line.
  crosier::SendString(sock, ToJsonRunCommand(command));

  // Reads the 1 byte return code.
  char return_code = 0;
  crosier::ReadBuffer(sock, &return_code, 1);

  // Reads the output.
  std::string output = crosier::ReadString(sock);

  LOG(INFO) << "RunCommand: " << command;
  LOG(INFO) << "Return Code: " << base::NumberToString(return_code);
  LOG(INFO) << "Output: " << output;

  sock.reset();

  // Clean up the client socket path.
  unlink(client_path.value().c_str());

  return return_code == 0;
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
                 addr_len) == 0);
  return client_sock;
}
