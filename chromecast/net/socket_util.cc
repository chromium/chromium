// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/socket_util.h"

#include <sys/socket.h>

#include <utility>

#include "base/logging.h"
#include "net/base/net_errors.h"
#include "net/base/sockaddr_storage.h"
#include "net/socket/socket_posix.h"
#include "net/socket/stream_socket.h"
#include "net/socket/unix_domain_client_socket_posix.h"

namespace chromecast {

bool CreateUnnamedSocketPair(base::ScopedFD* fd1, base::ScopedFD* fd2) {
  int raw_socks[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, raw_socks) == -1) {
    return false;
  }
  fd1->reset(raw_socks[0]);
  fd2->reset(raw_socks[1]);
  return true;
}

std::unique_ptr<net::StreamSocket> AdoptUnnamedSocketHandle(
    base::ScopedFD socket_fd) {
  if (!socket_fd.is_valid()) {
    LOG(ERROR) << "Received invalid socket descriptor: " << socket_fd.get();
    return nullptr;
  }

  net::SockaddrStorage address;
  auto socket_posix = std::make_unique<net::SocketPosix>();
  if (socket_posix->AdoptConnectedSocket(socket_fd.release(), address) !=
      net::OK) {
    LOG(ERROR) << "Cannot adopt connected socket.";
    return nullptr;
  }
  return std::make_unique<net::UnixDomainClientSocket>(std::move(socket_posix));
}

}  // namespace chromecast
