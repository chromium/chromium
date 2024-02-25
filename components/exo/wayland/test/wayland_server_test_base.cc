// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/wayland_server_test_base.h"

#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <wayland-client-core.h>

#include <memory>

#include "base/atomic_sequence_num.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "components/exo/display.h"
#include "components/exo/security_delegate.h"
#include "components/exo/test/exo_test_data_exchange_delegate.h"
#include "components/exo/test/test_security_delegate.h"
#include "components/exo/wayland/server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace wayland {
namespace test {

WaylandServerTestBase::ScopedTempSocket::ScopedTempSocket() {
  // We use CHECK here and throughout because this is test code and it should
  // fail fast.
  int raw_sock_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, /*protocol=*/0);
  CHECK(raw_sock_fd >= 0);
  fd_.reset(raw_sock_fd);

  char* runtime_dir = getenv("XDG_RUNTIME_DIR");
  CHECK(runtime_dir);
  CHECK(socket_dir_.CreateUniqueTempDirUnderPath(base::FilePath(runtime_dir)));

  server_path_ = socket_dir_.GetPath().Append("wl-test");

  // the pathname, with null terminator, has to fit into sun_path, a char[108].
  CHECK(server_path_.MaybeAsASCII().length() + 1 < 108);

  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, server_path_.MaybeAsASCII().c_str(), 108);
  int size = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path);
  CHECK(bind(fd_.get(), reinterpret_cast<struct sockaddr*>(&addr), size) == 0);
}

WaylandServerTestBase::ScopedTempSocket::~ScopedTempSocket() {
  // If TakeFd() was called this is a no-op, otherwise close it first.
  fd_.reset();

  // Even though it's scoped, manually delete so we can CHECK().
  //
  // This will fail intentionally if the socket has open FDs.
  CHECK(socket_dir_.Delete());
}

base::ScopedFD WaylandServerTestBase::ScopedTempSocket::TakeFd() {
  CHECK(fd_.is_valid());
  return std::move(fd_);
}

WaylandServerTestBase::WaylandServerTestBase() = default;

WaylandServerTestBase::~WaylandServerTestBase() = default;

void WaylandServerTestBase::SetUp() {
  ASSERT_TRUE(xdg_temp_dir_.CreateUniqueTempDir());
  setenv("XDG_RUNTIME_DIR", xdg_temp_dir_.GetPath().MaybeAsASCII().c_str(),
         1 /* overwrite */);
  TestBase::SetUp();
  display_ =
      std::make_unique<Display>(std::make_unique<TestDataExchangeDelegate>());
}

void WaylandServerTestBase::TearDown() {
  display_.reset();
  TestBase::TearDown();
}

std::unique_ptr<Server> WaylandServerTestBase::CreateServer() {
  return CreateServer(std::make_unique<::exo::test::TestSecurityDelegate>());
}

std::unique_ptr<Server> WaylandServerTestBase::CreateServer(
    std::unique_ptr<SecurityDelegate> security_delegate) {
  if (!security_delegate) {
    security_delegate = std::make_unique<::exo::test::TestSecurityDelegate>();
  }
  return Server::Create(display_.get(), std::move(security_delegate));
}

}  // namespace test
}  // namespace wayland
}  // namespace exo
