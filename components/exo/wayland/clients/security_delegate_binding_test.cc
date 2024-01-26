// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <remote-shell-unstable-v1-client-protocol.h>
#include <remote-shell-unstable-v2-client-protocol.h>
#include <wayland-client.h>
#include <wayland-server.h>
#include <xdg-shell-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

#include "base/memory/raw_ptr.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/shell_surface.h"
#include "components/exo/wayland/clients/client_helper.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/test/client_util.h"
#include "components/exo/wayland/test/resource_key.h"
#include "components/exo/wayland/test/server_util.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "components/exo/wayland/xdg_shell.h"
#include "components/exo/xdg_shell_surface.h"

namespace exo::wayland {
namespace {

class SecurityDelegateBindingTest : public test::WaylandServerTest {
 protected:
  void SetUp() override {
    WaylandServerTest::SetUp();
    server_security_delegate_ =
        GetSecurityDelegate(server_->GetWaylandDisplay());
    ASSERT_NE(server_security_delegate_, nullptr);
  }

  raw_ptr<SecurityDelegate, DanglingUntriaged> server_security_delegate_ =
      nullptr;
};

TEST_F(SecurityDelegateBindingTest, ShellSurfaceHasSecurityDelegate) {
  class ClientData : public test::TestClient::CustomData {
   public:
    std::unique_ptr<wl_surface> surface;
    std::unique_ptr<wl_shell_surface> shell_surface;
  };

  test::ResourceKey shell_surface_key;

  PostToClientAndWait([&](test::TestClient* client) {
    auto data = std::make_unique<ClientData>();

    data->surface.reset(wl_compositor_create_surface(client->compositor()));
    data->shell_surface.reset(
        wl_shell_get_shell_surface(client->shell(), data->surface.get()));

    shell_surface_key =
        test::client_util::GetResourceKey(data->shell_surface.get());

    client->set_data(std::move(data));
  });

  EXPECT_EQ(test::server_util::GetUserDataForResource<ShellSurface>(
                server_.get(), shell_surface_key)
                ->GetSecurityDelegate(),
            server_security_delegate_);

  PostToClientAndWait([](test::TestClient* client) {
    // Destroy the client objects.
    client->DestroyData();
  });

  EXPECT_EQ(test::server_util::GetUserDataForResource<ShellSurface>(
                server_.get(), shell_surface_key),
            nullptr);
}

TEST_F(SecurityDelegateBindingTest, XdgSurfaceHasSecurityDelegate) {
  class ClientData : public test::TestClient::CustomData {
   public:
    std::unique_ptr<wl_surface> surface;
    std::unique_ptr<xdg_surface> xdg_surface;
  };

  test::ResourceKey xdg_surface_key;

  PostToClientAndWait([&](test::TestClient* client) {
    auto data = std::make_unique<ClientData>();

    data->surface.reset(wl_compositor_create_surface(client->compositor()));
    data->xdg_surface.reset(xdg_wm_base_get_xdg_surface(client->xdg_wm_base(),
                                                        data->surface.get()));

    xdg_surface_key =
        test::client_util::GetResourceKey(data->xdg_surface.get());

    client->set_data(std::move(data));
  });

  EXPECT_EQ(test::server_util::GetUserDataForResource<WaylandXdgSurface>(
                server_.get(), xdg_surface_key)
                ->shell_surface->GetSecurityDelegate(),
            server_security_delegate_);

  PostToClientAndWait([](test::TestClient* client) {
    // Destroy the client objects.
    client->DestroyData();
  });

  EXPECT_EQ(test::server_util::GetUserDataForResource<WaylandXdgSurface>(
                server_.get(), xdg_surface_key),
            nullptr);
}

TEST_F(SecurityDelegateBindingTest, ZcrRemoteSurfaceV1HasSecurityDelegate) {
  class ClientData : public test::TestClient::CustomData {
   public:
    std::unique_ptr<wl_surface> surface;
    std::unique_ptr<zcr_remote_surface_v1> zcr1_surface;
  };

  test::ResourceKey zcr1_surface_key;

  PostToClientAndWait([&](test::TestClient* client) {
    auto data = std::make_unique<ClientData>();

    data->surface.reset(wl_compositor_create_surface(client->compositor()));
    data->zcr1_surface.reset(zcr_remote_shell_v1_get_remote_surface(
        client->cr_remote_shell_v1(), data->surface.get(),
        ZCR_REMOTE_SHELL_V1_CONTAINER_DEFAULT));

    zcr1_surface_key =
        test::client_util::GetResourceKey(data->zcr1_surface.get());

    client->set_data(std::move(data));
  });

  EXPECT_EQ(
      test::server_util::GetUserDataForResource<ClientControlledShellSurface>(
          server_.get(), zcr1_surface_key)
          ->GetSecurityDelegate(),
      server_security_delegate_);

  PostToClientAndWait([](test::TestClient* client) {
    // Destroy the client objects.
    client->DestroyData();
  });

  EXPECT_EQ(
      test::server_util::GetUserDataForResource<ClientControlledShellSurface>(
          server_.get(), zcr1_surface_key),
      nullptr);
}

TEST_F(SecurityDelegateBindingTest, ZcrRemoteSurfaceV2HasSecurityDelegate) {
  class ClientData : public test::TestClient::CustomData {
   public:
    std::unique_ptr<wl_surface> surface;
    std::unique_ptr<zcr_remote_surface_v2> zcr2_surface;
  };

  test::ResourceKey zcr2_surface_key;

  PostToClientAndWait([&](test::TestClient* client) {
    auto data = std::make_unique<ClientData>();

    data->surface.reset(wl_compositor_create_surface(client->compositor()));
    data->zcr2_surface.reset(zcr_remote_shell_v2_get_remote_surface(
        client->cr_remote_shell_v2(), data->surface.get(),
        ZCR_REMOTE_SHELL_V2_CONTAINER_DEFAULT));

    zcr2_surface_key =
        test::client_util::GetResourceKey(data->zcr2_surface.get());

    client->set_data(std::move(data));
  });

  EXPECT_EQ(
      test::server_util::GetUserDataForResource<ClientControlledShellSurface>(
          server_.get(), zcr2_surface_key)
          ->GetSecurityDelegate(),
      server_security_delegate_);

  PostToClientAndWait([](test::TestClient* client) {
    // Destroy the client objects.
    client->DestroyData();
  });

  EXPECT_EQ(
      test::server_util::GetUserDataForResource<ClientControlledShellSurface>(
          server_.get(), zcr2_surface_key),
      nullptr);
}

}  // namespace
}  // namespace exo::wayland
