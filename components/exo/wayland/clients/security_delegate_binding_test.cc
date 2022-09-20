// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <remote-shell-unstable-v1-client-protocol.h>
#include <remote-shell-unstable-v2-client-protocol.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <wayland-util.h>
#include <xdg-shell-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/shell_surface.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "components/exo/wayland/xdg_shell.h"
#include "components/exo/xdg_shell_surface.h"

namespace exo::wayland {
namespace {

class SecurityDelegateBindingTest : public test::WaylandServerTest {
 protected:
  void SetUp() override {
    test::WaylandServerTest::SetUp();
    server_security_delegate_ =
        GetSecurityDelegate(server_->GetWaylandDisplayForTesting());
    ASSERT_NE(server_security_delegate_, nullptr);
  }

  SecurityDelegate* server_security_delegate_ = nullptr;
};

// TODO(yzshen): Convert these to common test utils.
// Iterates over all the |server|'s clients for all of their wl_resources,
// returning the ones with the correct |resource_class|.
static std::vector<wl_resource*> GetResourcesByClass(
    Server* server,
    const char* resource_class) {
  struct Data {
    const char* resource_class;
    std::vector<wl_resource*> ret;
  };
  Data return_holder{.resource_class = resource_class};
  struct wl_client* client;
  struct wl_list* all_clients =
      wl_display_get_client_list(server->GetWaylandDisplayForTesting());
  auto save_resource = [](struct wl_resource* resource, void* data) {
    Data* return_hold = static_cast<Data*>(data);
    if (strcmp(wl_resource_get_class(resource), return_hold->resource_class) ==
        0) {
      return_hold->ret.push_back(resource);
    }
    return WL_ITERATOR_CONTINUE;
  };
  wl_client_for_each(client, all_clients) {
    wl_client_for_each_resource(client, save_resource, &return_holder);
  }
  return std::move(return_holder).ret;
}

template <typename UserData>
UserData* GetUserDataForInterface(Server* server,
                                  const struct wl_interface& interface) {
  std::vector<wl_resource*> surface_resources =
      GetResourcesByClass(server, interface.name);
  if (surface_resources.empty())
    return nullptr;
  return GetUserDataAs<UserData>(surface_resources[0]);
}

TEST_F(SecurityDelegateBindingTest, ShellSurfaceHasSecurityDelegate) {
  wl_surface* sfc = nullptr;
  wl_shell_surface* wl_sfc = nullptr;
  PostToClientAndWait([&](test::TestClient* client) {
    sfc = wl_compositor_create_surface(client->compositor());
    wl_sfc = wl_shell_get_shell_surface(client->shell(), sfc);
  });

  EXPECT_EQ(GetUserDataForInterface<ShellSurface>(server_.get(),
                                                  wl_shell_surface_interface)
                ->GetSecurityDelegate(),
            server_security_delegate_);

  PostToClientAndWait([&]() {
    wl_shell_surface_destroy(wl_sfc);
    wl_surface_destroy(sfc);
  });
}

TEST_F(SecurityDelegateBindingTest, XdgSurfaceHasSecurityDelegate) {
  wl_surface* sfc = nullptr;
  xdg_surface* xdg_sfc = nullptr;
  PostToClientAndWait([&](test::TestClient* client) {
    sfc = wl_compositor_create_surface(client->compositor());
    xdg_sfc = xdg_wm_base_get_xdg_surface(client->xdg_wm_base(), sfc);
  });

  EXPECT_EQ(GetUserDataForInterface<WaylandXdgSurface>(server_.get(),
                                                       xdg_surface_interface)
                ->shell_surface->GetSecurityDelegate(),
            server_security_delegate_);

  PostToClientAndWait([&]() {
    xdg_surface_destroy(xdg_sfc);
    wl_surface_destroy(sfc);
  });
}

TEST_F(SecurityDelegateBindingTest, ZxdgSurfaceV6HasSecurityDelegate) {
  wl_surface* sfc = nullptr;
  zxdg_surface_v6* zxdg_sfc = nullptr;
  PostToClientAndWait([&](test::TestClient* client) {
    sfc = wl_compositor_create_surface(client->compositor());
    zxdg_sfc = zxdg_shell_v6_get_xdg_surface(client->xdg_shell_v6(), sfc);
  });

  EXPECT_EQ(GetUserDataForInterface<WaylandXdgSurface>(
                server_.get(), zxdg_surface_v6_interface)
                ->shell_surface->GetSecurityDelegate(),
            server_security_delegate_);

  PostToClientAndWait([&]() {
    zxdg_surface_v6_destroy(zxdg_sfc);
    wl_surface_destroy(sfc);
  });
}

TEST_F(SecurityDelegateBindingTest, ZcrRemoteSurfaceV1HasSecurityDelegate) {
  wl_surface* sfc = nullptr;
  zcr_remote_surface_v1* zcr1_sfc = nullptr;
  PostToClientAndWait([&](test::TestClient* client) {
    sfc = wl_compositor_create_surface(client->compositor());
    zcr1_sfc = zcr_remote_shell_v1_get_remote_surface(
        client->cr_remote_shell_v1(), sfc,
        ZCR_REMOTE_SHELL_V1_CONTAINER_DEFAULT);
  });

  EXPECT_EQ(GetUserDataForInterface<ClientControlledShellSurface>(
                server_.get(), zcr_remote_surface_v1_interface)
                ->GetSecurityDelegate(),
            server_security_delegate_);

  PostToClientAndWait([&]() {
    zcr_remote_surface_v1_destroy(zcr1_sfc);
    wl_surface_destroy(sfc);
  });
}

TEST_F(SecurityDelegateBindingTest, ZcrRemoteSurfaceV2HasSecurityDelegate) {
  wl_surface* sfc = nullptr;
  zcr_remote_surface_v2* zcr2_sfc = nullptr;
  PostToClientAndWait([&](test::TestClient* client) {
    sfc = wl_compositor_create_surface(client->compositor());
    zcr2_sfc = zcr_remote_shell_v2_get_remote_surface(
        client->cr_remote_shell_v2(), sfc,
        ZCR_REMOTE_SHELL_V2_CONTAINER_DEFAULT);
  });

  EXPECT_EQ(GetUserDataForInterface<ClientControlledShellSurface>(
                server_.get(), zcr_remote_surface_v2_interface)
                ->GetSecurityDelegate(),
            server_security_delegate_);

  PostToClientAndWait([&]() {
    zcr_remote_surface_v2_destroy(zcr2_sfc);
    wl_surface_destroy(sfc);
  });
}

}  // namespace
}  // namespace exo::wayland
