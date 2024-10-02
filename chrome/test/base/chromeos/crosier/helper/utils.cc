// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/helper/utils.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"

namespace crosier {

namespace {

void SendBuffer(const base::ScopedFD& sock, const char* buf, int byte_size) {
  const char* p = buf;
  int remaining = byte_size;

  while (remaining > 0) {
    ssize_t bytes_sent = HANDLE_EINTR(send(sock.get(), p, remaining, 0));
    CHECK_GT(bytes_sent, 0);

    p += bytes_sent;
    remaining -= bytes_sent;
  }
}

}  // namespace

base::ScopedFD CreateSocketAndBind(const base::FilePath& path) {
  int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, /*protocol=*/0);
  PCHECK(fd >= 0);
  base::ScopedFD socket(fd);

  // `unlink` just in case there was left over from previous runs.
  unlink(path.value().c_str());

  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path.value().c_str(), sizeof(sockaddr_un::sun_path));

  PCHECK(bind(socket.get(), reinterpret_cast<struct sockaddr*>(&addr),
              sizeof(addr)) == 0);

  return socket;
}

std::string ReadString(const base::ScopedFD& sock) {
  constexpr int buf_size = 1024;
  char buf[buf_size];

  std::string result;
  bool done = false;
  while (!done) {
    ssize_t bytes_read = HANDLE_EINTR(recv(sock.get(), buf, buf_size, 0));
    CHECK_GE(bytes_read, 0);
    if (bytes_read == 0) {
      done = true;
      break;
    }

    if (buf[bytes_read - 1] == 0) {
      done = true;
      --bytes_read;  // No need to copy the null terminator.
    }
    result.append(buf, bytes_read);
  }
  return result;
}

void ReadBuffer(const base::ScopedFD& sock, void* buf, int byte_size) {
  char* p = reinterpret_cast<char*>(buf);
  int remaining = byte_size;

  while (remaining > 0) {
    ssize_t bytes_read = HANDLE_EINTR(recv(sock.get(), p, remaining, 0));
    CHECK_GE(bytes_read, 0);
    if (bytes_read == 0) {
      // The connection is lost before finishing read. Not supported.
      CHECK(false) << "Connection lost";
      break;
    }

    p += bytes_read;
    remaining -= bytes_read;
  }
}

void SendString(const base::ScopedFD& sock, std::string_view str) {
  SendBuffer(sock, str.data(), str.size());

  char terminator = 0;
  SendBuffer(sock, &terminator, 1);
}

}  // namespace crosier
