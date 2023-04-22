// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "components/exo/wayland/wayland_display_output.h"

#include <xdg-shell-client-protocol.h>
#include <cstdint>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/wayland/test/client_util.h"
#include "components/exo/wayland/test/server_util.h"
#include "components/exo/wayland/test/wayland_server_test.h"

namespace exo::wayland {

namespace {

// A custom shell object which can act as xdg toplevel or remote surface.
// TODO(oshima): Implement more key events complete and move to a separate file.
class ShellClientData : public test::TestClient::CustomData {
 public:
  explicit ShellClientData(test::TestClient* client)
      : client_(client),
        surface_(wl_compositor_create_surface(client->compositor())) {}
  ~ShellClientData() override { Close(); }

  // Xdg Shell related methods.
  static void OnXdgToplevelClose(void* data, struct xdg_toplevel* toplevel) {
    static_cast<ShellClientData*>(data)->Close();
  }

  void CreateXdgToplevel() {
    constexpr xdg_toplevel_listener xdg_toplevel_listener = {
        [](void*, xdg_toplevel*, int32_t, int32_t, wl_array*) {},
        &OnXdgToplevelClose,
        [](void*, xdg_toplevel*, int32_t, int32_t) {},
        [](void*, xdg_toplevel*, wl_array*) {},
    };

    xdg_surface_.reset(
        xdg_wm_base_get_xdg_surface(client_->xdg_wm_base(), surface_.get()));
    xdg_toplevel_.reset(xdg_surface_get_toplevel(xdg_surface_.get()));
    xdg_toplevel_add_listener(xdg_toplevel_.get(), &xdg_toplevel_listener,
                              this);
  }

  // Remote Shell related methods.
  static void OnRemoteSurfaceClose(void* data, zcr_remote_surface_v2*) {
    static_cast<ShellClientData*>(data)->Close();
  }

  void CreateRemoteSurface() {
    static constexpr zcr_remote_surface_v2_listener remote_surface_v2_listener =
        {
            &OnRemoteSurfaceClose,
            [](void*, zcr_remote_surface_v2*, uint32_t) {},
            [](void*, zcr_remote_surface_v2*, int, int, int, int) {},
            [](void*, zcr_remote_surface_v2*, uint32_t, uint32_t, int32_t,
               int32_t, int32_t, int32_t, uint32_t) {},
            [](void*, zcr_remote_surface_v2*, uint32_t) {},
            [](void*, zcr_remote_surface_v2*, int32_t, int32_t, int32_t) {},
            [](void*, zcr_remote_surface_v2*, int32_t) {},
            [](void*, zcr_remote_surface_v2*, wl_output*, int32_t, int32_t,
               int32_t, int32_t, uint32_t) {},
        };

    remote_surface_.reset(zcr_remote_shell_v2_get_remote_surface(
        client_->cr_remote_shell_v2(), surface_.get(),
        ZCR_REMOTE_SHELL_V2_CONTAINER_DEFAULT));
    zcr_remote_surface_v2_add_listener(remote_surface_.get(),
                                       &remote_surface_v2_listener, this);
  }

  void Pin() {
    zcr_remote_surface_v2_pin(remote_surface_.get(), /*trusted=*/true);
  }

  // Common to both xdg toplevel and remote surface.
  void CreateAndAttachBuffer(const gfx::Size& size) {
    buffer_ = client_->shm_buffer_factory()->CreateBuffer(0, size.width(),
                                                          size.height());
    wl_surface_attach(surface_.get(), buffer_->resource(), 0, 0);
  }

  void Commit() { wl_surface_commit(surface_.get()); }

  void DestroySurface() { wl_surface_destroy(surface_.release()); }

  void Close() {
    close_called_ = true;
    if (surface_) {
      wl_surface_attach(surface_.get(), nullptr, 0, 0);
    }
    if (buffer_) {
      wl_buffer_destroy(buffer_->GetResourceAndRelease());
    }
    buffer_.reset();
    if (xdg_toplevel_) {
      xdg_toplevel_destroy(xdg_toplevel_.release());
      xdg_surface_destroy(xdg_surface_.release());
    }
    if (remote_surface_) {
      zcr_remote_surface_v2_destroy(remote_surface_.release());
    }
    if (surface_) {
      wl_surface_destroy(surface_.release());
    }
  }

  test::ResourceKey GetSurfaceResourceKey() const {
    return test::client_util::GetResourceKey(surface_.get());
  }

  bool close_called() const { return close_called_; }

 private:
  bool close_called_ = false;
  const raw_ptr<test::TestClient, ExperimentalAsh> client_;
  std::unique_ptr<wl_surface> surface_;
  std::unique_ptr<xdg_surface> xdg_surface_;
  std::unique_ptr<xdg_toplevel> xdg_toplevel_;
  std::unique_ptr<zcr_remote_surface_v2> remote_surface_;
  std::unique_ptr<test::TestBuffer> buffer_;
};

enum TestCases {
  XdgByClient,
  XdgWidgetClose,
  XdgWidgetCloseNow,
  XdgWindowDelete,
  RemoteByClient,
  RemoteWidgetClose,
  RemoteWidgetCloseNow,
  RemoteWindowDelete,
};

class ShellTest : public test::WaylandServerTest,
                  public testing::WithParamInterface<TestCases> {
 public:
  ShellTest() = default;
  ShellTest(const ShellTest&) = delete;
  ShellTest& operator=(const ShellTest&) = delete;
  ~ShellTest() override = default;

  bool IsXdgShell() {
    return GetParam() == XdgWidgetCloseNow || GetParam() == XdgWindowDelete;
  }

  bool IsWidgetCloseNow() {
    return GetParam() == XdgWidgetCloseNow ||
           GetParam() == RemoteWidgetCloseNow;
  }
  bool IsWidgetClose() {
    return GetParam() == XdgWidgetClose || GetParam() == RemoteWidgetClose;
  }
  bool IsByClient() {
    return GetParam() == XdgByClient || GetParam() == RemoteByClient;
  }
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(Xdg,
                         ShellTest,
                         testing::Values(XdgByClient,
                                         XdgWidgetClose,
                                         XdgWidgetCloseNow,
                                         XdgWindowDelete));
INSTANTIATE_TEST_SUITE_P(Remote,
                         ShellTest,
                         testing::Values(RemoteByClient,
                                         RemoteWidgetClose,
                                         RemoteWidgetCloseNow,
                                         RemoteWindowDelete));

// Make sure that xdg topevel/remote surfaces can be
// destroyed via Widget::CloseNow and window deletion.
// (b/276351837)
TEST_P(ShellTest, ShellDestruction) {
  test::ResourceKey surface_key;

  PostToClientAndWait([&](test::TestClient* client) {
    ASSERT_TRUE(client->InitShmBufferFactory(256 * 256 * 4));

    auto data = std::make_unique<ShellClientData>(client);
    auto* data_ptr = data.get();
    client->set_data(std::move(data));
    if (IsXdgShell()) {
      data_ptr->CreateXdgToplevel();
    } else {
      data_ptr->CreateRemoteSurface();
    }
    data_ptr->CreateAndAttachBuffer({256, 256});
    data_ptr->Commit();
    surface_key = data_ptr->GetSurfaceResourceKey();
  });

  Surface* surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), surface_key);
  auto* shell_surface =
      GetShellSurfaceBaseForWindow(surface->window()->GetToplevelWindow());
  auto widget_weak_ptr = shell_surface->GetWidget()->GetWeakPtr();
  ASSERT_TRUE(shell_surface);
  ASSERT_TRUE(shell_surface->GetWidget()->IsVisible());

  if (IsWidgetClose()) {
    shell_surface->GetWidget()->Close();
    base::RunLoop().RunUntilIdle();
  } else if (IsWidgetCloseNow()) {
    shell_surface->GetWidget()->CloseNow();
  } else if (IsByClient()) {
    PostToClientAndWait([&](test::TestClient* client) {
      auto* data_ptr = client->GetDataAs<ShellClientData>();
      data_ptr->Close();
    });
  } else {
    delete shell_surface->GetWidget()->GetNativeWindow();
  }

  PostToClientAndWait([&](test::TestClient* client) {
    EXPECT_TRUE(client->GetDataAs<ShellClientData>()->close_called());
  });

  // Widget should be deleted.
  EXPECT_FALSE(widget_weak_ptr);
  // The surface resource should also be destroyed.
  EXPECT_FALSE(test::server_util::GetUserDataForResource<Surface>(server_.get(),
                                                                  surface_key));
}

using RemoteShellTest = test::WaylandServerTest;

// Calling SetPined w/o commit should not crash (crbug.com/979128).
// TODO(crbug.com/1432923): Re-enable this test
TEST_F(RemoteShellTest, DISABLED_DestroyRootSurfaceBeforeCommit) {
  test::ResourceKey surface_key;

  PostToClientAndWait([&](test::TestClient* client) {
    ASSERT_TRUE(client->InitShmBufferFactory(256 * 256 * 4));

    auto data = std::make_unique<ShellClientData>(client);
    auto* data_ptr = data.get();
    client->set_data(std::move(data));
    data_ptr->CreateRemoteSurface();
    data_ptr->CreateAndAttachBuffer({256, 256});
    surface_key = data_ptr->GetSurfaceResourceKey();
  });
  EXPECT_TRUE(test::server_util::GetUserDataForResource<Surface>(server_.get(),
                                                                 surface_key));
  PostToClientAndWait([&](test::TestClient* client) {
    auto* data_ptr = client->GetDataAs<ShellClientData>();
    data_ptr->DestroySurface();
    data_ptr->Pin();
  });

  EXPECT_FALSE(test::server_util::GetUserDataForResource<Surface>(server_.get(),
                                                                  surface_key));
}

}  // namespace exo::wayland
