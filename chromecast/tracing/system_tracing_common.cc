// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/tracing/system_tracing_common.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iterator>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace chromecast {
namespace tracing {
namespace {

const char kSocketPath[] = "/dev/socket/tracing/tracing";

// Access mode for the listening socket node: the service's user and group
// only. See CreateTracingServerSocket().
constexpr mode_t kSocketMode = 0660;

}  // namespace

sockaddr_un GetSystemTracingSocketAddress() {
  struct sockaddr_un addr;
  UNSAFE_TODO(memset(&addr, 0, sizeof(addr)));
  static_assert(sizeof(kSocketPath) <= sizeof(addr.sun_path),
                "Address too long");
  UNSAFE_TODO(strncpy(addr.sun_path, kSocketPath, sizeof(addr.sun_path) - 1));
  addr.sun_family = AF_UNIX;
  return addr;
}

base::ScopedFD CreateTracingServerSocket(const sockaddr_un& addr) {
  if (unlink(addr.sun_path) != 0 && errno != ENOENT) {
    PLOG(ERROR) << "unlink: " << addr.sun_path;
  }

  base::ScopedFD socket_fd(
      socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
  if (!socket_fd.is_valid()) {
    PLOG(ERROR) << "socket";
    return base::ScopedFD();
  }

  // bind() creates the node with mode 0777 & ~umask, so a permissive inherited
  // umask would publish a world-writable socket for the window between bind()
  // and the chmod() below. Narrow the umask across the bind() to close that
  // window. The caller is single threaded at this point, so temporarily
  // changing the process-wide umask is safe.
  const mode_t old_umask = umask(0777 & ~kSocketMode);
  const int bind_result =
      bind(socket_fd.get(), reinterpret_cast<const struct sockaddr*>(&addr),
           sizeof(addr));
  const int bind_errno = errno;
  umask(old_umask);

  if (bind_result) {
    errno = bind_errno;
    PLOG(ERROR) << "bind: " << addr.sun_path;
    return base::ScopedFD();
  }

  // Not all filesystems apply the umask to socket nodes, so also set the mode
  // explicitly.
  if (chmod(addr.sun_path, kSocketMode)) {
    PLOG(WARNING) << "chmod: " << addr.sun_path;
  }

  // Whatever the reason a restricted mode could not be applied, refuse to
  // serve on a node that arbitrary local processes can reach.
  struct stat st = {};
  if (stat(addr.sun_path, &st)) {
    PLOG(ERROR) << "stat: " << addr.sun_path;
    return base::ScopedFD();
  }
  if (st.st_mode & S_IRWXO) {
    LOG(ERROR) << "Refusing to listen on world-accessible socket "
               << addr.sun_path << " "
               << base::StringPrintf("(mode 0%o)",
                                     static_cast<unsigned>(st.st_mode & 07777));
    return base::ScopedFD();
  }

  static constexpr int kBacklog = 10;
  if (listen(socket_fd.get(), kBacklog)) {
    PLOG(ERROR) << "listen: " << addr.sun_path;
    return base::ScopedFD();
  }

  return socket_fd;
}

bool IsTracingClientAuthorized(int socket_fd) {
  struct ucred cred = {};
  socklen_t cred_size = sizeof(cred);
  if (getsockopt(socket_fd, SOL_SOCKET, SO_PEERCRED, &cred, &cred_size) < 0) {
    PLOG(ERROR) << "getsockopt(SO_PEERCRED)";
    return false;
  }
  if (cred_size != sizeof(cred)) {
    LOG(ERROR) << "getsockopt(SO_PEERCRED) returned " << cred_size
               << " bytes, expected " << sizeof(cred);
    return false;
  }
  if (cred.uid != geteuid()) {
    LOG(WARNING) << "Rejecting tracing client with uid " << cred.uid << " (pid "
                 << cred.pid << ")";
    return false;
  }
  return true;
}

}  // namespace tracing
}  // namespace chromecast
