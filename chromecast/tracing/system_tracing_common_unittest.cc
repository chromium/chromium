// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/tracing/system_tracing_common.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace tracing {
namespace {

sockaddr_un MakeSocketAddress(const base::FilePath& path) {
  sockaddr_un addr = {};
  addr.sun_family = AF_UNIX;
  // |addr| is value initialized, so the remainder of sun_path stays NUL.
  base::span(addr.sun_path).copy_prefix_from(path.value());
  return addr;
}

// Returns the permission bits of the node at |path|, or -1 if it can't be
// stat()ed.
int GetMode(const base::FilePath& path) {
  struct stat st = {};
  if (stat(path.value().c_str(), &st)) {
    return -1;
  }
  return st.st_mode & 07777;
}

class SystemTracingCommonTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    socket_path_ = temp_dir_.GetPath().Append("tracing");
    // sun_path is a fixed size buffer; bail out clearly rather than silently
    // truncating if the bot's temp dir is unusually deep.
    ASSERT_LT(socket_path_.value().size(), sizeof(sockaddr_un::sun_path));
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath socket_path_;
};

// The tracing service can drive system-wide ftrace, so its socket must never
// be reachable by arbitrary local processes. Run with a fully permissive
// umask, which is what made the socket world-writable before.
TEST_F(SystemTracingCommonTest, ServerSocketIsNotWorldAccessible) {
  const mode_t old_umask = umask(0);
  base::ScopedFD server_fd =
      CreateTracingServerSocket(MakeSocketAddress(socket_path_));
  umask(old_umask);

  ASSERT_TRUE(server_fd.is_valid());
  EXPECT_EQ(0660, GetMode(socket_path_));
}

// The umask is process wide, so it has to be put back.
TEST_F(SystemTracingCommonTest, ServerSocketRestoresUmask) {
  const mode_t old_umask = umask(022);
  base::ScopedFD server_fd =
      CreateTracingServerSocket(MakeSocketAddress(socket_path_));
  const mode_t umask_after = umask(old_umask);

  ASSERT_TRUE(server_fd.is_valid());
  EXPECT_EQ(022u, umask_after);
}

// The service is restarted over a socket node left behind by a previous run.
TEST_F(SystemTracingCommonTest, ServerSocketReplacesStaleNode) {
  ASSERT_TRUE(base::WriteFile(socket_path_, "stale"));

  base::ScopedFD server_fd =
      CreateTracingServerSocket(MakeSocketAddress(socket_path_));

  ASSERT_TRUE(server_fd.is_valid());
  EXPECT_EQ(0660, GetMode(socket_path_));
}

TEST_F(SystemTracingCommonTest, ServerSocketFailsOnUnwritablePath) {
  base::ScopedFD server_fd = CreateTracingServerSocket(
      MakeSocketAddress(temp_dir_.GetPath().Append("no_such_dir/tracing")));

  EXPECT_FALSE(server_fd.is_valid());
}

// End to end: a client connecting over the real listening socket is accepted
// and authorized, since the test process is its own peer.
TEST_F(SystemTracingCommonTest, AuthorizesPeerOnAcceptedConnection) {
  const sockaddr_un addr = MakeSocketAddress(socket_path_);
  base::ScopedFD server_fd = CreateTracingServerSocket(addr);
  ASSERT_TRUE(server_fd.is_valid());

  base::ScopedFD client_fd(
      socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, /*protocol=*/0));
  ASSERT_TRUE(client_fd.is_valid());
  ASSERT_EQ(0, connect(client_fd.get(),
                       reinterpret_cast<const struct sockaddr*>(&addr),
                       sizeof(addr)));

  base::ScopedFD connection_fd(
      accept4(server_fd.get(), nullptr, nullptr, SOCK_CLOEXEC));
  ASSERT_TRUE(connection_fd.is_valid());

  EXPECT_TRUE(IsTracingClientAuthorized(connection_fd.get()));
}

TEST_F(SystemTracingCommonTest, AuthorizesSameUserPeer) {
  int fds[2] = {-1, -1};
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET, /*protocol=*/0, fds));
  base::ScopedFD server_end(fds[0]);
  base::ScopedFD client_end(fds[1]);

  EXPECT_TRUE(IsTracingClientAuthorized(server_end.get()));
}

// Credentials that can't be read must not be treated as authorized.
TEST_F(SystemTracingCommonTest, RejectsPeerWithoutCredentials) {
  EXPECT_FALSE(IsTracingClientAuthorized(-1));
}

}  // namespace
}  // namespace tracing
}  // namespace chromecast
