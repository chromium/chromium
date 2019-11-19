// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/client_controlled_shell_surface.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/frame/header_view.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/frame/wide_frame_view.h"
#include "ash/public/cpp/caption_buttons/caption_button_model.h"
#include "ash/public/cpp/caption_buttons/frame_caption_button_container_view.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_pin_type.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_delegate.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_drag_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_drag_delegate.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/display_item_list.h"
#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/pointer.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/wm_helper.h"
#include "third_party/skia/include/utils/SkNoDrawCanvas.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/display/display.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_targeter.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/paint_info.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_types.h"

namespace exo {
namespace {
using ClientControlledShellSurfaceTest = test::ExoTestBase;

bool HasBackdrop() {
  ash::WorkspaceController* wc = ash::ShellTestApi().workspace_controller();
  return !!ash::WorkspaceControllerTestApi(wc).GetBackdropWindow();
}

bool IsWidgetPinned(views::Widget* widget) {
  ash::WindowPinType type =
      widget->GetNativeWindow()->GetProperty(ash::kWindowPinTypeKey);
  return type == ash::WindowPinType::kPinned ||
         type == ash::WindowPinType::kTrustedPinned;
}

int GetShadowElevation(aura::Window* window) {
  return window->GetProperty(wm::kShadowElevationKey);
}

void EnableTabletMode(bool enable) {
  ash::Shell::Get()->tablet_mode_controller()->SetEnabledForTest(enable);
}

// A canvas that just logs when a text blob is drawn.
class TestCanvas : public SkNoDrawCanvas {
 public:
  TestCanvas() : SkNoDrawCanvas(100, 100) {}
  ~TestCanvas() override {}

  void onDrawTextBlob(const SkTextBlob*,
                      SkScalar,
                      SkScalar,
                      const SkPaint&) override {
    text_was_drawn_ = true;
  }

  bool text_was_drawn() const { return text_was_drawn_; }

 private:
  bool text_was_drawn_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestCanvas);
};

}  // namespace

TEST_F(ClientControlledShellSurfaceTest, SetPinned) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

  std::unique_ptr<Surface> surface(new Surface);
  surface->Attach(buffer.get());
  surface->Commit();

  auto shell_surface(
      exo_test_helper()->CreateClientControlledShellSurface(surface.get()));

  shell_surface->SetPinned(ash::WindowPinType::kTrustedPinned);
  EXPECT_TRUE(IsWidgetPinned(shell_surface->GetWidget()));

  shell_surface->SetPinned(ash::WindowPinType::kNone);
  EXPECT_FALSE(IsWidgetPinned(shell_surface->GetWidget()));

  shell_surface->SetPinned(ash::WindowPinType::kPinned);
  EXPECT_TRUE(IsWidgetPinned(shell_surface->GetWidget()));

  shell_surface->SetPinned(ash::WindowPinType::kNone);
  EXPECT_FALSE(IsWidgetPinned(shell_surface->GetWidget()));
}

TEST_F(ClientControlledShellSurfaceTest, SetSystemUiVisibility) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());
  surface->Attach(buffer.get());
  surface->Commit();

  shell_surface->SetSystemUiVisibility(true);
  EXPECT_TRUE(
      ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow())
          ->autohide_shelf_when_maximized_or_fullscreen());

  shell_surface->SetSystemUiVisibility(false);
  EXPECT_FALSE(
      ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow())
          ->autohide_shelf_when_maximized_or_fullscreen());
}

TEST_F(ClientControlledShellSurfaceTest, SetTopInset) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  ASSERT_TRUE(window);
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));
  int top_inset_height = 20;
  shell_surface->SetTopInset(top_inset_height);
  surface->Commit();
  EXPECT_EQ(top_inset_height, window->GetProperty(aura::client::kTopViewInset));
}

TEST_F(ClientControlledShellSurfaceTest, ModalWindowDefaultActive) {
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get(),
                                                            /*is_modal=*/true);

  gfx::Size desktop_size(640, 480);
  std::unique_ptr<Buffer> desktop_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(desktop_size)));
  surface->Attach(desktop_buffer.get());
  surface->SetInputRegion(gfx::Rect(10, 10, 100, 100));
  ASSERT_FALSE(shell_surface->GetWidget());
  shell_surface->SetSystemModal(true);
  surface->Commit();

  EXPECT_TRUE(ash::Shell::IsSystemModalWindowOpen());
  EXPECT_TRUE(shell_surface->GetWidget()->IsActive());
}

TEST_F(ClientControlledShellSurfaceTest, UpdateModalWindow) {
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface = exo_test_helper()->CreateClientControlledShellSurface(
      surface.get(), /*is_modal=*/true);
  gfx::Size desktop_size(640, 480);
  std::unique_ptr<Buffer> desktop_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(desktop_size)));
  surface->Attach(desktop_buffer.get());
  surface->SetInputRegion(cc::Region());
  surface->Commit();

  EXPECT_FALSE(ash::Shell::IsSystemModalWindowOpen());
  EXPECT_FALSE(shell_surface->GetWidget()->IsActive());

  // Creating a surface without input region should not make it modal.
  std::unique_ptr<Display> display(new Display);
  std::unique_ptr<Surface> child = display->CreateSurface();
  gfx::Size buffer_size(128, 128);
  std::unique_ptr<Buffer> child_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  child->Attach(child_buffer.get());
  std::unique_ptr<SubSurface> sub_surface(
      display->CreateSubSurface(child.get(), surface.get()));
  surface->SetSubSurfacePosition(child.get(), gfx::Point(10, 10));
  child->Commit();
  surface->Commit();
  EXPECT_FALSE(ash::Shell::IsSystemModalWindowOpen());
  EXPECT_FALSE(shell_surface->GetWidget()->IsActive());

  // Making the surface opaque shouldn't make it modal either.
  child->SetBlendMode(SkBlendMode::kSrc);
  child->Commit();
  surface->Commit();
  EXPECT_FALSE(ash::Shell::IsSystemModalWindowOpen());
  EXPECT_FALSE(shell_surface->GetWidget()->IsActive());

  // Setting input regions won't make it modal either.
  surface->SetInputRegion(gfx::Rect(10, 10, 100, 100));
  surface->Commit();
  EXPECT_FALSE(ash::Shell::IsSystemModalWindowOpen());
  EXPECT_FALSE(shell_surface->GetWidget()->IsActive());

  // Only SetSystemModal changes modality.
  shell_surface->SetSystemModal(true);

  EXPECT_TRUE(ash::Shell::IsSystemModalWindowOpen());
  EXPECT_TRUE(shell_surface->GetWidget()->IsActive());

  shell_surface->SetSystemModal(false);

  EXPECT_FALSE(ash::Shell::IsSystemModalWindowOpen());
  EXPECT_FALSE(shell_surface->GetWidget()->IsActive());

  // If the non modal system window was active,
  shell_surface->GetWidget()->Activate();
  EXPECT_TRUE(shell_surface->GetWidget()->IsActive());

  shell_surface->SetSystemModal(true);
  EXPECT_TRUE(ash::Shell::IsSystemModalWindowOpen());
  EXPECT_TRUE(shell_surface->GetWidget()->IsActive());

  shell_surface->SetSystemModal(false);
  EXPECT_FALSE(ash::Shell::IsSystemModalWindowOpen());
  EXPECT_TRUE(shell_surface->GetWidget()->IsActive());
}

TEST_F(ClientControlledShellSurfaceTest,
       ModalWindowSetSystemModalBeforeCommit) {
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface = exo_test_helper()->CreateClientControlledShellSurface(
      surface.get(), /*is_modal=*/true);
  gfx::Size desktop_size(640, 480);
  std::unique_ptr<Buffer> desktop_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(desktop_size)));
  surface->Attach(desktop_buffer.get());
  surface->SetInputRegion(cc::Region());

  // Set SetSystemModal before any commit happens. Widget is not created at
  // this time.
  EXPECT_FALSE(shell_surface->GetWidget());
  shell_surface->SetSystemModal(true);

  surface->Commit();

  // It is expected that modal window is shown.
  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_TRUE(ash::Shell::IsSystemModalWindowOpen());

  // Now widget is created and setting modal state should be applied
  // immediately.
  shell_surface->SetSystemModal(false);
  EXPECT_FALSE(ash::Shell::IsSystemModalWindowOpen());
}

TEST_F(ClientControlledShellSurfaceTest, SurfaceShadow) {
  gfx::Size buffer_size(128, 128);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());
  surface->Attach(buffer.get());
  surface->Commit();

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  // 1) Initial state, no shadow (SurfaceFrameType is NONE);
  EXPECT_FALSE(wm::ShadowController::GetShadowForWindow(window));
  std::unique_ptr<Display> display(new Display);

  // 2) Just creating a sub surface won't create a shadow.
  std::unique_ptr<Surface> child = display->CreateSurface();
  std::unique_ptr<Buffer> child_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  child->Attach(child_buffer.get());
  std::unique_ptr<SubSurface> sub_surface(
      display->CreateSubSurface(child.get(), surface.get()));
  surface->Commit();

  EXPECT_FALSE(wm::ShadowController::GetShadowForWindow(window));

  // 3) Create a shadow.
  surface->SetFrame(SurfaceFrameType::SHADOW);
  shell_surface->SetShadowBounds(gfx::Rect(10, 10, 100, 100));
  surface->Commit();
  ui::Shadow* shadow = wm::ShadowController::GetShadowForWindow(window);
  ASSERT_TRUE(shadow);
  EXPECT_TRUE(shadow->layer()->visible());

  gfx::Rect before = shadow->layer()->bounds();

  // 4) Shadow bounds is independent of the sub surface.
  gfx::Size new_buffer_size(256, 256);
  std::unique_ptr<Buffer> new_child_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(new_buffer_size)));
  child->Attach(new_child_buffer.get());
  child->Commit();
  surface->Commit();

  EXPECT_EQ(before, shadow->layer()->bounds());

  // 4) Updating the widget's window bounds should not change the shadow bounds.
  // TODO(oshima): The following scenario only worked with Xdg/ShellSurface,
  // which never uses SetShadowBounds. This is broken with correct scenario, and
  // will be fixed when the bounds control is delegated to the client.
  //
  // window->SetBounds(gfx::Rect(10, 10, 100, 100));
  // EXPECT_EQ(before, shadow->layer()->bounds());

  // 5) This should disable shadow.
  shell_surface->SetShadowBounds(gfx::Rect());
  surface->Commit();

  EXPECT_EQ(wm::kShadowElevationNone, GetShadowElevation(window));
  EXPECT_FALSE(shadow->layer()->visible());

  // 6) This should enable non surface shadow again.
  shell_surface->SetShadowBounds(gfx::Rect(10, 10, 100, 100));
  surface->Commit();

  EXPECT_EQ(wm::kShadowElevationDefault, GetShadowElevation(window));
  EXPECT_TRUE(shadow->layer()->visible());
}

TEST_F(ClientControlledShellSurfaceTest, ShadowWithStateChange) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  // Postion the widget at 10,10 so that we get non zero offset.
  const gfx::Size content_size(100, 100);
  const gfx::Rect original_bounds(gfx::Point(10, 10), content_size);
  shell_surface->SetGeometry(original_bounds);
  surface->Attach(buffer.get());
  surface->SetFrame(SurfaceFrameType::SHADOW);
  surface->Commit();

  // In parent coordinates.
  const gfx::Rect shadow_bounds(gfx::Point(-10, -10), content_size);

  views::Widget* widget = shell_surface->GetWidget();
  aura::Window* window = widget->GetNativeWindow();
  ui::Shadow* shadow = wm::ShadowController::GetShadowForWindow(window);

  shell_surface->SetShadowBounds(shadow_bounds);
  surface->Commit();
  EXPECT_EQ(wm::kShadowElevationDefault, GetShadowElevation(window));

  EXPECT_TRUE(shadow->layer()->visible());
  // Origin must be in sync.
  EXPECT_EQ(shadow_bounds.origin(), shadow->content_bounds().origin());

  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  // Maximizing window hides the shadow.
  widget->Maximize();
  ASSERT_TRUE(widget->IsMaximized());
  EXPECT_FALSE(shadow->layer()->visible());

  shell_surface->SetShadowBounds(work_area);
  surface->Commit();
  EXPECT_FALSE(shadow->layer()->visible());

  // Restoring bounds will re-enable shadow. It's content size is set to work
  // area,/ thus not visible until new bounds is committed.
  widget->Restore();
  EXPECT_TRUE(shadow->layer()->visible());
  EXPECT_EQ(work_area, shadow->content_bounds());

  // The bounds is updated.
  shell_surface->SetShadowBounds(shadow_bounds);
  surface->Commit();
  EXPECT_EQ(shadow_bounds, shadow->content_bounds());
}

TEST_F(ClientControlledShellSurfaceTest, ShadowWithTransform) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  // Postion the widget at 10,10 so that we get non zero offset.
  const gfx::Size content_size(100, 100);
  const gfx::Rect original_bounds(gfx::Point(10, 10), content_size);
  shell_surface->SetGeometry(original_bounds);
  surface->Attach(buffer.get());
  surface->SetFrame(SurfaceFrameType::SHADOW);
  surface->Commit();

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  ui::Shadow* shadow = wm::ShadowController::GetShadowForWindow(window);

  // In parent coordinates.
  const gfx::Rect shadow_bounds(gfx::Point(-10, -10), content_size);

  // Shadow bounds relative to its parent should not be affected by a transform.
  gfx::Transform transform;
  transform.Translate(50, 50);
  window->SetTransform(transform);
  shell_surface->SetShadowBounds(shadow_bounds);
  surface->Commit();
  EXPECT_TRUE(shadow->layer()->visible());
  EXPECT_EQ(gfx::Rect(-10, -10, 100, 100), shadow->content_bounds());
}

TEST_F(ClientControlledShellSurfaceTest, ShadowStartMaximized) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

  std::unique_ptr<Surface> surface(new Surface);

  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());
  shell_surface->SetMaximized();
  surface->Attach(buffer.get());
  surface->SetFrame(SurfaceFrameType::SHADOW);
  surface->Commit();

  views::Widget* widget = shell_surface->GetWidget();
  aura::Window* window = widget->GetNativeWindow();

  // There is no shadow when started in maximized state.
  EXPECT_FALSE(wm::ShadowController::GetShadowForWindow(window));

  // Sending a shadow bounds in maximized state won't create a shadow.
  shell_surface->SetShadowBounds(gfx::Rect(10, 10, 100, 100));
  surface->Commit();
  EXPECT_FALSE(wm::ShadowController::GetShadowForWindow(window));

  // Restore the window and make sure the shadow is created, visible and
  // has the latest bounds.
  widget->Restore();
  ui::Shadow* shadow = wm::ShadowController::GetShadowForWindow(window);
  ASSERT_TRUE(shadow);
  EXPECT_TRUE(shadow->layer()->visible());
  EXPECT_EQ(gfx::Rect(10, 10, 100, 100), shadow->content_bounds());
}

TEST_F(ClientControlledShellSurfaceTest, Frame) {
  UpdateDisplay("800x600");

  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

  std::unique_ptr<Surface> surface(new Surface);

  gfx::Rect client_bounds(20, 50, 300, 200);
  gfx::Rect fullscreen_bounds(0, 0, 800, 600);
  // The window bounds is the client bounds + frame size.
  gfx::Rect normal_window_bounds(20, 18, 300, 232);

  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());
  surface->Attach(buffer.get());
  shell_surface->SetGeometry(client_bounds);
  surface->SetFrame(SurfaceFrameType::NORMAL);
  surface->Commit();

  views::Widget* widget = shell_surface->GetWidget();
  ash::NonClientFrameViewAsh* frame_view =
      static_cast<ash::NonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  // Normal state.
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(frame_view->GetVisible());
  EXPECT_EQ(normal_window_bounds, widget->GetWindowBoundsInScreen());
  EXPECT_EQ(client_bounds,
            frame_view->GetClientBoundsForWindowBounds(normal_window_bounds));

  // Maximized
  shell_surface->SetMaximized();
  shell_surface->SetGeometry(gfx::Rect(0, 0, 800, 568));
  surface->Commit();

  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(frame_view->GetVisible());
  EXPECT_EQ(fullscreen_bounds, widget->GetWindowBoundsInScreen());
  EXPECT_EQ(
      gfx::Size(800, 568),
      frame_view->GetClientBoundsForWindowBounds(fullscreen_bounds).size());

  // AutoHide
  surface->SetFrame(SurfaceFrameType::AUTOHIDE);
  shell_surface->SetGeometry(fullscreen_bounds);
  surface->Commit();

  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(frame_view->GetVisible());
  EXPECT_EQ(fullscreen_bounds, widget->GetWindowBoundsInScreen());
  EXPECT_EQ(fullscreen_bounds,
            frame_view->GetClientBoundsForWindowBounds(fullscreen_bounds));

  // Fullscreen state.
  shell_surface->SetFullscreen(true);
  surface->Commit();

  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(frame_view->GetVisible());
  EXPECT_EQ(fullscreen_bounds, widget->GetWindowBoundsInScreen());
  EXPECT_EQ(fullscreen_bounds,
            frame_view->GetClientBoundsForWindowBounds(fullscreen_bounds));

  // Updating frame, then window state should still update the frame state.
  surface->SetFrame(SurfaceFrameType::NORMAL);
  surface->Commit();

  widget->LayoutRootViewIfNecessary();
  EXPECT_FALSE(frame_view->GetHeaderView()->GetVisible());

  shell_surface->SetMaximized();
  surface->Commit();

  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(frame_view->GetHeaderView()->GetVisible());

  // Restore to normal state.
  shell_surface->SetRestored();
  shell_surface->SetGeometry(client_bounds);
  surface->SetFrame(SurfaceFrameType::NORMAL);
  surface->Commit();

  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(frame_view->GetVisible());
  EXPECT_EQ(normal_window_bounds, widget->GetWindowBoundsInScreen());
  EXPECT_EQ(client_bounds,
            frame_view->GetClientBoundsForWindowBounds(normal_window_bounds));

  // No frame. The all bounds are same as client bounds.
  shell_surface->SetRestored();
  shell_surface->SetGeometry(client_bounds);
  surface->SetFrame(SurfaceFrameType::NONE);
  surface->Commit();

  widget->LayoutRootViewIfNecessary();
  EXPECT_FALSE(frame_view->GetVisible());
  EXPECT_EQ(client_bounds, widget->GetWindowBoundsInScreen());
  EXPECT_EQ(client_bounds,
            frame_view->GetClientBoundsForWindowBounds(client_bounds));

  // Test NONE -> AUTOHIDE -> NONE
  shell_surface->SetMaximized();
  shell_surface->SetGeometry(fullscreen_bounds);
  surface->SetFrame(SurfaceFrameType::AUTOHIDE);
  surface->Commit();

  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(frame_view->GetVisible());
  EXPECT_TRUE(frame_view->GetHeaderView()->in_immersive_mode());

  surface->SetFrame(SurfaceFrameType::NONE);
  surface->Commit();

  widget->LayoutRootViewIfNecessary();
  EXPECT_FALSE(frame_view->GetVisible());
  EXPECT_FALSE(frame_view->GetHeaderView()->in_immersive_mode());
}

namespace {

class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler() = default;
  ~TestEventHandler() override = default;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override { received_event_ = true; }

  bool received_event() const { return received_event_; }

 private:
  bool received_event_ = false;
  DISALLOW_COPY_AND_ASSIGN(TestEventHandler);
};

}  // namespace

TEST_F(ClientControlledShellSurfaceTest, NoSynthesizedEventOnFrameChange) {
  UpdateDisplay("800x600");

  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);

  gfx::Rect fullscreen_bounds(0, 0, 800, 600);

  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());
  surface->Attach(buffer.get());
  surface->SetFrame(SurfaceFrameType::NORMAL);
  surface->Commit();

  // Maximized
  shell_surface->SetMaximized();
  shell_surface->SetGeometry(fullscreen_bounds);
  surface->Commit();

  // AutoHide
  base::RunLoop().RunUntilIdle();
  aura::Env* env = aura::Env::GetInstance();
  gfx::Rect cropped_fullscreen_bounds(0, 0, 800, 400);
  env->SetLastMouseLocation(gfx::Point(100, 30));
  TestEventHandler handler;
  env->AddPreTargetHandler(&handler);
  surface->SetFrame(SurfaceFrameType::AUTOHIDE);
  shell_surface->SetGeometry(cropped_fullscreen_bounds);
  surface->Commit();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(handler.received_event());
  env->RemovePreTargetHandler(&handler);
}

TEST_F(ClientControlledShellSurfaceTest, CompositorLockInRotation) {
  UpdateDisplay("800x600");
  const gfx::Size buffer_size(800, 600);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());
  ash::Shell* shell = ash::Shell::Get();
  shell->tablet_mode_controller()->SetEnabledForTest(true);

  // Start in maximized.
  shell_surface->SetMaximized();
  surface->Attach(buffer.get());
  surface->Commit();

  gfx::Rect maximum_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  shell_surface->SetGeometry(maximum_bounds);
  shell_surface->SetOrientation(Orientation::LANDSCAPE);
  surface->Commit();

  ui::Compositor* compositor =
      shell_surface->GetWidget()->GetNativeWindow()->layer()->GetCompositor();

  EXPECT_FALSE(compositor->IsLocked());

  UpdateDisplay("800x600/r");

  EXPECT_TRUE(compositor->IsLocked());

  shell_surface->SetOrientation(Orientation::PORTRAIT);
  surface->Commit();

  EXPECT_FALSE(compositor->IsLocked());
}

// If system tray is shown by click. It should be activated if user presses tab
// key while shell surface is active.
TEST_F(ClientControlledShellSurfaceTest,
       KeyboardNavigationWithUnifiedSystemTray) {
  const gfx::Size buffer_size(800, 600);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface());
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();

  EXPECT_TRUE(shell_surface->GetWidget()->IsActive());

  // Show system tray by performing a gesture tap at tray.
  ash::UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  system_tray->PerformAction(tap);
  ASSERT_TRUE(system_tray->GetWidget());

  // Confirm that system tray is not active at this time.
  EXPECT_TRUE(shell_surface->GetWidget()->IsActive());
  EXPECT_FALSE(system_tray->IsBubbleActive());

  // Send tab key event.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  event_generator->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);

  // Confirm that system tray is activated.
  EXPECT_FALSE(shell_surface->GetWidget()->IsActive());
  EXPECT_TRUE(system_tray->IsBubbleActive());
}

TEST_F(ClientControlledShellSurfaceTest, Maximize) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface(
      exo_test_helper()->CreateClientControlledShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
  shell_surface->SetMaximized();
  EXPECT_FALSE(HasBackdrop());
  surface->Commit();
  EXPECT_TRUE(HasBackdrop());
  EXPECT_TRUE(shell_surface->GetWidget()->IsMaximized());

  // We always show backdrop because the window may be cropped.
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  shell_surface->SetGeometry(display.bounds());
  surface->Commit();
  EXPECT_TRUE(HasBackdrop());

  shell_surface->SetGeometry(gfx::Rect(0, 0, 100, display.bounds().height()));
  surface->Commit();
  EXPECT_TRUE(HasBackdrop());

  shell_surface->SetGeometry(gfx::Rect(0, 0, display.bounds().width(), 100));
  surface->Commit();
  EXPECT_TRUE(HasBackdrop());

  // Toggle maximize.
  ash::WMEvent maximize_event(ash::WM_EVENT_TOGGLE_MAXIMIZE);
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  ash::WindowState::Get(window)->OnWMEvent(&maximize_event);
  EXPECT_FALSE(shell_surface->GetWidget()->IsMaximized());
  EXPECT_FALSE(HasBackdrop());

  ash::WindowState::Get(window)->OnWMEvent(&maximize_event);
  EXPECT_TRUE(shell_surface->GetWidget()->IsMaximized());
  EXPECT_TRUE(HasBackdrop());
}

TEST_F(ClientControlledShellSurfaceTest, Restore) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface(
      exo_test_helper()->CreateClientControlledShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
  // Note: Remove contents to avoid issues with maximize animations in tests.
  shell_surface->SetMaximized();
  EXPECT_FALSE(HasBackdrop());
  surface->Commit();
  EXPECT_TRUE(HasBackdrop());

  shell_surface->SetRestored();
  EXPECT_TRUE(HasBackdrop());
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
}

TEST_F(ClientControlledShellSurfaceTest, SetFullscreen) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface(
      exo_test_helper()->CreateClientControlledShellSurface(surface.get()));

  shell_surface->SetFullscreen(true);
  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_TRUE(HasBackdrop());

  // We always show backdrop becaues the window can be cropped.
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  shell_surface->SetGeometry(display.bounds());
  surface->Commit();
  EXPECT_TRUE(HasBackdrop());

  shell_surface->SetGeometry(gfx::Rect(0, 0, 100, display.bounds().height()));
  surface->Commit();
  EXPECT_TRUE(HasBackdrop());

  shell_surface->SetGeometry(gfx::Rect(0, 0, display.bounds().width(), 100));
  surface->Commit();
  EXPECT_TRUE(HasBackdrop());

  shell_surface->SetFullscreen(false);
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
  EXPECT_NE(CurrentContext()->bounds().ToString(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().ToString());
}

TEST_F(ClientControlledShellSurfaceTest, ToggleFullscreen) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface(
      exo_test_helper()->CreateClientControlledShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());

  shell_surface->SetMaximized();
  surface->Commit();
  EXPECT_TRUE(HasBackdrop());

  ash::WMEvent event(ash::WM_EVENT_TOGGLE_FULLSCREEN);
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  // Enter fullscreen mode.
  ash::WindowState::Get(window)->OnWMEvent(&event);
  EXPECT_TRUE(HasBackdrop());

  // Leave fullscreen mode.
  ash::WindowState::Get(window)->OnWMEvent(&event);
  EXPECT_TRUE(HasBackdrop());
}

TEST_F(ClientControlledShellSurfaceTest,
       DefaultDeviceScaleFactorForcedScaleFactor) {
  double scale = 1.5;
  display::Display::SetForceDeviceScaleFactor(scale);

  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::Display::SetInternalDisplayId(display_id);

  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface(
      exo_test_helper()->CreateClientControlledShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();
  gfx::Transform transform;
  transform.Scale(1.0 / scale, 1.0 / scale);

  EXPECT_EQ(
      transform.ToString(),
      shell_surface->host_window()->layer()->GetTargetTransform().ToString());
}

TEST_F(ClientControlledShellSurfaceTest,
       DefaultDeviceScaleFactorFromDisplayManager) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::Display::SetInternalDisplayId(display_id);
  gfx::Size size(1920, 1080);

  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();

  double scale = 1.25;
  display::ManagedDisplayMode mode(size, 60.f, false /* overscan */,
                                   true /*native*/, scale);

  display::ManagedDisplayInfo::ManagedDisplayModeList mode_list;
  mode_list.push_back(mode);

  display::ManagedDisplayInfo native_display_info(display_id, "test", false);
  native_display_info.SetManagedDisplayModes(mode_list);

  native_display_info.SetBounds(gfx::Rect(size));
  native_display_info.set_device_scale_factor(scale);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);

  display_manager->OnNativeDisplaysChanged(display_info_list);
  display_manager->UpdateInternalManagedDisplayModeListForTest();

  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface(
      exo_test_helper()->CreateClientControlledShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();

  gfx::Transform transform;
  transform.Scale(1.0 / scale, 1.0 / scale);

  EXPECT_EQ(
      transform.ToString(),
      shell_surface->host_window()->layer()->GetTargetTransform().ToString());
}

TEST_F(ClientControlledShellSurfaceTest, MouseAndTouchTarget) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface(
      exo_test_helper()->CreateClientControlledShellSurface(surface.get()));

  const gfx::Rect original_bounds(0, 0, 256, 256);
  shell_surface->SetGeometry(original_bounds);
  surface->Attach(buffer.get());
  surface->Commit();

  EXPECT_TRUE(shell_surface->CanResize());

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  aura::Window* root = window->GetRootWindow();
  ui::EventTargeter* targeter =
      root->GetHost()->dispatcher()->GetDefaultEventTargeter();

  gfx::Point mouse_location(256 + 5, 150);

  ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, mouse_location, mouse_location,
                       ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  EXPECT_EQ(window, targeter->FindTargetForEvent(root, &mouse));

  // Move 20px further away. Touch event can hit the window but
  // mouse event will not.
  gfx::Point touch_location(256 + 25, 150);
  ui::MouseEvent touch(ui::ET_TOUCH_PRESSED, touch_location, touch_location,
                       ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  EXPECT_EQ(window, targeter->FindTargetForEvent(root, &touch));

  ui::MouseEvent mouse_with_touch_loc(ui::ET_MOUSE_MOVED, touch_location,
                                      touch_location, ui::EventTimeForNow(),
                                      ui::EF_NONE, ui::EF_NONE);
  EXPECT_FALSE(window->Contains(static_cast<aura::Window*>(
      targeter->FindTargetForEvent(root, &mouse_with_touch_loc))));

  // Touching futher away shouldn't hit the window.
  gfx::Point no_touch_location(256 + 35, 150);
  ui::MouseEvent no_touch(ui::ET_TOUCH_PRESSED, no_touch_location,
                          no_touch_location, ui::EventTimeForNow(), ui::EF_NONE,
                          ui::EF_NONE);
  EXPECT_FALSE(window->Contains(static_cast<aura::Window*>(
      targeter->FindTargetForEvent(root, &no_touch))));
}

// The shell surface in SystemModal container should be unresizable.
TEST_F(ClientControlledShellSurfaceTest,
       ShellSurfaceInSystemModalIsUnresizable) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get(),
                                                            /*is_modal=*/true);
  surface->Attach(buffer.get());
  surface->Commit();

  EXPECT_FALSE(shell_surface->GetWidget()->widget_delegate()->CanResize());
}

// The shell surface in SystemModal container should not become target
// at the edge.
TEST_F(ClientControlledShellSurfaceTest, ShellSurfaceInSystemModalHitTest) {
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get(),
                                                            /*is_modal=*/true);
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Size desktop_size(640, 480);
  std::unique_ptr<Buffer> desktop_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(desktop_size)));
  surface->Attach(desktop_buffer.get());
  surface->SetInputRegion(gfx::Rect(0, 0, 0, 0));
  shell_surface->SetGeometry(display.bounds());
  surface->Commit();

  EXPECT_FALSE(shell_surface->GetWidget()->widget_delegate()->CanResize());
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  aura::Window* root = window->GetRootWindow();

  ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(100, 0),
                       gfx::Point(100, 0), ui::EventTimeForNow(), 0, 0);
  aura::WindowTargeter targeter;
  aura::Window* found =
      static_cast<aura::Window*>(targeter.FindTargetForEvent(root, &event));
  EXPECT_FALSE(window->Contains(found));
}

// Test the snap functionalities in splitscreen in tablet mode.
TEST_F(ClientControlledShellSurfaceTest, SnapWindowInSplitViewModeTest) {
  UpdateDisplay("807x607");
  ash::Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  const gfx::Size buffer_size(800, 600);
  std::unique_ptr<Buffer> buffer1(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface1(new Surface);
  auto shell_surface1 =
      exo_test_helper()->CreateClientControlledShellSurface(surface1.get());
  // Start in maximized.
  shell_surface1->SetGeometry(gfx::Rect(0, 0, 800, 600));
  shell_surface1->SetMaximized();
  surface1->Attach(buffer1.get());
  surface1->Commit();

  aura::Window* window1 = shell_surface1->GetWidget()->GetNativeWindow();
  ash::WindowState* window_state1 = ash::WindowState::Get(window1);
  ash::ClientControlledState* state1 = static_cast<ash::ClientControlledState*>(
      ash::WindowState::TestApi::GetStateImpl(window_state1));
  EXPECT_EQ(window_state1->GetStateType(), ash::WindowStateType::kMaximized);

  // Snap window to left.
  ash::SplitViewController* split_view_controller =
      ash::SplitViewController::Get(ash::Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(window1, ash::SplitViewController::LEFT);
  state1->set_bounds_locally(true);
  window1->SetBounds(split_view_controller->GetSnappedWindowBoundsInScreen(
      ash::SplitViewController::LEFT, window1));
  state1->set_bounds_locally(false);
  EXPECT_EQ(window_state1->GetStateType(), ash::WindowStateType::kLeftSnapped);
  EXPECT_EQ(shell_surface1->GetWidget()->GetWindowBoundsInScreen(),
            split_view_controller->GetSnappedWindowBoundsInScreen(
                ash::SplitViewController::LEFT,
                shell_surface1->GetWidget()->GetNativeWindow()));
  EXPECT_TRUE(HasBackdrop());
  split_view_controller->EndSplitView();

  // Snap window to right.
  split_view_controller->SnapWindow(window1, ash::SplitViewController::RIGHT);
  state1->set_bounds_locally(true);
  window1->SetBounds(split_view_controller->GetSnappedWindowBoundsInScreen(
      ash::SplitViewController::RIGHT, window1));
  state1->set_bounds_locally(false);
  EXPECT_EQ(window_state1->GetStateType(), ash::WindowStateType::kRightSnapped);
  EXPECT_EQ(shell_surface1->GetWidget()->GetWindowBoundsInScreen(),
            split_view_controller->GetSnappedWindowBoundsInScreen(
                ash::SplitViewController::RIGHT,
                shell_surface1->GetWidget()->GetNativeWindow()));
  EXPECT_TRUE(HasBackdrop());
}

// The shell surface in SystemModal container should not become target
// at the edge.
TEST_F(ClientControlledShellSurfaceTest, ClientIniatedResize) {
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Size window_size(100, 100);
  std::unique_ptr<Buffer> desktop_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(window_size)));
  surface->Attach(desktop_buffer.get());
  shell_surface->SetGeometry(gfx::Rect(window_size));
  surface->Commit();
  EXPECT_TRUE(shell_surface->GetWidget()->widget_delegate()->CanResize());
  shell_surface->StartDrag(HTTOP, gfx::Point(0, 0));

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  // Client cannot start drag if mouse isn't pressed.
  ash::WindowState* window_state = ash::WindowState::Get(window);
  ASSERT_FALSE(window_state->is_dragged());

  // Client can start drag only when the mouse is pressed on the widget.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window);
  event_generator->PressLeftButton();
  shell_surface->StartDrag(HTTOP, gfx::Point(0, 0));
  ASSERT_TRUE(window_state->is_dragged());
  event_generator->ReleaseLeftButton();
  ASSERT_FALSE(window_state->is_dragged());

  // Press pressed outside of the window.
  event_generator->MoveMouseTo(gfx::Point(200, 50));
  event_generator->PressLeftButton();
  shell_surface->StartDrag(HTTOP, gfx::Point(0, 0));
  ASSERT_FALSE(window_state->is_dragged());
}

namespace {

// This class is only meant to used by CloseWindowWhenDraggingTest.
// When a ClientControlledShellSurface is destroyed, its natvie window will be
// hidden first and at that time its window delegate should have been properly
// reset.
class ShellSurfaceWindowObserver : public aura::WindowObserver {
 public:
  explicit ShellSurfaceWindowObserver(aura::Window* window)
      : window_(window),
        has_delegate_(ash::WindowState::Get(window)->HasDelegate()) {
    window_->AddObserver(this);
  }
  ~ShellSurfaceWindowObserver() override {
    if (window_) {
      window_->RemoveObserver(this);
      window_ = nullptr;
    }
  }

  bool has_delegate() const { return has_delegate_; }

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    DCHECK_EQ(window_, window);

    if (!visible) {
      has_delegate_ = ash::WindowState::Get(window_)->HasDelegate();
      window_->RemoveObserver(this);
      window_ = nullptr;
    }
  }

 private:
  aura::Window* window_;
  bool has_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ShellSurfaceWindowObserver);
};

}  // namespace

// Test that when a shell surface is destroyed during its dragging, its window
// delegate should be reset properly.
TEST_F(ClientControlledShellSurfaceTest, CloseWindowWhenDraggingTest) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface());
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  const gfx::Rect original_bounds(0, 0, 256, 256);
  shell_surface->SetGeometry(original_bounds);
  surface->Attach(buffer.get());
  surface->Commit();

  // Press on the edge of the window and start dragging.
  gfx::Point touch_location(256, 150);
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveTouch(touch_location);
  event_generator->PressTouch();

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_TRUE(ash::WindowState::Get(window)->is_dragged());
  auto observer = std::make_unique<ShellSurfaceWindowObserver>(window);
  EXPECT_TRUE(observer->has_delegate());

  // Destroy the window.
  shell_surface.reset();
  EXPECT_FALSE(observer->has_delegate());
}

namespace {

class ClientControlledShellSurfaceDragTest : public test::ExoTestBase {
 public:
  ClientControlledShellSurfaceDragTest() = default;
  ~ClientControlledShellSurfaceDragTest() override = default;

  // Sends a gesture scroll sequence to TabletModeAppWindowDragController.
  void SendGestureEvents(aura::Window* window,
                         const gfx::Point& location,
                         bool fling = false,
                         float velocity = 0.f) {
    ash::WindowState* window_state = ash::WindowState::Get(window);
    window_state->CreateDragDetails(gfx::Point(0, 0), HTCLIENT,
                                    ::wm::WINDOW_MOVE_SOURCE_TOUCH);
    std::unique_ptr<ash::TabletModeWindowDragController> controller_ =
        std::make_unique<ash::TabletModeWindowDragController>(
            window_state,
            std::make_unique<ash::TabletModeBrowserWindowDragDelegate>());
    controller_->drag_delegate_for_testing()
        ->set_drag_start_deadline_for_testing(base::Time::Now());
    controller_->Drag(location, 0);
    if (fling) {
      ui::GestureEventDetails details =
          ui::GestureEventDetails(ui::ET_SCROLL_FLING_START, 0, velocity);
      ui::GestureEvent event =
          ui::GestureEvent(location.x(), location.y(), ui::EF_NONE,
                           base::TimeTicks::Now(), details);
      ui::Event::DispatcherApi(&event).set_target(window);
      controller_->FlingOrSwipe(&event);
    } else {
      controller_->CompleteDrag();
    }
    ash::WindowState::Get(window)->DeleteDragDetails();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientControlledShellSurfaceDragTest);
};

}  // namespace

// Test the functionalities of dragging a window from top in tablet mode.
TEST_F(ClientControlledShellSurfaceDragTest, DragWindowFromTopInTabletMode) {
  UpdateDisplay("800x600");
  ash::Shell* shell = ash::Shell::Get();
  shell->tablet_mode_controller()->SetEnabledForTest(true);
  std::unique_ptr<Surface> surface(new Surface());
  const gfx::Size window_size(800, 552);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(window_size)));
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());
  shell_surface->SetMaximized();
  surface->Attach(buffer.get());
  shell_surface->SetGeometry(gfx::Rect(window_size));
  surface->Commit();

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  ASSERT_TRUE(ash::WindowState::Get(window)->IsMaximized());
  surface->SetFrame(SurfaceFrameType::AUTOHIDE);
  surface->Commit();

  // Drag the window by a small amount of distance will maximize the window
  // again.
  SendGestureEvents(window, gfx::Point(0, 10));
  EXPECT_TRUE(ash::WindowState::Get(window)->IsMaximized());
  EXPECT_FALSE(shell->overview_controller()->InOverviewSession());

  // FLING the window not inisde preview area with large enough y veloicty
  // (larger than kFlingToOverviewThreshold) will drop the window into overview.
  SendGestureEvents(
      window, gfx::Point(400, 10), /*fling=*/true,
      ash::TabletModeWindowDragDelegate::kFlingToOverviewThreshold + 10.f);
  ASSERT_TRUE(shell->overview_controller()->InOverviewSession());
  EXPECT_TRUE(
      shell->overview_controller()->overview_session()->IsWindowInOverview(
          window));

  // Drag the window long enough (pass one fourth of the screen vertical
  // height) to snap the window to splitscreen.
  shell->overview_controller()->EndOverview();
  SendGestureEvents(window, gfx::Point(0, 210));
  EXPECT_EQ(ash::WindowState::Get(window)->GetStateType(),
            ash::WindowStateType::kLeftSnapped);
}

namespace {

class ClientControlledShellSurfaceDisplayTest : public test::ExoTestBase {
 public:
  ClientControlledShellSurfaceDisplayTest() = default;
  ~ClientControlledShellSurfaceDisplayTest() override = default;

  static ash::WindowResizer* CreateDragWindowResizer(
      aura::Window* window,
      const gfx::Point& point_in_parent,
      int window_component) {
    return ash::CreateWindowResizer(window, point_in_parent, window_component,
                                    ::wm::WINDOW_MOVE_SOURCE_MOUSE)
        .release();
  }

  int bounds_change_count() const { return bounds_change_count_; }

  const std::vector<gfx::Rect>& requested_bounds() const {
    return requested_bounds_;
  }

  const std::vector<int64_t>& requested_display_ids() const {
    return requested_display_ids_;
  }

  void OnBoundsChangeEvent(ClientControlledShellSurface* shell_surface,
                           ash::WindowStateType current_state,
                           ash::WindowStateType requested_state,
                           int64_t display_id,
                           const gfx::Rect& bounds_in_display,
                           bool is_resize,
                           int bounds_change) {
    bounds_change_count_++;
    requested_bounds_.push_back(bounds_in_display);
    requested_display_ids_.push_back(display_id);
  }

  void Reset() {
    bounds_change_count_ = 0;
    requested_bounds_.clear();
    requested_display_ids_.clear();
  }

  gfx::Point CalculateDragPoint(const ash::WindowResizer& resizer,
                                int delta_x,
                                int delta_y) {
    gfx::Point location = resizer.GetInitialLocation();
    location.set_x(location.x() + delta_x);
    location.set_y(location.y() + delta_y);
    return location;
  }

 private:
  int bounds_change_count_ = 0;
  std::vector<gfx::Rect> requested_bounds_;
  std::vector<int64_t> requested_display_ids_;

  DISALLOW_COPY_AND_ASSIGN(ClientControlledShellSurfaceDisplayTest);
};

}  // namespace

TEST_F(ClientControlledShellSurfaceDisplayTest, MoveToAnotherDisplayByDrag) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  gfx::Size window_size(200, 200);
  std::unique_ptr<Buffer> desktop_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(window_size)));
  surface->Attach(desktop_buffer.get());

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  gfx::Rect initial_bounds(-150, 10, 200, 200);
  shell_surface->SetBounds(primary_display.id(), initial_bounds);
  surface->Commit();
  shell_surface->GetWidget()->Show();

  EXPECT_EQ(initial_bounds,
            shell_surface->GetWidget()->GetWindowBoundsInScreen());

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  std::unique_ptr<ash::WindowResizer> resizer(
      CreateDragWindowResizer(window, gfx::Point(), HTCAPTION));

  // Drag the pointer to the right. Once it reaches the right edge of the
  // primary display, it warps to the secondary.
  display::Display secondary_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]);
  // TODO(crbug.com/990589): Unit tests should be able to simulate mouse input
  // without having to call |CursorManager::SetDisplay|.
  ash::Shell::Get()->cursor_manager()->SetDisplay(secondary_display);
  resizer->Drag(CalculateDragPoint(*resizer, 800, 0), 0);

  shell_surface->set_bounds_changed_callback(base::BindRepeating(
      &ClientControlledShellSurfaceDisplayTest::OnBoundsChangeEvent,
      base::Unretained(this), base::Unretained(shell_surface.get())));
  resizer->CompleteDrag();

  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  // TODO(oshima): We currently generate bounds change twice,
  // first when reparented, then set bounds. Chagne wm::SetBoundsInScreen
  // to simply request WM_EVENT_SET_BOUND with target display id.
  ASSERT_EQ(2, bounds_change_count());
  // Bounds is local to 2nd display.
  EXPECT_EQ(gfx::Rect(-150, 10, 200, 200), requested_bounds()[0]);
  EXPECT_EQ(gfx::Rect(-150, 10, 200, 200), requested_bounds()[1]);

  EXPECT_EQ(secondary_display.id(), requested_display_ids()[0]);
  EXPECT_EQ(secondary_display.id(), requested_display_ids()[1]);
}

TEST_F(ClientControlledShellSurfaceDisplayTest,
       MoveToAnotherDisplayByShortcut) {
  UpdateDisplay("400x600,800x600");
  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  gfx::Size window_size(200, 200);
  std::unique_ptr<Buffer> desktop_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(window_size)));
  surface->Attach(desktop_buffer.get());

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Rect initial_bounds(-174, 10, 200, 200);
  shell_surface->SetBounds(primary_display.id(), initial_bounds);
  surface->Commit();
  shell_surface->GetWidget()->Show();

  EXPECT_EQ(initial_bounds,
            shell_surface->GetWidget()->GetWindowBoundsInScreen());

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  shell_surface->set_bounds_changed_callback(base::BindRepeating(
      &ClientControlledShellSurfaceDisplayTest::OnBoundsChangeEvent,
      base::Unretained(this), base::Unretained(shell_surface.get())));

  display::Display secondary_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]);

  EXPECT_TRUE(
      ash::window_util::MoveWindowToDisplay(window, secondary_display.id()));

  ASSERT_EQ(1, bounds_change_count());
  EXPECT_EQ(gfx::Rect(-174, 10, 200, 200), requested_bounds()[0]);
  EXPECT_EQ(secondary_display.id(), requested_display_ids()[0]);

  gfx::Rect secondary_position(700, 10, 200, 200);
  shell_surface->SetBounds(secondary_display.id(), secondary_position);
  surface->Commit();
  EXPECT_EQ(gfx::Rect(1100, 10, 200, 200), window->GetBoundsInScreen());

  Reset();

  // Moving to the outside of another display.
  EXPECT_TRUE(
      ash::window_util::MoveWindowToDisplay(window, primary_display.id()));
  ASSERT_EQ(1, bounds_change_count());
  // Should fit in the primary display.
  EXPECT_EQ(gfx::Rect(375, 10, 200, 200), requested_bounds()[0]);
  EXPECT_EQ(primary_display.id(), requested_display_ids()[0]);
}

TEST_F(ClientControlledShellSurfaceTest, CaptionButtonModel) {
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  std::unique_ptr<Buffer> desktop_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(gfx::Size(64, 64))));
  surface->Attach(desktop_buffer.get());
  shell_surface->SetGeometry(gfx::Rect(0, 0, 64, 64));
  surface->Commit();

  constexpr views::CaptionButtonIcon kAllButtons[] = {
      views::CAPTION_BUTTON_ICON_MINIMIZE,
      views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
      views::CAPTION_BUTTON_ICON_CLOSE,
      views::CAPTION_BUTTON_ICON_BACK,
      views::CAPTION_BUTTON_ICON_MENU,
  };
  constexpr uint32_t kAllButtonMask =
      1 << views::CAPTION_BUTTON_ICON_MINIMIZE |
      1 << views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE |
      1 << views::CAPTION_BUTTON_ICON_CLOSE |
      1 << views::CAPTION_BUTTON_ICON_BACK |
      1 << views::CAPTION_BUTTON_ICON_MENU;

  ash::NonClientFrameViewAsh* frame_view =
      static_cast<ash::NonClientFrameViewAsh*>(
          shell_surface->GetWidget()->non_client_view()->frame_view());
  ash::FrameCaptionButtonContainerView* container =
      static_cast<ash::HeaderView*>(frame_view->GetHeaderView())
          ->caption_button_container();

  // Visible
  for (auto visible : kAllButtons) {
    uint32_t visible_buttons = 1 << visible;
    shell_surface->SetFrameButtons(visible_buttons, 0);
    const ash::CaptionButtonModel* model = container->model();
    for (auto not_visible : kAllButtons) {
      if (not_visible != visible)
        EXPECT_FALSE(model->IsVisible(not_visible));
    }
    EXPECT_TRUE(model->IsVisible(visible));
    EXPECT_FALSE(model->IsEnabled(visible));
  }

  // Enable
  for (auto enabled : kAllButtons) {
    uint32_t enabled_buttons = 1 << enabled;
    shell_surface->SetFrameButtons(kAllButtonMask, enabled_buttons);
    const ash::CaptionButtonModel* model = container->model();
    for (auto not_enabled : kAllButtons) {
      if (not_enabled != enabled)
        EXPECT_FALSE(model->IsEnabled(not_enabled));
    }
    EXPECT_TRUE(model->IsEnabled(enabled));
    EXPECT_TRUE(model->IsVisible(enabled));
  }

  // Zoom mode
  EXPECT_FALSE(container->model()->InZoomMode());
  shell_surface->SetFrameButtons(
      kAllButtonMask | 1 << views::CAPTION_BUTTON_ICON_ZOOM, kAllButtonMask);
  EXPECT_TRUE(container->model()->InZoomMode());
}

// Makes sure that the "extra title" is respected by the window frame. When not
// set, there should be no text in the window frame, but the window's name
// should still be set (for overview mode, accessibility, etc.). When the debug
// text is set, the window frame should paint it.
TEST_F(ClientControlledShellSurfaceTest, SetExtraTitle) {
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(gfx::Size(640, 64))));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());
  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->GetWidget()->Show();

  const base::string16 window_title(base::ASCIIToUTF16("title"));
  shell_surface->SetTitle(window_title);
  const aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(window_title, window->GetTitle());
  EXPECT_FALSE(
      shell_surface->GetWidget()->widget_delegate()->ShouldShowWindowTitle());

  // Paints the frame and returns whether text was drawn. Unforunately the text
  // is a blob so its actual value can't be detected.
  auto paint_does_draw_text = [&shell_surface]() {
    TestCanvas canvas;
    shell_surface->OnSetFrame(SurfaceFrameType::NORMAL);
    ash::NonClientFrameViewAsh* frame_view =
        static_cast<ash::NonClientFrameViewAsh*>(
            shell_surface->GetWidget()->non_client_view()->frame_view());
    frame_view->SetVisible(true);
    // Paint to a layer so we can pass a root PaintInfo.
    frame_view->GetHeaderView()->SetPaintToLayer();
    gfx::Rect bounds(100, 100);
    auto list = base::MakeRefCounted<cc::DisplayItemList>();
    frame_view->GetHeaderView()->Paint(views::PaintInfo::CreateRootPaintInfo(
        ui::PaintContext(list.get(), 1.f, bounds, false), bounds.size()));
    list->Finalize();
    list->Raster(&canvas);
    return canvas.text_was_drawn();
  };

  EXPECT_FALSE(paint_does_draw_text());
  EXPECT_FALSE(
      shell_surface->GetWidget()->widget_delegate()->ShouldShowWindowTitle());

  // Setting the extra title/debug text won't change the window's title, but it
  // will be drawn by the frame header.
  shell_surface->SetExtraTitle(base::ASCIIToUTF16("extra"));
  EXPECT_EQ(window_title, window->GetTitle());
  EXPECT_TRUE(paint_does_draw_text());
  EXPECT_FALSE(
      shell_surface->GetWidget()->widget_delegate()->ShouldShowWindowTitle());
}

TEST_F(ClientControlledShellSurfaceTest, WideFrame) {
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  std::unique_ptr<Buffer> desktop_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(gfx::Size(64, 64))));
  surface->Attach(desktop_buffer.get());
  surface->SetInputRegion(gfx::Rect(0, 0, 64, 64));
  shell_surface->SetGeometry(gfx::Rect(0, 0, 64, 64));
  shell_surface->SetMaximized();
  surface->SetFrame(SurfaceFrameType::NORMAL);
  surface->Commit();

  auto* wide_frame = shell_surface->wide_frame_for_test();
  ASSERT_TRUE(wide_frame);
  EXPECT_FALSE(wide_frame->header_view()->in_immersive_mode());

  // Check targeter is still CustomWindowTargeter.
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  ASSERT_TRUE(window->parent());

  auto* custom_targeter = window->targeter();
  gfx::Point mouse_location(1, 50);

  auto* root = window->GetRootWindow();
  aura::WindowTargeter targeter;
  aura::Window* target;
  {
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, mouse_location, mouse_location,
                         ui::EventTimeForNow(), 0, 0);
    target =
        static_cast<aura::Window*>(targeter.FindTargetForEvent(root, &event));
  }
  EXPECT_EQ(surface->window(), target);

  // Disable input region and the targeter no longer find the surface.
  surface->SetInputRegion(gfx::Rect(0, 0, 0, 0));
  surface->Commit();
  {
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, mouse_location, mouse_location,
                         ui::EventTimeForNow(), 0, 0);
    target =
        static_cast<aura::Window*>(targeter.FindTargetForEvent(root, &event));
  }
  EXPECT_NE(surface->window(), target);

  // Test AUTOHIDE -> NORMAL
  surface->SetFrame(SurfaceFrameType::AUTOHIDE);
  surface->Commit();
  EXPECT_TRUE(wide_frame->header_view()->in_immersive_mode());

  surface->SetFrame(SurfaceFrameType::NORMAL);
  surface->Commit();
  EXPECT_FALSE(wide_frame->header_view()->in_immersive_mode());

  EXPECT_EQ(custom_targeter, window->targeter());

  // Test AUTOHIDE -> NONE
  surface->SetFrame(SurfaceFrameType::AUTOHIDE);
  surface->Commit();
  EXPECT_TRUE(wide_frame->header_view()->in_immersive_mode());

  // Switching to NONE means no frame so it should delete wide frame.
  surface->SetFrame(SurfaceFrameType::NONE);
  surface->Commit();
  EXPECT_FALSE(shell_surface->wide_frame_for_test());
  {
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, mouse_location, mouse_location,
                         ui::EventTimeForNow(), 0, 0);
    target =
        static_cast<aura::Window*>(targeter.FindTargetForEvent(root, &event));
  }
  EXPECT_NE(surface->window(), target);

  // Unmaximize it and the frame should be normal.
  shell_surface->SetRestored();
  surface->Commit();

  EXPECT_FALSE(shell_surface->wide_frame_for_test());
  {
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, mouse_location, mouse_location,
                         ui::EventTimeForNow(), 0, 0);
    target =
        static_cast<aura::Window*>(targeter.FindTargetForEvent(root, &event));
  }
  EXPECT_NE(surface->window(), target);

  // Re-enable input region and the targeter should find the surface again.
  surface->SetInputRegion(gfx::Rect(0, 0, 64, 64));
  surface->Commit();
  {
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, mouse_location, mouse_location,
                         ui::EventTimeForNow(), 0, 0);
    target =
        static_cast<aura::Window*>(targeter.FindTargetForEvent(root, &event));
  }
  EXPECT_EQ(surface->window(), target);
}

TEST_F(ClientControlledShellSurfaceTest, NoFrameOnModalContainer) {
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get(),
                                                            /*is_modal=*/true);

  std::unique_ptr<Buffer> desktop_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(gfx::Size(64, 64))));
  surface->Attach(desktop_buffer.get());
  surface->SetFrame(SurfaceFrameType::NORMAL);
  surface->Commit();
  EXPECT_FALSE(shell_surface->frame_enabled());
  surface->SetFrame(SurfaceFrameType::AUTOHIDE);
  surface->Commit();
  EXPECT_FALSE(shell_surface->frame_enabled());
}

TEST_F(ClientControlledShellSurfaceTest,
       SetGeometryReparentsToDisplayOnFirstCommit) {
  UpdateDisplay("100x100,100x100");

  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

  const auto* screen = display::Screen::GetScreen();

  {
    std::unique_ptr<Surface> surface(new Surface);
    auto shell_surface =
        exo_test_helper()->CreateClientControlledShellSurface(surface.get());

    gfx::Rect geometry(16, 16, 32, 32);
    shell_surface->SetGeometry(geometry);
    surface->Attach(buffer.get());
    surface->Commit();
    EXPECT_EQ(geometry, shell_surface->GetWidget()->GetWindowBoundsInScreen());

    display::Display primary_display = screen->GetPrimaryDisplay();
    display::Display display = screen->GetDisplayNearestWindow(
        shell_surface->GetWidget()->GetNativeWindow());
    EXPECT_EQ(primary_display.id(), display.id());
  }

  {
    std::unique_ptr<Surface> surface(new Surface);
    auto shell_surface =
        exo_test_helper()->CreateClientControlledShellSurface(surface.get());

    gfx::Rect geometry(116, 16, 32, 32);
    shell_surface->SetGeometry(geometry);
    surface->Attach(buffer.get());
    surface->Commit();
    EXPECT_EQ(geometry, shell_surface->GetWidget()->GetWindowBoundsInScreen());

    auto root_windows = ash::Shell::GetAllRootWindows();
    display::Display secondary_display =
        screen->GetDisplayNearestWindow(root_windows[1]);
    display::Display display = screen->GetDisplayNearestWindow(
        shell_surface->GetWidget()->GetNativeWindow());
    EXPECT_EQ(secondary_display.id(), display.id());
  }
}

TEST_F(ClientControlledShellSurfaceTest, SetBoundsReparentsToDisplay) {
  UpdateDisplay("100x100,100+0-100x100");

  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

  const auto* screen = display::Screen::GetScreen();

  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  display::Display primary_display = screen->GetPrimaryDisplay();
  gfx::Rect geometry(16, 16, 32, 32);

  // Move to primary display with bounds inside display.
  shell_surface->SetBounds(primary_display.id(), geometry);
  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_EQ(geometry, shell_surface->GetWidget()->GetWindowBoundsInScreen());

  display::Display display = screen->GetDisplayNearestWindow(
      shell_surface->GetWidget()->GetNativeWindow());
  EXPECT_EQ(primary_display.id(), display.id());

  auto root_windows = ash::Shell::GetAllRootWindows();
  display::Display secondary_display =
      screen->GetDisplayNearestWindow(root_windows[1]);

  // Move to secondary display with bounds inside display.
  shell_surface->SetBounds(secondary_display.id(), geometry);
  surface->Commit();
  EXPECT_EQ(gfx::Rect(116, 16, 32, 32),
            shell_surface->GetWidget()->GetWindowBoundsInScreen());

  display = screen->GetDisplayNearestWindow(
      shell_surface->GetWidget()->GetNativeWindow());
  EXPECT_EQ(secondary_display.id(), display.id());

  // Move to primary display with bounds outside display.
  geometry.set_origin({-100, 0});
  shell_surface->SetBounds(primary_display.id(), geometry);
  surface->Commit();
  EXPECT_EQ(gfx::Rect(-6, 0, 32, 32),
            shell_surface->GetWidget()->GetWindowBoundsInScreen());

  display = screen->GetDisplayNearestWindow(
      shell_surface->GetWidget()->GetNativeWindow());
  EXPECT_EQ(primary_display.id(), display.id());

  // Move to secondary display with bounds outside display.
  shell_surface->SetBounds(secondary_display.id(), geometry);
  surface->Commit();
  EXPECT_EQ(gfx::Rect(94, 0, 32, 32),
            shell_surface->GetWidget()->GetWindowBoundsInScreen());

  display = screen->GetDisplayNearestWindow(
      shell_surface->GetWidget()->GetNativeWindow());
  EXPECT_EQ(secondary_display.id(), display.id());
}

// Set orientation lock to a window.
TEST_F(ClientControlledShellSurfaceTest, SetOrientationLock) {
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .SetFirstDisplayAsInternalDisplay();

  EnableTabletMode(true);
  ash::ScreenOrientationController* controller =
      ash::Shell::Get()->screen_orientation_controller();

  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);

  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());
  surface->Attach(buffer.get());
  shell_surface->SetMaximized();
  surface->Commit();

  shell_surface->SetOrientationLock(
      ash::OrientationLockType::kLandscapePrimary);
  EXPECT_TRUE(controller->rotation_locked());
  display::Display display(display::Screen::GetScreen()->GetPrimaryDisplay());
  gfx::Size displaySize = display.size();
  EXPECT_GT(displaySize.width(), displaySize.height());

  shell_surface->SetOrientationLock(ash::OrientationLockType::kAny);
  EXPECT_FALSE(controller->rotation_locked());

  EnableTabletMode(false);
}

// Tests adjust bounds locally should also request remote client bounds update.
TEST_F(ClientControlledShellSurfaceTest, AdjustBoundsLocally) {
  UpdateDisplay("800x600");
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(gfx::Size(64, 64))));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());
  gfx::Rect requested_bounds;
  shell_surface->set_bounds_changed_callback(base::BindRepeating(
      [](gfx::Rect* dst, ash::WindowStateType current_state,
         ash::WindowStateType requested_state, int64_t display_id,
         const gfx::Rect& bounds, bool is_resize,
         int bounds_change) { *dst = bounds; },
      base::Unretained(&requested_bounds)));
  surface->Attach(buffer.get());
  surface->Commit();

  gfx::Rect client_bounds(900, 0, 200, 300);
  shell_surface->SetGeometry(client_bounds);
  surface->Commit();

  views::Widget* widget = shell_surface->GetWidget();
  EXPECT_EQ(gfx::Rect(774, 0, 200, 300), widget->GetWindowBoundsInScreen());
  EXPECT_EQ(gfx::Rect(774, 0, 200, 300), requested_bounds);

  // Receiving the same bounds shouldn't try to update the bounds again.
  requested_bounds.SetRect(0, 0, 0, 0);
  shell_surface->SetGeometry(client_bounds);
  surface->Commit();

  EXPECT_TRUE(requested_bounds.IsEmpty());
}

TEST_F(ClientControlledShellSurfaceTest, SnappedInTabletMode) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface(
      exo_test_helper()->CreateClientControlledShellSurface(surface.get()));
  shell_surface->SetGeometry(gfx::Rect(buffer_size));
  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->GetWidget()->Show();
  auto* window = shell_surface->GetWidget()->GetNativeWindow();
  auto* window_state = ash::WindowState::Get(window);

  EnableTabletMode(true);

  ash::WMEvent event(ash::WM_EVENT_SNAP_LEFT);
  window_state->OnWMEvent(&event);
  EXPECT_EQ(window_state->GetStateType(), ash::WindowStateType::kLeftSnapped);

  ash::NonClientFrameViewAsh* frame_view =
      static_cast<ash::NonClientFrameViewAsh*>(
          shell_surface->GetWidget()->non_client_view()->frame_view());
  // Snapped window can also use auto hide.
  surface->SetFrame(SurfaceFrameType::AUTOHIDE);
  EXPECT_TRUE(frame_view->GetVisible());
  EXPECT_TRUE(frame_view->GetHeaderView()->in_immersive_mode());
}

TEST_F(ClientControlledShellSurfaceTest, PipWindowCannotBeActivated) {
  const gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface());
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();

  EXPECT_TRUE(shell_surface->GetWidget()->IsActive());
  EXPECT_TRUE(shell_surface->GetWidget()->CanActivate());

  // Entering PIP should unactivate the window and make the widget
  // unactivatable.
  shell_surface->SetPip();
  surface->Commit();

  EXPECT_FALSE(shell_surface->GetWidget()->IsActive());
  EXPECT_FALSE(shell_surface->GetWidget()->CanActivate());

  // Leaving PIP should make it activatable again.
  shell_surface->SetRestored();
  surface->Commit();

  EXPECT_TRUE(shell_surface->GetWidget()->CanActivate());
}

TEST_F(ClientControlledShellSurfaceDisplayTest,
       NoBoundsChangeEventInMinimized) {
  gfx::Size buffer_size(100, 100);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface(
      exo_test_helper()->CreateClientControlledShellSurface(surface.get()));
  surface->Attach(buffer.get());
  shell_surface->SetGeometry(gfx::Rect(buffer_size));
  surface->Commit();

  shell_surface->set_bounds_changed_callback(base::BindRepeating(
      &ClientControlledShellSurfaceDisplayTest::OnBoundsChangeEvent,
      base::Unretained(this), base::Unretained(shell_surface.get())));
  ASSERT_EQ(0, bounds_change_count());
  auto* window_state =
      ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow());
  int64_t display_id = window_state->GetDisplay().id();

  shell_surface->OnBoundsChangeEvent(ash::WindowStateType::kNormal,
                                     ash::WindowStateType::kNormal, display_id,
                                     gfx::Rect(10, 10, 100, 100), 0);
  ASSERT_EQ(1, bounds_change_count());

  EXPECT_FALSE(shell_surface->GetWidget()->IsMinimized());

  shell_surface->SetMinimized();
  surface->Commit();

  EXPECT_TRUE(shell_surface->GetWidget()->IsMinimized());
  shell_surface->OnBoundsChangeEvent(ash::WindowStateType::kMinimized,
                                     ash::WindowStateType::kMinimized,
                                     display_id, gfx::Rect(0, 0, 100, 100), 0);
  ASSERT_EQ(1, bounds_change_count());

  // Send bounds change when exiting minmized.
  shell_surface->OnBoundsChangeEvent(ash::WindowStateType::kMinimized,
                                     ash::WindowStateType::kNormal, display_id,
                                     gfx::Rect(0, 0, 100, 100), 0);
  ASSERT_EQ(2, bounds_change_count());

  // Snapped, in clamshell mode.
  ash::NonClientFrameViewAsh* frame_view =
      static_cast<ash::NonClientFrameViewAsh*>(
          shell_surface->GetWidget()->non_client_view()->frame_view());
  surface->SetFrame(SurfaceFrameType::NORMAL);
  surface->Commit();
  shell_surface->OnBoundsChangeEvent(ash::WindowStateType::kMinimized,
                                     ash::WindowStateType::kRightSnapped,
                                     display_id, gfx::Rect(0, 0, 100, 100), 0);
  EXPECT_EQ(3, bounds_change_count());
  EXPECT_EQ(
      frame_view->GetClientBoundsForWindowBounds(gfx::Rect(0, 0, 100, 100)),
      requested_bounds().back());
  EXPECT_NE(gfx::Rect(0, 0, 100, 100), requested_bounds().back());

  // Snapped, in tablet mode.
  EnableTabletMode(true);
  shell_surface->OnBoundsChangeEvent(ash::WindowStateType::kMinimized,
                                     ash::WindowStateType::kRightSnapped,
                                     display_id, gfx::Rect(0, 0, 100, 100), 0);
  EXPECT_EQ(4, bounds_change_count());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), requested_bounds().back());
}

TEST_F(ClientControlledShellSurfaceTest, SetPipWindowBoundsAnimates) {
  const gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface());
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());
  shell_surface->SetGeometry(gfx::Rect(buffer_size));
  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->SetPip();
  surface->Commit();
  shell_surface->GetWidget()->Show();

  ui::ScopedAnimationDurationScaleMode animation_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(gfx::Rect(0, 0, 256, 256), window->layer()->GetTargetBounds());
  EXPECT_EQ(gfx::Rect(0, 0, 256, 256), window->layer()->bounds());
  window->SetBounds(gfx::Rect(10, 10, 256, 256));
  EXPECT_EQ(gfx::Rect(10, 10, 256, 256), window->layer()->GetTargetBounds());
  EXPECT_EQ(gfx::Rect(0, 0, 256, 256), window->layer()->bounds());
}

TEST_F(ClientControlledShellSurfaceTest, PipWindowDragDoesNotAnimate) {
  const gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface());
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());
  shell_surface->SetGeometry(gfx::Rect(buffer_size));
  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->SetPip();
  surface->Commit();
  shell_surface->GetWidget()->Show();

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(gfx::Rect(0, 0, 256, 256), window->layer()->GetTargetBounds());
  EXPECT_EQ(gfx::Rect(0, 0, 256, 256), window->layer()->bounds());
  ui::ScopedAnimationDurationScaleMode animation_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  std::unique_ptr<ash::WindowResizer> resizer(ash::CreateWindowResizer(
      window, gfx::Point(), HTCAPTION, ::wm::WINDOW_MOVE_SOURCE_MOUSE));
  resizer->Drag(gfx::Point(10, 10), 0);
  EXPECT_EQ(gfx::Rect(10, 10, 256, 256), window->layer()->GetTargetBounds());
  EXPECT_EQ(gfx::Rect(10, 10, 256, 256), window->layer()->bounds());
  resizer->CompleteDrag();
}

TEST_F(ClientControlledShellSurfaceTest,
       PipWindowDragDoesNotAnimateWithExtraCommit) {
  const gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface());
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());
  shell_surface->SetGeometry(gfx::Rect(buffer_size));
  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->SetPip();
  surface->Commit();
  shell_surface->GetWidget()->Show();

  // Making an extra commit may set the next bounds change animation type
  // wrongly.
  surface->Commit();

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(gfx::Rect(0, 0, 256, 256), window->layer()->GetTargetBounds());
  EXPECT_EQ(gfx::Rect(0, 0, 256, 256), window->layer()->bounds());
  ui::ScopedAnimationDurationScaleMode animation_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  std::unique_ptr<ash::WindowResizer> resizer(ash::CreateWindowResizer(
      window, gfx::Point(), HTCAPTION, ::wm::WINDOW_MOVE_SOURCE_MOUSE));
  resizer->Drag(gfx::Point(10, 10), 0);
  EXPECT_EQ(gfx::Rect(10, 10, 256, 256), window->layer()->GetTargetBounds());
  EXPECT_EQ(gfx::Rect(10, 10, 256, 256), window->layer()->bounds());
  EXPECT_FALSE(window->layer()->GetAnimator()->is_animating());
  resizer->CompleteDrag();
}

TEST_F(ClientControlledShellSurfaceTest,
       ExpandingPipInTabletModeEndsSplitView) {
  EnableTabletMode(true);

  ash::SplitViewController* split_view_controller =
      ash::SplitViewController::Get(ash::Shell::GetPrimaryRootWindow());
  EXPECT_FALSE(split_view_controller->InSplitViewMode());

  // Create a PIP window:
  const gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface());
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->SetPip();
  surface->Commit();
  shell_surface->GetWidget()->Show();

  auto window_left = CreateTestWindow();
  auto window_right = CreateTestWindow();

  split_view_controller->SnapWindow(
      window_left.get(), ash::SplitViewController::SnapPosition::LEFT);
  split_view_controller->SnapWindow(
      window_right.get(), ash::SplitViewController::SnapPosition::RIGHT);
  EXPECT_TRUE(split_view_controller->InSplitViewMode());

  // Should end split view.
  shell_surface->SetRestored();
  surface->Commit();
  EXPECT_FALSE(split_view_controller->InSplitViewMode());
}

TEST_F(ClientControlledShellSurfaceTest,
       DismissingPipInTabletModeDoesNotEndSplitView) {
  EnableTabletMode(true);

  ash::SplitViewController* split_view_controller =
      ash::SplitViewController::Get(ash::Shell::GetPrimaryRootWindow());
  EXPECT_FALSE(split_view_controller->InSplitViewMode());

  // Create a PIP window:
  const gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface());
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->SetPip();
  surface->Commit();
  shell_surface->GetWidget()->Show();

  auto window_left = CreateTestWindow();
  auto window_right = CreateTestWindow();

  split_view_controller->SnapWindow(
      window_left.get(), ash::SplitViewController::SnapPosition::LEFT);
  split_view_controller->SnapWindow(
      window_right.get(), ash::SplitViewController::SnapPosition::RIGHT);
  EXPECT_TRUE(split_view_controller->InSplitViewMode());

  // Should not end split-view.
  shell_surface->SetMinimized();
  surface->Commit();
  EXPECT_TRUE(split_view_controller->InSplitViewMode());
}

TEST_F(ClientControlledShellSurfaceTest, DoNotReplayWindowStateRequest) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  shell_surface->set_state_changed_callback(
      base::BindRepeating([](ash::WindowStateType, ash::WindowStateType) {
        // This callback must not be called when a widget is created.
        EXPECT_TRUE(false);
      }));

  shell_surface->SetMinimized();
  surface->Attach(buffer.get());
  surface->Commit();
}

TEST_F(ClientControlledShellSurfaceDisplayTest,
       DoNotRequestBoundsChangeWithStateTransition) {
  gfx::Size buffer_size(64, 64);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();

  auto* widget = shell_surface->GetWidget();
  const gfx::Rect original_bounds(gfx::Point(20, 20), buffer_size);
  shell_surface->SetGeometry(original_bounds);
  widget->Restore();
  surface->Commit();

  shell_surface->set_bounds_changed_callback(base::BindRepeating(
      &ClientControlledShellSurfaceDisplayTest::OnBoundsChangeEvent,
      base::Unretained(this), base::Unretained(shell_surface.get())));

  shell_surface->SetPip();
  surface->Commit();

  ASSERT_EQ(0, bounds_change_count());
}

TEST_F(ClientControlledShellSurfaceTest,
       DoNotSavePipBoundsAcrossMultiplePipTransition) {
  // Create a PIP window:
  const gfx::Size content_size(100, 100);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(content_size));

  auto surface = std::make_unique<Surface>();
  auto shell_surface =
      exo_test_helper()->CreateClientControlledShellSurface(surface.get());
  surface->Attach(buffer.get());
  surface->Commit();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  const gfx::Rect original_bounds(gfx::Point(10, 10), content_size);
  shell_surface->SetGeometry(original_bounds);
  shell_surface->SetPip();
  surface->Commit();
  EXPECT_EQ(gfx::Rect(10, 10, 100, 100), window->bounds());
  shell_surface->GetWidget()->Show();

  const gfx::Rect moved_bounds(gfx::Point(20, 20), content_size);
  shell_surface->SetGeometry(moved_bounds);
  surface->Commit();
  EXPECT_EQ(gfx::Rect(20, 20, 100, 100), window->bounds());

  shell_surface->SetRestored();
  surface->Commit();
  shell_surface->SetGeometry(original_bounds);
  shell_surface->SetPip();
  surface->Commit();
  EXPECT_EQ(gfx::Rect(10, 10, 100, 100), window->bounds());

  shell_surface->SetGeometry(moved_bounds);
  surface->Commit();
  EXPECT_EQ(gfx::Rect(20, 20, 100, 100), window->bounds());

  shell_surface->SetRestored();
  surface->Commit();
  shell_surface->SetGeometry(moved_bounds);
  shell_surface->SetPip();
  surface->Commit();
  EXPECT_EQ(gfx::Rect(20, 20, 100, 100), window->bounds());
}

}  // namespace exo
