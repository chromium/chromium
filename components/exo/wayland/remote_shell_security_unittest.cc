// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <remote-shell-unstable-v1-client-protocol.h>
#include <remote-shell-unstable-v2-client-protocol.h>
#include <wayland-client-protocol.h>

#include "components/exo/test/test_security_delegate.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo::wayland {
namespace {

class RestrictedRemoteShellSecurityTest : public test::WaylandServerTest {
 public:
  RestrictedRemoteShellSecurityTest() = default;
  RestrictedRemoteShellSecurityTest(const RestrictedRemoteShellSecurityTest&) =
      delete;
  RestrictedRemoteShellSecurityTest& operator=(
      const RestrictedRemoteShellSecurityTest&) = delete;

  std::unique_ptr<Server> CreateServer() override {
    auto security_delegate =
        std::make_unique<::exo::test::TestSecurityDelegate>();
    security_delegate->SetCanAccessRemoteShell(false);
    return Server::Create(display_.get(), std::move(security_delegate));
  }
};

TEST_F(RestrictedRemoteShellSecurityTest, RemoteShellNotAdvertised) {
  PostToClientAndWait([](test::TestClient* client) {
    EXPECT_EQ(client->cr_remote_shell_v1(), nullptr);
    EXPECT_EQ(client->cr_remote_shell_v2(), nullptr);
  });
}

class UnrestrictedRemoteShellSecurityTest : public test::WaylandServerTest {
 public:
  UnrestrictedRemoteShellSecurityTest() = default;
  UnrestrictedRemoteShellSecurityTest(
      const UnrestrictedRemoteShellSecurityTest&) = delete;
  UnrestrictedRemoteShellSecurityTest& operator=(
      const UnrestrictedRemoteShellSecurityTest&) = delete;

  std::unique_ptr<Server> CreateServer() override {
    auto security_delegate =
        std::make_unique<::exo::test::TestSecurityDelegate>();
    security_delegate->SetCanAccessRemoteShell(true);
    return Server::Create(display_.get(), std::move(security_delegate));
  }
};

TEST_F(UnrestrictedRemoteShellSecurityTest, RemoteShellIsAdvertised) {
  PostToClientAndWait([](test::TestClient* client) {
    EXPECT_NE(client->cr_remote_shell_v1(), nullptr);
    EXPECT_NE(client->cr_remote_shell_v2(), nullptr);
  });
}

}  // namespace
}  // namespace exo::wayland
