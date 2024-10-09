// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/shell_surface.h"

#include <sstream>
#include <vector>

#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/frame_throttler/mock_frame_throttling_observer.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/test/test_widget_builder.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/raster_scale/raster_scale_controller.h"
#include "ash/wm/resize_shadow.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/buffer.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/permission.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/surface_test_util.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/test/mock_security_delegate.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/test/surface_tree_host_test_util.h"
#include "components/exo/test/test_security_delegate.h"
#include "components/exo/window_properties.h"
#include "components/exo/wm_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/display/display.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_properties.h"
#include "ui/wm/core/window_util.h"

namespace exo {

const gfx::BufferFormat kOpaqueFormat = gfx::BufferFormat::RGBX_8888;

using ShellSurfaceTest = test::ExoTestBase;

namespace {

bool HasBackdrop() {
  ash::WorkspaceController* wc = ash::ShellTestApi().workspace_controller();
  return !!ash::WorkspaceControllerTestApi(wc).GetBackdropWindow();
}

uint32_t ConfigureFullscreen(
    uint32_t serial,
    const gfx::Rect& bounds,
    chromeos::WindowStateType state_type,
    bool resizing,
    bool activated,
    const gfx::Vector2d& origin_offset,
    float raster_scale,
    aura::Window::OcclusionState occlusion_state,
    std::optional<chromeos::WindowStateType> restore_state_type) {
  EXPECT_EQ(chromeos::WindowStateType::kFullscreen, state_type);
  return serial;
}

std::unique_ptr<ShellSurface> CreatePopupShellSurface(
    Surface* popup_surface,
    ShellSurface* parent,
    const gfx::Point& origin) {
  auto popup_shell_surface = std::make_unique<ShellSurface>(popup_surface);
  popup_shell_surface->DisableMovement();
  popup_shell_surface->SetPopup();
  popup_shell_surface->SetParent(parent);
  popup_shell_surface->SetOrigin(origin);
  return popup_shell_surface;
}

std::unique_ptr<ShellSurface> CreateX11TransientShellSurface(
    ShellSurface* parent,
    const gfx::Size& size,
    const gfx::Point& origin) {
  return test::ShellSurfaceBuilder(size)
      .SetParent(parent)
      .SetOrigin(origin)
      .BuildShellSurface();
}

const viz::CompositorFrame& GetFrameFromSurface(ShellSurface* shell_surface,
                                                viz::SurfaceManager* manager) {
  viz::SurfaceId surface_id =
      *shell_surface->host_window()->layer()->GetSurfaceId();
  const viz::CompositorFrame& frame =
      manager->GetSurfaceForId(surface_id)->GetActiveFrame();
  return frame;
}

struct ConfigureData {
  gfx::Rect suggested_bounds;
  chromeos::WindowStateType state_type = chromeos::WindowStateType::kDefault;
  bool is_resizing = false;
  bool is_active = false;
  std::optional<chromeos::WindowStateType> restore_state_type = std::nullopt;
  float raster_scale = 1.0f;
  aura::Window::OcclusionState occlusion_state;
  size_t count = 0;
};

uint32_t Configure(
    ConfigureData* config_data,
    const gfx::Rect& bounds,
    chromeos::WindowStateType state_type,
    bool resizing,
    bool activated,
    const gfx::Vector2d& origin_offset,
    float raster_scale,
    aura::Window::OcclusionState occlusion_state,
    std::optional<chromeos::WindowStateType> restore_state_type) {
  config_data->suggested_bounds = bounds;
  config_data->state_type = state_type;
  config_data->is_resizing = resizing;
  config_data->is_active = activated;
  config_data->raster_scale = raster_scale;
  config_data->occlusion_state = occlusion_state;
  config_data->restore_state_type = restore_state_type;
  config_data->count++;
  return 0;
}

uint32_t ConfigureSerial(
    ConfigureData* config_data,
    const gfx::Rect& bounds,
    chromeos::WindowStateType state_type,
    bool resizing,
    bool activated,
    const gfx::Vector2d& origin_offset,
    float raster_scale,
    aura::Window::OcclusionState occlusion_state,
    std::optional<chromeos::WindowStateType> restore_state_type) {
  config_data->suggested_bounds = bounds;
  config_data->state_type = state_type;
  config_data->is_resizing = resizing;
  config_data->is_active = activated;
  config_data->raster_scale = raster_scale;
  config_data->occlusion_state = occlusion_state;
  config_data->restore_state_type = restore_state_type;
  config_data->count++;
  return config_data->count;
}

uint32_t ConfigureSerialVec(
    std::vector<ConfigureData>* config_vec,
    const gfx::Rect& bounds,
    chromeos::WindowStateType state_type,
    bool resizing,
    bool activated,
    const gfx::Vector2d& origin_offset,
    float raster_scale,
    aura::Window::OcclusionState occlusion_state,
    std::optional<chromeos::WindowStateType> restore_state_type) {
  config_vec->emplace_back(config_vec->empty() ? ConfigureData{}
                                               : config_vec->back());
  return ConfigureSerial(&config_vec->back(), bounds, state_type, resizing,
                         activated, origin_offset, raster_scale,
                         occlusion_state, restore_state_type);
}

bool IsCaptureWindow(ShellSurface* shell_surface) {
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  return WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow() ==
         window;
}

cc::Region CreateRegion(ui::Layer::ShapeRects shape_rects) {
  cc::Region shape_region;
  for (const gfx::Rect& rect : shape_rects) {
    shape_region.Union(rect);
  }
  return shape_region;
}

}  // namespace

TEST_F(ShellSurfaceTest, AcknowledgeConfigure) {
  constexpr gfx::Size kBufferSize(32, 32);
  auto shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  gfx::Point origin(100, 100);
  shell_surface->GetWidget()->SetBounds(gfx::Rect(origin, kBufferSize));
  EXPECT_EQ(origin.ToString(),
            surface->window()->GetBoundsInRootWindow().origin().ToString());

  const uint32_t kSerial = 1;
  shell_surface->set_configure_callback(
      base::BindRepeating(&ConfigureFullscreen, kSerial));
  shell_surface->SetFullscreen(true, display::kInvalidDisplayId);

  // Surface origin should not change until configure request is acknowledged.
  EXPECT_EQ(origin.ToString(),
            surface->window()->GetBoundsInRootWindow().origin().ToString());

  // Compositor should be locked until configure request is acknowledged.
  ui::Compositor* compositor =
      shell_surface->GetWidget()->GetNativeWindow()->layer()->GetCompositor();
  EXPECT_TRUE(compositor->IsLocked());

  shell_surface->AcknowledgeConfigure(kSerial);
  auto fullscreen_buffer =
      test::ExoTestHelper::CreateBuffer(GetContext()->bounds().size());
  surface->Attach(fullscreen_buffer.get());
  surface->Commit();

  EXPECT_EQ(gfx::Point().ToString(),
            surface->window()->GetBoundsInRootWindow().origin().ToString());
  EXPECT_FALSE(compositor->IsLocked());
}

TEST_F(ShellSurfaceTest, SetParent) {
  constexpr gfx::Size kBufferSize(256, 256);
  auto parent_shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();
  auto* parent_surface = parent_shell_surface->root_surface();

  auto shell_surface = test::ShellSurfaceBuilder(kBufferSize)
                           .SetParent(parent_shell_surface.get())
                           .BuildShellSurface();
  EXPECT_EQ(
      parent_shell_surface->GetWidget()->GetNativeWindow(),
      wm::GetTransientParent(shell_surface->GetWidget()->GetNativeWindow()));

  // Use OnSetParent to move shell surface to 10, 10.
  gfx::Point parent_origin =
      parent_shell_surface->GetWidget()->GetWindowBoundsInScreen().origin();
  shell_surface->OnSetParent(
      parent_surface,
      gfx::PointAtOffsetFromOrigin(gfx::Point(10, 10) - parent_origin));
  EXPECT_EQ(gfx::Rect(10, 10, 256, 256),
            shell_surface->GetWidget()->GetWindowBoundsInScreen());
  EXPECT_FALSE(shell_surface->CanActivate());
}

// Tests that pareting the shell surface to its transient ancestor is not
// allowed.
TEST_F(ShellSurfaceTest, DoNotParentToTransientAncestor) {
  constexpr gfx::Size kBufferSize(256, 256);
  auto grandparent_shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();

  auto parent_shell_surface = test::ShellSurfaceBuilder(kBufferSize)
                                  .SetParent(grandparent_shell_surface.get())
                                  .BuildShellSurface();
  EXPECT_EQ(grandparent_shell_surface->GetWidget()->GetNativeWindow(),
            wm::GetTransientParent(
                parent_shell_surface->GetWidget()->GetNativeWindow()));

  auto shell_surface = test::ShellSurfaceBuilder(kBufferSize)
                           .SetParent(parent_shell_surface.get())
                           .BuildShellSurface();
  EXPECT_EQ(
      parent_shell_surface->GetWidget()->GetNativeWindow(),
      wm::GetTransientParent(shell_surface->GetWidget()->GetNativeWindow()));

  // Cannot parent to grandparent or parent.
  grandparent_shell_surface->SetParent(shell_surface.get());
  EXPECT_NE(shell_surface->GetWidget()->GetNativeWindow(),
            wm::GetTransientParent(
                grandparent_shell_surface->GetWidget()->GetNativeWindow()));
  EXPECT_EQ(
      parent_shell_surface->GetWidget()->GetNativeWindow(),
      wm::GetTransientParent(shell_surface->GetWidget()->GetNativeWindow()));

  parent_shell_surface->SetParent(shell_surface.get());
  EXPECT_NE(shell_surface->GetWidget()->GetNativeWindow(),
            wm::GetTransientParent(
                parent_shell_surface->GetWidget()->GetNativeWindow()));
  EXPECT_EQ(
      parent_shell_surface->GetWidget()->GetNativeWindow(),
      wm::GetTransientParent(shell_surface->GetWidget()->GetNativeWindow()));
}

TEST_F(ShellSurfaceTest, DeleteShellSurfaceWithTransientChildren) {
  constexpr gfx::Size kBufferSize(256, 256);
  auto parent_shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();

  auto child1_shell_surface = test::ShellSurfaceBuilder(kBufferSize)
                                  .SetParent(parent_shell_surface.get())
                                  .BuildShellSurface();
  auto child2_shell_surface = test::ShellSurfaceBuilder(kBufferSize)
                                  .SetParent(parent_shell_surface.get())
                                  .BuildShellSurface();
  auto child3_shell_surface = test::ShellSurfaceBuilder(kBufferSize)
                                  .SetParent(parent_shell_surface.get())
                                  .BuildShellSurface();

  EXPECT_EQ(parent_shell_surface->GetWidget()->GetNativeWindow(),
            wm::GetTransientParent(
                child1_shell_surface->GetWidget()->GetNativeWindow()));
  EXPECT_EQ(parent_shell_surface->GetWidget()->GetNativeWindow(),
            wm::GetTransientParent(
                child2_shell_surface->GetWidget()->GetNativeWindow()));
  EXPECT_EQ(parent_shell_surface->GetWidget()->GetNativeWindow(),
            wm::GetTransientParent(
                child3_shell_surface->GetWidget()->GetNativeWindow()));
  parent_shell_surface.reset();
}

TEST_F(ShellSurfaceTest, CommitAndConfigure) {
  // Test that a commit produces an expected configure. A commit to a surface
  // without a buffer attached should produce a configure with zero size. Then
  // once a buffer is attached, a commit should produce a configure
  // corresponding to the size of the buffer.
  auto shell_surface =
      test::ShellSurfaceBuilder().SetNoCommit().BuildShellSurface();

  uint32_t serial = 0;
  gfx::Rect bounds;
  aura::Window::OcclusionState occlusion_state;
  auto configure_callback = base::BindRepeating(
      [](uint32_t* const serial_ptr, gfx::Rect* bounds_ptr,
         aura::Window::OcclusionState* occlusion_state_ptr,
         const gfx::Rect& bounds, chromeos::WindowStateType state_type,
         bool resizing, bool activated, const gfx::Vector2d& origin_offset,
         float raster_scale, aura::Window::OcclusionState occlusion_state,
         std::optional<chromeos::WindowStateType>) {
        *bounds_ptr = bounds;
        *occlusion_state_ptr = occlusion_state;
        return ++(*serial_ptr);
      },
      &serial, &bounds, &occlusion_state);
  shell_surface->set_configure_callback(configure_callback);

  // Receiving a commit without a buffer should result in an initial configure
  // with bounds gfx::Rect(0, 0, 0, 0). Note that although `ShellSurface` does
  // not implement xdg-shell, in xdg-shell spec sending 0x0 bounds is used to
  // hint to a client that the bounds should be determined by the client (i.e.
  // ShellSurface implicitly follows xdg-shell spec here).
  shell_surface->root_surface()->Commit();
  ASSERT_EQ(1u, serial);
  EXPECT_EQ(bounds, gfx::Rect(0, 0, 0, 0));

  // Attaching a buffer and committing should produce a single configure with
  // the size equal to the buffer size.
  constexpr gfx::Size kBufferSize(64, 64);
  auto buffer = test::ExoTestHelper::CreateBuffer(kBufferSize);
  shell_surface->root_surface()->Attach(buffer.get());
  shell_surface->root_surface()->Commit();
  ASSERT_EQ(2u, serial);
  EXPECT_EQ(bounds.size(), kBufferSize);
  // The occlusion state should be visible since there is no other window
  // occluding the surface.
  EXPECT_EQ(occlusion_state, aura::Window::OcclusionState::VISIBLE);
}

TEST_F(ShellSurfaceTest, Maximize) {
  auto shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  auto* surface = shell_surface->root_surface();
  EXPECT_TRUE(shell_surface->IsReady());

  EXPECT_FALSE(HasBackdrop());
  shell_surface->Maximize();
  EXPECT_FALSE(HasBackdrop());
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
  EXPECT_EQ(GetContext()->bounds().width(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().width());
  EXPECT_TRUE(shell_surface->GetWidget()->IsMaximized());

  // Toggle maximize.
  ash::WMEvent maximize_event(ash::WM_EVENT_TOGGLE_MAXIMIZE);
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  ash::WindowState::Get(window)->OnWMEvent(&maximize_event);
  EXPECT_FALSE(shell_surface->GetWidget()->IsMaximized());
  EXPECT_FALSE(HasBackdrop());

  ash::WindowState::Get(window)->OnWMEvent(&maximize_event);
  EXPECT_TRUE(shell_surface->GetWidget()->IsMaximized());
  EXPECT_FALSE(HasBackdrop());
}

TEST_F(ShellSurfaceTest, CanMaximizeResizableWindow) {
  auto shell_surface =
      test::ShellSurfaceBuilder({400, 300}).BuildShellSurface();

  // Make sure we've created a resizable window.
  EXPECT_TRUE(shell_surface->CanResize());

  // Assert: Resizable windows can be maximized.
  EXPECT_TRUE(shell_surface->CanMaximize());
}

TEST_F(ShellSurfaceTest, CannotMaximizeNonResizableWindow) {
  constexpr gfx::Size kBufferSize(400, 300);
  auto shell_surface = test::ShellSurfaceBuilder(kBufferSize)
                           .SetMinimumSize(kBufferSize)
                           .SetMaximumSize(kBufferSize)
                           .BuildShellSurface();

  // Make sure we've created a non-resizable window.
  EXPECT_FALSE(shell_surface->CanResize());

  // Assert: Non-resizable windows cannot be maximized.
  EXPECT_FALSE(shell_surface->CanMaximize());
}

TEST_F(ShellSurfaceTest, MaximizeFromFullscreen) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetMaximumSize(gfx::Size(10, 10))
          .BuildShellSurface();
  // Act: Maximize after fullscreen
  shell_surface->root_surface()->Commit();
  shell_surface->SetFullscreen(true, display::kInvalidDisplayId);
  shell_surface->root_surface()->Commit();
  shell_surface->Maximize();
  shell_surface->root_surface()->Commit();

  // Assert: Window should stay fullscreen.
  EXPECT_TRUE(shell_surface->GetWidget()->IsFullscreen());
}

TEST_F(ShellSurfaceTest, MaximizeExitsFullscreen) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetMaximumSize(gfx::Size(10, 10))
          .BuildShellSurface();

  // Act: Set window property kRestoreOrMaximizeExitsFullscreen
  // then maximize after fullscreen
  shell_surface->root_surface()->Commit();
  shell_surface->GetWidget()->GetNativeWindow()->SetProperty(
      kRestoreOrMaximizeExitsFullscreen, true);
  shell_surface->SetFullscreen(true, display::kInvalidDisplayId);
  shell_surface->root_surface()->Commit();
  shell_surface->Maximize();
  shell_surface->root_surface()->Commit();

  // Assert: Window should exit fullscreen and be maximized.
  EXPECT_TRUE(shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
      kRestoreOrMaximizeExitsFullscreen));
  EXPECT_TRUE(shell_surface->GetWidget()->IsMaximized());
}

TEST_F(ShellSurfaceTest, Minimize) {
  constexpr gfx::Size kBufferSize(256, 256);
  auto shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).SetNoCommit().BuildShellSurface();

  EXPECT_FALSE(shell_surface->IsReady());
  EXPECT_TRUE(shell_surface->CanMinimize());

  // Minimizing can be performed before the surface is committed, but
  // widget creation will be deferred.
  shell_surface->Minimize();
  EXPECT_FALSE(shell_surface->GetWidget());
  EXPECT_FALSE(shell_surface->IsReady());

  // Committing the buffer will create a widget with minimized state.
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "ExoShellSurface-0");
  uint32_t serial = 0;
  chromeos::WindowStateType state[1]{chromeos::WindowStateType::kNormal};
  auto configure_callback = base::BindRepeating(
      [](uint32_t* const serial_ptr, chromeos::WindowStateType* state_ptr,
         const gfx::Rect& bounds, chromeos::WindowStateType state_type,
         bool resizing, bool activated, const gfx::Vector2d& origin_offset,
         float raster_scale, aura::Window::OcclusionState occlusion_state,
         std::optional<chromeos::WindowStateType>) {
        state_ptr[*serial_ptr] = state_type;
        CHECK(*serial_ptr < 2);
        return ++(*serial_ptr);
      },
      &serial, state);
  shell_surface->set_configure_callback(configure_callback);

  shell_surface->root_surface()->Commit();
  EXPECT_TRUE(shell_surface->GetWidget()->IsMinimized());

  // Two configures (initial configure and the state change configure) should be
  // sent with the minimzied state.
  ASSERT_EQ(1u, serial);
  EXPECT_EQ(chromeos::WindowStateType::kMinimized, state[0]);
  shell_surface->set_configure_callback(ShellSurface::ConfigureCallback());

  // Minimized widget should be Shown.
  widget_waiter.WaitIfNeededAndGet();

  EXPECT_TRUE(shell_surface->IsReady());

  shell_surface->Restore();
  EXPECT_FALSE(shell_surface->GetWidget()->IsMinimized());

  auto child_shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).SetNoCommit().BuildShellSurface();
  auto* child_surface = child_shell_surface->root_surface();

  // Transient shell surfaces cannot be minimized.
  child_surface->SetParent(shell_surface->root_surface(), gfx::Point());
  child_surface->Commit();
  EXPECT_FALSE(child_shell_surface->CanMinimize());
}

TEST_F(ShellSurfaceTest, Restore) {
  constexpr gfx::Size kBufferSize(256, 256);
  auto shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();

  EXPECT_FALSE(HasBackdrop());
  // Note: Remove contents to avoid issues with maximize animations in tests.
  shell_surface->Maximize();
  EXPECT_FALSE(HasBackdrop());
  shell_surface->Restore();
  EXPECT_FALSE(HasBackdrop());
  EXPECT_EQ(
      kBufferSize.ToString(),
      shell_surface->GetWidget()->GetWindowBoundsInScreen().size().ToString());
}

TEST_F(ShellSurfaceTest, RestoreFromFullscreen) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetMaximumSize(gfx::Size(10, 10))
          .BuildShellSurface();

  // Act: Restore after fullscreen
  shell_surface->SetFullscreen(true, display::kInvalidDisplayId);
  shell_surface->root_surface()->Commit();
  shell_surface->Restore();
  shell_surface->root_surface()->Commit();

  // Assert: Window should stay fullscreen.
  EXPECT_TRUE(shell_surface->GetWidget()->IsFullscreen());
}

TEST_F(ShellSurfaceTest, RestoreExitsFullscreen) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();

  // Act: Set window property kRestoreOrMaximizeExitsFullscreen
  // then restore after fullscreen
  shell_surface->root_surface()->Commit();
  shell_surface->GetWidget()->GetNativeWindow()->SetProperty(
      kRestoreOrMaximizeExitsFullscreen, true);
  shell_surface->SetFullscreen(true, display::kInvalidDisplayId);
  shell_surface->Restore();
  shell_surface->root_surface()->Commit();

  // Assert: Window should exit fullscreen and be restored.
  EXPECT_TRUE(shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
      kRestoreOrMaximizeExitsFullscreen));
  EXPECT_EQ(gfx::Size(256, 256),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().size());
}

TEST_F(ShellSurfaceTest, HostWindowBoundsUpdatedAfterCommitWidget) {
  constexpr gfx::Point kOrigin(0, 0);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetOrigin(kOrigin)
          .BuildShellSurface();

  shell_surface->root_surface()->SetSurfaceHierarchyContentBoundsForTest(
      gfx::Rect(0, 0, 50, 50));

  // Host Window Bounds size before committing.
  EXPECT_EQ(gfx::Rect(0, 0, 256, 256), shell_surface->host_window()->bounds());
  EXPECT_TRUE(shell_surface->OnPreWidgetCommit());
  shell_surface->CommitWidget();
  // CommitWidget should update the Host Window Bounds.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), shell_surface->host_window()->bounds());
}

TEST_F(ShellSurfaceTest, HostWindowBoundsUpdatedWithNegativeCoordinate) {
  constexpr gfx::Point kOrigin(20, 20);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetOrigin(kOrigin)
          .BuildShellSurface();

  // Set content bounds to negative and larger than surface. This happens when
  // subsurfaces are outside of root surface boundary.
  shell_surface->root_surface()->SetSurfaceHierarchyContentBoundsForTest(
      gfx::Rect(-20, -20, 300, 300));

  // Host Window Bounds size before committing.
  EXPECT_EQ(gfx::Rect(0, 0, 256, 256), shell_surface->host_window()->bounds());
  EXPECT_TRUE(shell_surface->OnPreWidgetCommit());
  shell_surface->CommitWidget();
  // CommitWidget should update the Host Window Bounds.
  EXPECT_EQ(gfx::Rect(-20, -20, 300, 300),
            shell_surface->host_window()->bounds());
  // Root surface origin must be adjusted relative to host window.
  EXPECT_EQ(gfx::Point(20, 20), shell_surface->root_surface_origin_pixel());
}

TEST_F(ShellSurfaceTest, HostWindowIncludesAllSubSurfaces) {
  constexpr gfx::Point kOrigin(20, 20);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetOrigin(kOrigin)
          .BuildShellSurface();

  constexpr gfx::Size kChildBufferSize(32, 32);

  // Add child buffer at the upper-left corner of the root surface.
  auto child_buffer1 = test::ExoTestHelper::CreateBuffer(kChildBufferSize);
  auto child_surface1 = std::make_unique<Surface>();
  child_surface1->Attach(child_buffer1.get());
  auto subsurface1 = std::make_unique<SubSurface>(
      child_surface1.get(), shell_surface->root_surface());
  subsurface1->SetPosition(gfx::PointF(-10, -10));
  child_surface1->Commit();

  // Add child buffer at the bottom-right corner of the root surface.
  auto child_buffer2 = test::ExoTestHelper::CreateBuffer(kChildBufferSize);
  auto child_surface2 = std::make_unique<Surface>();
  child_surface2->Attach(child_buffer2.get());
  auto subsurface2 = std::make_unique<SubSurface>(
      child_surface2.get(), shell_surface->root_surface());
  subsurface2->SetPosition(gfx::PointF(250, 250));
  child_surface2->Commit();

  shell_surface->root_surface()->Commit();

  ASSERT_TRUE(shell_surface->GetWidget());
  shell_surface->SetGeometry(gfx::Rect(0, 0, 256, 256));
  shell_surface->root_surface()->Commit();

  // Host window must be set to include all children subsurfaces.
  EXPECT_EQ(gfx::Rect(-10, -10, 292, 292),
            shell_surface->host_window()->bounds());
  // Root surface origin must be adjusted relative to host window.
  EXPECT_EQ(gfx::Point(10, 10), shell_surface->root_surface_origin_pixel());
}

TEST_F(ShellSurfaceTest, HostWindowIncludesAllSubSurfacesWithScaleFactor) {
  constexpr gfx::Point kOrigin(20, 20);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetOrigin(kOrigin)
          .BuildShellSurface();

  // Set scale.
  constexpr float kScaleFactor = 2.0;
  shell_surface->set_client_submits_surfaces_in_pixel_coordinates(true);
  shell_surface->SetScaleFactor(kScaleFactor);

  constexpr gfx::Size kChildBufferSize(32, 32);

  // Add child buffer at the upper-left corner of the root surface.
  auto child_buffer1 = test::ExoTestHelper::CreateBuffer(kChildBufferSize);
  auto child_surface1 = std::make_unique<Surface>();
  child_surface1->Attach(child_buffer1.get());
  auto subsurface1 = std::make_unique<SubSurface>(
      child_surface1.get(), shell_surface->root_surface());
  subsurface1->SetPosition(gfx::PointF(-10, -10));
  child_surface1->Commit();

  // Add child buffer at the bottom-right corner of the root surface.
  auto child_buffer2 = test::ExoTestHelper::CreateBuffer(kChildBufferSize);
  auto child_surface2 = std::make_unique<Surface>();
  child_surface2->Attach(child_buffer2.get());
  auto subsurface2 = std::make_unique<SubSurface>(
      child_surface2.get(), shell_surface->root_surface());
  subsurface2->SetPosition(gfx::PointF(250, 250));
  child_surface2->Commit();

  shell_surface->root_surface()->Commit();

  ASSERT_TRUE(shell_surface->GetWidget());
  shell_surface->SetGeometry(gfx::Rect(0, 0, 256, 256));
  shell_surface->root_surface()->Commit();

  // Host window must be set to include all children subsurfaces.
  EXPECT_EQ(gfx::Rect(-5, -5, 146, 146),
            shell_surface->host_window()->bounds());
  // Root surface origin must be adjusted relative to host window.
  EXPECT_EQ(gfx::Point(10, 10), shell_surface->root_surface_origin_pixel());
}

TEST_F(ShellSurfaceTest, HostWindowNotIncludeAugmentedChild) {
  constexpr gfx::Point kOrigin(20, 20);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetOrigin(kOrigin)
          .BuildShellSurface();

  constexpr gfx::Size kChildBufferSize(32, 32);

  // Add child buffer at the upper-right corner of the root surface.
  auto child_buffer1 = test::ExoTestHelper::CreateBuffer(kChildBufferSize);
  auto child_surface1 = std::make_unique<Surface>();
  child_surface1->Attach(child_buffer1.get());
  child_surface1->set_is_augmented(true);
  child_surface1->SetClipRect(std::make_optional(gfx::RectF(5, 5, 32, 32)));
  auto subsurface1 = std::make_unique<SubSurface>(
      child_surface1.get(), shell_surface->root_surface());
  subsurface1->SetPosition(gfx::PointF(-10, -10));
  child_surface1->Commit();

  // Add child buffer at the bottom-left corner of the root surface.
  auto child_buffer2 = test::ExoTestHelper::CreateBuffer(kChildBufferSize);
  auto child_surface2 = std::make_unique<Surface>();
  child_surface2->Attach(child_buffer2.get());
  auto subsurface2 = std::make_unique<SubSurface>(
      child_surface2.get(), shell_surface->root_surface());
  subsurface2->SetPosition(gfx::PointF(-10, 250));
  child_surface2->Commit();

  shell_surface->root_surface()->Commit();

  ASSERT_TRUE(shell_surface->GetWidget());
  shell_surface->SetGeometry(gfx::Rect(256, 256));
  shell_surface->root_surface()->Commit();

  // Host window must be set to include all children subsurfaces, but not the
  // clipped area.
  EXPECT_EQ(gfx::Rect(-10, 0, 266, 282),
            shell_surface->host_window()->bounds());
  // Root surface origin must be adjusted relative to host window.
  EXPECT_EQ(gfx::Point(10, 0), shell_surface->root_surface_origin_pixel());
}

TEST_F(ShellSurfaceTest, HostWindowNotIncludeAugmentedChildWithScaleFactor) {
  constexpr gfx::Point kOrigin(20, 20);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetOrigin(kOrigin)
          .BuildShellSurface();

  // Set scale.
  constexpr float kScaleFactor = 2.0;
  shell_surface->set_client_submits_surfaces_in_pixel_coordinates(true);
  shell_surface->SetScaleFactor(kScaleFactor);

  constexpr gfx::Size kChildBufferSize(32, 32);

  // Add child buffer at the upper-right corner of the root surface.
  auto child_buffer1 = test::ExoTestHelper::CreateBuffer(kChildBufferSize);
  auto child_surface1 = std::make_unique<Surface>();
  child_surface1->Attach(child_buffer1.get());
  child_surface1->set_is_augmented(true);
  child_surface1->SetClipRect(std::make_optional(gfx::RectF(5, 5, 32, 32)));
  auto subsurface1 = std::make_unique<SubSurface>(
      child_surface1.get(), shell_surface->root_surface());
  subsurface1->SetPosition(gfx::PointF(-10, -10));
  child_surface1->Commit();

  // Add child buffer at the bottom-left corner of the root surface.
  auto child_buffer2 = test::ExoTestHelper::CreateBuffer(kChildBufferSize);
  auto child_surface2 = std::make_unique<Surface>();
  child_surface2->Attach(child_buffer2.get());
  auto subsurface2 = std::make_unique<SubSurface>(
      child_surface2.get(), shell_surface->root_surface());
  subsurface2->SetPosition(gfx::PointF(-10, 250));
  child_surface2->Commit();

  shell_surface->root_surface()->Commit();

  ASSERT_TRUE(shell_surface->GetWidget());
  shell_surface->SetGeometry(gfx::Rect(0, 0, 256, 256));
  shell_surface->root_surface()->Commit();

  // Host window must be set to include all children subsurfaces, but not the
  // clipped area.
  EXPECT_EQ(gfx::Rect(-5, 0, 133, 141), shell_surface->host_window()->bounds());
  // Root surface origin must be adjusted relative to host window.
  EXPECT_EQ(gfx::Point(10, 0), shell_surface->root_surface_origin_pixel());
}

TEST_F(ShellSurfaceTest, LocalSurfaceIdUpdatedOnHostWindowOriginChanged) {
  constexpr gfx::Point kOrigin(100, 100);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({100, 100})
          .SetOrigin(kOrigin)
          .SetGeometry(gfx::Rect(100, 100))
          .BuildShellSurface();

  auto* root_surface = shell_surface->root_surface();

  auto child_buffer = test::ExoTestHelper::CreateBuffer(gfx::Size(200, 200));
  auto child_surface = std::make_unique<Surface>();
  child_surface->Attach(child_buffer.get());
  auto subsurface =
      std::make_unique<SubSurface>(child_surface.get(), root_surface);
  subsurface->SetPosition(gfx::PointF(-50, -50));

  child_surface->Commit();
  root_surface->Commit();
  EXPECT_EQ(gfx::Rect(-50, -50, 200, 200),
            shell_surface->host_window()->bounds());

  // Store the current local surface id.
  const viz::LocalSurfaceId old_id =
      shell_surface->GetSurfaceId().local_surface_id();

  // If nothing is changed, no need to update local surface id.
  child_surface->Commit();
  root_surface->Commit();
  EXPECT_EQ(shell_surface->GetSurfaceId().local_surface_id(), old_id);

  // If the host window origin is updated, need to update local surface id.
  subsurface->SetPosition(gfx::PointF(-25, -25));
  child_surface->Commit();
  root_surface->Commit();
  EXPECT_EQ(gfx::Rect(-25, -25, 200, 200),
            shell_surface->host_window()->bounds());
  EXPECT_TRUE(
      shell_surface->GetSurfaceId().local_surface_id().IsNewerThan(old_id));

  EXPECT_EQ(gfx::Vector2dF(),
            shell_surface->host_window()->layer()->GetSubpixelOffset());
}

TEST_F(ShellSurfaceTest,
       LocalSurfaceIdUpdatedOnHostWindowOriginChangedWithScaleFactor) {
  constexpr gfx::Point kOrigin(100, 100);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({100, 100})
          .SetOrigin(kOrigin)
          .SetGeometry(gfx::Rect(100, 100))
          .BuildShellSurface();

  // Set scale.
  constexpr float kScaleFactor = 2.0;
  shell_surface->set_client_submits_surfaces_in_pixel_coordinates(true);
  shell_surface->SetScaleFactor(kScaleFactor);

  auto* root_surface = shell_surface->root_surface();

  auto child_buffer = test::ExoTestHelper::CreateBuffer(gfx::Size(200, 200));
  auto child_surface = std::make_unique<Surface>();
  child_surface->Attach(child_buffer.get());
  auto subsurface =
      std::make_unique<SubSurface>(child_surface.get(), root_surface);
  subsurface->SetPosition(gfx::PointF(-50, -50));

  child_surface->Commit();
  root_surface->Commit();
  EXPECT_EQ(gfx::Rect(-25, -25, 100, 100),
            shell_surface->host_window()->bounds());

  // Store the current local surface id.
  const viz::LocalSurfaceId old_id =
      shell_surface->GetSurfaceId().local_surface_id();

  // If nothing is changed, no need to update local surface id.
  child_surface->Commit();
  root_surface->Commit();
  EXPECT_EQ(shell_surface->GetSurfaceId().local_surface_id(), old_id);

  // If the host window origin is updated, need to update local surface id.
  subsurface->SetPosition(gfx::PointF(-25, -25));
  child_surface->Commit();
  root_surface->Commit();
  EXPECT_EQ(gfx::Rect(-12, -12, 100, 100),
            shell_surface->host_window()->bounds());
  EXPECT_TRUE(
      shell_surface->GetSurfaceId().local_surface_id().IsNewerThan(old_id));

  EXPECT_EQ(gfx::Vector2dF(-0.5, -0.5),
            shell_surface->host_window()->layer()->GetSubpixelOffset());
}

TEST_F(ShellSurfaceTest,
       LocalSurfaceIdNotUpdatedOnSurfaceOutOfWindowButClipped) {
  constexpr gfx::Point kOrigin(100, 100);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({100, 100})
          .SetOrigin(kOrigin)
          .SetGeometry(gfx::Rect(100, 100))
          .BuildShellSurface();

  auto* root_surface = shell_surface->root_surface();

  auto child_buffer = test::ExoTestHelper::CreateBuffer(gfx::Size(200, 200));
  auto child_surface = std::make_unique<Surface>();
  child_surface->Attach(child_buffer.get());
  child_surface->set_is_augmented(true);
  child_surface->SetClipRect(std::make_optional(gfx::RectF(50, 50, 100, 100)));
  auto subsurface =
      std::make_unique<SubSurface>(child_surface.get(), root_surface);
  subsurface->SetPosition(gfx::PointF(-50, -50));

  child_surface->Commit();
  root_surface->Commit();
  EXPECT_EQ(gfx::Rect(100, 100), shell_surface->host_window()->bounds());

  // Store the current local surface id.
  const viz::LocalSurfaceId old_id =
      shell_surface->GetSurfaceId().local_surface_id();

  // If nothing is changed, no need to update local surface id.
  child_surface->Commit();
  root_surface->Commit();
  EXPECT_EQ(shell_surface->GetSurfaceId().local_surface_id(), old_id);

  // If the surface is moving around the out of window while it's clipped, we do
  // not allocate local surface id.
  child_surface->SetClipRect(std::make_optional(gfx::RectF(25, 25, 100, 100)));
  subsurface->SetPosition(gfx::PointF(-25, -25));
  child_surface->Commit();
  root_surface->Commit();
  EXPECT_EQ(gfx::Rect(100, 100), shell_surface->host_window()->bounds());
  EXPECT_EQ(old_id, shell_surface->GetSurfaceId().local_surface_id());

  EXPECT_EQ(gfx::Vector2dF(),
            shell_surface->host_window()->layer()->GetSubpixelOffset());
}

TEST_F(ShellSurfaceTest, EventTargetWithNegativeHostWindowOrigin) {
  constexpr gfx::Point kOrigin(20, 20);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetOrigin(kOrigin)
          .SetGeometry(gfx::Rect(256, 256))
          .SetInputRegion(gfx::Rect(256, 256))
          .SetFrame(SurfaceFrameType::SHADOW)
          .BuildShellSurface();

  auto* root_surface = shell_surface->root_surface();

  // Add child buffer at the upper-left corner of the root surface with empty
  // input region.
  auto* child_surface1 = test::ShellSurfaceBuilder::AddChildSurface(
      root_surface, gfx::Rect(-10, -10, 32, 32));
  child_surface1->SetInputRegion(cc::Region());
  // Add child buffer at the bottom-right corner of the root surface with empty
  // input region.
  auto* child_surface2 = test::ShellSurfaceBuilder::AddChildSurface(
      root_surface, gfx::Rect(250, 250, 32, 32));
  child_surface2->SetInputRegion(cc::Region());

  child_surface1->Commit();
  child_surface2->Commit();
  root_surface->Commit();

  ASSERT_TRUE(shell_surface->GetWidget());
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  aura::Window* root_window = window->GetRootWindow();
  ui::EventTargeter* targeter =
      root_window->GetHost()->dispatcher()->GetDefaultEventTargeter();

  {
    // Mouse is in the middle of the root surface.
    ui::MouseEvent event(ui::EventType::kMouseMoved, gfx::Point(120, 120),
                         gfx::Point(120, 120), ui::EventTimeForNow(),
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_EQ(root_surface->window(),
              targeter->FindTargetForEvent(root_window, &event));
  }

  {
    // Mouse is on upper-left of the root surface.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(21, 21),
                         gfx::Point(21, 21), ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    EXPECT_EQ(root_surface->window(),
              targeter->FindTargetForEvent(root_window, &event));
  }

  {
    // Mouse is on bottom-right of the root surface.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(275, 275),
                         gfx::Point(275, 275), ui::EventTimeForNow(),
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_EQ(root_surface->window(),
              targeter->FindTargetForEvent(root_window, &event));
  }

  {
    // Mouse is outside of the root surface and host window.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(300, 300),
                         gfx::Point(300, 300), ui::EventTimeForNow(),
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_FALSE(window->Contains(static_cast<aura::Window*>(
        targeter->FindTargetForEvent(root_window, &event))));
  }

  {
    // Mouse is on the left side of the root surface but inside host window.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(19, 100),
                         gfx::Point(19, 100), ui::EventTimeForNow(),
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_FALSE(window->Contains(static_cast<aura::Window*>(
        targeter->FindTargetForEvent(root_window, &event))));
  }

  {
    // Mouse is on the right side of the root surface but inside host window.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(277, 100),
                         gfx::Point(277, 100), ui::EventTimeForNow(),
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_FALSE(window->Contains(static_cast<aura::Window*>(
        targeter->FindTargetForEvent(root_window, &event))));
  }

  {
    // Mouse is above the root surface but inside host window.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(100, 19),
                         gfx::Point(100, 19), ui::EventTimeForNow(),
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_FALSE(window->Contains(static_cast<aura::Window*>(
        targeter->FindTargetForEvent(root_window, &event))));
  }

  {
    // Mouse is below the root surface but inside host window.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(100, 277),
                         gfx::Point(100, 277), ui::EventTimeForNow(),
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_FALSE(window->Contains(static_cast<aura::Window*>(
        targeter->FindTargetForEvent(root_window, &event))));
  }

  {
    // Mouse is on `child_surface1` but not on the root surface.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(19, 19),
                         gfx::Point(19, 19), ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    EXPECT_FALSE(window->Contains(static_cast<aura::Window*>(
        targeter->FindTargetForEvent(root_window, &event))));
  }

  {
    // Mouse is on `child_surface2` but not on the root surface.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(277, 277),
                         gfx::Point(277, 277), ui::EventTimeForNow(),
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_FALSE(window->Contains(static_cast<aura::Window*>(
        targeter->FindTargetForEvent(root_window, &event))));
  }
}

TEST_F(ShellSurfaceTest,
       EventTargetWithNegativeHostWindowOriginWithScaleFactor) {
  constexpr gfx::Point kOrigin(20, 20);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetOrigin(kOrigin)
          .SetGeometry(gfx::Rect(256, 256))
          .SetInputRegion(gfx::Rect(256, 256))
          .SetFrame(SurfaceFrameType::SHADOW)
          .BuildShellSurface();

  // Set scale.
  constexpr float kScaleFactor = 2.0;
  shell_surface->set_client_submits_surfaces_in_pixel_coordinates(true);
  shell_surface->SetScaleFactor(kScaleFactor);

  auto* root_surface = shell_surface->root_surface();

  // Add child buffer at the upper-left corner of the root surface with empty
  // input region.
  auto* child_surface1 = test::ShellSurfaceBuilder::AddChildSurface(
      root_surface, gfx::Rect(-10, -10, 32, 32));
  child_surface1->SetInputRegion(cc::Region());
  // Add child buffer at the bottom-right corner of the root surface with empty
  // input region.
  auto* child_surface2 = test::ShellSurfaceBuilder::AddChildSurface(
      root_surface, gfx::Rect(250, 250, 32, 32));
  child_surface2->SetInputRegion(cc::Region());

  child_surface1->Commit();
  child_surface2->Commit();
  root_surface->Commit();

  ASSERT_TRUE(shell_surface->GetWidget());
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  aura::Window* root_window = window->GetRootWindow();
  ui::EventTargeter* targeter =
      root_window->GetHost()->dispatcher()->GetDefaultEventTargeter();

  {
    // Mouse is in the middle of the root surface.
    ui::MouseEvent event(ui::EventType::kMouseMoved, gfx::Point(80, 80),
                         gfx::Point(80, 80), ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    EXPECT_EQ(root_surface->window(),
              targeter->FindTargetForEvent(root_window, &event));
  }

  {
    // Mouse is on upper-left of the root surface.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(21, 21),
                         gfx::Point(21, 21), ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    EXPECT_EQ(root_surface->window(),
              targeter->FindTargetForEvent(root_window, &event));
  }

  {
    // Mouse is on bottom-right of the root surface.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(147, 147),
                         gfx::Point(147, 147), ui::EventTimeForNow(),
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_EQ(root_surface->window(),
              targeter->FindTargetForEvent(root_window, &event));
  }

  {
    // Mouse is outside of the root surface and host window.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(200, 200),
                         gfx::Point(200, 200), ui::EventTimeForNow(),
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_FALSE(window->Contains(static_cast<aura::Window*>(
        targeter->FindTargetForEvent(root_window, &event))));
  }

  {
    // Mouse is on the left side of the root surface but inside host window.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(19, 100),
                         gfx::Point(19, 100), ui::EventTimeForNow(),
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_FALSE(window->Contains(static_cast<aura::Window*>(
        targeter->FindTargetForEvent(root_window, &event))));
  }

  {
    // Mouse is on the right side of the root surface but inside host window.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(149, 100),
                         gfx::Point(149, 100), ui::EventTimeForNow(),
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_FALSE(window->Contains(static_cast<aura::Window*>(
        targeter->FindTargetForEvent(root_window, &event))));
  }

  {
    // Mouse is above the root surface but inside host window.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(100, 19),
                         gfx::Point(100, 19), ui::EventTimeForNow(),
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_FALSE(window->Contains(static_cast<aura::Window*>(
        targeter->FindTargetForEvent(root_window, &event))));
  }

  {
    // Mouse is below the root surface but inside host window.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(100, 149),
                         gfx::Point(100, 149), ui::EventTimeForNow(),
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_FALSE(window->Contains(static_cast<aura::Window*>(
        targeter->FindTargetForEvent(root_window, &event))));
  }

  {
    // Mouse is on `child_surface1` but not on the root surface.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(19, 19),
                         gfx::Point(19, 19), ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    EXPECT_FALSE(window->Contains(static_cast<aura::Window*>(
        targeter->FindTargetForEvent(root_window, &event))));
  }

  {
    // Mouse is on `child_surface2` but not on the root surface.
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(149, 149),
                         gfx::Point(149, 149), ui::EventTimeForNow(),
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_FALSE(window->Contains(static_cast<aura::Window*>(
        targeter->FindTargetForEvent(root_window, &event))));
  }
}

TEST_F(ShellSurfaceTest, SetFullscreen) {
  auto shell_surface =
      test::ShellSurfaceBuilder({256, 256}).SetNoCommit().BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  shell_surface->SetFullscreen(true, display::kInvalidDisplayId);
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
  EXPECT_EQ(GetContext()->bounds().ToString(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().ToString());
  shell_surface->SetFullscreen(false, display::kInvalidDisplayId);
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
  EXPECT_NE(GetContext()->bounds().ToString(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().ToString());
}

TEST_F(ShellSurfaceTest, PreWidgetUnfullscreen) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256}).SetNoCommit().BuildShellSurface();
  shell_surface->Maximize();
  shell_surface->SetFullscreen(false, display::kInvalidDisplayId);
  EXPECT_EQ(shell_surface->GetWidget(), nullptr);
  shell_surface->root_surface()->Commit();
  EXPECT_TRUE(shell_surface->GetWidget()->IsMaximized());
}

TEST_F(ShellSurfaceTest, PreWidgetMaximizeFromFullscreen) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetNoCommit()
          .SetMaximumSize(gfx::Size(10, 10))
          .BuildShellSurface();
  // Fullscreen -> Maximize for non Lacros surfaces should stay fullscreen
  shell_surface->SetFullscreen(true, display::kInvalidDisplayId);
  shell_surface->Maximize();
  EXPECT_EQ(shell_surface->GetWidget(), nullptr);
  shell_surface->root_surface()->Commit();
  EXPECT_TRUE(shell_surface->GetWidget()->IsFullscreen());
}

TEST_F(ShellSurfaceTest, SetTitle) {
  auto shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();

  shell_surface->SetTitle(std::u16string(u"test"));

  // NativeWindow's title is used within the overview mode, so it should
  // have the specified title.
  EXPECT_EQ(u"test", shell_surface->GetWidget()->GetNativeWindow()->GetTitle());
  // The titlebar shouldn't show the title.
  EXPECT_FALSE(
      shell_surface->GetWidget()->widget_delegate()->ShouldShowWindowTitle());
}

TEST_F(ShellSurfaceTest, SetApplicationId) {
  auto shell_surface =
      test::ShellSurfaceBuilder({64, 64}).SetNoCommit().BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  EXPECT_FALSE(shell_surface->GetWidget());
  shell_surface->SetApplicationId("pre-widget-id");

  surface->Commit();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ("pre-widget-id", *GetShellApplicationId(window));
  shell_surface->SetApplicationId("test");
  EXPECT_EQ("test", *GetShellApplicationId(window));
  EXPECT_FALSE(ash::WindowState::Get(window)->allow_set_bounds_direct());

  shell_surface->SetApplicationId(nullptr);
  EXPECT_EQ(nullptr, GetShellApplicationId(window));
}

TEST_F(ShellSurfaceTest, ActivationPermissionLegacy) {
  auto shell_surface = test::ShellSurfaceBuilder({64, 64}).BuildShellSurface();

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  ASSERT_TRUE(window);

  // No permission granted so can't activate.
  EXPECT_FALSE(HasPermissionToActivate(window));

  // Can grant permission.
  GrantPermissionToActivate(window, base::Days(1));
  exo::Permission* permission = window->GetProperty(kPermissionKey);
  EXPECT_TRUE(permission->Check(Permission::Capability::kActivate));
  EXPECT_TRUE(HasPermissionToActivate(window));

  // Can revoke permission.
  RevokePermissionToActivate(window);
  EXPECT_FALSE(HasPermissionToActivate(window));

  // Can grant permission again.
  GrantPermissionToActivate(window, base::Days(2));
  exo::Permission* permission2 = window->GetProperty(kPermissionKey);
  EXPECT_TRUE(permission2->Check(Permission::Capability::kActivate));
  EXPECT_TRUE(HasPermissionToActivate(window));
}

TEST_F(ShellSurfaceTest, WidgetActivationLegacy) {
  constexpr gfx::Size kBufferSize(64, 64);
  auto security_delegate = std::make_unique<test::TestSecurityDelegate>();

  auto shell_surface1 = test::ShellSurfaceBuilder(kBufferSize)
                            .SetSecurityDelegate(security_delegate.get())
                            .BuildShellSurface();
  auto* surface1 = shell_surface1->root_surface();

  // The window is active.
  views::Widget* widget1 = shell_surface1->GetWidget();
  EXPECT_TRUE(widget1->IsActive());

  // Create a second window.
  auto shell_surface2 = test::ShellSurfaceBuilder(kBufferSize)
                            .SetSecurityDelegate(security_delegate.get())
                            .BuildShellSurface();
  auto* surface2 = shell_surface2->root_surface();

  // Now the second window is active.
  views::Widget* widget2 = shell_surface2->GetWidget();
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_TRUE(widget2->IsActive());

  // Grant permission to activate the first window.
  GrantPermissionToActivate(widget1->GetNativeWindow(), base::Days(1));

  // The first window can activate itself.
  surface1->RequestActivation();
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_FALSE(widget2->IsActive());

  // The second window cannot activate itself.
  surface2->RequestActivation();
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_FALSE(widget2->IsActive());
}

TEST_F(ShellSurfaceTest, WidgetActivation) {
  test::MockSecurityDelegate security_delegate;
  constexpr gfx::Size kBufferSize(64, 64);
  std::unique_ptr<ShellSurface> shell_surface1 =
      test::ShellSurfaceBuilder(kBufferSize)
          .SetSecurityDelegate(&security_delegate)
          .BuildShellSurface();

  // The window is active.
  views::Widget* widget1 = shell_surface1->GetWidget();
  EXPECT_TRUE(widget1->IsActive());

  // Create a second window.
  std::unique_ptr<ShellSurface> shell_surface2 =
      test::ShellSurfaceBuilder(kBufferSize)
          .SetSecurityDelegate(&security_delegate)
          .BuildShellSurface();

  // Now the second window is active.
  views::Widget* widget2 = shell_surface2->GetWidget();
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_TRUE(widget2->IsActive());

  // The first window can activate itself.
  EXPECT_CALL(security_delegate, CanSelfActivate(widget1->GetNativeWindow()))
      .WillOnce(testing::Return(true));
  shell_surface1->surface_for_testing()->RequestActivation();
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_FALSE(widget2->IsActive());

  // The second window cannot activate itself.
  EXPECT_CALL(security_delegate, CanSelfActivate(widget2->GetNativeWindow()))
      .WillOnce(testing::Return(false));
  shell_surface2->surface_for_testing()->RequestActivation();
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_FALSE(widget2->IsActive());
}

TEST_F(ShellSurfaceTest, EmulateOverrideRedirect) {
  constexpr gfx::Size kBufferSize(64, 64);
  auto buffer = test::ExoTestHelper::CreateBuffer(kBufferSize);
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  EXPECT_FALSE(shell_surface->GetWidget());
  surface->Attach(buffer.get());
  surface->Commit();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_FALSE(ash::WindowState::Get(window)->allow_set_bounds_direct());

  // Only surface with no app id with parent surface is considered
  // override redirect.
  std::unique_ptr<Surface> child_surface(new Surface);
  std::unique_ptr<ShellSurface> child_shell_surface(
      new ShellSurface(child_surface.get()));

  child_surface->SetParent(surface.get(), gfx::Point());
  child_surface->Attach(buffer.get());
  child_surface->Commit();
  aura::Window* child_window =
      child_shell_surface->GetWidget()->GetNativeWindow();

  // The window will not have a window state, thus will no be managed by window
  // manager.
  EXPECT_TRUE(ash::WindowState::Get(child_window)->allow_set_bounds_direct());
  EXPECT_EQ(ash::kShellWindowId_ShelfBubbleContainer,
            child_window->parent()->GetId());

  // NONE/SHADOW frame type should work on override redirect.
  child_surface->SetFrame(SurfaceFrameType::SHADOW);
  child_surface->Commit();
  EXPECT_EQ(wm::kShadowElevationMenuOrTooltip,
            wm::GetShadowElevationConvertDefault(child_window));

  child_surface->SetFrame(SurfaceFrameType::NONE);
  child_surface->Commit();
  EXPECT_EQ(wm::kShadowElevationNone,
            wm::GetShadowElevationConvertDefault(child_window));
}

TEST_F(ShellSurfaceTest, SetStartupId) {
  auto shell_surface =
      test::ShellSurfaceBuilder({64, 64}).SetNoCommit().BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  EXPECT_FALSE(shell_surface->GetWidget());
  shell_surface->SetStartupId("pre-widget-id");

  surface->Commit();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ("pre-widget-id", *GetShellStartupId(window));
  shell_surface->SetStartupId("test");
  EXPECT_EQ("test", *GetShellStartupId(window));

  shell_surface->SetStartupId(nullptr);
  EXPECT_EQ(nullptr, GetShellStartupId(window));
}

TEST_F(ShellSurfaceTest, AckRotateFocus) {
  std::unique_ptr<ShellSurface> surface1 =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();

  uint32_t serial = 0;

  auto dummy_cb = base::BindLambdaForTesting(
      [&serial](ash::FocusCycler::Direction, bool) { return serial; });

  views::View* v1 = new views::View();
  v1->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  surface1->AddChildView(v1);
  surface1->set_rotate_focus_callback(dummy_cb);

  std::unique_ptr<ShellSurface> surface2 =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  views::View* v2 = new views::View();
  v2->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  surface2->AddChildView(v2);
  surface2->set_rotate_focus_callback(dummy_cb);

  std::unique_ptr<ShellSurface> surface3 =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  views::View* v3 = new views::View();
  v3->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  surface3->AddChildView(v3);
  surface3->set_rotate_focus_callback(dummy_cb);

  ash::Shell::Get()->focus_cycler()->AddWidget(surface1->GetWidget());
  ash::Shell::Get()->focus_cycler()->AddWidget(surface2->GetWidget());
  ash::Shell::Get()->focus_cycler()->AddWidget(surface3->GetWidget());

  // We will do most of our testing with surface2 because it is in the middle.
  // This will allow us to easily test directional logic.
  ash::Shell::Get()->focus_cycler()->FocusWidget(surface2->GetWidget());
  ASSERT_TRUE(surface2->GetWidget()->IsActive());

  // Test handled. This should result in no rotation.
  surface2->RotatePaneFocusFromView(v2, true, false);
  surface2->AckRotateFocus(serial++, true);
  ASSERT_TRUE(surface2->GetWidget()->IsActive());

  surface2->RotatePaneFocusFromView(v2, true, false);
  surface2->AckRotateFocus(serial++, true);
  ASSERT_TRUE(surface2->GetWidget()->IsActive());

  // Now test unhandled in the forward direction. The next widget should be
  // focused.
  surface2->RotatePaneFocusFromView(v2, true, false);
  surface2->AckRotateFocus(serial++, false);
  ASSERT_TRUE(surface3->GetWidget()->IsActive());

  // Reset
  ash::Shell::Get()->focus_cycler()->FocusWidget(surface2->GetWidget());
  ASSERT_TRUE(surface2->GetWidget()->IsActive());

  // Now test unhandled in the forward direction. The next widget should be
  // focused.
  surface2->RotatePaneFocusFromView(v2, false, false);
  surface2->AckRotateFocus(serial++, false);
  ASSERT_TRUE(surface1->GetWidget()->IsActive());
}

TEST_F(ShellSurfaceTest, RotatePaneFocusFromView) {
  using ::testing::Return;

  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  base::MockRepeatingCallback<uint32_t(ash::FocusCycler::Direction, bool)> cb;
  shell_surface->set_rotate_focus_callback(cb.Get());

  auto serial = 0;

  EXPECT_CALL(cb, Run(ash::FocusCycler::FORWARD, true))
      .WillOnce(Return(serial++));
  auto rotated = shell_surface->RotatePaneFocusFromView(nullptr, true, true);
  // Async operations always return successful rotation immediately.
  EXPECT_TRUE(rotated);

  EXPECT_CALL(cb, Run(ash::FocusCycler::BACKWARD, false))
      .WillOnce(Return(serial++));
  rotated = shell_surface->RotatePaneFocusFromView(nullptr, false, false);
  EXPECT_TRUE(rotated);
}

TEST_F(ShellSurfaceTest, RotatePaneFocusFromView_NoCallback) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();

  auto rotated = shell_surface->RotatePaneFocusFromView(nullptr, true, true);
  // No focusable view for the shell surface. This should result in a
  // non-rotation using the base rotation logic.
  EXPECT_FALSE(rotated);
}

TEST_F(ShellSurfaceTest, StartMove) {
  auto shell_surface = test::ShellSurfaceBuilder({64, 64}).BuildShellSurface();

  ASSERT_TRUE(shell_surface->GetWidget());

  aura::Env::GetInstance()->set_mouse_button_flags(ui::EF_LEFT_MOUSE_BUTTON);
  // The interactive move should end when surface is destroyed.
  ASSERT_TRUE(shell_surface->StartMove());

  // Test that destroying the shell surface before move ends is OK.
  shell_surface.reset();
}

TEST_F(ShellSurfaceTest, StartResize) {
  constexpr gfx::Size kBufferSize(64, 64);
  auto buffer = test::ExoTestHelper::CreateBuffer(kBufferSize);
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  // Map shell surface.
  surface->Attach(buffer.get());
  surface->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());

  aura::Env::GetInstance()->set_mouse_button_flags(ui::EF_LEFT_MOUSE_BUTTON);
  // The interactive resize should end when surface is destroyed.
  ASSERT_TRUE(shell_surface->StartResize(HTBOTTOMRIGHT));

  // Test that destroying the surface before resize ends is OK.
  surface.reset();
}

TEST_F(ShellSurfaceTest, StartResizeAndDestroyShell) {
  auto shell_surface =
      test::ShellSurfaceBuilder({64, 64}).SetNoCommit().BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  uint32_t serial = 0;
  auto configure_callback = base::BindRepeating(
      [](uint32_t* const serial_ptr, const gfx::Rect& bounds,
         chromeos::WindowStateType state_type, bool resizing, bool activated,
         const gfx::Vector2d& origin_offset, float raster_scale,
         aura::Window::OcclusionState occlusion_state,
         std::optional<chromeos::WindowStateType>) { return ++(*serial_ptr); },
      &serial);

  // Map shell surface.
  shell_surface->set_configure_callback(configure_callback);

  surface->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());

  aura::Env::GetInstance()->set_mouse_button_flags(ui::EF_LEFT_MOUSE_BUTTON);
  // The interactive resize should end when surface is destroyed.
  ASSERT_TRUE(shell_surface->StartResize(HTBOTTOMRIGHT));

  // Go through configure/commit stage to update the resize component.
  shell_surface->AcknowledgeConfigure(serial);
  surface->Commit();

  shell_surface->set_configure_callback(base::BindRepeating(
      [](const gfx::Rect& bounds, chromeos::WindowStateType state_type,
         bool resizing, bool activated, const gfx::Vector2d& origin_offset,
         float raster_scale, aura::Window::OcclusionState occlusion_state,
         std::optional<chromeos::WindowStateType>) {
        ADD_FAILURE() << "Configure Should not be called";
        return uint32_t{0};
      }));

  // Test that destroying the surface before resize ends is OK.
  shell_surface.reset();
}

TEST_F(ShellSurfaceTest, SetGeometry) {
  constexpr gfx::Size kBufferSize(64, 64);
  gfx::Rect geometry(16, 16, 32, 32);
  auto shell_surface = test::ShellSurfaceBuilder(kBufferSize)
                           .SetGeometry(geometry)
                           .BuildShellSurface();

  EXPECT_EQ(
      geometry.size().ToString(),
      shell_surface->GetWidget()->GetWindowBoundsInScreen().size().ToString());
  EXPECT_EQ(gfx::Rect(gfx::Point() - geometry.OffsetFromOrigin(), kBufferSize)
                .ToString(),
            shell_surface->host_window()->bounds().ToString());
}

TEST_F(ShellSurfaceTest, SetMinimumSize) {
  constexpr gfx::Size kBufferSize(64, 64);
  auto shell_surface = test::ShellSurfaceBuilder(kBufferSize)
                           .SetFrameColors(SK_ColorWHITE, SK_ColorWHITE)
                           .BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  constexpr gfx::Size kSizes[] = {{50, 50}, {100, 50}};
  for (const gfx::Size size : kSizes) {
    SCOPED_TRACE(
        base::StringPrintf("MinSize=%dx%d", size.width(), size.height()));
    ConfigureData config_data;
    shell_surface->set_configure_callback(
        base::BindRepeating(&Configure, base::Unretained(&config_data)));

    shell_surface->SetMinimumSize(size);
    surface->Commit();
    EXPECT_EQ(size, shell_surface->GetMinimumSize());
    EXPECT_EQ(size, shell_surface->GetWidget()->GetMinimumSize());
    EXPECT_EQ(size, shell_surface->GetWidget()
                        ->GetNativeWindow()
                        ->delegate()
                        ->GetMinimumSize());
    gfx::Size expected_size(kBufferSize);
    expected_size.set_width(std::max(kBufferSize.width(), size.width()));
    EXPECT_EQ(expected_size,
              shell_surface->GetWidget()->GetWindowBoundsInScreen().size());
    if (kBufferSize.width() > size.width()) {
      EXPECT_TRUE(config_data.suggested_bounds.IsEmpty());
    } else {
      EXPECT_EQ(expected_size, config_data.suggested_bounds.size());
    }
  }
  // Reset configure callback because config_data is out of scope.
  shell_surface->set_configure_callback(base::NullCallback());
  // With frame.
  surface->SetFrame(SurfaceFrameType::NORMAL);
  for (const gfx::Size size : kSizes) {
    SCOPED_TRACE(base::StringPrintf("MinSize=%dx%d with frame", size.width(),
                                    size.height()));
    ConfigureData config_data;
    shell_surface->set_configure_callback(
        base::BindRepeating(&Configure, base::Unretained(&config_data)));

    const gfx::Size size_with_frame(size.width(), size.height() + 32);
    shell_surface->SetMinimumSize(size);
    surface->Commit();
    EXPECT_EQ(size, shell_surface->GetMinimumSize());
    EXPECT_EQ(size_with_frame, shell_surface->GetWidget()->GetMinimumSize());
    EXPECT_EQ(size_with_frame, shell_surface->GetWidget()
                                   ->GetNativeWindow()
                                   ->delegate()
                                   ->GetMinimumSize());
    gfx::Size expected_size(kBufferSize);
    expected_size.set_width(std::max(kBufferSize.width(), size.width()));
    if (kBufferSize.width() > size.width()) {
      EXPECT_TRUE(config_data.suggested_bounds.IsEmpty());
    } else {
      EXPECT_EQ(expected_size, config_data.suggested_bounds.size());
    }
    shell_surface->set_configure_callback(base::NullCallback());
  }
}

TEST_F(ShellSurfaceTest, SetMinimumSizeTooLargeAndTranform) {
  auto* screen = display::Screen::GetScreen();
  auto fullscreen_bounds = screen->GetPrimaryDisplay().bounds();
  auto work_area_bounds = screen->GetPrimaryDisplay().work_area();

  auto shell_surface = test::ShellSurfaceBuilder({64, 64})
                           .SetMinimumSize(fullscreen_bounds.size())
                           .SetMaximumSize(fullscreen_bounds.size())
                           .SetBounds(work_area_bounds)
                           .BuildShellSurface();

  auto* surface = shell_surface->root_surface();
  auto* widget = shell_surface->GetWidget();

  EXPECT_EQ(work_area_bounds, widget->GetWindowBoundsInScreen());

  widget->GetNativeWindow()->SetTransform(
      gfx::Transform::Affine(1, 1, 1, 1, 10, 10));

  // Updating the buffer with expected (work area) size should not
  // update the widget's bounds even when the transform is applied.
  auto buffer = test::ExoTestHelper::CreateBuffer(work_area_bounds.size());
  surface->Attach(buffer.get());
  surface->Commit();
  widget->GetNativeWindow()->SetTransform(gfx::Transform());

  EXPECT_EQ(work_area_bounds, widget->GetWindowBoundsInScreen());
}

TEST_F(ShellSurfaceTest, SetMaximumSize) {
  constexpr gfx::Size kBufferSize(256, 256);
  auto shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  constexpr gfx::Size kSizes[] = {{300, 300}, {200, 300}};
  for (const gfx::Size size : kSizes) {
    SCOPED_TRACE(
        base::StringPrintf("MaxSize=%dx%d", size.width(), size.height()));
    ConfigureData config_data;
    shell_surface->set_configure_callback(
        base::BindRepeating(&Configure, base::Unretained(&config_data)));

    shell_surface->SetMaximumSize(size);
    surface->Commit();
    EXPECT_EQ(size, shell_surface->GetMaximumSize());
    gfx::Size expected_size(kBufferSize);
    expected_size.set_width(std::min(size.width(), kBufferSize.width()));
    EXPECT_EQ(expected_size,
              shell_surface->GetWidget()->GetWindowBoundsInScreen().size());

    if (kBufferSize.width() < size.width()) {
      EXPECT_TRUE(config_data.suggested_bounds.IsEmpty());
    } else {
      EXPECT_EQ(expected_size, config_data.suggested_bounds.size());
    }
  }
}

TEST_F(ShellSurfaceTest, MinimumSizeAlwaysEqualOrSmallerThanMaximumSize) {
  constexpr gfx::Size kBufferSize(256, 256);
  auto shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  constexpr gfx::Size kMinSize = {300, 300};
  constexpr gfx::Size kMaxSize = {100, 100};
  ConfigureData config_data;
  shell_surface->set_configure_callback(
      base::BindRepeating(&Configure, base::Unretained(&config_data)));

  shell_surface->SetMinimumSize(kMinSize);
  shell_surface->SetMaximumSize(kMaxSize);
  surface->Commit();

  // The maximum size smaller than the minimum size is ignored and the minimum
  // size is returned by GetMaximumSize() instead.
  EXPECT_EQ(kMinSize, shell_surface->GetMaximumSize());
  EXPECT_EQ(kMinSize, shell_surface->GetMinimumSize());

  // Reset minimum size
  shell_surface->SetMinimumSize(gfx::Size(0, 0));
  surface->Commit();
  // Previously ignored maximum size is restored automatically because it's
  // stored in |pending_maximum_size_|.
  EXPECT_EQ(kMaxSize, shell_surface->GetMaximumSize());
}

void PreClose(int* pre_close_count, int* close_count) {
  EXPECT_EQ(*pre_close_count, *close_count);
  (*pre_close_count)++;
}

void Close(int* pre_close_count, int* close_count) {
  (*close_count)++;
  EXPECT_EQ(*pre_close_count, *close_count);
}

TEST_F(ShellSurfaceTest, CloseCallback) {
  auto shell_surface =
      test::ShellSurfaceBuilder({64, 64}).SetNoCommit().BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  int pre_close_call_count = 0;
  int close_call_count = 0;
  shell_surface->set_pre_close_callback(
      base::BindRepeating(&PreClose, base::Unretained(&pre_close_call_count),
                          base::Unretained(&close_call_count)));
  shell_surface->set_close_callback(
      base::BindRepeating(&Close, base::Unretained(&pre_close_call_count),
                          base::Unretained(&close_call_count)));

  surface->Commit();

  EXPECT_EQ(0, pre_close_call_count);
  EXPECT_EQ(0, close_call_count);
  shell_surface->GetWidget()->Close();
  EXPECT_EQ(1, pre_close_call_count);
  EXPECT_EQ(1, close_call_count);
}

void DestroyShellSurface(std::unique_ptr<ShellSurface>* shell_surface) {
  shell_surface->reset();
}

TEST_F(ShellSurfaceTest, SurfaceDestroyedCallback) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->set_surface_destroyed_callback(
      base::BindOnce(&DestroyShellSurface, base::Unretained(&shell_surface)));

  surface->Commit();

  EXPECT_TRUE(shell_surface.get());
  surface.reset();
  EXPECT_FALSE(shell_surface.get());
}

TEST_F(ShellSurfaceTest, ConfigureCallbackSendsRestoreState) {
  ConfigureData config_data;
  auto shell_surface = test::ShellSurfaceBuilder({256, 256})
                           .SetMaximumSize(gfx::Size(10, 10))
                           .BuildShellSurface();
  shell_surface->set_configure_callback(
      base::BindRepeating(&Configure, base::Unretained(&config_data)));

  shell_surface->root_surface()->Commit();
  shell_surface->Maximize();
  shell_surface->root_surface()->Commit();
  shell_surface->SetFullscreen(true, display::kInvalidDisplayId);
  shell_surface->root_surface()->Commit();
  EXPECT_EQ(chromeos::WindowStateType::kFullscreen, config_data.state_type);
  EXPECT_EQ(chromeos::WindowStateType::kMaximized,
            config_data.restore_state_type.value());
}

TEST_F(ShellSurfaceTest, ConfigureCallback) {
  // Must be before shell_surface so it outlives it, for shell_surface's
  // destructor calls Configure() referencing these 4 variables.
  ConfigureData config_data;

  auto shell_surface =
      test::ShellSurfaceBuilder().SetNoCommit().BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  shell_surface->set_configure_callback(
      base::BindRepeating(&Configure, base::Unretained(&config_data)));

  gfx::Rect geometry(16, 16, 32, 32);
  shell_surface->SetGeometry(geometry);

  // Commit without contents should result in a configure callback with empty
  // suggested size as a mechanisms to ask the client size itself.
  surface->Commit();
  ASSERT_TRUE(config_data.suggested_bounds.IsEmpty());
  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_FALSE(shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(geometry.size(), shell_surface->CalculatePreferredSize({}));

  gfx::Rect maximized_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  // State change should be sent even if the content is not attached.
  // See crbug.com/1138978.
  shell_surface->Maximize();
  shell_surface->AcknowledgeConfigure(0);

  EXPECT_FALSE(config_data.suggested_bounds.IsEmpty());
  EXPECT_EQ(maximized_bounds.size(), config_data.suggested_bounds.size());
  EXPECT_EQ(chromeos::WindowStateType::kMaximized, config_data.state_type);

  constexpr gfx::Size kBufferSize(64, 64);
  auto buffer = test::ExoTestHelper::CreateBuffer(kBufferSize);
  surface->Attach(buffer.get());
  surface->Commit();

  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_EQ(maximized_bounds.size(), config_data.suggested_bounds.size());
  EXPECT_EQ(chromeos::WindowStateType::kMaximized, config_data.state_type);
  shell_surface->Restore();
  shell_surface->AcknowledgeConfigure(0);
  // It should be restored to the original geometry size.
  EXPECT_EQ(geometry.size(), shell_surface->CalculatePreferredSize({}));

  shell_surface->SetFullscreen(true, display::kInvalidDisplayId);
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_EQ(GetContext()->bounds().size(), config_data.suggested_bounds.size());
  EXPECT_EQ(chromeos::WindowStateType::kFullscreen, config_data.state_type);
  shell_surface->SetFullscreen(false, display::kInvalidDisplayId);
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_EQ(geometry.size(), shell_surface->CalculatePreferredSize({}));

  shell_surface->GetWidget()->Activate();
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_TRUE(config_data.is_active);
  shell_surface->GetWidget()->Deactivate();
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_FALSE(config_data.is_active);

  EXPECT_FALSE(config_data.is_resizing);

  aura::Env::GetInstance()->set_mouse_button_flags(ui::EF_LEFT_MOUSE_BUTTON);
  ASSERT_TRUE(shell_surface->StartResize(HTBOTTOMRIGHT));
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_TRUE(config_data.is_resizing);
}

TEST_F(ShellSurfaceTest, CreateMinimizedWindow) {
  // Must be before shell_surface so it outlives it, for shell_surface's
  // destructor calls Configure() referencing these 4 variables.
  ConfigureData config_data;

  auto shell_surface =
      test::ShellSurfaceBuilder().SetNoCommit().BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  shell_surface->set_configure_callback(
      base::BindRepeating(&Configure, base::Unretained(&config_data)));

  gfx::Rect geometry(0, 0, 1, 1);
  shell_surface->SetGeometry(geometry);
  shell_surface->Minimize();
  shell_surface->AcknowledgeConfigure(0);
  // Commit without contents should result in a configure callback with empty
  // suggested size as a mechanisms to ask the client size itself.
  surface->Commit();

  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_TRUE(shell_surface->GetWidget()->IsMinimized());
  EXPECT_TRUE(config_data.suggested_bounds.IsEmpty());
  EXPECT_EQ(geometry.size(), shell_surface->CalculatePreferredSize({}));
}

TEST_F(ShellSurfaceTest, CreateMinimizedWindow2) {
  // Must be before shell_surface so it outlives it, for shell_surface's
  // destructor calls Configure() referencing these 4 variables.
  ConfigureData config_data;

  auto shell_surface =
      test::ShellSurfaceBuilder().SetNoCommit().BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  shell_surface->set_configure_callback(
      base::BindRepeating(&Configure, base::Unretained(&config_data)));

  gfx::Rect geometry(0, 0, 1, 1);
  shell_surface->SetGeometry(geometry);

  // Commit without contents should result in a configure callback with empty
  // suggested size as a mechanisms to ask the client size itself.
  surface->Commit();
  EXPECT_TRUE(config_data.suggested_bounds.IsEmpty());
  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_FALSE(shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(geometry.size(), shell_surface->CalculatePreferredSize({}));

  shell_surface->Minimize();
  shell_surface->AcknowledgeConfigure(0);

  // Commit without contents should result in a configure callback with empty
  // suggested size as a mechanisms to ask the client size itself.
  surface->Commit();

  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_TRUE(shell_surface->GetWidget()->IsMinimized());
  EXPECT_EQ(geometry.size(), shell_surface->CalculatePreferredSize({}));

  // Once the initial empty size is sent in configure,
  // new configure should send the size requested.
  EXPECT_EQ(geometry.size(), config_data.suggested_bounds.size());
}

TEST_F(ShellSurfaceTest,
       CreateMaximizedWindowWithRestoreBoundsWithoutInitialBuffer) {
  // Must be before shell_surface so it outlives it, for shell_surface's
  // destructor calls Configure() referencing these 4 variables.
  ConfigureData config_data;
  constexpr gfx::Size kBufferSize(256, 256);

  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder(kBufferSize)
          .SetNoRootBuffer()
          .BuildShellSurface();

  shell_surface->set_configure_callback(
      base::BindRepeating(&Configure, base::Unretained(&config_data)));

  Surface* root_surface = shell_surface->surface_for_testing();
  root_surface->Commit();
  shell_surface->AcknowledgeConfigure(0);

  // Ash may maximize the window.
  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_FALSE(shell_surface->GetWidget()->IsVisible());
  EXPECT_FALSE(shell_surface->GetWidget()->IsMaximized());

  shell_surface->Maximize();
  shell_surface->AcknowledgeConfigure(0);

  EXPECT_FALSE(shell_surface->GetWidget()->IsVisible());
  EXPECT_TRUE(shell_surface->GetWidget()->IsMaximized());

  auto buffer = test::ExoTestHelper::CreateBuffer(kBufferSize);
  root_surface->Attach(buffer.get());

  gfx::Rect geometry_full(kBufferSize);
  shell_surface->SetGeometry(geometry_full);

  // Commit without contents should result in a configure callback with empty
  // suggested size as a mechanisms to ask the client size itself.
  root_surface->Commit();
  shell_surface->AcknowledgeConfigure(0);

  EXPECT_TRUE(shell_surface->GetWidget()->IsVisible());
  EXPECT_TRUE(shell_surface->GetWidget()->IsMaximized());

  auto* window_state =
      ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow());

  EXPECT_TRUE(window_state->HasRestoreBounds());

  auto bounds = window_state->GetRestoreBoundsInParent();
  EXPECT_EQ(geometry_full.size(), bounds.size());
}

TEST_F(ShellSurfaceTest, CreateMaximizedWindowWithRestoreBounds) {
  // Must be before shell_surface so it outlives it, for shell_surface's
  // destructor calls Configure() referencing these 4 variables.
  ConfigureData config_data;
  constexpr gfx::Size kBufferSize(256, 256);
  auto buffer = test::ExoTestHelper::CreateBuffer(kBufferSize);

  auto shell_surface =
      test::ShellSurfaceBuilder().SetNoCommit().BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  shell_surface->set_configure_callback(
      base::BindRepeating(&Configure, base::Unretained(&config_data)));

  gfx::Rect geometry(0, 0, 1, 1);
  shell_surface->SetGeometry(geometry);
  shell_surface->Maximize();

  // Commit without contents should result in a configure callback with empty
  // suggested size as a mechanisms to ask the client size itself.
  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->AcknowledgeConfigure(0);

  gfx::Rect geometry_full(0, 0, 100, 100);
  shell_surface->SetGeometry(geometry_full);

  surface->Commit();
  shell_surface->AcknowledgeConfigure(1);

  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_TRUE(shell_surface->GetWidget()->IsMaximized());
  EXPECT_EQ(geometry_full.size(), shell_surface->CalculatePreferredSize({}));

  auto* window_state =
      ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow());

  EXPECT_TRUE(window_state->HasRestoreBounds());

  auto bounds = window_state->GetRestoreBoundsInParent();
  EXPECT_EQ(geometry.width(), bounds.width());
  EXPECT_EQ(geometry.height(), bounds.height());
}

TEST_F(ShellSurfaceTest, ToggleFullscreen) {
  constexpr gfx::Size kBufferSize(256, 256);
  auto shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();

  EXPECT_FALSE(HasBackdrop());
  EXPECT_EQ(kBufferSize,
            shell_surface->GetWidget()->GetWindowBoundsInScreen().size());
  shell_surface->Maximize();
  EXPECT_FALSE(HasBackdrop());
  EXPECT_EQ(GetContext()->bounds().width(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().width());

  ash::WMEvent event(ash::WM_EVENT_TOGGLE_FULLSCREEN);
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  // Enter fullscreen mode.
  ash::WindowState::Get(window)->OnWMEvent(&event);

  EXPECT_FALSE(HasBackdrop());
  EXPECT_EQ(GetContext()->bounds().ToString(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().ToString());

  // Leave fullscreen mode.
  ash::WindowState::Get(window)->OnWMEvent(&event);
  EXPECT_FALSE(HasBackdrop());

  // Check that shell surface is maximized.
  EXPECT_EQ(GetContext()->bounds().width(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().width());
}

TEST_F(ShellSurfaceTest, FrameColors) {
  auto shell_surface =
      test::ShellSurfaceBuilder({64, 64}).SetNoCommit().BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  shell_surface->OnSetFrameColors(SK_ColorRED, SK_ColorTRANSPARENT);
  surface->Commit();

  const ash::NonClientFrameViewAsh* frame =
      static_cast<const ash::NonClientFrameViewAsh*>(
          shell_surface->GetWidget()->non_client_view()->frame_view());

  // Test if colors set before initial commit are set.
  EXPECT_EQ(SK_ColorRED, frame->GetActiveFrameColorForTest());
  // Frame should be fully opaque.
  EXPECT_EQ(SK_ColorBLACK, frame->GetInactiveFrameColorForTest());

  shell_surface->OnSetFrameColors(SK_ColorTRANSPARENT, SK_ColorBLUE);
  EXPECT_EQ(SK_ColorBLACK, frame->GetActiveFrameColorForTest());
  EXPECT_EQ(SK_ColorBLUE, frame->GetInactiveFrameColorForTest());
}

TEST_F(ShellSurfaceTest, CycleSnap) {
  constexpr gfx::Size kBufferSize(256, 256);
  auto shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  EXPECT_EQ(kBufferSize,
            shell_surface->GetWidget()->GetWindowBoundsInScreen().size());

  ash::WindowSnapWMEvent event(ash::WM_EVENT_CYCLE_SNAP_PRIMARY);
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  // Enter snapped mode.
  ash::WindowState::Get(window)->OnWMEvent(&event);

  EXPECT_EQ(GetContext()->bounds().width() / 2,
            shell_surface->GetWidget()->GetWindowBoundsInScreen().width());

  surface->Commit();

  // Commit shouldn't change widget bounds when snapped.
  EXPECT_EQ(GetContext()->bounds().width() / 2,
            shell_surface->GetWidget()->GetWindowBoundsInScreen().width());
}

TEST_F(ShellSurfaceTest, ShellSurfaceMaximize) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetMaximumSize(gfx::Size(10, 10))
          .BuildShellSurface();
  auto* window_state =
      ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow());

  // Expect: Can't resize when max_size is set.
  EXPECT_FALSE(window_state->CanMaximize());
  EXPECT_FALSE(window_state->CanSnap());

  shell_surface->SetMaximumSize(gfx::Size(0, 0));
  shell_surface->root_surface()->Commit();

  // Expect: Can resize without a max_size.
  EXPECT_TRUE(window_state->CanMaximize());
  EXPECT_TRUE(window_state->CanSnap());
}

TEST_F(ShellSurfaceTest, ShellSurfaceMaxSizeResizabilityOnlyMaximise) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetMaximumSize(gfx::Size(10, 10))
          .SetMinimumSize(gfx::Size(0, 0))
          .BuildShellSurface();
  shell_surface->GetWidget()->GetNativeWindow()->SetProperty(
      kMaximumSizeForResizabilityOnly, true);
  shell_surface->root_surface()->Commit();

  auto* window_state =
      ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow());

  // Expect: Can resize with max_size > min_size.
  EXPECT_TRUE(window_state->CanMaximize());
  EXPECT_TRUE(window_state->CanResize());
  EXPECT_TRUE(window_state->CanSnap());

  shell_surface->SetMaximumSize(gfx::Size(0, 0));
  shell_surface->root_surface()->Commit();

  // Expect: Can resize with max_size unset.
  EXPECT_TRUE(window_state->CanMaximize());
  EXPECT_TRUE(window_state->CanResize());
  EXPECT_TRUE(window_state->CanSnap());

  shell_surface->SetMaximumSize(gfx::Size(10, 10));
  shell_surface->SetMinimumSize(gfx::Size(10, 10));
  shell_surface->root_surface()->Commit();

  // Expect: Can't resize where max_size is set and max_size == min_size.
  EXPECT_FALSE(window_state->CanMaximize());
  EXPECT_FALSE(window_state->CanResize());
  EXPECT_FALSE(window_state->CanSnap());
}

TEST_F(ShellSurfaceTest, Transient) {
  constexpr gfx::Size kBufferSize(256, 256);
  auto parent_shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();
  auto* parent_surface = parent_shell_surface->root_surface();

  auto child_shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).SetNoCommit().BuildShellSurface();
  auto* child_surface = child_shell_surface->root_surface();
  // Importantly, a transient window has an associated application.
  child_surface->SetApplicationId("fake_app_id");
  child_surface->SetParent(parent_surface, gfx::Point(50, 50));
  child_surface->Commit();

  aura::Window* parent_window =
      parent_shell_surface->GetWidget()->GetNativeWindow();
  aura::Window* child_window =
      child_shell_surface->GetWidget()->GetNativeWindow();
  ASSERT_TRUE(parent_window && child_window);

  // The visibility of transient windows is controlled by the parent.
  parent_window->Hide();
  EXPECT_FALSE(child_window->IsVisible());
  parent_window->Show();
  EXPECT_TRUE(child_window->IsVisible());
}

TEST_F(ShellSurfaceTest, X11Transient) {
  auto parent = test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();

  gfx::Point origin(50, 50);

  auto transient =
      CreateX11TransientShellSurface(parent.get(), gfx::Size(100, 100), origin);
  EXPECT_TRUE(transient->GetWidget()->movement_disabled());
  EXPECT_EQ(transient->GetWidget()->GetWindowBoundsInScreen().origin(), origin);
}

TEST_F(ShellSurfaceTest, Popup) {
  constexpr gfx::Size kBufferSize(256, 256);
  auto shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  auto popup_buffer = test::ExoTestHelper::CreateBuffer(kBufferSize);
  std::unique_ptr<Surface> popup_surface(new Surface);
  popup_surface->Attach(popup_buffer.get());
  std::unique_ptr<ShellSurface> popup_shell_surface(CreatePopupShellSurface(
      popup_surface.get(), shell_surface.get(), gfx::Point(50, 50)));
  popup_shell_surface->Grab();
  popup_surface->Commit();
  ASSERT_EQ(gfx::Rect(50, 50, 256, 256),
            popup_shell_surface->GetWidget()->GetWindowBoundsInScreen());

  // Verify that created shell surface is popup and has capture.
  EXPECT_EQ(aura::client::WINDOW_TYPE_POPUP,
            popup_shell_surface->GetWidget()->GetNativeWindow()->GetType());
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface.get()));

  // Setting frame type on popup should have no effect.
  popup_surface->SetFrame(SurfaceFrameType::NORMAL);
  EXPECT_FALSE(popup_shell_surface->frame_enabled());

  // ShellSurface can capture the event even after it is created.
  auto sub_popup_buffer = test::ExoTestHelper::CreateBuffer(kBufferSize);
  std::unique_ptr<Surface> sub_popup_surface(new Surface);
  sub_popup_surface->Attach(sub_popup_buffer.get());
  std::unique_ptr<ShellSurface> sub_popup_shell_surface(CreatePopupShellSurface(
      sub_popup_surface.get(), popup_shell_surface.get(), gfx::Point(100, 50)));
  sub_popup_shell_surface->Grab();
  sub_popup_surface->Commit();
  ASSERT_EQ(gfx::Rect(100, 50, 256, 256),
            sub_popup_shell_surface->GetWidget()->GetWindowBoundsInScreen());
  aura::Window* target =
      sub_popup_shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(aura::client::WINDOW_TYPE_POPUP, target->GetType());
  // The capture should be on `sub_popup_shell_surface`.
  EXPECT_TRUE(IsCaptureWindow(sub_popup_shell_surface.get()));

  {
    // Mouse is on the top most popup.
    ui::MouseEvent event(ui::EventType::kMouseMoved, gfx::Point(0, 0),
                         gfx::Point(100, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(sub_popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
  {
    // Move the mouse to the parent popup.
    ui::MouseEvent event(ui::EventType::kMouseMoved, gfx::Point(-25, 0),
                         gfx::Point(75, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
  {
    // Move the mouse to the main window.
    ui::MouseEvent event(ui::EventType::kMouseMoved, gfx::Point(-25, -25),
                         gfx::Point(75, 25), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(surface, GetTargetSurfaceForLocatedEvent(&event));
  }

  // Removing top most popup moves the grab to parent popup.
  sub_popup_shell_surface.reset();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface.get()));

  {
    // Targetting should still work.
    ui::MouseEvent event(ui::EventType::kMouseMoved, gfx::Point(0, 0),
                         gfx::Point(50, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(
        popup_shell_surface->GetWidget()->GetNativeWindow());
    EXPECT_EQ(popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
}

TEST_F(ShellSurfaceTest, GainCaptureFromSiblingSubPopup) {
  // Test that in the following setup:
  //
  //     popup_shell_surface1     popup_shell_surface2
  //  (has grab, loses capture) (has grab, gains capture)
  //                 \                /
  //                popup_shell_surface
  //
  // when popup_shell_surface2 is added, capture is correctly transferred to it
  // from popup_shell_surface1.

  constexpr gfx::Size kBufferSize(256, 256);
  auto shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  auto popup_buffer = test::ExoTestHelper::CreateBuffer(kBufferSize);
  auto popup_surface = std::make_unique<Surface>();
  popup_surface->Attach(popup_buffer.get());
  std::unique_ptr<ShellSurface> popup_shell_surface(CreatePopupShellSurface(
      popup_surface.get(), shell_surface.get(), gfx::Point(50, 50)));
  popup_shell_surface->Grab();
  popup_surface->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface.get()));

  auto popup_buffer1 = test::ExoTestHelper::CreateBuffer(kBufferSize);
  auto popup_surface1 = std::make_unique<Surface>();
  popup_surface1->Attach(popup_buffer1.get());
  std::unique_ptr<ShellSurface> popup_shell_surface1(CreatePopupShellSurface(
      popup_surface1.get(), popup_shell_surface.get(), gfx::Point(100, 50)));
  popup_shell_surface1->Grab();
  popup_surface1->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface1.get()));

  auto popup_buffer2 = test::ExoTestHelper::CreateBuffer(kBufferSize);
  auto popup_surface2 = std::make_unique<Surface>();
  popup_surface2->Attach(popup_buffer2.get());
  std::unique_ptr<ShellSurface> popup_shell_surface2(CreatePopupShellSurface(
      popup_surface2.get(), popup_shell_surface.get(), gfx::Point(50, 100)));
  popup_shell_surface2->Grab();
  popup_surface2->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface2.get()));
}

TEST_F(ShellSurfaceTest, GainCaptureFromNieceSubPopup) {
  // Test that in the following setup:
  //
  //    popup_shell_surface3
  //           (no grab)
  //              |
  //    popup_shell_surface2
  //  (has grab; loses capture)
  //              |
  //    popup_shell_surface1    popup_shell_surface4
  //         (no grab)        (has grab; gains capture)
  //                 \                /
  //                popup_shell_surface
  //
  // when popup_shell_surface4 is added, capture is correctly transferred to
  // it from popup_shell_surface2.

  constexpr gfx::Size kBufferSize(256, 256);
  auto shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  auto popup_buffer = test::ExoTestHelper::CreateBuffer(kBufferSize);
  auto popup_surface = std::make_unique<Surface>();
  popup_surface->Attach(popup_buffer.get());
  std::unique_ptr<ShellSurface> popup_shell_surface(CreatePopupShellSurface(
      popup_surface.get(), shell_surface.get(), gfx::Point(50, 50)));
  popup_shell_surface->Grab();
  popup_surface->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface.get()));

  auto popup_buffer1 = test::ExoTestHelper::CreateBuffer(kBufferSize);
  auto popup_surface1 = std::make_unique<Surface>();
  popup_surface1->Attach(popup_buffer1.get());
  std::unique_ptr<ShellSurface> popup_shell_surface1(CreatePopupShellSurface(
      popup_surface1.get(), popup_shell_surface.get(), gfx::Point(100, 50)));
  popup_surface1->Commit();
  // Doesn't change capture.
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface.get()));

  auto popup_buffer2 = test::ExoTestHelper::CreateBuffer(kBufferSize);
  auto popup_surface2 = std::make_unique<Surface>();
  popup_surface2->Attach(popup_buffer2.get());
  std::unique_ptr<ShellSurface> popup_shell_surface2(CreatePopupShellSurface(
      popup_surface2.get(), popup_shell_surface1.get(), gfx::Point(150, 50)));
  popup_shell_surface2->Grab();
  popup_surface2->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface2.get()));

  auto popup_buffer3 = test::ExoTestHelper::CreateBuffer(kBufferSize);
  auto popup_surface3 = std::make_unique<Surface>();
  popup_surface3->Attach(popup_buffer3.get());
  std::unique_ptr<ShellSurface> popup_shell_surface3(CreatePopupShellSurface(
      popup_surface3.get(), popup_shell_surface2.get(), gfx::Point(200, 50)));
  popup_surface3->Commit();
  // Doesn't change capture.
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface2.get()));

  auto popup_buffer4 = test::ExoTestHelper::CreateBuffer(kBufferSize);
  auto popup_surface4 = std::make_unique<Surface>();
  popup_surface4->Attach(popup_buffer4.get());
  std::unique_ptr<ShellSurface> popup_shell_surface4(CreatePopupShellSurface(
      popup_surface4.get(), popup_shell_surface.get(), gfx::Point(50, 100)));
  popup_shell_surface4->Grab();
  popup_surface4->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface4.get()));
}

TEST_F(ShellSurfaceTest, GainCaptureFromDescendantSubPopup) {
  // Test that in the following setup:
  //
  //    popup_shell_surface3
  //  (has grab; loses capture)
  //              |
  //    popup_shell_surface2
  //          (no grab)
  //              |
  //    popup_shell_surface1
  //  (has grab; gains capture)
  //              |
  //    popup_shell_surface
  //
  // when popup_shell_surface3 is closed, capture is correctly transferred to
  // popup_shell_surface1.

  constexpr gfx::Size kBufferSize(256, 256);
  auto shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  auto popup_buffer = test::ExoTestHelper::CreateBuffer(kBufferSize);
  auto popup_surface = std::make_unique<Surface>();
  popup_surface->Attach(popup_buffer.get());
  std::unique_ptr<ShellSurface> popup_shell_surface(CreatePopupShellSurface(
      popup_surface.get(), shell_surface.get(), gfx::Point(50, 50)));
  popup_shell_surface->Grab();
  popup_surface->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface.get()));

  auto popup_buffer1 = test::ExoTestHelper::CreateBuffer(kBufferSize);
  auto popup_surface1 = std::make_unique<Surface>();
  popup_surface1->Attach(popup_buffer1.get());
  std::unique_ptr<ShellSurface> popup_shell_surface1(CreatePopupShellSurface(
      popup_surface1.get(), popup_shell_surface.get(), gfx::Point(100, 50)));
  popup_shell_surface1->Grab();
  popup_surface1->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface1.get()));

  auto popup_buffer2 = test::ExoTestHelper::CreateBuffer(kBufferSize);
  auto popup_surface2 = std::make_unique<Surface>();
  popup_surface2->Attach(popup_buffer2.get());
  std::unique_ptr<ShellSurface> popup_shell_surface2(CreatePopupShellSurface(
      popup_surface2.get(), popup_shell_surface1.get(), gfx::Point(150, 50)));
  popup_surface2->Commit();
  // Doesn't change capture.
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface1.get()));

  auto popup_buffer3 = test::ExoTestHelper::CreateBuffer(kBufferSize);
  auto popup_surface3 = std::make_unique<Surface>();
  popup_surface3->Attach(popup_buffer3.get());
  std::unique_ptr<ShellSurface> popup_shell_surface3(CreatePopupShellSurface(
      popup_surface3.get(), popup_shell_surface2.get(), gfx::Point(200, 50)));
  popup_shell_surface3->Grab();
  popup_surface3->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface3.get()));

  popup_shell_surface3.reset();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface1.get()));
}

TEST_F(ShellSurfaceTest, Menu) {
  std::unique_ptr<ShellSurface> root_shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  EXPECT_TRUE(root_shell_surface->GetWidget()->IsVisible());

  std::unique_ptr<ShellSurface> menu_shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsMenu()
          .SetParent(root_shell_surface.get())
          .BuildShellSurface();
  EXPECT_TRUE(menu_shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_MENU,
            menu_shell_surface->GetWidget()->GetNativeWindow()->GetType());
}

TEST_F(ShellSurfaceTest, MenuOnPopup) {
  std::unique_ptr<ShellSurface> root_shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  EXPECT_TRUE(root_shell_surface->GetWidget()->IsVisible());

  std::unique_ptr<ShellSurface> popup_shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsPopup()
          .SetParent(root_shell_surface.get())
          .BuildShellSurface();
  EXPECT_TRUE(popup_shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_POPUP,
            popup_shell_surface->GetWidget()->GetNativeWindow()->GetType());

  std::unique_ptr<ShellSurface> menu_shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsMenu()
          .SetParent(popup_shell_surface.get())
          .BuildShellSurface();
  EXPECT_TRUE(menu_shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_MENU,
            menu_shell_surface->GetWidget()->GetNativeWindow()->GetType());
}

TEST_F(ShellSurfaceTest, PopupOnMenu) {
  std::unique_ptr<ShellSurface> root_shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  EXPECT_TRUE(root_shell_surface->GetWidget()->IsVisible());

  std::unique_ptr<ShellSurface> menu_shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsMenu()
          .SetParent(root_shell_surface.get())
          .BuildShellSurface();
  EXPECT_TRUE(menu_shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_MENU,
            menu_shell_surface->GetWidget()->GetNativeWindow()->GetType());

  std::unique_ptr<ShellSurface> popup_shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsPopup()
          .SetParent(menu_shell_surface.get())
          .BuildShellSurface();
  EXPECT_TRUE(popup_shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_POPUP,
            popup_shell_surface->GetWidget()->GetNativeWindow()->GetType());
}

TEST_F(ShellSurfaceTest, PopupOnPopup) {
  std::unique_ptr<ShellSurface> root_shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  EXPECT_TRUE(root_shell_surface->GetWidget()->IsVisible());

  std::unique_ptr<ShellSurface> popup_shell_surface_1 =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsPopup()
          .SetParent(root_shell_surface.get())
          .BuildShellSurface();
  EXPECT_TRUE(popup_shell_surface_1->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_POPUP,
            popup_shell_surface_1->GetWidget()->GetNativeWindow()->GetType());

  std::unique_ptr<ShellSurface> popup_shell_surface_2 =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsPopup()
          .SetParent(popup_shell_surface_1.get())
          .BuildShellSurface();
  EXPECT_TRUE(popup_shell_surface_2->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_POPUP,
            popup_shell_surface_2->GetWidget()->GetNativeWindow()->GetType());
}

TEST_F(ShellSurfaceTest, MenuOnMenu) {
  std::unique_ptr<ShellSurface> root_shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  EXPECT_TRUE(root_shell_surface->GetWidget()->IsVisible());

  std::unique_ptr<ShellSurface> menu_shell_surface_1 =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsMenu()
          .SetParent(root_shell_surface.get())
          .BuildShellSurface();
  EXPECT_TRUE(menu_shell_surface_1->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_MENU,
            menu_shell_surface_1->GetWidget()->GetNativeWindow()->GetType());

  std::unique_ptr<ShellSurface> menu_shell_surface_2 =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsMenu()
          .SetParent(menu_shell_surface_1.get())
          .BuildShellSurface();
  EXPECT_TRUE(menu_shell_surface_2->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_MENU,
            menu_shell_surface_2->GetWidget()->GetNativeWindow()->GetType());
}

TEST_F(ShellSurfaceTest, PopupWithInputRegion) {
  constexpr gfx::Size kBufferSize(256, 256);
  auto shell_surface = test::ShellSurfaceBuilder(kBufferSize)
                           .SetInputRegion(cc::Region())
                           .BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  auto child_buffer = test::ExoTestHelper::CreateBuffer(kBufferSize);
  std::unique_ptr<Surface> child_surface(new Surface);
  child_surface->Attach(child_buffer.get());

  auto subsurface = std::make_unique<SubSurface>(child_surface.get(), surface);
  subsurface->SetPosition(gfx::PointF(10, 10));
  child_surface->SetInputRegion(cc::Region(gfx::Rect(0, 0, 256, 2560)));
  child_surface->Commit();
  surface->Commit();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  auto popup_buffer = test::ExoTestHelper::CreateBuffer(kBufferSize);
  std::unique_ptr<Surface> popup_surface(new Surface);
  popup_surface->Attach(popup_buffer.get());
  std::unique_ptr<ShellSurface> popup_shell_surface(CreatePopupShellSurface(
      popup_surface.get(), shell_surface.get(), gfx::Point(50, 50)));
  popup_shell_surface->Grab();
  popup_surface->Commit();
  ASSERT_EQ(gfx::Rect(50, 50, 256, 256),
            popup_shell_surface->GetWidget()->GetWindowBoundsInScreen());

  // Verify that created shell surface is popup and has capture.
  EXPECT_EQ(aura::client::WINDOW_TYPE_POPUP,
            popup_shell_surface->GetWidget()->GetNativeWindow()->GetType());
  EXPECT_EQ(WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow(),
            popup_shell_surface->GetWidget()->GetNativeWindow());

  // Setting frame type on popup should have no effect.
  popup_surface->SetFrame(SurfaceFrameType::NORMAL);
  EXPECT_FALSE(popup_shell_surface->frame_enabled());

  aura::Window* target = popup_shell_surface->GetWidget()->GetNativeWindow();

  {
    // Mouse is on the popup.
    ui::MouseEvent event(ui::EventType::kMouseMoved, gfx::Point(0, 0),
                         gfx::Point(50, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }

  {
    // If it matches the parent's sub surface, use it.
    ui::MouseEvent event(ui::EventType::kMouseMoved, gfx::Point(-25, 0),
                         gfx::Point(25, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(child_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
  {
    // If it didnt't match any surface in the parent, fallback to
    // the popup's surface.
    ui::MouseEvent event(ui::EventType::kMouseMoved, gfx::Point(-50, 0),
                         gfx::Point(0, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
  popup_surface.reset();
  EXPECT_EQ(WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow(),
            nullptr);

  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_TEXTURED);
  window->SetBounds(gfx::Rect(0, 0, 256, 256));
  shell_surface->GetWidget()->GetNativeWindow()->AddChild(window.get());
  window->Show();

  {
    // If non surface window covers the window,
    /// GetTargetSurfaceForLocatedEvent should return nullptr.
    ui::MouseEvent event(ui::EventType::kMouseMoved, gfx::Point(0, 0),
                         gfx::Point(50, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(window.get());
    EXPECT_EQ(nullptr, GetTargetSurfaceForLocatedEvent(&event));
  }
}

// Test that popup does not close when trying to take a screenshot.
TEST_F(ShellSurfaceTest, PopupWithCaptureMode) {
  // Setup popup_shell_surface.
  constexpr gfx::Size kBufferSize(256, 256);
  auto shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();
  auto popup_buffer = test::ExoTestHelper::CreateBuffer(kBufferSize);
  auto popup_surface = std::make_unique<Surface>();
  popup_surface->Attach(popup_buffer.get());
  std::unique_ptr<ShellSurface> popup_shell_surface(CreatePopupShellSurface(
      popup_surface.get(), shell_surface.get(), gfx::Point(50, 50)));
  popup_shell_surface->Grab();
  popup_surface->Commit();

  bool closed = false;
  auto callback =
      base::BindRepeating([](bool* closed) { *closed = true; }, &closed);
  popup_shell_surface->set_close_callback(callback);

  // This simulates enabling (screenshot) capture mode.
  ash::GetTestDelegate()->OnSessionStateChanged(true);
  popup_shell_surface->OnCaptureChanged(
      popup_shell_surface->GetWidget()->GetNativeWindow(), nullptr);
  // With (screenshot) capture mode on, losing capture should not close the
  // shell surface.
  EXPECT_FALSE(closed);

  // This simulates ending (screenshot) capture mode.
  ash::GetTestDelegate()->OnSessionStateChanged(false);
  popup_shell_surface->OnCaptureChanged(
      popup_shell_surface->GetWidget()->GetNativeWindow(), nullptr);
  // With (screenshot) capture mode off, losing capture should close the shell
  // surface.
  EXPECT_TRUE(closed);
}

TEST_F(ShellSurfaceTest, PopupWithInvisibleParent) {
  // Invisible main window.
  std::unique_ptr<ShellSurface> root_shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetNoRootBuffer()
          .BuildShellSurface();
  EXPECT_FALSE(root_shell_surface->GetWidget()->IsVisible());

  std::unique_ptr<ShellSurface> popup_shell_surface_1 =
      test::ShellSurfaceBuilder({256, 256})
          .SetNoRootBuffer()
          .SetAsPopup()
          .SetDisableMovement()
          .SetParent(root_shell_surface.get())
          .BuildShellSurface();

  EXPECT_EQ(root_shell_surface->GetWidget()->GetNativeWindow(),
            wm::GetTransientParent(
                popup_shell_surface_1->GetWidget()->GetNativeWindow()));

  EXPECT_FALSE(popup_shell_surface_1->GetWidget()->IsVisible());

  // Create visible popup.
  std::unique_ptr<ShellSurface> popup_shell_surface_2 =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsPopup()
          .SetDisableMovement()
          .SetParent(popup_shell_surface_1.get())
          .BuildShellSurface();

  EXPECT_TRUE(popup_shell_surface_2->GetWidget()->IsVisible());

  EXPECT_EQ(popup_shell_surface_1->GetWidget()->GetNativeWindow(),
            wm::GetTransientParent(
                popup_shell_surface_2->GetWidget()->GetNativeWindow()));
}

TEST_F(ShellSurfaceTest, Caption) {
  auto shell_surface =
      test::ShellSurfaceBuilder({256, 256}).SetNoCommit().BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  shell_surface->OnSetFrame(SurfaceFrameType::NORMAL);
  surface->Commit();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  aura::Window* target = shell_surface->GetWidget()->GetNativeWindow();
  target->SetCapture();
  EXPECT_EQ(WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow(),
            shell_surface->GetWidget()->GetNativeWindow());
  {
    // Move the mouse at the caption of the captured window.
    ui::MouseEvent event(ui::EventType::kMouseMoved, gfx::Point(5, 5),
                         gfx::Point(5, 5), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(nullptr, GetTargetSurfaceForLocatedEvent(&event));
  }

  {
    // Move the mouse at the center of the captured window.
    gfx::Rect bounds = shell_surface->GetWidget()->GetWindowBoundsInScreen();
    gfx::Point center = bounds.CenterPoint();
    ui::MouseEvent event(ui::EventType::kMouseMoved,
                         center - bounds.OffsetFromOrigin(), center,
                         ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(surface, GetTargetSurfaceForLocatedEvent(&event));
  }
}

TEST_F(ShellSurfaceTest, DragMaximizedWindow) {
  auto shell_surface =
      test::ShellSurfaceBuilder({256, 256}).SetNoCommit().BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  shell_surface->OnSetFrame(SurfaceFrameType::NORMAL);
  surface->Commit();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  // Maximize the window.
  shell_surface->Maximize();

  chromeos::WindowStateType configured_state =
      chromeos::WindowStateType::kDefault;
  shell_surface->set_configure_callback(base::BindLambdaForTesting(
      [&](const gfx::Rect& bounds, chromeos::WindowStateType state,
          bool resizing, bool activated, const gfx::Vector2d& origin_offset,
          float raster_scale, aura::Window::OcclusionState occlusion_state,
          std::optional<chromeos::WindowStateType>) {
        configured_state = state;
        return uint32_t{0};
      }));

  // Initiate caption bar dragging for a window.
  gfx::Size size = shell_surface->GetWidget()->GetWindowBoundsInScreen().size();
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  // Move mouse pointer to caption bar.
  event_generator->MoveMouseTo(size.width() / 2, 2);
  constexpr int kDragAmount = 10;
  // Start dragging a window.
  event_generator->DragMouseBy(kDragAmount, kDragAmount);

  EXPECT_FALSE(shell_surface->GetWidget()->IsMaximized());
  // `WindowStateType` after dragging should be `kNormal`.
  EXPECT_EQ(chromeos::WindowStateType::kNormal, configured_state);
}

TEST_F(ShellSurfaceTest, CaptionWithPopup) {
  constexpr gfx::Size kBufferSize(256, 256);
  auto shell_surface = test::ShellSurfaceBuilder(kBufferSize)
                           .SetRootBufferFormat(kOpaqueFormat)
                           .SetFrame(SurfaceFrameType::NORMAL)
                           .BuildShellSurface();
  auto* surface = shell_surface->root_surface();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 288));

  auto popup_buffer =
      test::ExoTestHelper::CreateBuffer(kBufferSize, kOpaqueFormat);
  auto popup_surface = std::make_unique<Surface>();
  popup_surface->Attach(popup_buffer.get());
  std::unique_ptr<ShellSurface> popup_shell_surface(CreatePopupShellSurface(
      popup_surface.get(), shell_surface.get(), gfx::Point(50, 50)));
  popup_shell_surface->Grab();
  popup_surface->Commit();
  aura::Window* target = popup_shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow(),
            target);
  {
    // Move the mouse at the popup window.
    ui::MouseEvent event(ui::EventType::kMouseMoved, gfx::Point(5, 5),
                         gfx::Point(55, 55), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }

  {
    // Move the mouse at the caption of the main window.
    ui::MouseEvent event(ui::EventType::kMouseMoved, gfx::Point(-45, -45),
                         gfx::Point(5, 5), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(nullptr, GetTargetSurfaceForLocatedEvent(&event));
  }

  {
    // Move the mouse in the main window.
    ui::MouseEvent event(ui::EventType::kMouseMoved, gfx::Point(-25, 0),
                         gfx::Point(25, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(surface, GetTargetSurfaceForLocatedEvent(&event));
  }
}

TEST_F(ShellSurfaceTest, SkipImeProcessingPropagateToSurface) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetFrame(SurfaceFrameType::NORMAL)
          .BuildShellSurface();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  ASSERT_FALSE(window->GetProperty(aura::client::kSkipImeProcessing));
  ASSERT_FALSE(shell_surface->root_surface()->window()->GetProperty(
      aura::client::kSkipImeProcessing));

  window->SetProperty(aura::client::kSkipImeProcessing, true);
  EXPECT_TRUE(window->GetProperty(aura::client::kSkipImeProcessing));
  EXPECT_TRUE(shell_surface->root_surface()->window()->GetProperty(
      aura::client::kSkipImeProcessing));
}

TEST_F(ShellSurfaceTest, NotifyLeaveEnter) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256}).SetNoCommit().BuildShellSurface();

  auto func = [](int64_t* old_display_id, int64_t* new_display_id,
                 int64_t old_id, int64_t new_id) {
    DCHECK_EQ(0, *old_display_id);
    DCHECK_EQ(0, *new_display_id);
    *old_display_id = old_id;
    *new_display_id = new_id;
    return true;
  };

  int64_t old_display_id = 0, new_display_id = 0;

  shell_surface->root_surface()->set_leave_enter_callback(
      base::BindRepeating(func, &old_display_id, &new_display_id));

  // Creating a new shell surface should notify on which display
  // it is created.
  shell_surface->root_surface()->Commit();
  EXPECT_EQ(display::kInvalidDisplayId, old_display_id);
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().id(),
            new_display_id);

  // Attaching a 2nd display should not change where the surface
  // is located.
  old_display_id = 0;
  new_display_id = 0;
  UpdateDisplay("800x600, 800x600");
  EXPECT_EQ(0, old_display_id);
  EXPECT_EQ(0, new_display_id);

  // Move the window to 2nd display.
  shell_surface->GetWidget()->SetBounds(gfx::Rect(1000, 0, 256, 256));

  int64_t secondary_id =
      display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
          .GetSecondaryDisplay()
          .id();

  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().id(),
            old_display_id);
  EXPECT_EQ(secondary_id, new_display_id);

  // Disconnect the display the surface is currently on.
  old_display_id = 0;
  new_display_id = 0;
  UpdateDisplay("800x600");
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().id(),
            new_display_id);
  EXPECT_EQ(secondary_id, old_display_id);
}

// Make sure that the server side triggers resize when the
// set_server_start_resize is called, and the resize shadow is created for the
// window.
TEST_F(ShellSurfaceTest, ServerStartResize) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64}).SetNoCommit().BuildShellSurface();
  shell_surface->OnSetServerStartResize();
  shell_surface->root_surface()->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());

  auto* widget = shell_surface->GetWidget();

  gfx::Size size = widget->GetWindowBoundsInScreen().size();
  widget->SetBounds(gfx::Rect(size));

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(size.width() + 2, size.height() / 2);

  // Ash creates resize shadow for resizable exo window when the
  // server_start_resize is set.
  ash::ResizeShadow* resize_shadow =
      ash::Shell::Get()->resize_shadow_controller()->GetShadowForWindowForTest(
          widget->GetNativeWindow());
  ASSERT_TRUE(resize_shadow);

  constexpr int kDragAmount = 10;
  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(kDragAmount, 0);
  event_generator->ReleaseLeftButton();

  EXPECT_EQ(widget->GetWindowBoundsInScreen().size().width(),
            size.width() + kDragAmount);
}

TEST_F(ShellSurfaceTest, LacrosToggleAxisMaximize) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64})
          .SetOrigin({10, 10})
          .BuildShellSurface();
  shell_surface->OnSetServerStartResize();
  auto* widget = shell_surface->GetWidget();
  gfx::Size size = widget->GetWindowBoundsInScreen().size();

  gfx::Rect restored_bounds = shell_surface->GetBoundsInScreen();

  ui::test::EventGenerator* event_generator = GetEventGenerator();

  // Move mouse to top middle and double click to vertically maximize.
  event_generator->MoveMouseTo(10 + size.width() / 2, 10);
  event_generator->DoubleClickLeftButton();

  gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect bounds_in_screen = shell_surface->GetBoundsInScreen();

  EXPECT_EQ(restored_bounds.x(), bounds_in_screen.x());
  EXPECT_EQ(restored_bounds.width(), bounds_in_screen.width());
  EXPECT_EQ(work_area.y(), bounds_in_screen.y());
  EXPECT_EQ(work_area.height(), bounds_in_screen.height());

  // Move mouse to top middle and double click to vertically Restore.
  event_generator->MoveMouseTo(10 + size.width() / 2, 0);
  event_generator->DoubleClickLeftButton();
  bounds_in_screen = shell_surface->GetBoundsInScreen();
  EXPECT_EQ(restored_bounds, bounds_in_screen);
}

TEST_F(ShellSurfaceTest, ServerStartResizeComponent) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64}).SetNoCommit().BuildShellSurface();
  shell_surface->OnSetServerStartResize();
  shell_surface->root_surface()->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());

  auto* widget = shell_surface->GetWidget();

  gfx::Size size = widget->GetWindowBoundsInScreen().size();
  widget->SetBounds(gfx::Rect(size));

  uint32_t serial = 0;
  auto configure_callback = base::BindRepeating(
      [](uint32_t* const serial_ptr, const gfx::Rect& bounds,
         chromeos::WindowStateType state_type, bool resizing, bool activated,
         const gfx::Vector2d& origin_offset, float raster_scale,
         aura::Window::OcclusionState occlusion_state,
         std::optional<chromeos::WindowStateType>) { return ++(*serial_ptr); },
      &serial);

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(size.width() + 2, size.height() / 2);

  EXPECT_EQ(shell_surface->resize_component_for_test(), HTCAPTION);

  constexpr int kDragAmount = 10;
  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(kDragAmount, 0);
  EXPECT_EQ(shell_surface->resize_component_for_test(), HTCAPTION);
  shell_surface->AcknowledgeConfigure(serial);
  shell_surface->root_surface()->Commit();
  EXPECT_EQ(shell_surface->resize_component_for_test(), HTRIGHT);
  event_generator->ReleaseLeftButton();
  shell_surface->AcknowledgeConfigure(serial);
  shell_surface->root_surface()->Commit();
  EXPECT_EQ(shell_surface->resize_component_for_test(), HTCAPTION);
}

// Make sure that dragging to another display will update the origin to
// correct value.
TEST_F(ShellSurfaceTest, UpdateBoundsWhenDraggedToAnotherDisplay) {
  exo::test::TestSecurityDelegate securityDelegate;
  securityDelegate.SetCanSetBounds(
      SecurityDelegate::SetBoundsPolicy::DCHECK_IF_DECORATED);
  UpdateDisplay("800x600, 800x600");
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64})
          .SetSecurityDelegate(&securityDelegate)
          .BuildShellSurface();
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  shell_surface->SetWindowBounds({0, 0, 64, 64});

  gfx::Point last_origin;
  auto origin_change = [&](const gfx::Point& p) { last_origin = p; };

  shell_surface->set_origin_change_callback(
      base::BindLambdaForTesting(origin_change));
  event_generator->MoveMouseTo(1, 1);
  event_generator->PressLeftButton();
  ASSERT_TRUE(shell_surface->StartMove());
  event_generator->MoveMouseTo(801, 1);
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(last_origin, gfx::Point(800, 0));
}

// Make sure that commit during window drag should not move the
// window to another display.
TEST_F(ShellSurfaceTest, CommitShouldNotMoveDisplay) {
  UpdateDisplay("800x600, 800x600");
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64})
          .SetOrigin({750, 0})
          .BuildShellSurface();
  auto* screen = display::Screen::GetScreen();
  auto* root_surface = shell_surface->root_surface();

  EXPECT_EQ(screen->GetPrimaryDisplay().id(),
            screen
                ->GetDisplayNearestWindow(
                    shell_surface->GetWidget()->GetNativeWindow())
                .id());

  aura::Env::GetInstance()->set_mouse_button_flags(ui::EF_LEFT_MOUSE_BUTTON);
  ASSERT_TRUE(shell_surface->StartMove());

  constexpr gfx::Size kBufferSize(256, 256);
  auto new_buffer = test::ExoTestHelper::CreateBuffer(kBufferSize);
  root_surface->Attach(new_buffer.get());
  root_surface->Commit();

  EXPECT_EQ(screen->GetPrimaryDisplay().id(),
            screen
                ->GetDisplayNearestWindow(
                    shell_surface->GetWidget()->GetNativeWindow())
                .id());

  GetEventGenerator()->ReleaseLeftButton();

  // shell_surface->EndDrag();

  // Ending drag will not move the window unless the mouse cursor enters
  // another display.
  EXPECT_EQ(screen->GetPrimaryDisplay().id(),
            screen
                ->GetDisplayNearestWindow(
                    shell_surface->GetWidget()->GetNativeWindow())
                .id());
}

TEST_F(ShellSurfaceTest, ShadowBoundsWithNegativeCoordinate) {
  constexpr gfx::Point kOrigin(20, 20);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetOrigin(kOrigin)
          .BuildShellSurface();
  shell_surface->root_surface()->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());

  // Create subsurface outside of root surface.
  constexpr gfx::Size kChildBufferSize(32, 32);
  auto child_buffer = test::ExoTestHelper::CreateBuffer(kChildBufferSize);
  auto child_surface = std::make_unique<Surface>();
  child_surface->Attach(child_buffer.get());
  auto subsurface = std::make_unique<SubSurface>(child_surface.get(),
                                                 shell_surface->root_surface());
  // Set subsurface to left and upper of the window.
  subsurface->SetPosition(gfx::PointF(-10, -10));
  child_surface->Commit();
  shell_surface->root_surface()->Commit();

  ASSERT_TRUE(shell_surface->GetWidget());
  auto* widget = shell_surface->GetWidget();
  // Geometry is relative to the root surface.
  shell_surface->SetGeometry(gfx::Rect(0, 0, 256, 256));
  shell_surface->root_surface()->SetFrame(SurfaceFrameType::SHADOW);
  shell_surface->root_surface()->Commit();
  EXPECT_FALSE(widget->GetNativeWindow()->GetProperty(
      aura::client::kUseWindowBoundsForShadow));
  // Host window should be a rectangle including root surface and subsurface.
  EXPECT_EQ(gfx::Rect(-10, -10, 266, 266),
            shell_surface->host_window()->bounds());

  // Shadow content bounds is relative to ExoShellSurface and should use window
  // bounds.
  ui::Shadow* shadow =
      wm::ShadowController::GetShadowForWindow(widget->GetNativeWindow());
  ASSERT_TRUE(shadow);
  EXPECT_EQ(gfx::Rect(0, 0, 256, 256), shadow->content_bounds());
}

TEST_F(ShellSurfaceTest, ShadowBoundsWithScaleFactor) {
  constexpr gfx::Point kOrigin(20, 20);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetOrigin(kOrigin)
          .BuildShellSurface();
  shell_surface->root_surface()->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());

  // Set scale.
  constexpr float kScaleFactor = 2.0;
  shell_surface->set_client_submits_surfaces_in_pixel_coordinates(true);
  shell_surface->SetScaleFactor(kScaleFactor);

  // Create subsurface outside of root surface.
  constexpr gfx::Size kChildBufferSize(32, 32);
  auto child_buffer = test::ExoTestHelper::CreateBuffer(kChildBufferSize);
  auto child_surface = std::make_unique<Surface>();
  child_surface->Attach(child_buffer.get());
  auto subsurface = std::make_unique<SubSurface>(child_surface.get(),
                                                 shell_surface->root_surface());
  // Set subsurface to left and upper of the window.
  subsurface->SetPosition(gfx::PointF(-10, -10));
  child_surface->Commit();
  shell_surface->root_surface()->Commit();

  ASSERT_TRUE(shell_surface->GetWidget());
  auto* widget = shell_surface->GetWidget();
  // Geometry is relative to the root surface.
  shell_surface->SetGeometry(gfx::Rect(0, 0, 256, 256));
  shell_surface->root_surface()->SetFrame(SurfaceFrameType::SHADOW);
  shell_surface->root_surface()->Commit();
  EXPECT_FALSE(widget->GetNativeWindow()->GetProperty(
      aura::client::kUseWindowBoundsForShadow));
  // Host window should be a rectangle including root surface and subsurface.
  EXPECT_EQ(gfx::Rect(-5, -5, 133, 133),
            shell_surface->host_window()->bounds());

  // Shadow content bounds is relative to ExoShellSurface and should use window
  // bounds.
  ui::Shadow* shadow =
      wm::ShadowController::GetShadowForWindow(widget->GetNativeWindow());
  ASSERT_TRUE(shadow);
  EXPECT_EQ(gfx::Rect(0, 0, 256, 256), shadow->content_bounds());
}

TEST_F(ShellSurfaceTest, ShadowRoundedCorners) {
  constexpr gfx::Point kOrigin(20, 20);
  constexpr int kWindowCornerRadius = 12;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {chromeos::features::kRoundedWindows,
       chromeos::features::kFeatureManagementRoundedWindows},
      /*disabled_features=*/{});

  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetOrigin(kOrigin)
          .SetWindowState(chromeos::WindowStateType::kNormal)
          .SetFrame(SurfaceFrameType::NORMAL)
          .BuildShellSurface();

  Surface* root_surface = shell_surface->root_surface();

  root_surface->Commit();
  views::Widget* widget = shell_surface->GetWidget();
  ASSERT_TRUE(widget);

  aura::Window* window = widget->GetNativeWindow();
  ui::Shadow* shadow = wm::ShadowController::GetShadowForWindow(window);
  ASSERT_TRUE(shadow);

  // Window shadow radius needs to match the window radius.
  EXPECT_EQ(shadow->rounded_corner_radius_for_testing(), 0);

  // Have a window with radius of 12dp.
  shell_surface->SetWindowCornersRadii(
      gfx::RoundedCornersF(kWindowCornerRadius));
  root_surface->Commit();

  shadow = wm::ShadowController::GetShadowForWindow(window);
  ASSERT_TRUE(shadow);
  EXPECT_EQ(shadow->rounded_corner_radius_for_testing(), kWindowCornerRadius);

  // Have a window with radius of 0dp.
  shell_surface->SetWindowCornersRadii(gfx::RoundedCornersF());
  root_surface->Commit();

  shadow = wm::ShadowController::GetShadowForWindow(window);
  ASSERT_TRUE(shadow);
  EXPECT_EQ(shadow->rounded_corner_radius_for_testing(), 0);
}

TEST_F(ShellSurfaceTest, RoundedWindows) {
  constexpr gfx::Point kOrigin(20, 20);
  constexpr int kWindowCornerRadius = 12;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {chromeos::features::kRoundedWindows,
       chromeos::features::kFeatureManagementRoundedWindows},
      /*disabled_features=*/{});

  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetOrigin(kOrigin)
          .SetWindowState(chromeos::WindowStateType::kNormal)
          .SetFrame(SurfaceFrameType::NORMAL)
          .BuildShellSurface();

  Surface* root_surface = shell_surface->root_surface();

  // Add a sub-surface.
  constexpr gfx::Size kChildBufferSize(32, 32);
  auto child_buffer1 = test::ExoTestHelper::CreateBuffer(kChildBufferSize);
  auto child_surface1 = std::make_unique<Surface>();
  child_surface1->Attach(child_buffer1.get());
  auto subsurface1 = std::make_unique<SubSurface>(
      child_surface1.get(), shell_surface->root_surface());
  subsurface1->SetPosition(gfx::PointF(kOrigin));
  child_surface1->SetRoundedCorners(
      gfx::RRectF({32, 32}, gfx::RoundedCornersF(1)),
      /*commit_override=*/false);
  child_surface1->Commit();

  root_surface->Commit();

  shell_surface->SetWindowCornersRadii(
      gfx::RoundedCornersF(kWindowCornerRadius));
  root_surface->Commit();

  auto mask_filter_info_at([](int index, const viz::CompositorFrame& frame) {
    return frame.render_pass_list.back()
        ->quad_list.ElementAt(index)
        ->shared_quad_state->mask_filter_info;
  });

  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get(), GetSurfaceManager());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(2u, frame.render_pass_list.back()->quad_list.size());

    gfx::RRectF expected_bounds({256, 256}, gfx::RoundedCornersF(0, 0, 12, 12));
    EXPECT_EQ(mask_filter_info_at(0, frame).rounded_corner_bounds(),
              expected_bounds);

    // `SetWindowCornersRadii` overrides the window rounded corner bounds of the
    // either surface tree rooted at `root_surface()`.
    EXPECT_EQ(mask_filter_info_at(1, frame).rounded_corner_bounds(),
              expected_bounds);
  }

  shell_surface->SetWindowCornersRadii(gfx::RoundedCornersF(0));
  root_surface->Commit();

  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get(), GetSurfaceManager());

    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(2u, frame.render_pass_list.back()->quad_list.size());
    EXPECT_TRUE(mask_filter_info_at(0, frame).IsEmpty());
    EXPECT_TRUE(mask_filter_info_at(1, frame).IsEmpty());
  }
}

// Make sure that resize shadow does not update until commit when the window
// property |aura::client::kUseWindowBoundsForShadow| is false.
TEST_F(ShellSurfaceTest, ResizeShadowIndependentBounds) {
  constexpr gfx::Point kOrigin(10, 10);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64})
          .SetOrigin(kOrigin)
          .BuildShellSurface();
  shell_surface->OnSetServerStartResize();
  shell_surface->root_surface()->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());

  auto* widget = shell_surface->GetWidget();

  gfx::Size size = widget->GetWindowBoundsInScreen().size();
  widget->SetBounds(gfx::Rect(size));

  // Starts mouse event to make sure resize shadow is created.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(size.width() + 2, size.height() / 2);

  // Creates resize shadow and normal shadow for resizable exo window.
  ash::ResizeShadow* resize_shadow =
      ash::Shell::Get()->resize_shadow_controller()->GetShadowForWindowForTest(
          widget->GetNativeWindow());
  ASSERT_TRUE(resize_shadow);
  shell_surface->root_surface()->SetFrame(SurfaceFrameType::SHADOW);
  shell_surface->root_surface()->Commit();
  EXPECT_FALSE(widget->GetNativeWindow()->GetProperty(
      aura::client::kUseWindowBoundsForShadow));

  ui::Shadow* normal_shadow =
      wm::ShadowController::GetShadowForWindow(widget->GetNativeWindow());
  ASSERT_TRUE(normal_shadow);

  // ash::ResizeShadow::InitParams set the default |thickness| to 8.
  constexpr int kResizeShadowThickness = 8;

  EXPECT_EQ(gfx::Size(size.width() + kResizeShadowThickness, size.height()),
            resize_shadow->GetLayerForTest()->bounds().size());
  EXPECT_EQ(size, normal_shadow->content_bounds().size());

  constexpr gfx::Rect kNewBounds(kOrigin, {100, 100});
  uint32_t serial = 0;
  auto configure_callback = base::BindRepeating(
      [](uint32_t* const serial_ptr, const gfx::Rect& bounds,
         chromeos::WindowStateType state_type, bool resizing, bool activated,
         const gfx::Vector2d& origin_offset, float raster_scale,
         aura::Window::OcclusionState occlusion_state,
         std::optional<chromeos::WindowStateType>) { return ++(*serial_ptr); },
      &serial);

  shell_surface->set_configure_callback(configure_callback);

  // Resize the widget and set geometry.
  aura::Env::GetInstance()->set_mouse_button_flags(ui::EF_LEFT_MOUSE_BUTTON);
  ASSERT_TRUE(shell_surface->StartResize(HTBOTTOMRIGHT));
  shell_surface->SetWidgetBounds(kNewBounds, /*adjusted by server=*/false);
  shell_surface->SetGeometry(gfx::Rect(kNewBounds.size()));

  // Client acknowledge configure for resizing. Shadow sizes should not be
  // updated yet until commit.
  shell_surface->AcknowledgeConfigure(serial);
  EXPECT_EQ(gfx::Size(size.width() + kResizeShadowThickness, size.height()),
            resize_shadow->GetLayerForTest()->bounds().size());
  EXPECT_EQ(size, normal_shadow->content_bounds().size());

  // Normal and resize shadow sizes are updated after commit.
  shell_surface->root_surface()->Commit();
  EXPECT_EQ(gfx::Size(kNewBounds.width() + kResizeShadowThickness,
                      kNewBounds.height()),
            resize_shadow->GetLayerForTest()->bounds().size());
  EXPECT_EQ(kNewBounds.size(), normal_shadow->content_bounds().size());

  // Explicitly ends the drag here.
  ash::Shell::Get()->toplevel_window_event_handler()->CompleteDragForTesting(
      ash::ToplevelWindowEventHandler::DragResult::SUCCESS);
  // Hide Shadow
  event_generator->MoveMouseTo({0, 0});

  EXPECT_FALSE(resize_shadow->visible());

  // Move
  UpdateDisplay("800x600,1200x1000");
  shell_surface->GetWidget()->SetBounds({{1000, 100}, kNewBounds.size()});

  shell_surface->AcknowledgeConfigure(serial);
  shell_surface->root_surface()->Commit();

  auto* screen = display::Screen::GetScreen();
  int64_t secondary_id =
      display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
          .GetSecondaryDisplay()
          .id();
  ASSERT_EQ(secondary_id, screen
                              ->GetDisplayNearestWindow(
                                  shell_surface->GetWidget()->GetNativeWindow())
                              .id());

  // Use outside to start drag because exo consumes events inside.
  event_generator->MoveMouseTo({999, 99});

  gfx::Rect bounds = shell_surface->GetWidget()->GetNativeWindow()->bounds();

  EXPECT_TRUE(resize_shadow->visible());
  gfx::Rect expected_shadow_on_secondary(
      bounds.x() - kResizeShadowThickness, bounds.y() - kResizeShadowThickness,
      bounds.width() + kResizeShadowThickness,
      bounds.height() + kResizeShadowThickness);
  EXPECT_EQ(expected_shadow_on_secondary,
            resize_shadow->GetLayerForTest()->bounds());

  aura::Env::GetInstance()->set_mouse_button_flags(ui::EF_LEFT_MOUSE_BUTTON);
  constexpr gfx::Rect kResizedBoundsOn2nd{950, 50, 150, 150};
  ASSERT_TRUE(shell_surface->StartResize(HTTOPLEFT));
  shell_surface->GetWidget()->SetBounds(kResizedBoundsOn2nd);
  shell_surface->SetGeometry(gfx::Rect(kResizedBoundsOn2nd.size()));
  shell_surface->AcknowledgeConfigure(serial);

  EXPECT_EQ(expected_shadow_on_secondary,
            resize_shadow->GetLayerForTest()->bounds());

  shell_surface->root_surface()->Commit();
  constexpr gfx::Rect kExpectedShadowBoundsOn2nd(
      150 - kResizeShadowThickness, 50 - kResizeShadowThickness,
      kResizedBoundsOn2nd.width() + kResizeShadowThickness,
      kResizedBoundsOn2nd.height() + kResizeShadowThickness);

  EXPECT_EQ(kExpectedShadowBoundsOn2nd,
            resize_shadow->GetLayerForTest()->bounds());
}

// Make sure that resize shadow updates as soon as widget bounds change when
// the window property |aura::client::kUseWindowBoundsForShadow| is false.
TEST_F(ShellSurfaceTest, ResizeShadowDependentBounds) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64}).BuildShellSurface();
  shell_surface->OnSetServerStartResize();
  shell_surface->root_surface()->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());

  auto* widget = shell_surface->GetWidget();

  gfx::Size size = widget->GetWindowBoundsInScreen().size();
  widget->SetBounds(gfx::Rect(size));

  // Starts mouse event to make sure resize shadow is created.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(size.width() + 2, size.height() / 2);

  // Creates resize shadow and normal shadow for resizable exo window.
  ash::ResizeShadow* resize_shadow =
      ash::Shell::Get()->resize_shadow_controller()->GetShadowForWindowForTest(
          widget->GetNativeWindow());
  ASSERT_TRUE(resize_shadow);
  shell_surface->root_surface()->SetFrame(SurfaceFrameType::SHADOW);
  shell_surface->root_surface()->Commit();
  EXPECT_FALSE(widget->GetNativeWindow()->GetProperty(
      aura::client::kUseWindowBoundsForShadow));
  // Override the property to update the shadow bounds immediately.
  widget->GetNativeWindow()->SetProperty(
      aura::client::kUseWindowBoundsForShadow, true);

  ui::Shadow* normal_shadow =
      wm::ShadowController::GetShadowForWindow(widget->GetNativeWindow());
  ASSERT_TRUE(normal_shadow);

  // ash::ResizeShadow::InitParams set the default |thickness| to 8.
  const int kResizeShadowThickness = 8;

  EXPECT_EQ(gfx::Size(size.width() + kResizeShadowThickness, size.height()),
            resize_shadow->GetLayerForTest()->bounds().size());
  EXPECT_EQ(size, normal_shadow->content_bounds().size());

  gfx::Size new_size(100, 100);
  gfx::Rect new_bounds(new_size);
  aura::Env::GetInstance()->set_mouse_button_flags(ui::EF_LEFT_MOUSE_BUTTON);
  // Resize the widget and set geometry.
  ASSERT_TRUE(shell_surface->StartResize(HTBOTTOMRIGHT));
  shell_surface->SetWidgetBounds(new_bounds, /*adjusted_by_server=*/false);
  shell_surface->SetGeometry(new_bounds);
  // Shadow bounds are updated as soon as the widget bounds change.
  EXPECT_EQ(
      gfx::Size(new_size.width() + kResizeShadowThickness, new_size.height()),
      resize_shadow->GetLayerForTest()->bounds().size());
  EXPECT_EQ(new_size, normal_shadow->content_bounds().size());
}

TEST_F(ShellSurfaceTest, PropertyResolverTest) {
  class TestPropertyResolver : public exo::WMHelper::AppPropertyResolver {
   public:
    TestPropertyResolver() = default;
    ~TestPropertyResolver() override = default;
    void PopulateProperties(
        const Params& params,
        ui::PropertyHandler& out_properties_container) override {
      if (expected_app_id == params.app_id) {
        out_properties_container.AcquireAllPropertiesFrom(
            std::move(params.for_creation ? properties_for_creation
                                          : properties_after_creation));
      }
    }
    std::string expected_app_id;
    ui::PropertyHandler properties_for_creation;
    ui::PropertyHandler properties_after_creation;
  };
  std::unique_ptr<TestPropertyResolver> resolver_holder =
      std::make_unique<TestPropertyResolver>();
  auto* resolver = resolver_holder.get();
  WMHelper::GetInstance()->RegisterAppPropertyResolver(
      std::move(resolver_holder));

  resolver->properties_for_creation.SetProperty(ash::kShelfItemTypeKey, 1);
  resolver->properties_after_creation.SetProperty(ash::kShelfItemTypeKey, 2);
  resolver->expected_app_id = "test";

  // Make sure that properties are properly populated for both
  // "before widget creation", and "after widget creation".
  {
    auto shell_surface =
        test::ShellSurfaceBuilder({256, 256}).SetNoCommit().BuildShellSurface();
    auto* surface = shell_surface->root_surface();

    surface->SetApplicationId("test");
    surface->Commit();
    EXPECT_EQ(1, shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                     ash::kShelfItemTypeKey));
    surface->SetApplicationId("test");
    EXPECT_EQ(2, shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                     ash::kShelfItemTypeKey));
  }

  // Make sure that properties are will not be popluated when the app ids
  // mismatch "before" and "after" widget is created.
  resolver->properties_for_creation.SetProperty(ash::kShelfItemTypeKey, 1);
  resolver->properties_after_creation.SetProperty(ash::kShelfItemTypeKey, 2);
  {
    auto shell_surface =
        test::ShellSurfaceBuilder({256, 256}).SetNoCommit().BuildShellSurface();
    auto* surface = shell_surface->root_surface();

    surface->SetApplicationId("testx");
    surface->Commit();
    EXPECT_NE(1, shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                     ash::kShelfItemTypeKey));

    surface->SetApplicationId("testy");
    surface->Commit();
    EXPECT_NE(1, shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                     ash::kShelfItemTypeKey));
    EXPECT_NE(2, shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                     ash::kShelfItemTypeKey));

    // Updating to the matching |app_id| should set the window property.
    surface->SetApplicationId("test");
    EXPECT_EQ(2, shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                     ash::kShelfItemTypeKey));
  }
}

TEST_F(ShellSurfaceTest, Overlay) {
  auto shell_surface =
      test::ShellSurfaceBuilder({100, 100}).BuildShellSurface();
  shell_surface->GetWidget()->GetNativeWindow()->SetProperty(
      aura::client::kSkipImeProcessing, true);

  EXPECT_FALSE(shell_surface->HasOverlay());

  auto textfield = std::make_unique<views::Textfield>();
  auto* textfield_ptr = textfield.get();

  ShellSurfaceBase::OverlayParams params(std::move(textfield));
  shell_surface->AddOverlay(std::move(params));
  EXPECT_TRUE(shell_surface->HasOverlay());
  EXPECT_NE(shell_surface->GetWidget()->GetFocusManager()->GetFocusedView(),
            textfield_ptr);

  EXPECT_TRUE(shell_surface->GetWidget()->widget_delegate()->CanResize());
  EXPECT_EQ(gfx::Size(100, 100), shell_surface->overlay_widget_for_testing()
                                     ->GetWindowBoundsInScreen()
                                     .size());

  PressAndReleaseKey(ui::VKEY_X);
  EXPECT_EQ(textfield_ptr->GetText(), u"");

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseToCenterOf(shell_surface->GetWidget()->GetNativeWindow());
  generator->ClickLeftButton();

  // Test normal key input, which should go through IME.
  EXPECT_EQ(shell_surface->GetWidget()->GetFocusManager()->GetFocusedView(),
            textfield_ptr);
  PressAndReleaseKey(ui::VKEY_X);
  EXPECT_EQ(textfield_ptr->GetText(), u"x");
  EXPECT_TRUE(textfield_ptr->GetSelectedText().empty());

  // Controls (Select all) should work.
  PressAndReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(textfield_ptr->GetText(), u"x");
  EXPECT_EQ(textfield_ptr->GetSelectedText(), u"x");

  auto* widget = ash::TestWidgetBuilder()
                     .SetBounds(gfx::Rect(200, 200))
                     .BuildOwnedByNativeWidget();
  ASSERT_TRUE(widget->IsActive());

  PressAndReleaseKey(ui::VKEY_Y);

  EXPECT_EQ(textfield_ptr->GetText(), u"x");
  EXPECT_EQ(textfield_ptr->GetSelectedText(), u"x");

  // Re-activate the surface and make sure that the overlay can still handle
  // keys.
  shell_surface->GetWidget()->Activate();
  // The current text will be replaced with new character because
  // the text is selected.
  PressAndReleaseKey(ui::VKEY_Y);
  EXPECT_EQ(textfield_ptr->GetText(), u"y");
  EXPECT_TRUE(textfield_ptr->GetSelectedText().empty());
}

TEST_F(ShellSurfaceTest, OverlayOverlapsFrame) {
  auto shell_surface = test::ShellSurfaceBuilder({100, 100})
                           .SetFrame(SurfaceFrameType::NORMAL)
                           .BuildShellSurface();
  shell_surface->GetWidget()->GetNativeWindow()->SetProperty(
      aura::client::kSkipImeProcessing, true);

  EXPECT_FALSE(shell_surface->HasOverlay());

  ShellSurfaceBase::OverlayParams params(std::make_unique<views::View>());
  params.overlaps_frame = false;
  shell_surface->AddOverlay(std::move(params));
  EXPECT_TRUE(shell_surface->HasOverlay());

  {
    gfx::Size overlay_size =
        shell_surface->GetWidget()->GetWindowBoundsInScreen().size();
    overlay_size.set_height(
        overlay_size.height() -
        views::GetCaptionButtonLayoutSize(
            views::CaptionButtonLayoutSize::kNonBrowserCaption)
            .height());
    EXPECT_EQ(overlay_size, shell_surface->overlay_widget_for_testing()
                                ->GetWindowBoundsInScreen()
                                .size());
  }

  shell_surface->OnSetFrame(SurfaceFrameType::NONE);
  {
    gfx::Size overlay_size =
        shell_surface->GetWidget()->GetWindowBoundsInScreen().size();
    EXPECT_EQ(overlay_size, shell_surface->overlay_widget_for_testing()
                                ->GetWindowBoundsInScreen()
                                .size());
  }
}

TEST_F(ShellSurfaceTest, AccessibleProperties) {
  auto shell_surface = test::ShellSurfaceBuilder({100, 100})
                           .SetFrame(SurfaceFrameType::NORMAL)
                           .BuildShellSurface();
  EXPECT_EQ(shell_surface->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kClient);
}

TEST_F(ShellSurfaceTest, OverlayCanResize) {
  auto shell_surface = test::ShellSurfaceBuilder({100, 100})
                           .SetFrame(SurfaceFrameType::NORMAL)
                           .BuildShellSurface();
  shell_surface->GetWidget()->GetNativeWindow()->SetProperty(
      aura::client::kSkipImeProcessing, true);

  EXPECT_FALSE(shell_surface->HasOverlay());
  {
    ShellSurfaceBase::OverlayParams params(std::make_unique<views::View>());
    params.can_resize = false;
    shell_surface->AddOverlay(std::move(params));
  }
  EXPECT_FALSE(shell_surface->GetWidget()->widget_delegate()->CanResize());

  shell_surface->RemoveOverlay();
  {
    ShellSurfaceBase::OverlayParams params(std::make_unique<views::View>());
    params.can_resize = true;
    shell_surface->AddOverlay(std::move(params));
  }
  EXPECT_TRUE(shell_surface->GetWidget()->widget_delegate()->CanResize());
}

class TestWindowObserver : public WMHelper::ExoWindowObserver {
 public:
  TestWindowObserver() = default;

  TestWindowObserver(const TestWindowObserver&) = delete;
  TestWindowObserver& operator=(const TestWindowObserver&) = delete;

  // WMHelper::ExoWindowObserver overrides
  void OnExoWindowCreated(aura::Window* window) override {
    windows_.push_back(window);
  }

  const std::vector<raw_ptr<aura::Window, VectorExperimental>>&
  observed_windows() {
    return windows_;
  }

 private:
  std::vector<raw_ptr<aura::Window, VectorExperimental>> windows_;
};

TEST_F(ShellSurfaceTest, NotifyOnWindowCreation) {
  auto shell_surface =
      test::ShellSurfaceBuilder({100, 100}).SetNoCommit().BuildShellSurface();

  TestWindowObserver observer;
  WMHelper::GetInstance()->AddExoWindowObserver(&observer);

  // Committing a surface triggers window creation if it isn't already attached
  // to the root.
  shell_surface->surface_for_testing()->Commit();

  EXPECT_EQ(1u, observer.observed_windows().size());
}

TEST_F(ShellSurfaceTest, Reparent) {
  auto shell_surface1 = test::ShellSurfaceBuilder({20, 20}).BuildShellSurface();
  views::Widget* widget1 = shell_surface1->GetWidget();

  // Create a second window.
  auto shell_surface2 = test::ShellSurfaceBuilder({20, 20}).BuildShellSurface();
  views::Widget* widget2 = shell_surface2->GetWidget();

  auto child_shell_surface =
      test::ShellSurfaceBuilder({20, 20}).BuildShellSurface();
  child_shell_surface->SetParent(shell_surface1.get());
  views::Widget* child_widget = child_shell_surface->GetWidget();
  // By default, a child widget is not activatable. Explicitly make it
  // activatable so that calling child_surface->RequestActivation() is
  // possible.
  child_widget->widget_delegate()->SetCanActivate(true);

  GrantPermissionToActivateIndefinitely(widget1->GetNativeWindow());
  GrantPermissionToActivateIndefinitely(widget2->GetNativeWindow());
  GrantPermissionToActivateIndefinitely(child_widget->GetNativeWindow());

  shell_surface2->Activate();
  EXPECT_FALSE(child_widget->ShouldPaintAsActive());
  EXPECT_FALSE(widget1->ShouldPaintAsActive());
  EXPECT_TRUE(widget2->ShouldPaintAsActive());

  child_shell_surface->Activate();
  // A widget should have the same paint-as-active state with its parent.
  EXPECT_TRUE(child_widget->ShouldPaintAsActive());
  EXPECT_TRUE(widget1->ShouldPaintAsActive());
  EXPECT_FALSE(widget2->ShouldPaintAsActive());

  // Reparent child to widget2.
  child_shell_surface->SetParent(shell_surface2.get());
  EXPECT_TRUE(child_widget->ShouldPaintAsActive());
  EXPECT_TRUE(widget2->ShouldPaintAsActive());
  EXPECT_FALSE(widget1->ShouldPaintAsActive());

  shell_surface1->Activate();
  EXPECT_FALSE(child_widget->ShouldPaintAsActive());
  EXPECT_FALSE(widget2->ShouldPaintAsActive());
  EXPECT_TRUE(widget1->ShouldPaintAsActive());

  // Delete widget1 (i.e. the non-parent widget) shouldn't crash.
  widget1->Close();
  shell_surface1.reset();

  child_shell_surface->Activate();
  EXPECT_TRUE(child_widget->ShouldPaintAsActive());
  EXPECT_TRUE(widget2->ShouldPaintAsActive());
}

TEST_F(ShellSurfaceTest, ThrottleFrameRate) {
  auto shell_surface = test::ShellSurfaceBuilder({20, 20}).BuildShellSurface();
  SurfaceObserverForTest observer(
      shell_surface->root_surface()->window()->GetOcclusionState());
  shell_surface->root_surface()->AddSurfaceObserver(&observer);
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  EXPECT_CALL(observer, ThrottleFrameRate(true));
  window->SetProperty(ash::kFrameRateThrottleKey, true);

  EXPECT_CALL(observer, ThrottleFrameRate(false));
  window->SetProperty(ash::kFrameRateThrottleKey, false);

  shell_surface->root_surface()->RemoveSurfaceObserver(&observer);
}

// Tests raster scale changes happen before occlusion changes on entering
// overview mode, and raster scale changes happen after occlusion changes on
// exiting overview mode. This to ensure windows that become visible do not get
// rastered twice.
TEST_F(ShellSurfaceTest, RasterScaleChangeVsOcclusionChangeOrder) {
  ash::Shell::Get()
      ->raster_scale_controller()
      ->set_raster_scale_slop_proportion_for_testing(0.0f);
  ash::Shell::Get()->overview_controller()->set_windows_have_snapshot_for_test(
      false);
  auto* overview_controller = ash::Shell::Get()->overview_controller();
  overview_controller->set_occlusion_pause_duration_for_end_for_test(
      base::Milliseconds(1));
  std::vector<ConfigureData> config_vec;
  auto shell_surface =
      test::ShellSurfaceBuilder({100, 100})
          .SetConfigureCallback(base::BindRepeating(
              &ConfigureSerialVec, base::Unretained(&config_vec)))
          .BuildShellSurface();
  auto* surface = shell_surface->root_surface();
  aura::Window* root_window = surface->window();
  aura::Window* widget_window = shell_surface->GetWidget()->GetNativeWindow();
  shell_surface->Minimize();

  // Initial configure and configure for minimize.
  EXPECT_EQ(2u, config_vec.size());

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  EXPECT_EQ(aura::Window::OcclusionState::HIDDEN,
            root_window->GetOcclusionState());
  EXPECT_EQ(1.0, widget_window->GetProperty(aura::client::kRasterScale));
  overview_controller->StartOverview(ash::OverviewStartAction::kTests);
  ash::WaitForOverviewEnterAnimation();

  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            root_window->GetOcclusionState());
  EXPECT_NE(1.0, widget_window->GetProperty(aura::client::kRasterScale));

  // Make sure raster scale was changed first.
  ASSERT_EQ(4u, config_vec.size());
  EXPECT_NE(1.0, config_vec[2].raster_scale);
  EXPECT_EQ(aura::Window::OcclusionState::HIDDEN,
            config_vec[2].occlusion_state);
  EXPECT_NE(1.0, config_vec[3].raster_scale);
  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            config_vec[3].occlusion_state);

  ash::WaitForOverviewEnterAnimation();

  // No extra configure should occur.
  EXPECT_EQ(4u, config_vec.size());

  overview_controller->EndOverview(ash::OverviewEndAction::kTests);

  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            root_window->GetOcclusionState());
  EXPECT_NE(1.0, widget_window->GetProperty(aura::client::kRasterScale));

  // No extra configure should occur.
  EXPECT_EQ(4u, config_vec.size());

  ash::WaitForOverviewExitAnimation();

  // Overview mode on exiting pauses the occlusion tracker for a while.
  ash::WaitForOcclusionStateChange(root_window,
                                   aura::Window::OcclusionState::HIDDEN);
  EXPECT_EQ(1.0, widget_window->GetProperty(aura::client::kRasterScale));

  // Make sure occlusion is updated before raster scale.
  ASSERT_EQ(6u, config_vec.size());
  EXPECT_NE(1.0, config_vec[4].raster_scale);
  EXPECT_EQ(aura::Window::OcclusionState::HIDDEN,
            config_vec[4].occlusion_state);
  EXPECT_EQ(1.0, config_vec[5].raster_scale);
  EXPECT_EQ(aura::Window::OcclusionState::HIDDEN,
            config_vec[5].occlusion_state);
}

TEST_F(ShellSurfaceTest, ThrottleFrameRateViaController) {
  ash::FrameThrottlingController* frame_throttling_controller =
      ash::Shell::Get()->frame_throttling_controller();
  for (auto app_type : {chromeos::AppType::LACROS, chromeos::AppType::BROWSER,
                        chromeos::AppType::CROSTINI_APP}) {
    auto shell_surface = test::ShellSurfaceBuilder({20, 20})
                             .SetAppType(app_type)
                             .BuildShellSurface();

    aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
    frame_throttling_controller->StartThrottling({window});

    // Crostini should not be throttled currently.
    const auto should_throttle_set =
        app_type != chromeos::AppType::CROSTINI_APP
            ? testing::UnorderedElementsAreArray(
                  {shell_surface->GetSurfaceId().frame_sink_id()})
            : testing::UnorderedElementsAreArray<viz::FrameSinkId>({});
    EXPECT_THAT(frame_throttling_controller->GetFrameSinkIdsToThrottle(),
                should_throttle_set);

    // ash::kFrameRateThrottleKey is only set for lacros.
    const bool should_set_property = app_type == chromeos::AppType::LACROS;
    EXPECT_EQ(should_set_property,
              window->GetProperty(ash::kFrameRateThrottleKey));
  }
}

TEST_F(ShellSurfaceTest, ThrottleFrameRateViaControllerArc) {
  ash::MockFrameThrottlingObserver observer;
  ash::FrameThrottlingController* frame_throttling_controller =
      ash::Shell::Get()->frame_throttling_controller();
  frame_throttling_controller->AddArcObserver(&observer);

  auto shell_surface = test::ShellSurfaceBuilder({20, 20})
                           .SetAppType(chromeos::AppType::ARC_APP)
                           .BuildShellSurface();

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  EXPECT_CALL(observer,
              OnThrottlingStarted(
                  testing::UnorderedElementsAreArray({window}),
                  frame_throttling_controller->GetCurrentThrottledFrameRate()));
  frame_throttling_controller->StartThrottling({window});

  frame_throttling_controller->RemoveArcObserver(&observer);
}

namespace {

struct ShellSurfaceCallbacks {
  struct ConfigureState {
    gfx::Rect bounds;
    chromeos::WindowStateType state_type;
    bool resizing;
    bool activated;
  };

  uint32_t OnConfigure(
      const gfx::Rect& bounds,
      chromeos::WindowStateType state_type,
      bool resizing,
      bool activated,
      const gfx::Vector2d& origin_offset,
      float raster_scale,
      aura::Window::OcclusionState occlusion_state,
      std::optional<chromeos::WindowStateType> restore_state_type) {
    configure_state.emplace();
    *configure_state = {bounds, state_type, resizing, activated};
    return ++serial;
  }
  void OnOriginChange(const gfx::Point& origin_) { origin = origin_; }
  void Reset() {
    configure_state.reset();
    origin.reset();
  }
  std::optional<ConfigureState> configure_state;
  std::optional<gfx::Point> origin;
  int32_t serial = 0;
};

}  // namespace

TEST_F(ShellSurfaceTest, DragWithHTCLIENT) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64}).BuildShellSurface();
  ShellSurfaceCallbacks callbacks;
  shell_surface->set_configure_callback(base::BindRepeating(
      &ShellSurfaceCallbacks::OnConfigure, base::Unretained(&callbacks)));
  ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow())
      ->CreateDragDetails(gfx::PointF(0, 0), HTCLIENT,
                          ::wm::WINDOW_MOVE_SOURCE_TOUCH);

  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 80, 80));
  shell_surface->AcknowledgeConfigure(callbacks.serial);
  shell_surface->root_surface()->Commit();
}

TEST_F(ShellSurfaceTest, ScreenCoordinates) {
  exo::test::TestSecurityDelegate securityDelegate;
  securityDelegate.SetCanSetBounds(
      SecurityDelegate::SetBoundsPolicy::DCHECK_IF_DECORATED);
  auto shell_surface = test::ShellSurfaceBuilder({20, 20})
                           .SetSecurityDelegate(&securityDelegate)
                           .BuildShellSurface();
  ShellSurfaceCallbacks callbacks;

  shell_surface->set_configure_callback(base::BindRepeating(
      &ShellSurfaceCallbacks::OnConfigure, base::Unretained(&callbacks)));
  shell_surface->set_origin_change_callback(base::BindRepeating(
      &ShellSurfaceCallbacks::OnOriginChange, base::Unretained(&callbacks)));

  shell_surface->SetWindowBounds(gfx::Rect(10, 10, 300, 300));
  ASSERT_TRUE(!!callbacks.configure_state);
  ASSERT_TRUE(!callbacks.origin);
  EXPECT_EQ(callbacks.configure_state->bounds, gfx::Rect(10, 10, 300, 300));

  callbacks.Reset();
  shell_surface->SetWindowBounds(gfx::Rect(100, 100, 300, 300));
  ASSERT_TRUE(!callbacks.configure_state);
  ASSERT_TRUE(!!callbacks.origin);
  EXPECT_EQ(*callbacks.origin, gfx::Point(100, 100));

  callbacks.Reset();
  shell_surface->SetWindowBounds(gfx::Rect(0, 0, 300000, 300000));
  ASSERT_TRUE(!!callbacks.configure_state);
  EXPECT_EQ(callbacks.configure_state->bounds,
            display::Screen::GetScreen()->GetPrimaryDisplay().work_area());
}

TEST_F(ShellSurfaceTest, InitialBounds) {
  {
    gfx::Size size(20, 30);
    auto shell_surface =
        test::ShellSurfaceBuilder(size).SetNoCommit().BuildShellSurface();

    EXPECT_FALSE(shell_surface->GetWidget());
    gfx::Rect bounds(gfx::Point(35, 135), size);
    shell_surface->SetWindowBounds(bounds);
    shell_surface->root_surface()->Commit();

    ASSERT_TRUE(shell_surface->GetWidget());
    EXPECT_EQ(bounds, shell_surface->GetWidget()->GetWindowBoundsInScreen());
  }
  {
    // Requesting larger than display work area bounds should not be allowed.
    gfx::Size size(2000, 3000);
    auto shell_surface =
        test::ShellSurfaceBuilder(size).SetNoCommit().BuildShellSurface();

    EXPECT_FALSE(shell_surface->GetWidget());
    gfx::Rect bounds(gfx::Point(35, 135), size);
    shell_surface->SetWindowBounds(bounds);
    shell_surface->root_surface()->Commit();

    ASSERT_TRUE(shell_surface->GetWidget());
    EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().work_area(),
              shell_surface->GetWidget()->GetWindowBoundsInScreen());
  }
}

// Make sure that the centering logic can use the correct size
// even if there is a pending configure.
TEST_F(ShellSurfaceTest, InitialCenteredBoundsWithConfigure) {
  auto shell_surface = test::ShellSurfaceBuilder(gfx::Size(0, 0))
                           .SetNoRootBuffer()
                           .SetNoCommit()
                           .BuildShellSurface();
  EXPECT_FALSE(shell_surface->IsReady());
  ShellSurfaceCallbacks callbacks;
  shell_surface->set_configure_callback(base::BindRepeating(
      &ShellSurfaceCallbacks::OnConfigure, base::Unretained(&callbacks)));
  shell_surface->root_surface()->Commit();
  EXPECT_FALSE(shell_surface->GetWidget()->IsVisible());
  EXPECT_FALSE(shell_surface->IsReady());

  gfx::Size size(256, 256);
  auto new_buffer = test::ExoTestHelper::CreateBuffer(size);
  shell_surface->root_surface()->Attach(new_buffer.get());
  shell_surface->root_surface()->Commit();
  EXPECT_TRUE(shell_surface->GetWidget()->IsVisible());
  EXPECT_TRUE(shell_surface->IsReady());

  gfx::Rect expected =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  expected.ClampToCenteredSize(size);
  EXPECT_EQ(expected, shell_surface->GetWidget()->GetWindowBoundsInScreen());
}

// Test that restore info is set correctly.
TEST_F(ShellSurfaceTest, SetRestoreInfo) {
  int32_t restore_session_id = 200;
  int32_t restore_window_id = 100;

  gfx::Size size(20, 30);
  auto shell_surface =
      test::ShellSurfaceBuilder(size).SetNoCommit().BuildShellSurface();

  shell_surface->SetRestoreInfo(restore_session_id, restore_window_id);
  shell_surface->Restore();
  shell_surface->root_surface()->Commit();

  EXPECT_TRUE(shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(restore_session_id,
            shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                app_restore::kWindowIdKey));
  EXPECT_EQ(restore_window_id,
            shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                app_restore::kRestoreWindowIdKey));
}

TEST_F(ShellSurfaceTest, SetNotPersistable) {
  auto shell_surface = test::ShellSurfaceBuilder(gfx::Size(20, 30))
                           .SetNoCommit()
                           .BuildShellSurface();
  shell_surface->SetPersistable(/*persistable=*/false);
  shell_surface->root_surface()->Commit();

  EXPECT_TRUE(shell_surface->GetWidget()->IsVisible());
  EXPECT_FALSE(shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
      wm::kPersistableKey));
}

// Test that restore id is set correctly.
TEST_F(ShellSurfaceTest, SetRestoreInfoWithWindowIdSource) {
  int32_t restore_session_id = 200;
  const std::string app_id = "app_id";

  gfx::Size size(20, 30);
  auto shell_surface =
      test::ShellSurfaceBuilder(size).SetNoCommit().BuildShellSurface();

  shell_surface->SetRestoreInfoWithWindowIdSource(restore_session_id, app_id);
  shell_surface->Restore();
  shell_surface->root_surface()->Commit();

  EXPECT_TRUE(shell_surface->GetWidget()->IsVisible());

  // FetchRestoreWindowId will return 0, because no app with id "app_id" is
  // installed.
  EXPECT_EQ(0, shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                   app_restore::kRestoreWindowIdKey));
  EXPECT_EQ(app_id, *shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                        app_restore::kAppIdKey));
}

// Surfaces without non-client view should not crash.
TEST_F(ShellSurfaceTest, NoNonClientViewWithConfigure) {
  exo::test::TestSecurityDelegate securityDelegate;
  securityDelegate.SetCanSetBounds(
      SecurityDelegate::SetBoundsPolicy::DCHECK_IF_DECORATED);
  // Popup windows don't have a non-client view.
  auto shell_surface = test::ShellSurfaceBuilder({20, 20})
                           .SetAsPopup()
                           .SetSecurityDelegate(&securityDelegate)
                           .BuildShellSurface();
  ShellSurfaceCallbacks callbacks;

  // Having a configure callback leads to a call to GetClientBoundsInScreen().
  shell_surface->set_configure_callback(base::BindRepeating(
      &ShellSurfaceCallbacks::OnConfigure, base::Unretained(&callbacks)));

  shell_surface->SetWindowBounds(gfx::Rect(10, 10, 300, 300));
  EXPECT_TRUE(callbacks.configure_state);
  EXPECT_EQ(callbacks.configure_state->bounds, gfx::Rect(10, 10, 300, 300));
}

TEST_F(ShellSurfaceTest, WindowIsResizableWithEmptySizeConstraints) {
  auto shell_surface = test::ShellSurfaceBuilder({20, 20})
                           .SetMinimumSize(gfx::Size(0, 0))
                           .SetMaximumSize(gfx::Size(0, 0))
                           .BuildShellSurface();
  EXPECT_TRUE(shell_surface->GetWidget()->widget_delegate()->CanResize());
}

TEST_F(ShellSurfaceTest, SetSystemModal) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetUseSystemModalContainer()
          .SetNoCommit()
          .BuildShellSurface();

  shell_surface->SetSystemModal(true);
  shell_surface->root_surface()->Commit();

  EXPECT_EQ(ui::mojom::ModalType::kSystem, shell_surface->GetModalType());
  EXPECT_FALSE(shell_surface->frame_enabled());
}

TEST_F(ShellSurfaceTest, PipInitialPosition) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetUseSystemModalContainer()
          .SetNoCommit()
          .BuildShellSurface();
  shell_surface->SetWindowBounds(gfx::Rect(20, 20, 256, 256));
  shell_surface->SetPip();
  shell_surface->root_surface()->Commit();
  // PIP positioner place the PIP window to the edge that is closer to the given
  // position
  EXPECT_EQ(gfx::Rect(8, 20, 256, 256),
            shell_surface->GetWidget()->GetWindowBoundsInScreen());
}

TEST_F(ShellSurfaceTest, PostWindowChangeCallback) {
  chromeos::WindowStateType state_type = chromeos::WindowStateType::kDefault;
  auto test_callback = base::BindRepeating(
      [](chromeos::WindowStateType* state_type, const gfx::Rect&,
         chromeos::WindowStateType new_type, bool, bool, const gfx::Vector2d&,
         float, aura::Window::OcclusionState,
         std::optional<chromeos::WindowStateType>) -> uint32_t {
        *state_type = new_type;
        return 0;
      },
      &state_type);

  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();

  shell_surface->set_configure_callback(test_callback);

  auto* state = ash::WindowState::Get(
      shell_surface->GetWidget()->GetNativeWindow()->GetToplevelWindow());

  // Make sure we are in a non-snapped state before testing state change.
  ASSERT_FALSE(state->IsSnapped());

  auto snap_event =
      std::make_unique<ash::WindowSnapWMEvent>(ash::WM_EVENT_SNAP_PRIMARY);

  // Trigger a snap event, this should cause a configure event.
  state->OnWMEvent(snap_event.get());

  EXPECT_EQ(state_type, chromeos::WindowStateType::kPrimarySnapped);
}

// A single configuration event should be sent when both the bounds and the
// window state change.
TEST_F(ShellSurfaceTest, ConfigureOnlySentOnceForBoundsAndWindowStateChange) {
  int times_configured = 0;
  auto test_callback = base::BindRepeating(
      [](int* times_configured, const gfx::Rect&,
         chromeos::WindowStateType new_type, bool, bool, const gfx::Vector2d&,
         float, aura::Window::OcclusionState,
         std::optional<chromeos::WindowStateType>) -> uint32_t {
        ++(*times_configured);
        return 0;
      },
      &times_configured);

  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({1, 1}).BuildShellSurface();

  shell_surface->set_configure_callback(test_callback);

  auto* state = ash::WindowState::Get(
      shell_surface->GetWidget()->GetNativeWindow()->GetToplevelWindow());

  // Make sure we are in normal mode. Maximizing from this state should result
  // in BOTH the bounds and state changing.
  ASSERT_TRUE(state->IsNormalStateType());

  auto maximize_event = std::make_unique<ash::WMEvent>(ash::WM_EVENT_MAXIMIZE);

  // Trigger a snap event, this should cause a configure event.
  state->OnWMEvent(maximize_event.get());

  // The bounds change event should have been suppressed because the window
  // state is changing.
  EXPECT_EQ(times_configured, 1);
}

TEST_F(ShellSurfaceTest, SetImmersiveModeTriggersConfigure) {
  int times_configured = 0;
  auto test_callback = base::BindRepeating(
      [](int* times_configured, const gfx::Rect&,
         chromeos::WindowStateType new_type, bool, bool, const gfx::Vector2d&,
         float, aura::Window::OcclusionState,
         std::optional<chromeos::WindowStateType>) -> uint32_t {
        ++(*times_configured);
        return 0;
      },
      &times_configured);

  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({1, 1}).BuildShellSurface();

  shell_surface->set_configure_callback(test_callback);

  shell_surface->SetUseImmersiveForFullscreen(true);

  EXPECT_EQ(times_configured, 1);
}

TEST_F(ShellSurfaceTest,
       SetRasterScaleWindowPropertyConfiguresRasterScaleAndWaitsForAck) {
  ConfigureData config_data;
  constexpr gfx::Size kBufferSize(256, 256);

  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();

  shell_surface->set_configure_callback(
      base::BindRepeating(&Configure, base::Unretained(&config_data)));

  auto* window = shell_surface->GetWidget()->GetNativeWindow();
  window->SetProperty(aura::client::kRasterScale, 0.1f);
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_EQ(0.1f, config_data.raster_scale);

  window->SetProperty(aura::client::kRasterScale, 1.0f);
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_EQ(1.0f, config_data.raster_scale);
}

TEST_F(ShellSurfaceTest, MoveParentWithoutWidget) {
  UpdateDisplay("800x600, 800x600");
  constexpr gfx::Size kSize{256, 256};
  std::unique_ptr<ShellSurface> parent_surface =
      test::ShellSurfaceBuilder(kSize).BuildShellSurface();

  std::unique_ptr<ShellSurface> child_surface =
      test::ShellSurfaceBuilder(kSize).SetNoCommit().BuildShellSurface();
  child_surface->SetParent(parent_surface.get());
  auto* parent_widget = parent_surface->GetWidget();
  auto* root_before = parent_widget->GetNativeWindow()->GetRootWindow();
  parent_widget->SetBounds({{1000, 0}, kSize});
  // Crash (crbug.com/1395433) happens when a transient parent moved
  // to another root window before widget is created. Make sure that
  // happened.
  EXPECT_NE(root_before, parent_widget->GetNativeWindow()->GetRootWindow());
}

// Assert SetShape() applies the shape to the host window's layer on commit.
TEST_F(ShellSurfaceTest, SetShapeAppliedAfterSurfaceCommit) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64}).BuildShellSurface();
  shell_surface->OnSetServerStartResize();
  shell_surface->OnSetFrame(SurfaceFrameType::SHADOW);
  shell_surface->root_surface()->Commit();
  const views::Widget* widget = shell_surface->GetWidget();
  ASSERT_TRUE(widget);

  // Windows shadows should be applied with resizing enabled.
  EXPECT_NE(wm::kShadowElevationNone,
            wm::GetShadowElevationConvertDefault(widget->GetNativeWindow()));
  EXPECT_TRUE(shell_surface->server_side_resize());

  // Create a window shape from two unique rects.
  const cc::Region shape_region =
      CreateRegion({{10, 10, 32, 32}, {20, 20, 32, 32}});

  // Apply the shape to the surface. This should not yet be reflected on the
  // host window's layer.
  shell_surface->SetShape(shape_region);
  const ui::Layer::ShapeRects* layer_shape_rects =
      widget->GetNativeWindow()->layer()->alpha_shape();
  EXPECT_FALSE(layer_shape_rects);

  // After surface commit the shape should have been applied to the layer.
  shell_surface->root_surface()->Commit();
  layer_shape_rects = widget->GetNativeWindow()->layer()->alpha_shape();
  EXPECT_TRUE(layer_shape_rects);
  EXPECT_EQ(shape_region, CreateRegion(*layer_shape_rects));

  // Window shadows and resizing should be disabled when window shapes are set.
  EXPECT_EQ(wm::kShadowElevationNone,
            wm::GetShadowElevationConvertDefault(widget->GetNativeWindow()));
  EXPECT_FALSE(shell_surface->server_side_resize());

  // Ensure the window targeter correctly passes through events to areas of the
  // window not covered by the shape.
  gfx::Rect target_bounds = widget->GetWindowBoundsInScreen();
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  {
    // Send an event to the point just outside of the region, it should not
    // target the root surface.
    gfx::Point location = target_bounds.origin() + gfx::Vector2d(9, 9);
    ui::MouseEvent event(ui::EventType::kMouseMoved, location, location,
                         ui::EventTimeForNow(), 0, 0);
    event_generator->Dispatch(&event);
    EXPECT_NE(shell_surface->root_surface(),
              GetTargetSurfaceForLocatedEvent(&event));
  }
  {
    // Send an event to the point just within of the region, it should target
    // the root surface.
    gfx::Point location = target_bounds.origin() + gfx::Vector2d(11, 11);
    ui::MouseEvent event(ui::EventType::kMouseMoved, location, location,
                         ui::EventTimeForNow(), 0, 0);
    event_generator->Dispatch(&event);
    EXPECT_EQ(shell_surface->root_surface(),
              GetTargetSurfaceForLocatedEvent(&event));
  }
}

// Assert SetShape() updates the host window's layer with the most recent shape
// when the surface commits.
TEST_F(ShellSurfaceTest, SetShapeUpdatesAndUnsetsCorrectlyAfterCommit) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64})
          .SetFrame(SurfaceFrameType::SHADOW)
          .BuildShellSurface();
  shell_surface->OnSetServerStartResize();
  shell_surface->root_surface()->Commit();
  const views::Widget* widget = shell_surface->GetWidget();
  ASSERT_TRUE(widget);

  // Create several unique window shapes.
  const cc::Region shape_region_1 =
      CreateRegion({{5, 5, 32, 32}, {10, 10, 32, 32}});
  const cc::Region shape_region_2 =
      CreateRegion({{15, 15, 32, 32}, {20, 20, 32, 32}});
  const cc::Region shape_region_3 =
      CreateRegion({{25, 25, 32, 32}, {30, 40, 32, 32}});

  // Apply two shapes to the surface without committing. Neither should be
  // applied to the host window's layer.
  shell_surface->SetShape(shape_region_1);
  shell_surface->SetShape(shape_region_2);
  const ui::Layer::ShapeRects* layer_shape_rects =
      widget->GetNativeWindow()->layer()->alpha_shape();
  EXPECT_FALSE(layer_shape_rects);

  // After surface commit only the most recent shape should have been applied.
  shell_surface->root_surface()->Commit();
  layer_shape_rects = widget->GetNativeWindow()->layer()->alpha_shape();
  EXPECT_TRUE(layer_shape_rects);
  EXPECT_EQ(shape_region_2, CreateRegion(*layer_shape_rects));

  // Apply another shape to the surface. The layer shape should not change.
  shell_surface->SetShape(shape_region_3);
  layer_shape_rects = widget->GetNativeWindow()->layer()->alpha_shape();
  EXPECT_TRUE(layer_shape_rects);
  EXPECT_EQ(shape_region_2, CreateRegion(*layer_shape_rects));

  // The new shape should have been applied after the surface is committed.
  shell_surface->root_surface()->Commit();
  layer_shape_rects = widget->GetNativeWindow()->layer()->alpha_shape();
  EXPECT_TRUE(layer_shape_rects);
  EXPECT_EQ(shape_region_3, CreateRegion(*layer_shape_rects));

  // Setting a null shape should unset the host window's layer shape.
  shell_surface->SetShape(std::nullopt);
  shell_surface->root_surface()->Commit();
  layer_shape_rects = widget->GetNativeWindow()->layer()->alpha_shape();
  EXPECT_FALSE(layer_shape_rects);
}

// SetShape() is not supported for windows with the frame enabled.
TEST_F(ShellSurfaceTest, SetShapeWithFrameNotSupported) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64})
          .SetFrame(SurfaceFrameType::NORMAL)
          .BuildShellSurface();

  shell_surface->OnSetServerStartResize();
  shell_surface->root_surface()->Commit();
  const views::Widget* widget = shell_surface->GetWidget();
  ASSERT_TRUE(widget);

  // Create a window shape from two unique rects.
  const cc::Region shape_region =
      CreateRegion({{10, 10, 32, 32}, {20, 20, 32, 32}});

  // Try to apply the shape to the surface and commit, this should have no
  // effect.
  shell_surface->SetShape(shape_region);
  const ui::Layer::ShapeRects* layer_shape_rects =
      widget->GetNativeWindow()->layer()->alpha_shape();
  shell_surface->root_surface()->Commit();
  layer_shape_rects = widget->GetNativeWindow()->layer()->alpha_shape();
  EXPECT_FALSE(layer_shape_rects);
}

TEST_F(ShellSurfaceTest, MaximizedOrFullscreenInitialState) {
  UpdateDisplay("800x600, 800x600");
  constexpr gfx::Size kEmptySize{0, 0};
  // on secondary display.
  constexpr gfx::Rect kInitialBounds{800, 0, 100, 100};
  const auto primary_display = GetPrimaryDisplay();
  const auto secondary_display = GetSecondaryDisplay();
  for (auto initial_state : {chromeos::WindowStateType::kMaximized,
                             chromeos::WindowStateType::kFullscreen}) {
    std::stringstream ss;
    ss << initial_state;
    SCOPED_TRACE(ss.str());
    gfx::Rect primary_bounds =
        initial_state == chromeos::WindowStateType::kMaximized
            ? primary_display.work_area()
            : primary_display.bounds();
    gfx::Rect secondary_bounds =
        initial_state == chromeos::WindowStateType::kMaximized
            ? secondary_display.work_area()
            : secondary_display.bounds();
    // While it is possible to start in fullscreen, SessionRestore restores the
    // originally fullscreen window to maximized, so the fullscreen window won't
    // have restore bounds.
    bool verify_restore_bounds =
        initial_state == chromeos::WindowStateType::kMaximized;
    {
      ConfigureData config_data;
      std::unique_ptr<ShellSurface> shell_surface =
          test::ShellSurfaceBuilder(kEmptySize)
              .SetConfigureCallback(base::BindRepeating(
                  &Configure, base::Unretained(&config_data)))
              .SetWindowState(initial_state)
              .BuildShellSurface();
      EXPECT_EQ(1u, config_data.count);
      EXPECT_EQ(initial_state, config_data.state_type);
      EXPECT_EQ(primary_bounds, config_data.suggested_bounds);
    }
    {
      ConfigureData config_data;
      std::unique_ptr<ShellSurface> shell_surface =
          test::ShellSurfaceBuilder(kEmptySize)
              .SetConfigureCallback(base::BindRepeating(
                  &Configure, base::Unretained(&config_data)))
              .SetWindowState(initial_state)
              .SetDisplayId(secondary_display.id())
              .BuildShellSurface();
      EXPECT_EQ(1u, config_data.count);
      EXPECT_EQ(initial_state, config_data.state_type);
      EXPECT_EQ(secondary_bounds, config_data.suggested_bounds);
    }
    {
      ConfigureData config_data;
      std::unique_ptr<ShellSurface> shell_surface =
          test::ShellSurfaceBuilder(kEmptySize)
              .SetConfigureCallback(base::BindRepeating(
                  &Configure, base::Unretained(&config_data)))
              .SetWindowState(initial_state)
              .SetBounds(kInitialBounds)
              .BuildShellSurface();
      EXPECT_EQ(1u, config_data.count);
      EXPECT_EQ(initial_state, config_data.state_type);
      EXPECT_EQ(secondary_bounds, config_data.suggested_bounds);
      EXPECT_EQ(secondary_bounds,
                shell_surface->GetWidget()->GetWindowBoundsInScreen());
      if (verify_restore_bounds) {
        EXPECT_EQ(kInitialBounds,
                  shell_surface->GetWidget()->GetRestoredBounds());
      }
    }
    {
      ConfigureData config_data;
      std::unique_ptr<ShellSurface> shell_surface =
          test::ShellSurfaceBuilder(kEmptySize)
              .SetConfigureCallback(base::BindRepeating(
                  &Configure, base::Unretained(&config_data)))
              .SetWindowState(initial_state)
              .SetBounds(kInitialBounds)
              .BuildShellSurface();
      EXPECT_EQ(1u, config_data.count);
      EXPECT_EQ(initial_state, config_data.state_type);
      EXPECT_EQ(secondary_bounds, config_data.suggested_bounds);
      if (verify_restore_bounds) {
        EXPECT_EQ(kInitialBounds,
                  shell_surface->GetWidget()->GetRestoredBounds());
      }
    }
    {
      // The display id has higher priority.
      ConfigureData config_data;
      std::unique_ptr<ShellSurface> shell_surface =
          test::ShellSurfaceBuilder(kEmptySize)
              .SetConfigureCallback(base::BindRepeating(
                  &Configure, base::Unretained(&config_data)))
              .SetWindowState(initial_state)
              .SetBounds(kInitialBounds)
              .SetDisplayId(primary_display.id())
              .BuildShellSurface();
      EXPECT_EQ(1u, config_data.count);
      EXPECT_EQ(initial_state, config_data.state_type);
      EXPECT_EQ(primary_bounds, config_data.suggested_bounds);
      if (verify_restore_bounds) {
        EXPECT_EQ(kInitialBounds,
                  shell_surface->GetWidget()->GetRestoredBounds());
      }
    }
  }
}

TEST_F(ShellSurfaceTest, MinimizedInitialState) {
  constexpr gfx::Rect kInitialBounds(100, 50, 400, 300);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder()
          .SetBounds(kInitialBounds)
          .SetGeometry(gfx::Rect(kInitialBounds.size()))
          .SetWindowState(chromeos::WindowStateType::kMinimized)
          .BuildShellSurface();
  EXPECT_TRUE(shell_surface->GetWidget()->IsMinimized());
  EXPECT_EQ(kInitialBounds, shell_surface->GetWidget()->GetRestoredBounds());
  // The buffer hasn't been attached yet.
  ASSERT_FALSE(shell_surface->root_surface()->GetBuffer());

  // Initial buffer arrives and the window should stay in minimized.
  auto new_buffer = test::ExoTestHelper::CreateBuffer(kInitialBounds.size());
  shell_surface->root_surface()->Attach(new_buffer.get());
  shell_surface->root_surface()->Commit();
  EXPECT_TRUE(shell_surface->GetWidget()->IsMinimized());

  shell_surface->GetWidget()->Activate();
  EXPECT_FALSE(shell_surface->GetWidget()->IsMinimized());
  EXPECT_EQ(kInitialBounds,
            shell_surface->GetWidget()->GetWindowBoundsInScreen());
}

TEST_F(ShellSurfaceTest, NoGeometryWidgetBoundsUpdate) {
  constexpr gfx::Size kInitialSize(100, 100);
  constexpr gfx::Size kLargerSize(256, 256);
  constexpr gfx::Size kSmallerSize(50, 50);

  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder(kInitialSize).BuildShellSurface();

  EXPECT_EQ(kInitialSize,
            shell_surface->GetWidget()->GetWindowBoundsInScreen().size());

  auto larger_buffer = test::ExoTestHelper::CreateBuffer(kLargerSize);

  shell_surface->root_surface()->Attach(larger_buffer.get());
  shell_surface->root_surface()->Commit();

  EXPECT_EQ(kLargerSize,
            shell_surface->GetWidget()->GetWindowBoundsInScreen().size());

  auto smaller_buffer = test::ExoTestHelper::CreateBuffer(kSmallerSize);

  shell_surface->root_surface()->Attach(smaller_buffer.get());
  shell_surface->root_surface()->Commit();

  EXPECT_EQ(kSmallerSize,
            shell_surface->GetWidget()->GetWindowBoundsInScreen().size());
}

TEST_F(ShellSurfaceTest, SubpixelPositionOffset) {
  UpdateDisplay("1200x800*1.6");
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetOrigin({20, 20})
          .SetFrame(SurfaceFrameType::NORMAL)
          .BuildShellSurface();
  // Enabling a normal frame makes `GetClientViewBounds().origin()` return
  // (0, 32), which makes the host window not align with any pixel boundary.
  shell_surface->root_surface()->SetSurfaceHierarchyContentBoundsForTest(
      gfx::Rect(-20, -20, 256, 256));
  EXPECT_TRUE(shell_surface->OnPreWidgetCommit());
  shell_surface->CommitWidget();
  EXPECT_EQ(gfx::Point(20, 20), shell_surface->root_surface_origin_pixel());
  EXPECT_EQ(gfx::Rect(-12, 20, 256, 256),
            shell_surface->host_window()->bounds());
  // Verify that 'root_surface_origin()' is exactly preservered in pixels with
  // subpixel offset.
  // (0, -0.125) is caused by the frame like above, and (-0.5, -0.5) is for
  // 'root_surface_origin()'.
  EXPECT_EQ(gfx::Vector2dF(-0.5, -0.625),
            shell_surface->host_window()->layer()->GetSubpixelOffset());
}

// Make sure the shell surface with capture can be safely deleted
// even if the widget is not visible.
TEST_F(ShellSurfaceTest, DeleteWithGrab) {
  auto shell_surface =
      test::ShellSurfaceBuilder({200, 200}).BuildShellSurface();
  auto popup_shell_surface = test::ShellSurfaceBuilder({100, 100})
                                 .SetAsPopup()
                                 .SetParent(shell_surface.get())
                                 .SetGrab()
                                 .BuildShellSurface();
  popup_shell_surface->GetWidget()->GetNativeWindow()->layer()->SetVisible(
      false);
  popup_shell_surface.reset();

  // Close with grab.
  popup_shell_surface = test::ShellSurfaceBuilder({100, 100})
                            .SetAsPopup()
                            .SetParent(shell_surface.get())
                            .SetGrab()
                            .BuildShellSurface();
  popup_shell_surface->GetWidget()->Close();
  // Close is async.
  base::RunLoop().RunUntilIdle();
  popup_shell_surface.reset();

  // CloseNow with grab.
  popup_shell_surface = test::ShellSurfaceBuilder({100, 100})
                            .SetAsPopup()
                            .SetParent(shell_surface.get())
                            .SetGrab()
                            .BuildShellSurface();
  popup_shell_surface->GetWidget()->CloseNow();
  popup_shell_surface.reset();
}

TEST_F(ShellSurfaceTest, WindowPropertyChangedNotificationWithoutRootSurface) {
  // Test OnWindowPropertyChanged() notification on a ShellSurface, whose root
  // surface has gone.

  auto* overview_controller = ash::Shell::Get()->overview_controller();

  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetAppType(chromeos::AppType::LACROS)
          .BuildShellSurface();

  std::unique_ptr<ShellSurface> shell_surface1 =
      test::ShellSurfaceBuilder({256, 256})
          .SetAppType(chromeos::AppType::LACROS)
          .BuildShellSurface();

  overview_controller->StartOverview(ash::OverviewStartAction::kTests);
  ash::WaitForOverviewEnterAnimation();

  test::ShellSurfaceBuilder::DestroyRootSurface(shell_surface1.get());

  // Destroying `shell_surface` will close its aura window, causing update of
  // frame throttling in the overview mode for the remaining Lacros window(s).
  // In this case, the remaining window is associated with `shell_surface1`. It
  // receives OnWindowPropertyChanged() notification with
  // ash::kFrameRateThrottleKey key. The root surface of `shell_surface1` has
  // gone at this point. Verify that it doesn't cause crash.
  shell_surface.reset();

  overview_controller->EndOverview(ash::OverviewEndAction::kTests);
}

TEST_F(ShellSurfaceTest, SurfaceSyncWithShellSurfaceCreatedOnDisplayWithScale) {
  ash::Shell::GetRootWindowForNewWindows()->layer()->OnDeviceScaleFactorChanged(
      2.f);

  // Empty means no initial size.
  constexpr gfx::Size kEmptySize(0, 0);
  ConfigureData config_data;
  auto shell_surface =
      test::ShellSurfaceBuilder(kEmptySize)
          .SetNoCommit()
          .SetConfigureCallback(base::BindRepeating(
              &ConfigureSerial, base::Unretained(&config_data)))
          .BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  // Creates the shell widget, and add the surface_tree_host's window as child.
  surface->Commit();

  EXPECT_EQ(config_data.count, 1u);
  EXPECT_EQ(config_data.suggested_bounds, gfx::Rect());
  EXPECT_EQ(shell_surface->GetCommitTargetLayer(),
            shell_surface->host_window()->layer());
  EXPECT_EQ(shell_surface->GetSurfaceId(),
            shell_surface->host_window()->GetSurfaceId());
}

TEST_F(ShellSurfaceTest,
       DisplayScaleChangeTakesCompositorLockForMaximizedWindow) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetWindowState(chromeos::WindowStateType::kMaximized)
          .BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  uint32_t serial = 0;
  auto configure_callback = base::BindRepeating(
      [](uint32_t* const serial_ptr, const gfx::Rect& bounds,
         chromeos::WindowStateType state_type, bool resizing, bool activated,
         const gfx::Vector2d& origin_offset, float raster_scale,
         aura::Window::OcclusionState occlusion_state,
         std::optional<chromeos::WindowStateType>) { return ++(*serial_ptr); },
      &serial);
  shell_surface->set_configure_callback(configure_callback);

  ui::Compositor* compositor =
      shell_surface->GetWidget()->GetNativeWindow()->layer()->GetCompositor();
  EXPECT_FALSE(compositor->IsLocked());

  auto* display_manager = ash::Shell::Get()->display_manager();
  const auto display_id = display_manager->GetDisplayAt(0).id();
  display_manager->ZoomDisplay(display_id, /*up=*/true);

  // Compositor locked until maximized window updates.
  EXPECT_TRUE(compositor->IsLocked());

  shell_surface->AcknowledgeConfigure(serial);
  EXPECT_TRUE(compositor->IsLocked());

  surface->Commit();
  EXPECT_FALSE(compositor->IsLocked());

  display_manager->ZoomDisplay(display_id, /*up=*/false);

  // Compositor locked until maximized window updates.
  EXPECT_TRUE(compositor->IsLocked());

  shell_surface->AcknowledgeConfigure(serial);
  EXPECT_TRUE(compositor->IsLocked());

  surface->Commit();
  EXPECT_FALSE(compositor->IsLocked());
}

TEST_F(ShellSurfaceTest,
       DisplayScaleChangeDoesNotTakeCompositorLockForFreeformWindow) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();

  ui::Compositor* compositor =
      shell_surface->GetWidget()->GetNativeWindow()->layer()->GetCompositor();
  EXPECT_FALSE(compositor->IsLocked());

  auto* display_manager = ash::Shell::Get()->display_manager();
  const auto display_id = display_manager->GetDisplayAt(0).id();
  display_manager->ZoomDisplay(display_id, /*up=*/true);

  // Should not take compositor lock.
  EXPECT_FALSE(compositor->IsLocked());

  display_manager->ZoomDisplay(display_id, /*up=*/false);

  // Should not take compositor lock.
  EXPECT_FALSE(compositor->IsLocked());
}

// Tests that updates to the display layout configuration update the origin on
// relevant hosted surfaces.
TEST_F(ShellSurfaceTest, DisplayLayoutConfigurationUpdatesSurfaceOrigin) {
  // Start with a single display configuration.
  UpdateDisplay("800x600");

  // Create the surface.
  constexpr gfx::Size kBufferSize(256, 256);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).SetNoCommit().BuildShellSurface();

  // Set origin and leave/enter callbacks.
  gfx::Point client_origin;
  int64_t old_display_id = display::kInvalidDisplayId;
  int64_t new_display_id = display::kInvalidDisplayId;
  shell_surface->set_origin_change_callback(base::BindLambdaForTesting(
      [&](const gfx::Point& origin) { client_origin = origin; }));
  shell_surface->root_surface()->set_leave_enter_callback(
      base::BindLambdaForTesting([&](int64_t old_id, int64_t new_id) {
        old_display_id = old_id;
        new_display_id = new_id;
        return true;
      }));

  // Creating a new shell surface should notify on which display it is created.
  constexpr gfx::Point kInitialOrigin = {200, 200};
  shell_surface->root_surface()->Commit();
  shell_surface->GetWidget()->SetBounds({kInitialOrigin, kBufferSize});
  EXPECT_EQ(display::kInvalidDisplayId, old_display_id);
  EXPECT_EQ(GetPrimaryDisplay().id(), new_display_id);
  EXPECT_EQ(kInitialOrigin, client_origin);

  // Attaching a second display should not change where the surface is located.
  UpdateDisplay("800x600,800x600");
  EXPECT_EQ(kInitialOrigin, client_origin);

  // Move the window to second display.
  constexpr gfx::Point kNewOrigin = {1000, 200};
  shell_surface->GetWidget()->SetBounds({kNewOrigin, kBufferSize});
  EXPECT_EQ(GetPrimaryDisplay().id(), old_display_id);
  EXPECT_EQ(GetSecondaryDisplay().id(), new_display_id);
  EXPECT_EQ(kNewOrigin, client_origin);

  // Reposition the second display, the surface should receive an origin change
  // event representing the updated bounds in screen coordinates.
  constexpr int kVerticalOffset = 200;
  display::DisplayLayoutBuilder builder(GetPrimaryDisplay().id());
  builder.SetSecondaryPlacement(GetSecondaryDisplay().id(),
                                display::DisplayPlacement::RIGHT,
                                kVerticalOffset);
  ash::Shell::Get()->display_manager()->SetLayoutForCurrentDisplays(
      builder.Build());

  EXPECT_EQ(kNewOrigin + gfx::Vector2d(0, kVerticalOffset), client_origin);
}

// Tests the unnecessary occlusion events are fired when opaque buffer and no
// frame are used.
TEST_F(ShellSurfaceTest, DisplayScaleChangeDoesNotSendOcclusionUpdates) {
  std::unique_ptr<ShellSurface> shell_surface1 =
      test::ShellSurfaceBuilder({256, 256})
          .SetRootBufferFormat(kOpaqueFormat)
          .BuildShellSurface();
  std::unique_ptr<ShellSurface> shell_surface2 =
      test::ShellSurfaceBuilder({256, 256})
          .SetRootBufferFormat(kOpaqueFormat)
          .BuildShellSurface();
  auto* surface1 = shell_surface1->root_surface();
  auto* surface2 = shell_surface2->root_surface();
  auto* window1 = shell_surface1->GetWidget()->GetNativeWindow();
  auto* window2 = shell_surface2->GetWidget()->GetNativeWindow();

  // xdg-shell without a frame type will use NOT_DRAWN layer type and
  // should control the opacity by themselves.
  EXPECT_EQ(ui::LAYER_NOT_DRAWN, window1->layer()->type());
  EXPECT_FALSE(window1->GetProperty(chromeos::kWindowManagerManagesOpacityKey));
  EXPECT_EQ(ui::LAYER_NOT_DRAWN, window2->layer()->type());
  EXPECT_FALSE(window2->GetProperty(chromeos::kWindowManagerManagesOpacityKey));

  const std::vector<gfx::Rect> kNormalOpaqueRegion{gfx::Rect(256, 256)};

  // Normal window should have custom opacity regions.
  EXPECT_TRUE(window1->GetTransparent());
  EXPECT_TRUE(window2->GetTransparent());
  EXPECT_EQ(kNormalOpaqueRegion, window1->opaque_regions_for_occlusion());
  EXPECT_EQ(kNormalOpaqueRegion, window2->opaque_regions_for_occlusion());

  // Test Maximzied State
  shell_surface1->Maximize();
  shell_surface2->Maximize();

  EXPECT_FALSE(window1->GetTransparent());
  EXPECT_FALSE(window2->GetTransparent());
  const std::vector<gfx::Rect> kMaximizedOpaqueRegion{gfx::Rect(800, 552)};
  EXPECT_EQ(kMaximizedOpaqueRegion, window1->opaque_regions_for_occlusion());
  EXPECT_EQ(kMaximizedOpaqueRegion, window2->opaque_regions_for_occlusion());

  // Update root surfaces (this happens asynchronously normally) and set
  // occlusion tracking.
  surface1->SetOcclusionTracking(true);
  auto surface1_buffer =
      test::ExoTestHelper::CreateBuffer(shell_surface1.get(), kOpaqueFormat);
  surface1->Attach(surface1_buffer.get());
  surface1->Commit();
  surface2->SetOcclusionTracking(true);
  auto surface2_buffer =
      test::ExoTestHelper::CreateBuffer(shell_surface2.get(), kOpaqueFormat);
  surface2->Attach(surface2_buffer.get());
  surface2->Commit();

  SurfaceObserverForTest observer1(surface1->window()->GetOcclusionState());
  surface1->AddSurfaceObserver(&observer1);
  SurfaceObserverForTest observer2(surface2->window()->GetOcclusionState());
  surface2->AddSurfaceObserver(&observer2);

  EXPECT_EQ(aura::Window::OcclusionState::OCCLUDED,
            surface1->window()->GetOcclusionState());
  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            surface2->window()->GetOcclusionState());

  auto* display_manager = ash::Shell::Get()->display_manager();
  const auto display_id = display_manager->GetDisplayAt(0).id();

  EXPECT_EQ(0, observer1.num_occlusion_state_changes());
  EXPECT_EQ(0, observer2.num_occlusion_state_changes());

  display_manager->ZoomDisplay(display_id, /*up=*/true);
  // Update root surfaces (this happens asynchronously normally).
  {
    auto surface1_buffer_zoom =
        test::ExoTestHelper::CreateBuffer(shell_surface1.get(), kOpaqueFormat);
    surface1->Attach(surface1_buffer_zoom.get());
    surface1->Commit();
    auto surface2_buffer_zoom =
        test::ExoTestHelper::CreateBuffer(shell_surface2.get(), kOpaqueFormat);
    surface2->Attach(surface2_buffer_zoom.get());
    surface2->Commit();
  }
  EXPECT_EQ(0, observer1.num_occlusion_state_changes());
  EXPECT_EQ(0, observer2.num_occlusion_state_changes());

  display_manager->ZoomDisplay(display_id, /*up=*/false);
  {
    auto surface1_buffer_zoom =
        test::ExoTestHelper::CreateBuffer(shell_surface1.get(), kOpaqueFormat);
    surface1->Attach(surface1_buffer_zoom.get());
    surface1->Commit();
    auto surface2_buffer_zoom =
        test::ExoTestHelper::CreateBuffer(shell_surface2.get(), kOpaqueFormat);
    surface2->Attach(surface2_buffer_zoom.get());
    surface2->Commit();
  }
  // Should not get any occlusion changes - requires occlusion tracking clip
  // to the root window and that the shelf occlude what is below it, too.
  EXPECT_EQ(0, observer1.num_occlusion_state_changes());
  EXPECT_EQ(0, observer2.num_occlusion_state_changes());

  // Test Snapped State
  ash::WindowSnapWMEvent snap_event(ash::WM_EVENT_SNAP_PRIMARY);
  ash::WindowState* window_state1 = ash::WindowState::Get(window1);
  ash::WindowState* window_state2 = ash::WindowState::Get(window2);

  window_state1->OnWMEvent(&snap_event);
  window_state2->OnWMEvent(&snap_event);
  EXPECT_EQ(0, observer1.num_occlusion_state_changes());
  EXPECT_EQ(0, observer2.num_occlusion_state_changes());

  EXPECT_TRUE(window1->GetTransparent());
  EXPECT_TRUE(window2->GetTransparent());

  const std::vector<gfx::Rect> kSnappedOpaqueRegion{gfx::Rect(400, 552)};
  EXPECT_EQ(kSnappedOpaqueRegion, window1->opaque_regions_for_occlusion());
  EXPECT_EQ(kSnappedOpaqueRegion, window2->opaque_regions_for_occlusion());
  {
    auto snapped_buffer1 =
        test::ExoTestHelper::CreateBuffer(shell_surface1.get(), kOpaqueFormat);
    surface1->Attach(snapped_buffer1.get());
    surface1->Commit();

    auto snapped_buffer2 =
        test::ExoTestHelper::CreateBuffer(shell_surface2.get(), kOpaqueFormat);
    surface2->Attach(snapped_buffer2.get());
    surface2->Commit();
  }
  EXPECT_EQ(0, observer1.num_occlusion_state_changes());
  EXPECT_EQ(0, observer2.num_occlusion_state_changes());

  display_manager->ZoomDisplay(display_id, /*up=*/true);
  {
    auto snapped_buffer1 =
        test::ExoTestHelper::CreateBuffer(shell_surface1.get(), kOpaqueFormat);
    surface1->Attach(snapped_buffer1.get());
    surface1->Commit();

    auto snapped_buffer2 =
        test::ExoTestHelper::CreateBuffer(shell_surface2.get(), kOpaqueFormat);
    surface2->Attach(snapped_buffer2.get());
    surface2->Commit();
  }
  EXPECT_EQ(0, observer1.num_occlusion_state_changes());
  EXPECT_EQ(0, observer2.num_occlusion_state_changes());

  display_manager->ZoomDisplay(display_id, /*up=*/false);
  {
    auto snapped_buffer1 =
        test::ExoTestHelper::CreateBuffer(shell_surface1.get(), kOpaqueFormat);
    surface1->Attach(snapped_buffer1.get());
    surface1->Commit();

    auto snapped_buffer2 =
        test::ExoTestHelper::CreateBuffer(shell_surface2.get(), kOpaqueFormat);
    surface2->Attach(snapped_buffer2.get());
    surface2->Commit();
  }
  EXPECT_EQ(0, observer1.num_occlusion_state_changes());
  EXPECT_EQ(0, observer2.num_occlusion_state_changes());

  // Make sure the occlusion tracking is working.
  surface2->RemoveSurfaceObserver(&observer2);
  shell_surface2.reset();

  EXPECT_EQ(1, observer1.num_occlusion_state_changes());

  surface1->RemoveSurfaceObserver(&observer1);
}

TEST_F(ShellSurfaceTest, GetWidgetHitTestMask) {
  auto shell_surface = test::ShellSurfaceBuilder({256, 256})
                           .SetOrigin({100, 100})
                           .BuildShellSurface();

  EXPECT_TRUE(shell_surface->WidgetHasHitTestMask());
  SkPath mask;
  shell_surface->GetWidgetHitTestMask(&mask);
  // Returned HitMask should be in the widget local coordinates.
  EXPECT_EQ(SkRect::MakeLTRB(0, 0, 256, 256), mask.getBounds());
}

TEST_F(ShellSurfaceTest, HostWindowOpaqueRegionForOcclusion) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  auto* root_surface = shell_surface->root_surface();

  // Makes root surface fill bounds opaquely.
  root_surface->SetBlendMode(SkBlendMode::kSrc);
  root_surface->Commit();
  ASSERT_TRUE(shell_surface->host_window()->GetTransparent());
  ASSERT_TRUE(root_surface->FillsBoundsOpaquely());

  auto opaque_occlusion_region =
      shell_surface->host_window()->opaque_regions_for_occlusion();
  EXPECT_EQ(opaque_occlusion_region.size(), 1u);
  EXPECT_EQ(opaque_occlusion_region.back(), gfx::Rect(256, 256));

  // Adding a subsurface will expand the host window to include the sub-surface.
  constexpr gfx::Size kChildBufferSize(32, 32);
  auto child_buffer1 = test::ExoTestHelper::CreateBuffer(kChildBufferSize);
  auto child_surface1 = std::make_unique<Surface>();
  child_surface1->Attach(child_buffer1.get());
  auto subsurface1 = std::make_unique<SubSurface>(
      child_surface1.get(), shell_surface->root_surface());
  subsurface1->SetPosition(gfx::PointF(256, 256));
  child_surface1->Commit();

  root_surface->Commit();

  constexpr gfx::Rect kUpdatedHostWindowBounds(0, 0, 288, 288);
  EXPECT_EQ(shell_surface->host_window()->bounds(), kUpdatedHostWindowBounds);

  // After bounds update, host window does not fill the bounds opaquely, since
  // the content_size of root surface does not match host_window size. In this
  // case, host_window does not specify any region for occlusion.
  opaque_occlusion_region =
      shell_surface->host_window()->opaque_regions_for_occlusion();
  EXPECT_EQ(opaque_occlusion_region.size(), 0u);

  // Update the content_size of root surface. This ensure, that host window
  // fills the bounds opaquely again.
  root_surface->SetViewport(gfx::SizeF(kUpdatedHostWindowBounds.size()));
  root_surface->Commit();

  // Confirm that new occlusion region reflects the updated bounds of host
  // window.
  opaque_occlusion_region =
      shell_surface->host_window()->opaque_regions_for_occlusion();
  EXPECT_EQ(opaque_occlusion_region.size(), 1u);
  EXPECT_EQ(opaque_occlusion_region.back(), kUpdatedHostWindowBounds);
}

TEST_F(ShellSurfaceTest, InitiallyMaximizedWindowIsOpaque) {
  auto shell_surface = test::ShellSurfaceBuilder({256, 256})
                           .SetFrame(SurfaceFrameType::SHADOW)
                           .SetOrigin({100, 100})
                           .SetNoCommit()
                           .BuildShellSurface();
  shell_surface->Maximize();

  shell_surface->root_surface()->Commit();

  EXPECT_FALSE(shell_surface->GetWidget()->GetNativeWindow()->GetTransparent());
}

TEST_F(ShellSurfaceTest, ConfigureOcclusionSentAfterShellSurfaceIsReady) {
  std::vector<ConfigureData> config_vec;
  auto shell_surface =
      test::ShellSurfaceBuilder({100, 100})
          .SetConfigureCallback(base::BindRepeating(
              &ConfigureSerialVec, base::Unretained(&config_vec)))
          .SetWindowState(chromeos::WindowStateType::kMinimized)
          .SetNoRootBuffer()
          .BuildShellSurface();

  Surface* root_surface = shell_surface->root_surface();
  root_surface->Commit();

  // Expect initial configure with no content (buffer for root surface).
  EXPECT_EQ(1u, config_vec.size());

  // TODO(crbug.com/328172097): We use VISIBLE window state for minimized
  // windows on initial configure for now.
  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            config_vec[0].occlusion_state);

  constexpr gfx::Size kBufferSize(256, 256);
  auto buffer = test::ExoTestHelper::CreateBuffer(kBufferSize);
  root_surface->Attach(buffer.get());
  root_surface->Commit();

  // Once we provide a buffer for the root surface, expect that the occlusion
  // state is updated to HIDDEN, since it is a minimized window.
  EXPECT_EQ(2u, config_vec.size());
  EXPECT_EQ(aura::Window::OcclusionState::HIDDEN,
            config_vec[1].occlusion_state);
}

// Regression test for crbug.com/322388171. Ensure shell surfaces with no
// backing widget handle display changes without crashing.
TEST_F(ShellSurfaceTest, HandlesDisplayChangeNoWidget) {
  // Start with a single display configuration.
  UpdateDisplay("800x600");

  // Create the surface.
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  EXPECT_TRUE(shell_surface->GetWidget());

  // Keep the shell surface alive but close the widget.
  shell_surface->GetWidget()->CloseNow();
  EXPECT_FALSE(shell_surface->GetWidget());

  // Trigger an update to the display configuration, the shell surface should
  // handle this without crashing.
  UpdateDisplay("1200x800");
}

TEST_F(ShellSurfaceTest, AccessibleChildTreeNodeAppId) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  ui::AXNodeData data;

  shell_surface->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(
      data.HasStringAttribute(ax::mojom::StringAttribute::kChildTreeNodeAppId));

  data = ui::AXNodeData();
  shell_surface->SetApplicationId("child_app_id");
  shell_surface->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(
      "child_app_id",
      data.GetStringAttribute(ax::mojom::StringAttribute::kChildTreeNodeAppId));

  data = ui::AXNodeData();
  shell_surface->SetApplicationId(nullptr);
  shell_surface->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(
      data.HasStringAttribute(ax::mojom::StringAttribute::kChildTreeNodeAppId));
}

}  // namespace exo
