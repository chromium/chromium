// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <xdg-shell-client-protocol.h>

#include <cstdint>

#include "ash/host/ash_window_tree_host_platform.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "cc/trees/layer_tree_host.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/wayland/test/client_util.h"
#include "components/exo/wayland/test/server_util.h"
#include "components/exo/wayland/test/shell_client_data.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/begin_main_frame_waiter.h"

namespace exo::wayland {

namespace {

enum TestCases {
  // Xdg Client (Laros/Crostini)
  XdgByClient,
  XdgWidgetClose,
  XdgWidgetCloseNow,
  XdgWindowDelete,
  // RemoteSehell (ARC++)
  RemoteByClient,
  RemoteWidgetClose,
  RemoteWidgetCloseNow,
  RemoteWindowDelete,
};

class ShellDestructionTest : public test::WaylandServerTest,
                             public testing::WithParamInterface<TestCases> {
 public:
  ShellDestructionTest() = default;
  ShellDestructionTest(const ShellDestructionTest&) = delete;
  ShellDestructionTest& operator=(const ShellDestructionTest&) = delete;
  ~ShellDestructionTest() override = default;

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
                         ShellDestructionTest,
                         testing::Values(XdgByClient,
                                         XdgWidgetClose,
                                         XdgWidgetCloseNow,
                                         XdgWindowDelete));
INSTANTIATE_TEST_SUITE_P(Remote,
                         ShellDestructionTest,
                         testing::Values(RemoteByClient,
                                         RemoteWidgetClose,
                                         RemoteWidgetCloseNow,
                                         RemoteWindowDelete));

// Make sure that xdg topevel/remote surfaces can be
// destroyed via Widget::CloseNow and window deletion.
// (b/276351837)
TEST_P(ShellDestructionTest, ShellDestruction) {
  test::ResourceKey surface_key;

  PostToClientAndWait([&](test::TestClient* client) {
    ASSERT_TRUE(client->InitShmBufferFactory(256 * 256 * 4));

    auto data = std::make_unique<test::ShellClientData>(client);
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
      auto* data_ptr = client->GetDataAs<test::ShellClientData>();
      data_ptr->Close();
    });
  } else {
    delete shell_surface->GetWidget()->GetNativeWindow();
  }

  PostToClientAndWait([&](test::TestClient* client) {
    EXPECT_TRUE(client->GetDataAs<test::ShellClientData>()->close_called());
  });

  // Widget should be deleted.
  EXPECT_FALSE(widget_weak_ptr);
  // The surface resource should also be destroyed.
  EXPECT_FALSE(test::server_util::GetUserDataForResource<Surface>(server_.get(),
                                                                  surface_key));
}

using ShellWithClientTest = test::WaylandServerTest;

// Calling SetPined w/o commit should not crash (crbug.com/979128).
TEST_F(ShellWithClientTest, DestroyRootSurfaceBeforeCommit) {
  test::ResourceKey surface_key;
  PostToClientAndWait([&](test::TestClient* client) {
    ASSERT_TRUE(client->InitShmBufferFactory(256 * 256 * 4));

    auto data = std::make_unique<test::ShellClientData>(client);
    auto* data_ptr = data.get();
    client->set_data(std::move(data));
    data_ptr->CreateRemoteSurface();
    data_ptr->CreateAndAttachBuffer({256, 256});
    surface_key = data_ptr->GetSurfaceResourceKey();
  });
  EXPECT_TRUE(test::server_util::GetUserDataForResource<Surface>(server_.get(),
                                                                 surface_key));
  PostToClientAndWait([&](test::TestClient* client) {
    auto* data_ptr = client->GetDataAs<test::ShellClientData>();
    data_ptr->Pin();
    data_ptr->DestroySurface();
  });
  EXPECT_FALSE(test::server_util::GetUserDataForResource<Surface>(server_.get(),
                                                                  surface_key));
}

// Tests UnsetSnap() w/o attaching buffer doesn't crash (b/278147793).
TEST_F(ShellWithClientTest, UnsetSnapBeforeCommit) {
  test::ResourceKey surface_key;

  PostToClientAndWait([&](test::TestClient* client) {
    auto data = std::make_unique<test::ShellClientData>(client);
    auto* data_ptr = data.get();
    client->set_data(std::move(data));
    data_ptr->CreateXdgToplevel();
    surface_key = data_ptr->GetSurfaceResourceKey();
  });
  EXPECT_TRUE(test::server_util::GetUserDataForResource<Surface>(server_.get(),
                                                                 surface_key));
  // Verify the widget is not created yet.
  Surface* surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), surface_key);
  ShellSurfaceBase* shell_surface_base =
      static_cast<ShellSurfaceBase*>(surface->GetDelegateForTesting());
  ASSERT_TRUE(shell_surface_base);
  EXPECT_FALSE(shell_surface_base->GetWidget());
  PostToClientAndWait([&](test::TestClient* client) {
    auto* data_ptr = client->GetDataAs<test::ShellClientData>();
    data_ptr->UnsetSnap();
  });
  EXPECT_TRUE(test::server_util::GetUserDataForResource<Surface>(server_.get(),
                                                                 surface_key));
}

TEST_F(ShellWithClientTest, CreateWithDisplayId) {
  UpdateDisplay("800x600, 800x600");

  auto primary_id = GetPrimaryDisplay().id();
  auto secondary_id = GetSecondaryDisplay().id();

  // Initialize client.
  PostToClientAndWait([&](test::TestClient* client) {
    ASSERT_TRUE(client->InitShmBufferFactory(800 * 100 * 4));
    ASSERT_EQ(client->globals().outputs.size(), 2u);
  });

  auto create_new_window = [this](const gfx::Rect& bounds, int output_index) {
    test::ResourceKey surface_key;
    PostToClientAndWait([&](test::TestClient* client) {
      auto data = std::make_unique<test::ShellClientData>(client);
      auto* data_ptr = data.get();
      client->set_data(std::move(data));
      data_ptr->CreateXdgToplevel();
      wl_output* target_output =
          output_index == -1 ? nullptr
                             : client->globals().outputs[output_index].get();
      data_ptr->RequestWindowBounds(bounds, target_output);
      data_ptr->Commit();
      surface_key = data_ptr->GetSurfaceResourceKey();
    });

    EXPECT_TRUE(test::server_util::GetUserDataForResource<Surface>(
        server_.get(), surface_key));
    // Verify the widget is not created yet.
    Surface* surface = test::server_util::GetUserDataForResource<Surface>(
        server_.get(), surface_key);
    ShellSurfaceBase* shell_surface_base =
        static_cast<ShellSurfaceBase*>(surface->GetDelegateForTesting());
    return shell_surface_base;
  };

  auto* screen = display::Screen::GetScreen();
  constexpr gfx::Rect kPrimarilyOnPrimary{100, 0, 800, 100};
  {
    auto* shell_surface_base = create_new_window(kPrimarilyOnPrimary, 1);
    EXPECT_EQ(secondary_id,
              screen
                  ->GetDisplayNearestWindow(
                      shell_surface_base->GetWidget()->GetNativeWindow())
                  .id());
    EXPECT_EQ(kPrimarilyOnPrimary,
              shell_surface_base->GetWidget()->GetWindowBoundsInScreen());
  }
  {
    auto* shell_surface_base = create_new_window(kPrimarilyOnPrimary, 0);
    EXPECT_EQ(primary_id,
              screen
                  ->GetDisplayNearestWindow(
                      shell_surface_base->GetWidget()->GetNativeWindow())
                  .id());
    EXPECT_EQ(kPrimarilyOnPrimary,
              shell_surface_base->GetWidget()->GetWindowBoundsInScreen());
  }
  {
    auto* shell_surface_base = create_new_window(kPrimarilyOnPrimary, -1);
    EXPECT_EQ(primary_id,
              screen
                  ->GetDisplayNearestWindow(
                      shell_surface_base->GetWidget()->GetNativeWindow())
                  .id());
    // If display is not specified, new window will be placed fully inside the
    // display.
    // TODO(crbug.com/40212799): This logic is not consistent with
    // ash. This has to be updated once the bug is fixed.
    EXPECT_EQ(gfx::Rect{kPrimarilyOnPrimary.size()},
              shell_surface_base->GetWidget()->GetWindowBoundsInScreen());
  }

  constexpr gfx::Rect kAlmostOnPrimary{101, 0, 700, 100};
  {
    auto* shell_surface_base = create_new_window(kAlmostOnPrimary, 1);
    // The window should stay on the secondary display (output_index=1).
    EXPECT_EQ(secondary_id,
              screen
                  ->GetDisplayNearestWindow(
                      shell_surface_base->GetWidget()->GetNativeWindow())
                  .id());
    EXPECT_EQ(kAlmostOnPrimary,
              shell_surface_base->GetWidget()->GetWindowBoundsInScreen());
  }
  {
    auto* shell_surface_base = create_new_window(kAlmostOnPrimary, 0);
    EXPECT_EQ(primary_id,
              screen
                  ->GetDisplayNearestWindow(
                      shell_surface_base->GetWidget()->GetNativeWindow())
                  .id());
    EXPECT_EQ(kAlmostOnPrimary,
              shell_surface_base->GetWidget()->GetWindowBoundsInScreen());
  }
  {
    auto* shell_surface_base = create_new_window(kAlmostOnPrimary, -1);
    EXPECT_EQ(primary_id,
              screen
                  ->GetDisplayNearestWindow(
                      shell_surface_base->GetWidget()->GetNativeWindow())
                  .id());
    // TODO(crbug.com/40212799): This logic is not consistent with
    // ash. This has to be updated once the bug is fixed.
    EXPECT_EQ(gfx::Rect({100, 0}, kAlmostOnPrimary.size()),
              shell_surface_base->GetWidget()->GetWindowBoundsInScreen());
  }
}

// TODO(crbug.com/338519156): Fix and enable on MSan.
#if defined(MEMORY_SANITIZER)
#define MAYBE_BufferCommitNoNeedsCommit DISABLED_BufferCommitNoNeedsCommit
#else
#define MAYBE_BufferCommitNoNeedsCommit BufferCommitNoNeedsCommit
#endif
TEST_F(ShellWithClientTest, MAYBE_BufferCommitNoNeedsCommit) {
  auto* ash_window_tree_host = static_cast<ash::AshWindowTreeHostPlatform*>(
      ash::Shell::GetPrimaryRootWindow()->GetHost());
  // The compositor may receive draw request upon X11's damage event, which
  // results in commit request. The event is not important in this test, so
  // simply ignore the damage rect event.
  ash_window_tree_host->set_ignore_platform_damage_rect_for_test(true);
  auto* compositor = ash_window_tree_host->compositor();

  // Wait if the commit requests during initialization still exists.
  if (compositor->host_for_testing()->CommitRequested()) {
    ui::BeginMainFrameWaiter(compositor).Wait();
  }

  {
    ui::BeginMainFrameWaiter waiter(compositor);
    PostToClientAndWait([&](test::TestClient* client) {
      ASSERT_TRUE(client->InitShmBufferFactory(800 * 100 * 4));
      auto data = std::make_unique<test::ShellClientData>(client);
      auto* data_ptr = data.get();
      client->set_data(std::move(data));
      data_ptr->CreateXdgToplevel();
    });

    // Make sure a commit never been received nor processed.
    EXPECT_FALSE(waiter.begin_main_frame_received());
    EXPECT_FALSE(compositor->host_for_testing()->CommitRequested());
  }
  {
    ui::BeginMainFrameWaiter waiter(compositor);
    PostToClientAndWait([&](test::TestClient* client) {
      auto* data_ptr = client->GetDataAs<test::ShellClientData>();
      data_ptr->CreateAndAttachBuffer({256, 256});
      data_ptr->Commit();
    });
    // BeginMainFrame might have been already processed so check both
    // condition.
    EXPECT_TRUE(waiter.begin_main_frame_received() ||
                compositor->host_for_testing()->CommitRequested());
  }

  if (compositor->host_for_testing()->CommitRequested()) {
    ui::BeginMainFrameWaiter(compositor).Wait();
  }

  {
    ui::BeginMainFrameWaiter waiter(compositor);
    PostToClientAndWait([&](test::TestClient* client) {
      auto* data_ptr = client->GetDataAs<test::ShellClientData>();
      data_ptr->CreateAndAttachBuffer({256, 256});
      data_ptr->Commit();
    });
    EXPECT_FALSE(waiter.begin_main_frame_received());
    EXPECT_FALSE(compositor->host_for_testing()->CommitRequested());
  }

  {
    ui::BeginMainFrameWaiter waiter(compositor);
    PostToClientAndWait([&](test::TestClient* client) {
      auto* data_ptr = client->GetDataAs<test::ShellClientData>();
      data_ptr->CreateAndAttachBuffer({256, 128});
      data_ptr->Commit();
    });
    EXPECT_TRUE(waiter.begin_main_frame_received() ||
                compositor->host_for_testing()->CommitRequested());
  }
}

}  // namespace exo::wayland
