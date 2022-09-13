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
#include "components/exo/wayland/clients/test/wayland_client_test.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/xdg_shell.h"
#include "components/exo/xdg_shell_surface.h"

namespace exo::wayland {

namespace {

using SecurityDelegateBindingTest = WaylandClientTest;

class GlobalBindings {
 public:
  explicit GlobalBindings(struct wl_display* display) {
    registry_ = wl_display_get_registry(display);
    struct wl_registry_listener compositor_binding_listener = {
        [](void* data, wl_registry* registry, uint32_t id,
           const char* interface, uint32_t version) {
          static_cast<GlobalBindings*>(data)->BindGlobal(registry, id,
                                                         interface, version);
        },
        nullptr};
    wl_registry_add_listener(registry_, &compositor_binding_listener, this);
    wl_display_roundtrip(display);
  }

  ~GlobalBindings() {
    zcr_remote_shell_v2_destroy(zcr_remote_shell_v2_);
    zcr_remote_shell_v1_destroy(zcr_remote_shell_v1_);
    zxdg_shell_v6_destroy(zxdg_shell_);
    xdg_wm_base_destroy(xdg_wm_base_);
    wl_shell_destroy(shell_);
    wl_compositor_destroy(compositor_);
    wl_registry_destroy(registry_);
  }

  struct wl_registry* registry() { return registry_; }
  struct wl_compositor* compositor() { return compositor_; }
  struct wl_shell* shell() { return shell_; }
  struct xdg_wm_base* xdg_wm_base() { return xdg_wm_base_; }
  struct zxdg_shell_v6* zxdg_shell() { return zxdg_shell_; }
  struct zcr_remote_shell_v1* zcr_remote_shell_v1() {
    return zcr_remote_shell_v1_;
  }
  struct zcr_remote_shell_v2* zcr_remote_shell_v2() {
    return zcr_remote_shell_v2_;
  }

 private:
  void BindGlobal(wl_registry* registry,
                  uint32_t id,
                  const char* interface,
                  uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
      compositor_ = static_cast<struct wl_compositor*>(
          wl_registry_bind(registry, id, &wl_compositor_interface, version));
    } else if (strcmp(interface, wl_shell_interface.name) == 0) {
      shell_ = static_cast<struct wl_shell*>(
          wl_registry_bind(registry, id, &wl_shell_interface, version));
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
      xdg_wm_base_ = static_cast<struct xdg_wm_base*>(
          wl_registry_bind(registry, id, &xdg_wm_base_interface, version));
    } else if (strcmp(interface, zxdg_shell_v6_interface.name) == 0) {
      zxdg_shell_ = static_cast<struct zxdg_shell_v6*>(
          wl_registry_bind(registry, id, &zxdg_shell_v6_interface, version));
    } else if (strcmp(interface, zcr_remote_shell_v1_interface.name) == 0) {
      zcr_remote_shell_v1_ =
          static_cast<struct zcr_remote_shell_v1*>(wl_registry_bind(
              registry, id, &zcr_remote_shell_v1_interface, version));
    } else if (strcmp(interface, zcr_remote_shell_v2_interface.name) == 0) {
      zcr_remote_shell_v2_ =
          static_cast<struct zcr_remote_shell_v2*>(wl_registry_bind(
              registry, id, &zcr_remote_shell_v2_interface, version));
    }
  }

  struct wl_registry* registry_ = nullptr;
  struct wl_compositor* compositor_ = nullptr;
  struct wl_shell* shell_ = nullptr;
  struct xdg_wm_base* xdg_wm_base_ = nullptr;
  struct zxdg_shell_v6* zxdg_shell_ = nullptr;
  struct zcr_remote_shell_v1* zcr_remote_shell_v1_ = nullptr;
  struct zcr_remote_shell_v2* zcr_remote_shell_v2_ = nullptr;
};

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

}  // namespace

TEST_F(SecurityDelegateBindingTest, ShellSurfacesHaveSecurityDelegate) {
  // Due to a limitation in the viz::TestGpuServiceHolder, we are only allowed
  // one instance of the WaylandTestHelper. For this reason, all checks must be
  // done in a single test.
  struct wl_display* display = wl_display_connect(nullptr);
  auto gb = std::make_unique<GlobalBindings>(display);
  SecurityDelegate* server_security_delegate =
      GetSecurityDelegate(GetServer()->GetWaylandDisplayForTesting());
  ASSERT_NE(server_security_delegate, nullptr);

  wl_surface* sfc;

  // wl_shell_surface
  sfc = wl_compositor_create_surface(gb->compositor());
  wl_shell_surface* wl_sfc = wl_shell_get_shell_surface(gb->shell(), sfc);
  wl_display_roundtrip(display);
  EXPECT_EQ(GetUserDataForInterface<ShellSurface>(GetServer(),
                                                  wl_shell_surface_interface)
                ->GetSecurityDelegate(),
            server_security_delegate);
  wl_shell_surface_destroy(wl_sfc);
  wl_surface_destroy(sfc);

  // xdg_surface
  sfc = wl_compositor_create_surface(gb->compositor());
  xdg_surface* xdg_sfc = xdg_wm_base_get_xdg_surface(gb->xdg_wm_base(), sfc);
  wl_display_roundtrip(display);
  EXPECT_EQ(GetUserDataForInterface<WaylandXdgSurface>(GetServer(),
                                                       xdg_surface_interface)
                ->shell_surface->GetSecurityDelegate(),
            server_security_delegate);
  xdg_surface_destroy(xdg_sfc);
  wl_surface_destroy(sfc);

  // zxdg_surface
  sfc = wl_compositor_create_surface(gb->compositor());
  zxdg_surface_v6* zxdg_sfc =
      zxdg_shell_v6_get_xdg_surface(gb->zxdg_shell(), sfc);
  wl_display_roundtrip(display);
  EXPECT_EQ(GetUserDataForInterface<WaylandXdgSurface>(
                GetServer(), zxdg_surface_v6_interface)
                ->shell_surface->GetSecurityDelegate(),
            server_security_delegate);
  zxdg_surface_v6_destroy(zxdg_sfc);
  wl_surface_destroy(sfc);

  // zcr_remote_surface_v1
  sfc = wl_compositor_create_surface(gb->compositor());
  zcr_remote_surface_v1* zcr1_sfc = zcr_remote_shell_v1_get_remote_surface(
      gb->zcr_remote_shell_v1(), sfc, ZCR_REMOTE_SHELL_V1_CONTAINER_DEFAULT);
  wl_display_roundtrip(display);
  EXPECT_EQ(GetUserDataForInterface<ClientControlledShellSurface>(
                GetServer(), zcr_remote_surface_v1_interface)
                ->GetSecurityDelegate(),
            server_security_delegate);
  zcr_remote_surface_v1_destroy(zcr1_sfc);
  wl_surface_destroy(sfc);

  // zcr_remote_surface_v2
  sfc = wl_compositor_create_surface(gb->compositor());
  zcr_remote_surface_v2* zcr2_sfc = zcr_remote_shell_v2_get_remote_surface(
      gb->zcr_remote_shell_v2(), sfc, ZCR_REMOTE_SHELL_V2_CONTAINER_DEFAULT);
  wl_display_roundtrip(display);
  EXPECT_EQ(GetUserDataForInterface<ClientControlledShellSurface>(
                GetServer(), zcr_remote_surface_v2_interface)
                ->GetSecurityDelegate(),
            server_security_delegate);
  zcr_remote_surface_v2_destroy(zcr2_sfc);
  wl_surface_destroy(sfc);

  gb.reset();
  wl_display_disconnect(display);
}

}  // namespace exo::wayland
