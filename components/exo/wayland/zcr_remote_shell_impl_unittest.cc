// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_remote_shell_impl.h"

#include <wayland-server-core.h>
#include <wayland-server.h>

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/posix/unix_domain_socket.h"
#include "components/exo/display.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/wayland/server_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace exo {
namespace wayland {
namespace {

struct WlDisplayDeleter {
  void operator()(wl_display* ptr) const { wl_display_destroy(ptr); }
};
using ScopedWlDisplay = std::unique_ptr<wl_display, WlDisplayDeleter>;

struct WlClientDeleter {
  void operator()(wl_client* ptr) const { wl_client_destroy(ptr); }
};
using ScopedWlClient = std::unique_ptr<wl_client, WlClientDeleter>;

struct WlResourceDeleter {
  void operator()(wl_resource* ptr) const { wl_resource_destroy(ptr); }
};
using ScopedWlResource = std::unique_ptr<wl_resource, WlResourceDeleter>;

}  // namespace

class WaylandRemoteShellTest : public test::ExoTestBase {
 public:
  WaylandRemoteShellTest()
      : test::ExoTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  WaylandRemoteShellTest(const WaylandRemoteShellTest&) = delete;
  WaylandRemoteShellTest& operator=(const WaylandRemoteShellTest&) = delete;

  // test::ExoTestBase:
  void SetUp() override {
    test::ExoTestBase::SetUp();

    ResetCounter();

    UpdateDisplay("800x600");

    base::CreateSocketPair(&reader_, &writer_);

    wl_display_.reset(wl_display_create());
    wl_client_.reset(wl_client_create(wl_display_.get(), reader_.release()));
    // Use 0 as the id here to avoid the id conflict (i.e. let wayland library
    // choose the id from available ids.) Otherwise that will cause memory leak.
    wl_shell_resource_.reset(wl_resource_create(wl_client_.get(),
                                                &zcr_remote_shell_v2_interface,
                                                /*version=*/1, /*id=*/0));

    display_ = std::make_unique<Display>();
    shell_ = std::make_unique<WaylandRemoteShell>(
        display_.get(), wl_shell_resource_.get(),
        base::BindRepeating(
            [](int64_t) { return static_cast<wl_resource*>(nullptr); }),
        test_event_mapping_,
        /*use_default_scale_cancellation_default=*/true);
  }
  void TearDown() override {
    shell_.reset();
    display_.reset();

    test::ExoTestBase::TearDown();
  }

  void EnableTabletMode(bool enable) {
    ash::Shell::Get()->tablet_mode_controller()->SetEnabledForTest(enable);
  }

  void ResetCounter() { send_bounds_changed_counter_ = 0; }

  WaylandRemoteShell* shell() { return shell_.get(); }

  wl_client* wl_client() { return wl_client_.get(); }

  static int send_bounds_changed_counter() {
    return send_bounds_changed_counter_;
  }

  static int last_desktop_focus_state() { return last_desktop_focus_state_; }

 private:
  std::unique_ptr<Display> display_;

  base::ScopedFD reader_, writer_;
  ScopedWlDisplay wl_display_;
  ScopedWlClient wl_client_;
  ScopedWlResource wl_shell_resource_;

  std::unique_ptr<WaylandRemoteShell> shell_;

  static int send_bounds_changed_counter_;

  static uint32_t last_desktop_focus_state_;

  const WaylandRemoteShellEventMapping test_event_mapping_ = {
      /*send_window_geometry_changed=*/+[](struct wl_resource*,
                                           int32_t,
                                           int32_t,
                                           int32_t,
                                           int32_t) {},
      /*send_change_zoom_level=*/+[](struct wl_resource*, int32_t) {},
      /*send_state_type_changed=*/+[](struct wl_resource*, uint32_t) {},
      /*send_bounds_changed_in_output=*/
      +[](struct wl_resource*,
          struct wl_resource*,
          int32_t,
          int32_t,
          int32_t,
          int32_t,
          uint32_t) {},
      /*send_bounds_changed=*/
      +[](struct wl_resource*,
          uint32_t,
          uint32_t,
          int32_t,
          int32_t,
          int32_t,
          int32_t,
          uint32_t) { send_bounds_changed_counter_++; },
      /*send_activated=*/
      +[](struct wl_resource*, struct wl_resource*, struct wl_resource*) {},
      /*send_desktop_focus_state_changed=*/
      +[](struct wl_resource*, uint32_t state) {
        last_desktop_focus_state_ = state;
      },
      /*send_workspace_info=*/
      +[](struct wl_resource*,
          uint32_t,
          uint32_t,
          int32_t,
          int32_t,
          int32_t,
          int32_t,
          int32_t,
          int32_t,
          int32_t,
          int32_t,
          int32_t,
          int32_t,
          int32_t,
          int32_t,
          int32_t,
          int32_t,
          uint32_t,
          struct wl_array*) {},
      /*send_drag_finished=*/
      +[](struct wl_resource*, int32_t, int32_t, int32_t) {},
      /*send_drag_started=*/+[](struct wl_resource*, uint32_t) {},
      /*send_layout_mode=*/+[](struct wl_resource*, uint32_t) {},
      /*send_default_device_scale_factor=*/+[](struct wl_resource*, int32_t) {},
      /*send_configure=*/+[](struct wl_resource*, uint32_t) {},
      /*bounds_changed_in_output_since_version=*/0,
      /*desktop_focus_state_changed_since_version=*/0,
      /*layout_mode_since_version=*/0,
      /*default_device_scale_factor_since_version=*/0,
      /*change_zoom_level_since_version=*/0,
      /*send_workspace_info_since_version=*/0,
      /*set_use_default_scale_cancellation_since_version=*/0,
  };
};
int WaylandRemoteShellTest::send_bounds_changed_counter_ = 0;
uint32_t WaylandRemoteShellTest::last_desktop_focus_state_ = 0;

// Test that all bounds change requests are deferred while the tablet transition
// is happening until it's finished.
TEST_F(WaylandRemoteShellTest, DeferBoundsChangeWhileTabletTransition) {
  // Setup buffer/surface/window.
  const gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  ScopedWlResource wl_res(wl_resource_create(
      wl_client(), &zcr_remote_surface_v2_interface, /*version=*/1, /*id=*/0));
  shell_surface->set_delegate(
      shell()->CreateShellSurfaceDelegate(wl_res.get()));

  surface->Attach(buffer.get());
  surface->Commit();
  auto* const widget = shell_surface->GetWidget();
  auto* const window = widget->GetNativeWindow();

  // Snap window.
  ash::WMEvent event(ash::WM_EVENT_SNAP_PRIMARY);
  ash::WindowState::Get(window)->OnWMEvent(&event);
  shell_surface->SetSnappedToPrimary();
  shell_surface->SetGeometry(gfx::Rect(0, 0, 400, 520));
  surface->Commit();

  // Enable tablet mode.
  ResetCounter();
  EnableTabletMode(true);
  task_environment()->FastForwardBy(base::Seconds(1));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(send_bounds_changed_counter(), 1);
}

// Test that the desktop focus state event is called with the proper value in
// response to window focus change.
TEST_F(WaylandRemoteShellTest, DesktopFocusState) {
  // Setup buffer/surface/window.
  const gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  ScopedWlResource wl_res(wl_resource_create(
      wl_client(), &zcr_remote_surface_v2_interface, /*version=*/1, /*id=*/0));
  shell_surface->set_delegate(
      shell()->CreateShellSurfaceDelegate(wl_res.get()));
  SetSurfaceResource(surface.get(), wl_res.get());

  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_EQ(last_desktop_focus_state(), 2);

  shell_surface->SetMinimized();
  surface->Commit();
  EXPECT_EQ(last_desktop_focus_state(), 1);

  auto* other_client_window =
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 100));
  other_client_window->Show();
  other_client_window->Focus();
  EXPECT_EQ(last_desktop_focus_state(), 3);
}

}  // namespace wayland
}  // namespace exo
