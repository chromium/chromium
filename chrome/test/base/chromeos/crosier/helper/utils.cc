// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/helper/utils.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <array>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"

namespace crosier {

namespace {

void SendBuffer(const base::ScopedFD& sock, base::span<const uint8_t> buf) {
  base::span<const uint8_t> remaining = buf;
  while (!remaining.empty()) {
    ssize_t bytes_sent =
        HANDLE_EINTR(send(sock.get(), remaining.data(), remaining.size(), 0));
    CHECK_GT(bytes_sent, 0);
    remaining = remaining.subspan(static_cast<size_t>(bytes_sent));
  }
}

}  // namespace

base::ScopedFD CreateSocketAndBind(const base::FilePath& path) {
  int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, /*protocol=*/0);
  PCHECK(fd >= 0);
  base::ScopedFD socket(fd);

  // `unlink` just in case there was left over from previous runs.
  std::string path_str = path.value();
  unlink(path_str.c_str());

  struct sockaddr_un addr = {};
  addr.sun_family = AF_UNIX;
  CHECK_LT(path_str.size(), sizeof(addr.sun_path));
  base::span(addr.sun_path)
      .first(path_str.size())
      .copy_from(base::span(path_str));

  PCHECK(bind(socket.get(), reinterpret_cast<struct sockaddr*>(&addr),
              sizeof(addr)) == 0);

  return socket;
}

std::string ReadString(const base::ScopedFD& sock) {
  constexpr int buf_size = 1024;
  std::array<char, buf_size> buf;

  std::string result;
  bool done = false;
  while (!done) {
    ssize_t bytes_read =
        HANDLE_EINTR(recv(sock.get(), buf.data(), buf.size(), 0));
    CHECK_GE(bytes_read, 0);
    if (bytes_read == 0) {
      done = true;
      break;
    }

    auto read_span = base::span(buf).first(static_cast<size_t>(bytes_read));
    if (read_span.back() == 0) {
      done = true;
      read_span = read_span.first(read_span.size() - 1);
    }
    // Strip the null terminator.
    result.append(read_span.begin(), read_span.end());
  }
  return result;
}

void ReadBuffer(const base::ScopedFD& sock, base::span<uint8_t> buf) {
  base::span<uint8_t> remaining = buf;
  while (!remaining.empty()) {
    ssize_t bytes_read =
        HANDLE_EINTR(recv(sock.get(), remaining.data(), remaining.size(), 0));
    CHECK_GE(bytes_read, 0);
    if (bytes_read == 0) {
      // The connection is lost before finishing read. Not supported.
      NOTREACHED() << "Connection lost";
    }
    remaining = remaining.subspan(static_cast<size_t>(bytes_read));
  }
}

void SendString(const base::ScopedFD& sock, std::string_view str) {
  SendBuffer(sock, base::as_byte_span(str));

  char terminator = 0;
  SendBuffer(sock, base::byte_span_from_ref(terminator));
}

}  // namespace crosier
