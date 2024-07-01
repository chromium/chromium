// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_remote_shell_impl.h"

#include <wayland-server-core.h>
#include <wayland-server.h>

#include <memory>
#include <vector>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/bit_cast.h"
#include "base/functional/bind.h"
#include "base/posix/unix_domain_socket.h"
#include "components/exo/display.h"
#include "components/exo/shell_surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/wayland/server_util.h"
#include "ui/aura/window_delegate.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace exo {
namespace wayland {
namespace {

const int kDefaultWindowLength = 100;

enum class RemoteShellEventType { kSendBoundsChanged, kSendWorkspaceInfo };

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

    ResetEventRecords();

    UpdateDisplay("800x600");

    base::CreateSocketPair(&reader_, &writer_);

    wl_display_.reset(wl_display_create());
    wl_client_.reset(wl_client_create(wl_display_.get(), reader_.release()));
    // Use 0 as the id here to avoid the id conflict (i.e. let wayland library
    // choose the id from available ids.) Otherwise that will cause memory leak.
    wl_shell_resource_.reset(wl_resource_create(wl_client_.get(),
                                                &zcr_remote_shell_v2_interface,
                                                /*version=*/1, /*id=*/0));
    wl_remote_surface_resource_.reset(
        wl_resource_create(wl_client(), &zcr_remote_surface_v2_interface,
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
    wl_remote_surface_resource_.reset();

    test::ExoTestBase::TearDown();
  }

  std::unique_ptr<ClientControlledShellSurface::Delegate> CreateDelegate() {
    return shell()->CreateShellSurfaceDelegate(
        wl_remote_surface_resource_.get());
  }

  void ResetEventRecords() {
    remote_shell_event_sequence_.clear();
    remote_shell_requested_bounds_changes_.clear();
  }

  WaylandRemoteShell* shell() { return shell_.get(); }

  wl_client* wl_client() { return wl_client_.get(); }

  wl_resource* wl_remote_surface() { return wl_remote_surface_resource_.get(); }

  static std::vector<RemoteShellEventType> remote_shell_event_sequence() {
    return remote_shell_event_sequence_;
  }

  static std::vector<WaylandRemoteShell::BoundsChangeData>
  remote_shell_requested_bounds_changes() {
    return remote_shell_requested_bounds_changes_;
  }

  static int last_desktop_focus_state() { return last_desktop_focus_state_; }

 private:
  std::unique_ptr<Display> display_;

  base::ScopedFD reader_, writer_;
  ScopedWlDisplay wl_display_;
  ScopedWlClient wl_client_;
  ScopedWlResource wl_shell_resource_;
  ScopedWlResource wl_remote_surface_resource_;

  std::unique_ptr<WaylandRemoteShell> shell_;

  static std::vector<RemoteShellEventType> remote_shell_event_sequence_;
  static std::vector<WaylandRemoteShell::BoundsChangeData>
      remote_shell_requested_bounds_changes_;

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
          uint32_t display_id_hi,
          uint32_t display_id_lo,
          int32_t x,
          int32_t y,
          int32_t width,
          int32_t height,
          uint32_t reason) {
        remote_shell_event_sequence_.push_back(
            RemoteShellEventType::kSendBoundsChanged);
        remote_shell_requested_bounds_changes_.emplace_back(
            (((int64_t)display_id_hi << 32) | display_id_lo),
            gfx::Rect(x, y, width, height),
            static_cast<zcr_remote_surface_v1_bounds_change_reason>(reason));
      },
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
          struct wl_array*) {
        remote_shell_event_sequence_.push_back(
            RemoteShellEventType::kSendWorkspaceInfo);
      },
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
      /*has_bounds_change_reason_float=*/true,
  };
};
std::vector<RemoteShellEventType>
    WaylandRemoteShellTest::remote_shell_event_sequence_;
std::vector<WaylandRemoteShell::BoundsChangeData>
    WaylandRemoteShellTest::remote_shell_requested_bounds_changes_;
uint32_t WaylandRemoteShellTest::last_desktop_focus_state_ = 0;

// Test that all bounds change requests are deferred while the tablet transition
// is happening until it's finished.
TEST_F(WaylandRemoteShellTest, TabletTransition) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .SetDelegate(CreateDelegate())
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  auto* const widget = shell_surface->GetWidget();
  auto* const window = widget->GetNativeWindow();

  // Snap window.
  ash::WindowSnapWMEvent event(ash::WM_EVENT_SNAP_PRIMARY);
  ash::WindowState::Get(window)->OnWMEvent(&event);
  shell_surface->SetSnapPrimary(chromeos::kDefaultSnapRatio);
  shell_surface->SetGeometry(gfx::Rect(0, 0, 400, 520));
  surface->Commit();

  // Enable tablet mode.
  ResetEventRecords();
  ash::TabletModeControllerTestApi().EnterTabletMode();
  task_environment()->FastForwardBy(base::Seconds(1));
  task_environment()->RunUntilIdle();

  const auto expected_sequence = std::vector<RemoteShellEventType>{
      RemoteShellEventType::kSendWorkspaceInfo,
      RemoteShellEventType::kSendBoundsChanged};
  EXPECT_EQ(expected_sequence, remote_shell_event_sequence());
  // TODO(b/236432849): Add a reasonable bounds check.
}

// Verifies bounds change events and workspace info events are triggered with
// proper values and in proper order when display zoom happens. A bounds change
// event must be triggered only for PIP.
TEST_F(WaylandRemoteShellTest, DisplayZoom) {
  // Test a restored window first.
  auto shell_surface =
      exo::test::ShellSurfaceBuilder({256, 256})
          .SetDelegate(CreateDelegate())
          .SetWindowState(chromeos::WindowStateType::kNormal)
          .SetGeometry({100, 100, kDefaultWindowLength, kDefaultWindowLength})
          .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  auto* window = shell_surface->GetWidget()->GetNativeWindow();
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);

  ResetEventRecords();
  ash::Shell::Get()->display_manager()->ZoomDisplay(display.id(), /*up=*/true);
  task_environment()->RunUntilIdle();
  const auto expected_sequence_for_restored = std::vector<RemoteShellEventType>{
      RemoteShellEventType::kSendWorkspaceInfo};
  EXPECT_EQ(expected_sequence_for_restored, remote_shell_event_sequence());

  // Test a maximized window.
  shell_surface->SetMaximized();
  surface->Commit();
  ResetEventRecords();
  ash::Shell::Get()->display_manager()->ZoomDisplay(display.id(), /*up=*/true);
  task_environment()->RunUntilIdle();
  const auto expected_sequence_for_maximized =
      std::vector<RemoteShellEventType>{
          RemoteShellEventType::kSendWorkspaceInfo};
  EXPECT_EQ(expected_sequence_for_maximized, remote_shell_event_sequence());

  // Test a PIP window.
  shell_surface->SetPip();
  // Place PIP at the bottom-right corner so the position will be adjusted with
  // display size change. This means no bounds change event is triggered if PIP
  // is at the top-left corner, but this is fine as the position doesn't need
  // to be adjusted on the client side.
  shell_surface->SetGeometry(
      gfx::Rect(display.bounds().right() - kDefaultWindowLength,
                display.bounds().bottom() - kDefaultWindowLength,
                kDefaultWindowLength, kDefaultWindowLength));
  surface->Commit();
  ResetEventRecords();
  ash::Shell::Get()->display_manager()->ZoomDisplay(display.id(), /*up=*/true);
  task_environment()->RunUntilIdle();
  const auto expected_sequence_for_pip = std::vector<RemoteShellEventType>{
      RemoteShellEventType::kSendWorkspaceInfo,
      RemoteShellEventType::kSendBoundsChanged};
  EXPECT_EQ(remote_shell_event_sequence(), expected_sequence_for_pip);
  ASSERT_EQ(1UL, remote_shell_requested_bounds_changes().size());
  const auto bounds_change = remote_shell_requested_bounds_changes()[0];
  EXPECT_EQ(display.id(), bounds_change.display_id);
  // Verify that the new bounds is scaled larger in pixels.
  EXPECT_GT(kDefaultWindowLength, bounds_change.bounds_in_display.width());
  EXPECT_GT(kDefaultWindowLength, bounds_change.bounds_in_display.height());
  EXPECT_EQ(ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_PIP,
            bounds_change.reason);
}

// Verifies bounds change events and workspace info events are triggered with
// proper values and in proper order when display rotation happens. A bounds
// change event must be triggered only for PIP.
TEST_F(WaylandRemoteShellTest, DisplayRotation) {
  // Test a restored window first.
  auto shell_surface =
      exo::test::ShellSurfaceBuilder({256, 256})
          .SetDelegate(CreateDelegate())
          .SetWindowState(chromeos::WindowStateType::kNormal)
          .SetGeometry({100, 100, kDefaultWindowLength, kDefaultWindowLength})
          .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  auto* window = shell_surface->GetWidget()->GetNativeWindow();
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);

  ResetEventRecords();
  ash::Shell::Get()->display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACCELEROMETER);
  task_environment()->RunUntilIdle();
  const auto expected_sequence_for_restored = std::vector<RemoteShellEventType>{
      RemoteShellEventType::kSendWorkspaceInfo};
  EXPECT_EQ(expected_sequence_for_restored, remote_shell_event_sequence());

  // Test a maximized window.
  shell_surface->SetMaximized();
  surface->Commit();
  ResetEventRecords();
  ash::Shell::Get()->display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_180,
      display::Display::RotationSource::ACCELEROMETER);
  task_environment()->RunUntilIdle();
  const auto expected_sequence_for_maximized =
      std::vector<RemoteShellEventType>{
          RemoteShellEventType::kSendWorkspaceInfo,
          RemoteShellEventType::kSendBoundsChanged};
  EXPECT_EQ(expected_sequence_for_maximized, remote_shell_event_sequence());

  // Test a PIP window.
  shell_surface->SetPip();
  // Place PIP at the bottom-right corner so the position will be adjusted with
  // display rotation.
  shell_surface->SetGeometry(
      gfx::Rect(display.bounds().right(), display.bounds().bottom(),
                kDefaultWindowLength, kDefaultWindowLength));
  surface->Commit();
  const gfx::Rect bounds = window->GetBoundsInScreen();
  const int right_inset = display.bounds().right() - bounds.right();
  const int bottom_inset = display.bounds().bottom() - bounds.bottom();
  ResetEventRecords();
  ash::Shell::Get()->display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_270,
      display::Display::RotationSource::ACCELEROMETER);
  task_environment()->RunUntilIdle();
  const auto expected_sequence_for_pip = std::vector<RemoteShellEventType>{
      RemoteShellEventType::kSendWorkspaceInfo,
      RemoteShellEventType::kSendBoundsChanged};
  EXPECT_EQ(expected_sequence_for_pip, remote_shell_event_sequence());
  ASSERT_EQ(1UL, remote_shell_requested_bounds_changes().size());
  const auto bounds_change = remote_shell_requested_bounds_changes()[0];
  EXPECT_EQ(display.id(), bounds_change.display_id);
  const display::Display& rotated_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  const int expected_x =
      rotated_display.bounds().right() - right_inset - kDefaultWindowLength;
  const int expected_y =
      rotated_display.bounds().bottom() - bottom_inset - kDefaultWindowLength;
  const gfx::Rect expected_bounds = gfx::Rect(
      expected_x, expected_y, kDefaultWindowLength, kDefaultWindowLength);
  EXPECT_EQ(expected_bounds, bounds_change.bounds_in_display);
  EXPECT_EQ(ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_PIP,
            bounds_change.reason);
}

// Test that bounds changes are properly handled when the display is rotated in
// tablet mode.
TEST_F(WaylandRemoteShellTest, DisplayRotationInTabletMode) {
  UpdateDisplay("800x600");
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .SetFirstDisplayAsInternalDisplay();
  // Enable tablet mode.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  task_environment()->RunUntilIdle();

  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .SetDelegate(CreateDelegate())
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  auto* const widget = shell_surface->GetWidget();
  auto* const window = widget->GetNativeWindow();
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);

  // Snap window.
  ash::WindowSnapWMEvent event(ash::WM_EVENT_SNAP_SECONDARY);
  ash::WindowState::Get(window)->OnWMEvent(&event);
  shell_surface->SetSnapSecondary(chromeos::kDefaultSnapRatio);
  shell_surface->SetGeometry(gfx::Rect(400, 0, 400, 520));
  surface->Commit();

  // Rotate the display.
  ResetEventRecords();
  ash::Shell::Get()->display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACCELEROMETER);
  // Any bounds change due to display rotation is deferred until the next event
  // loop.
  EXPECT_TRUE(remote_shell_event_sequence().empty());
  // When the bounds set by the client requires the "adjustment" on the new
  // display configuration, do not adjust it.
  shell_surface->SetBounds(display.id(), gfx::Rect(600, 0, 400, 520));
  surface->Commit();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(1UL, remote_shell_requested_bounds_changes().size());
  EXPECT_EQ(
      ash::SplitViewController::Get(window->GetRootWindow())
          ->GetSnappedWindowBoundsInScreen(ash::SnapPosition::kSecondary,
                                           window, chromeos::kDefaultSnapRatio,
                                           /*account_for_divider_width=*/true),
      remote_shell_requested_bounds_changes()[0].bounds_in_display);
}

// Removing secandary display and re-reconnect it restores the bounds of
// windows on secandary display. This test verifies bounds change events
// and workspace info events are triggered with proper values and in
// proper order.
TEST_F(WaylandRemoteShellTest, DisplayRemovalAddition) {
  auto shell_surface = exo::test::ShellSurfaceBuilder(
                           {kDefaultWindowLength, kDefaultWindowLength})
                           .SetDelegate(CreateDelegate())
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();

  // Add secondary display with a different scale factor.
  UpdateDisplay("800x600,800x600*2");
  auto* display_manager = ash::Shell::Get()->display_manager();
  const int64_t primary_display_id = display_manager->GetDisplayAt(0).id();
  const int64_t secondary_display_id = display_manager->GetDisplayAt(1).id();
  display::ManagedDisplayInfo primary_display_info =
      display_manager->GetDisplayInfo(primary_display_id);
  display::ManagedDisplayInfo secondary_display_info =
      display_manager->GetDisplayInfo(secondary_display_id);

  // Move the window to the secandary display.
  const int initial_x = 100;
  const int initial_y = 100;
  shell_surface->SetScaleFactor(2.f);
  shell_surface->SetBounds(secondary_display_id,
                           gfx::Rect(initial_x, initial_y, kDefaultWindowLength,
                                     kDefaultWindowLength));
  surface->Commit();

  // Disconnect secondary display.
  ResetEventRecords();
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_display_info);
  display_manager->OnNativeDisplaysChanged(display_info_list);
  task_environment()->RunUntilIdle();
  const auto event_sequence_disconnect = std::vector<RemoteShellEventType>{
      RemoteShellEventType::kSendWorkspaceInfo,
      RemoteShellEventType::kSendBoundsChanged};
  EXPECT_EQ(remote_shell_event_sequence(), event_sequence_disconnect);

  ASSERT_EQ(1UL, remote_shell_requested_bounds_changes().size());
  const auto bounds_change = remote_shell_requested_bounds_changes()[0];
  EXPECT_EQ(bounds_change.display_id, primary_display_id);
  // Verify the new bounds is scaled in pixles with the scale factor of the
  // primary display.
  const gfx::Rect expected_bounds_after_disconnection = gfx::Rect(
      initial_x, initial_y, kDefaultWindowLength / 2, kDefaultWindowLength / 2);
  EXPECT_EQ(expected_bounds_after_disconnection,
            bounds_change.bounds_in_display);
  EXPECT_EQ(ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_MOVE,
            bounds_change.reason);

  // Reconnects the previously connected secondary display.
  ResetEventRecords();
  display_info_list.push_back(secondary_display_info);
  display_manager->OnNativeDisplaysChanged(display_info_list);
  task_environment()->RunUntilIdle();
  // Reconnecting the secondary display seems to cause two workspace info
  // events: One for a display metrics change for the primary display, and the
  // other for a display addition event of the secondary display.
  const auto event_sequence_reconnect = std::vector<RemoteShellEventType>{
      RemoteShellEventType::kSendWorkspaceInfo,
      RemoteShellEventType::kSendWorkspaceInfo,
      RemoteShellEventType::kSendBoundsChanged};
  EXPECT_EQ(event_sequence_reconnect, remote_shell_event_sequence());
  ASSERT_EQ(1UL, remote_shell_requested_bounds_changes().size());
  const auto bounds_change_to_secondary =
      remote_shell_requested_bounds_changes()[0];
  EXPECT_EQ(secondary_display_id, bounds_change_to_secondary.display_id);
  const gfx::Rect expected_bounds_after_reconnection = gfx::Rect(
      initial_x, initial_y, kDefaultWindowLength, kDefaultWindowLength);
  EXPECT_EQ(expected_bounds_after_reconnection,
            bounds_change_to_secondary.bounds_in_display);
  EXPECT_EQ(ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_MOVE,
            bounds_change_to_secondary.reason);
}

// Test that the desktop focus state event is called with the proper value in
// response to window focus change.
// Note that some clients such as ARC T+ rely on the behavior that the desktop
// focus change event is invoked immediately once focus switches in ash, which
// means, for example, we must not call `RunLoop::RunUntilIdle()` to wait for
// the event in this test.
TEST_F(WaylandRemoteShellTest, DesktopFocusState) {
  auto client_controlled_shell_surface =
      exo::test::ShellSurfaceBuilder(
          {kDefaultWindowLength, kDefaultWindowLength})
          .SetDelegate(CreateDelegate())
          .SetNoCommit()
          .BuildClientControlledShellSurface();
  auto* surface = client_controlled_shell_surface->root_surface();
  SetSurfaceResource(surface, wl_remote_surface());
  surface->Commit();
  EXPECT_EQ(last_desktop_focus_state(), 2);

  client_controlled_shell_surface->SetMinimized();
  surface->Commit();
  EXPECT_EQ(last_desktop_focus_state(), 1);

  auto shell_surface = exo::test::ShellSurfaceBuilder(
                           {kDefaultWindowLength, kDefaultWindowLength})
                           .BuildShellSurface();
  auto* other_client_window = shell_surface->GetWidget()->GetNativeWindow();
  other_client_window->Show();
  other_client_window->Focus();
  EXPECT_EQ(last_desktop_focus_state(), 3);
}

// Test that the float procedure works.
TEST_F(WaylandRemoteShellTest, FloatSurface) {
  auto shell_surface =
      exo::test::ShellSurfaceBuilder(
          {kDefaultWindowLength, kDefaultWindowLength})
          .SetDelegate(CreateDelegate())
          .SetGeometry({100, 100, kDefaultWindowLength, kDefaultWindowLength})
          .BuildClientControlledShellSurface();
  auto* const surface = shell_surface->root_surface();
  auto* const window_state =
      ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow());
  SetImplementation(wl_remote_surface(), /*implementation=*/nullptr,
                    std::move(shell_surface));

  // Emitting float event.
  const ash::WindowFloatWMEvent float_event(
      chromeos::FloatStartLocation::kBottomRight);
  window_state->OnWMEvent(&float_event);
  ASSERT_EQ(1UL, remote_shell_requested_bounds_changes().size());
  ASSERT_EQ(remote_shell_requested_bounds_changes()[0].reason,
            ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGE_REASON_FLOAT);

  // Set float state from clients.
  zcr_remote_shell::remote_surface_set_float(wl_client(), wl_remote_surface());
  surface->Commit();
  EXPECT_TRUE(window_state->IsFloated());
}

// Move the window across displays with the different scale factors.
TEST_F(WaylandRemoteShellTest, MoveAcrossDisplaysWithDifferentScaleFactors) {
  UpdateDisplay("800x600,800x600*2");

  auto shell_surface =
      exo::test::ShellSurfaceBuilder(
          {kDefaultWindowLength, kDefaultWindowLength})
          .SetDelegate(CreateDelegate())
          .SetGeometry({100, 100, kDefaultWindowLength, kDefaultWindowLength})
          // Disable maximize for verifying the max size.
          .SetCanMaximize(false)
          .BuildClientControlledShellSurface();
  const auto* window = shell_surface->GetWidget()->GetNativeWindow();
  auto* const shell_surface_ptr = shell_surface.get();
  auto* const surface = shell_surface->root_surface();
  SetImplementation(wl_remote_surface(), /*implementation=*/nullptr,
                    std::move(shell_surface));

  const auto* display_manager = ash::Shell::Get()->display_manager();

  // Parameters in dp.
  constexpr gfx::Rect bounds_in_dp(10, 20, kDefaultWindowLength,
                                   kDefaultWindowLength);
  constexpr gfx::Size min_size_in_dp = bounds_in_dp.size();
  const gfx::Size max_size_in_dp =
      gfx::ScaleToRoundedSize(bounds_in_dp.size(), 2);

  // Move the window inside the primary display, and then move it to the
  // secondary display.
  for (int displayIndex = 0; displayIndex < 2; displayIndex++) {
    const int64_t display_id = display_manager->GetDisplayAt(displayIndex).id();
    const auto device_scale_factor =
        display_manager->GetDisplayInfo(display_id).device_scale_factor();

    // Parameters in pixels.
    const auto bounds_in_px =
        gfx::ScaleToRoundedRect(bounds_in_dp, device_scale_factor);
    const auto min_size_in_px =
        gfx::ScaleToRoundedSize(min_size_in_dp, device_scale_factor);
    const auto max_size_in_px =
        gfx::ScaleToRoundedSize(max_size_in_dp, device_scale_factor);

    const uint scale_factor_value =
        base::bit_cast<const uint>(device_scale_factor);
    zcr_remote_shell::remote_surface_set_scale_factor(
        wl_client(), wl_remote_surface(), scale_factor_value);

    // Set bounds, min size, max size, and then commit.
    shell_surface_ptr->SetBounds(display_id, bounds_in_px);
    zcr_remote_shell::remote_surface_set_min_size(
        wl_client(), wl_remote_surface(), min_size_in_px.width(),
        min_size_in_px.height());
    zcr_remote_shell::remote_surface_set_max_size(
        wl_client(), wl_remote_surface(), max_size_in_px.width(),
        max_size_in_px.height());
    surface->Commit();

    EXPECT_EQ(window->GetBoundsInRootWindow(), bounds_in_dp);
    EXPECT_EQ(window->delegate()->GetMinimumSize(), min_size_in_dp);
    EXPECT_EQ(window->delegate()->GetMaximumSize(), max_size_in_dp);
  }
}

// Change the display's device scale factor.
TEST_F(WaylandRemoteShellTest, DeviceScaleFactorChange) {
  UpdateDisplay("800x600");

  auto shell_surface =
      exo::test::ShellSurfaceBuilder(
          {kDefaultWindowLength, kDefaultWindowLength})
          .SetDelegate(CreateDelegate())
          .SetGeometry({100, 100, kDefaultWindowLength, kDefaultWindowLength})
          // Disable maximize for verifying the max size.
          .SetCanMaximize(false)
          .BuildClientControlledShellSurface();
  const auto* window = shell_surface->GetWidget()->GetNativeWindow();
  auto* const shell_surface_ptr = shell_surface.get();
  auto* const surface = shell_surface->root_surface();
  SetImplementation(wl_remote_surface(), /*implementation=*/nullptr,
                    std::move(shell_surface));

  // Change the display's device scale factor.
  UpdateDisplay("800x600*2");

  const auto* display_manager = ash::Shell::Get()->display_manager();
  const int64_t display_id = display_manager->GetDisplayAt(0).id();
  const auto device_scale_factor =
      display_manager->GetDisplayInfo(display_id).device_scale_factor();

  // Parameters in dp.
  constexpr gfx::Rect bounds_in_dp(10, 20, kDefaultWindowLength,
                                   kDefaultWindowLength);
  constexpr gfx::Size min_size_in_dp = bounds_in_dp.size();
  const gfx::Size max_size_in_dp =
      gfx::ScaleToRoundedSize(bounds_in_dp.size(), 2);

  // Parameters in pixels.
  const auto bounds_in_px =
      gfx::ScaleToRoundedRect(bounds_in_dp, device_scale_factor);
  const auto min_size_in_px =
      gfx::ScaleToRoundedSize(min_size_in_dp, device_scale_factor);
  const auto max_size_in_px =
      gfx::ScaleToRoundedSize(max_size_in_dp, device_scale_factor);

  // Set bounds, min size, max size, and then commit.
  shell_surface_ptr->SetBounds(display_id, bounds_in_px);
  zcr_remote_shell::remote_surface_set_min_size(
      wl_client(), wl_remote_surface(), min_size_in_px.width(),
      min_size_in_px.height());
  zcr_remote_shell::remote_surface_set_max_size(
      wl_client(), wl_remote_surface(), max_size_in_px.width(),
      max_size_in_px.height());
  surface->Commit();

  EXPECT_EQ(window->GetBoundsInRootWindow(), bounds_in_dp);
  EXPECT_EQ(window->delegate()->GetMinimumSize(), min_size_in_dp);
  EXPECT_EQ(window->delegate()->GetMaximumSize(), max_size_in_dp);
}

}  // namespace wayland
}  // namespace exo
