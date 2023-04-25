// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/client_controlled_shell_surface.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/frame/wide_frame_view.h"
#include "ash/public/cpp/arc_resize_lock_type.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/test_widget_builder.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_restore/window_restore_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "cc/paint/display_item_list.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/caption_button_model.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/header_view.h"
#include "chromeos/ui/wm/window_util.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/permission.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/test/surface_tree_host_test_util.h"
#include "third_party/skia/include/utils/SkNoDrawCanvas.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/display/display.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/util/display_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_targeter.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/paint_info.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/caption_button_types.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_types.h"

using chromeos::WindowStateType;

namespace exo {
namespace {

class ClientControlledShellSurfaceTest
    : public test::ExoTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ClientControlledShellSurfaceTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(kExoReactiveFrameSubmission);
    } else {
      feature_list_.InitAndDisableFeature(kExoReactiveFrameSubmission);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

bool HasBackdrop() {
  ash::WorkspaceController* wc = ash::ShellTestApi().workspace_controller();
  return !!ash::WorkspaceControllerTestApi(wc).GetBackdropWindow();
}

bool IsWidgetPinned(views::Widget* widget) {
  return ash::WindowState::Get(widget->GetNativeWindow())->IsPinned();
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

  TestCanvas(const TestCanvas&) = delete;
  TestCanvas& operator=(const TestCanvas&) = delete;

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
};

}  // namespace

// Instantiate the values of disabling/enabling reactive frame submission in the
// parameterized tests.
INSTANTIATE_TEST_SUITE_P(All,
                         ClientControlledShellSurfaceTest,
                         testing::Values(false, true));

TEST_P(ClientControlledShellSurfaceTest, SetPinned) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .BuildClientControlledShellSurface();

  shell_surface->SetPinned(chromeos::WindowPinType::kTrustedPinned);
  EXPECT_FALSE(IsWidgetPinned(shell_surface->GetWidget()));
  shell_surface->root_surface()->Commit();
  EXPECT_TRUE(IsWidgetPinned(shell_surface->GetWidget()));

  shell_surface->SetRestored();
  EXPECT_TRUE(IsWidgetPinned(shell_surface->GetWidget()));
  shell_surface->root_surface()->Commit();
  EXPECT_FALSE(IsWidgetPinned(shell_surface->GetWidget()));

  shell_surface->SetPinned(chromeos::WindowPinType::kPinned);
  EXPECT_FALSE(IsWidgetPinned(shell_surface->GetWidget()));
  shell_surface->root_surface()->Commit();
  EXPECT_TRUE(IsWidgetPinned(shell_surface->GetWidget()));

  shell_surface->SetRestored();
  EXPECT_TRUE(IsWidgetPinned(shell_surface->GetWidget()));
  shell_surface->root_surface()->Commit();
  EXPECT_FALSE(IsWidgetPinned(shell_surface->GetWidget()));
}

TEST_P(ClientControlledShellSurfaceTest, SetSystemUiVisibility) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .BuildClientControlledShellSurface();

  shell_surface->SetSystemUiVisibility(true);
  EXPECT_TRUE(
      ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow())
          ->autohide_shelf_when_maximized_or_fullscreen());

  shell_surface->SetSystemUiVisibility(false);
  EXPECT_FALSE(
      ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow())
          ->autohide_shelf_when_maximized_or_fullscreen());
}

TEST_P(ClientControlledShellSurfaceTest, SetTopInset) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({64, 64})
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  ASSERT_TRUE(window);
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));
  int top_inset_height = 20;
  shell_surface->SetTopInset(top_inset_height);
  surface->Commit();
  EXPECT_EQ(top_inset_height, window->GetProperty(aura::client::kTopViewInset));
}

TEST_P(ClientControlledShellSurfaceTest, UpdateModalWindow) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({640, 480})
                           .SetUseSystemModalContainer()
                           .SetInputRegion(cc::Region())
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
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
      display->CreateSubSurface(child.get(), surface));
  surface->SetSubSurfacePosition(child.get(), gfx::PointF(10, 10));
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

TEST_P(ClientControlledShellSurfaceTest,
       ModalWindowSetSystemModalBeforeCommit) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({640, 480})
                           .SetUseSystemModalContainer()
                           .SetInputRegion(cc::Region())
                           .SetNoCommit()
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();

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

TEST_P(ClientControlledShellSurfaceTest,
       NonSystemModalContainerCantChangeModality) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({640, 480})
                           .SetInputRegion(cc::Region())
                           .EnableSystemModal()
                           .BuildClientControlledShellSurface();
  // It is expected that a non system modal container is unable to set a system
  // modal.
  EXPECT_FALSE(ash::Shell::IsSystemModalWindowOpen());
}

TEST_P(ClientControlledShellSurfaceTest, SurfaceShadow) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({128, 128})
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  // 1) Initial state, no shadow (SurfaceFrameType is NONE);
  EXPECT_FALSE(wm::ShadowController::GetShadowForWindow(window));
  std::unique_ptr<Display> display(new Display);

  // 2) Just creating a sub surface won't create a shadow.
  auto* child =
      test::ShellSurfaceBuilder::AddChildSurface(surface, {0, 0, 128, 128});
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

TEST_P(ClientControlledShellSurfaceTest, ShadowWithStateChange) {
  const gfx::Size content_size(100, 100);
  // Position the widget at 10,10 so that we get non zero offset.
  auto shell_surface = exo::test::ShellSurfaceBuilder(content_size)
                           .SetGeometry({gfx::Point(10, 10), content_size})
                           .SetFrame(SurfaceFrameType::SHADOW)
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();

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

TEST_P(ClientControlledShellSurfaceTest, ShadowWithTransform) {
  const gfx::Size content_size(100, 100);
  // Position the widget at 10,10 so that we get non zero offset.
  auto shell_surface = exo::test::ShellSurfaceBuilder(content_size)
                           .SetGeometry({gfx::Point(10, 10), content_size})
                           .SetFrame(SurfaceFrameType::SHADOW)
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
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

TEST_P(ClientControlledShellSurfaceTest, ShadowStartMaximized) {
  auto shell_surface =
      exo::test::ShellSurfaceBuilder({256, 256})
          .SetWindowState(chromeos::WindowStateType::kMaximized)
          .SetFrame(SurfaceFrameType::SHADOW)
          .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
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

TEST_P(ClientControlledShellSurfaceTest, Frame) {
  UpdateDisplay("800x600");

  gfx::Rect client_bounds(20, 50, 300, 200);
  gfx::Rect fullscreen_bounds(0, 0, 800, 600);
  // The window bounds is the client bounds + frame size.
  gfx::Rect normal_window_bounds(20, 18, 300, 232);

  auto shell_surface = exo::test::ShellSurfaceBuilder({client_bounds.size()})
                           .SetGeometry(client_bounds)
                           .SetFrame(SurfaceFrameType::NORMAL)
                           .SetNoCommit()
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  shell_surface->SetSystemUiVisibility(true);  // disable shelf.
  surface->Commit();

  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();

  views::Widget* widget = shell_surface->GetWidget();
  ash::NonClientFrameViewAsh* frame_view =
      static_cast<ash::NonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  // Normal state.
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(frame_view->GetFrameEnabled());
  EXPECT_EQ(normal_window_bounds, widget->GetWindowBoundsInScreen());
  EXPECT_EQ(client_bounds,
            frame_view->GetClientBoundsForWindowBounds(normal_window_bounds));

  // Maximized
  shell_surface->SetMaximized();
  shell_surface->SetGeometry(gfx::Rect(0, 0, 800, 568));
  surface->Commit();

  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(frame_view->GetFrameEnabled());
  EXPECT_EQ(fullscreen_bounds, widget->GetWindowBoundsInScreen());
  EXPECT_EQ(
      gfx::Size(800, 568),
      frame_view->GetClientBoundsForWindowBounds(fullscreen_bounds).size());

  // With work area top insets.
  display_manager->UpdateWorkAreaOfDisplay(display_id,
                                           gfx::Insets::TLBR(200, 0, 0, 0));
  shell_surface->SetGeometry(gfx::Rect(0, 0, 800, 368));
  surface->Commit();

  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(frame_view->GetFrameEnabled());
  EXPECT_EQ(gfx::Rect(0, 200, 800, 400), widget->GetWindowBoundsInScreen());

  display_manager->UpdateWorkAreaOfDisplay(display_id, gfx::Insets());

  // AutoHide
  surface->SetFrame(SurfaceFrameType::AUTOHIDE);
  shell_surface->SetGeometry(fullscreen_bounds);
  surface->Commit();

  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(frame_view->GetFrameEnabled());
  EXPECT_EQ(fullscreen_bounds, widget->GetWindowBoundsInScreen());
  EXPECT_EQ(fullscreen_bounds,
            frame_view->GetClientBoundsForWindowBounds(fullscreen_bounds));

  // Fullscreen state.
  shell_surface->SetFullscreen(true);
  surface->Commit();

  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(frame_view->GetFrameEnabled());
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
  EXPECT_TRUE(frame_view->GetFrameEnabled());
  EXPECT_EQ(normal_window_bounds, widget->GetWindowBoundsInScreen());
  EXPECT_EQ(client_bounds,
            frame_view->GetClientBoundsForWindowBounds(normal_window_bounds));

  // No frame. The all bounds are same as client bounds.
  shell_surface->SetRestored();
  shell_surface->SetGeometry(client_bounds);
  surface->SetFrame(SurfaceFrameType::NONE);
  surface->Commit();

  widget->LayoutRootViewIfNecessary();
  EXPECT_FALSE(frame_view->GetFrameEnabled());
  EXPECT_EQ(client_bounds, widget->GetWindowBoundsInScreen());
  EXPECT_EQ(client_bounds,
            frame_view->GetClientBoundsForWindowBounds(client_bounds));

  // Test NONE -> AUTOHIDE -> NONE
  shell_surface->SetMaximized();
  shell_surface->SetGeometry(fullscreen_bounds);
  surface->SetFrame(SurfaceFrameType::AUTOHIDE);
  surface->Commit();

  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(frame_view->GetFrameEnabled());
  EXPECT_TRUE(frame_view->GetHeaderView()->in_immersive_mode());

  surface->SetFrame(SurfaceFrameType::NONE);
  surface->Commit();

  widget->LayoutRootViewIfNecessary();
  EXPECT_FALSE(frame_view->GetFrameEnabled());
  EXPECT_FALSE(frame_view->GetHeaderView()->in_immersive_mode());
}

namespace {

class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler() = default;

  TestEventHandler(const TestEventHandler&) = delete;
  TestEventHandler& operator=(const TestEventHandler&) = delete;

  ~TestEventHandler() override = default;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    mouse_events_.push_back(*event);
  }

  const std::vector<ui::MouseEvent>& mouse_events() const {
    return mouse_events_;
  }

 private:
  std::vector<ui::MouseEvent> mouse_events_;
};

}  // namespace

TEST_P(ClientControlledShellSurfaceTest, NoSynthesizedEventOnFrameChange) {
  UpdateDisplay("800x600");

  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .SetWindowState(chromeos::WindowStateType::kNormal)
                           .SetFrame(SurfaceFrameType::NORMAL)
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();

  // Maximized
  gfx::Rect fullscreen_bounds(0, 0, 800, 600);
  shell_surface->SetMaximized();
  shell_surface->SetGeometry(fullscreen_bounds);
  surface->Commit();

  // AutoHide
  test::WaitForLastFrameAck(shell_surface.get());
  aura::Env* env = aura::Env::GetInstance();
  gfx::Rect cropped_fullscreen_bounds(0, 0, 800, 400);
  env->SetLastMouseLocation(gfx::Point(100, 30));
  TestEventHandler handler;
  env->AddPreTargetHandler(&handler);
  surface->SetFrame(SurfaceFrameType::AUTOHIDE);
  shell_surface->SetGeometry(cropped_fullscreen_bounds);
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  EXPECT_TRUE(handler.mouse_events().empty());
  env->RemovePreTargetHandler(&handler);
}

// Shell surfaces should not emit extra events on commit even if using pixel
// coordinates and a cursor is hovering over the window.
// https://crbug.com/1296315.
TEST_P(ClientControlledShellSurfaceTest,
       NoSynthesizedEventsForPixelCoordinates) {
  TestEventHandler event_handler;

  auto shell_surface = exo::test::ShellSurfaceBuilder({400, 400})
                           .SetNoCommit()
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  // Pixel coordinates add a transform to the underlying layer.
  shell_surface->set_client_submits_surfaces_in_pixel_coordinates(true);

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  gfx::Rect initial_bounds(150, 10, 200, 200);
  shell_surface->SetBounds(primary_display.id(), initial_bounds);

  // Tested condition only happens when cursor is over the window.
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  generator.MoveMouseTo(200, 110);

  shell_surface->host_window()->AddPreTargetHandler(&event_handler);
  shell_surface->Activate();
  // Commit an arbitrary number of frames. We expect that this will not generate
  // synthetic events.
  for (int i = 0; i < 5; i++) {
    surface->Commit();
    test::WaitForLastFrameAck(shell_surface.get());
  }

  // There should be 2 events.  One for mouse enter and the other for move.
  const auto& events = event_handler.mouse_events();
  ASSERT_EQ(events.size(), 2UL);
  EXPECT_EQ(events[0].type(), ui::ET_MOUSE_ENTERED);
  EXPECT_EQ(events[1].type(), ui::ET_MOUSE_MOVED);

  shell_surface->host_window()->RemovePreTargetHandler(&event_handler);
}

TEST_P(ClientControlledShellSurfaceTest, CompositorLockInRotation) {
  UpdateDisplay("800x600");

  ash::Shell* shell = ash::Shell::Get();
  shell->tablet_mode_controller()->SetEnabledForTest(true);
  gfx::Rect maximum_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();

  // Start in maximized.
  auto shell_surface =
      exo::test::ShellSurfaceBuilder({800, 600})
          .SetWindowState(chromeos::WindowStateType::kMaximized)
          .SetGeometry(maximum_bounds)
          .SetNoCommit()
          .BuildClientControlledShellSurface();
  shell_surface->SetOrientation(Orientation::LANDSCAPE);
  auto* surface = shell_surface->root_surface();
  surface->Commit();

  ui::Compositor* compositor =
      shell_surface->GetWidget()->GetNativeWindow()->layer()->GetCompositor();

  EXPECT_FALSE(compositor->IsLocked());

  UpdateDisplay("800x600/r");

  EXPECT_TRUE(compositor->IsLocked());

  shell_surface->SetOrientation(Orientation::PORTRAIT);
  surface->Commit();

  test::WaitForLastFrameAck(shell_surface.get());

  EXPECT_FALSE(compositor->IsLocked());
}

// If system tray is shown by click. It should be activated if user presses tab
// key while shell surface is active.
TEST_P(ClientControlledShellSurfaceTest,
       KeyboardNavigationWithUnifiedSystemTray) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({800, 600})
                           .BuildClientControlledShellSurface();

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
  PressAndReleaseKey(ui::VKEY_TAB);

  // Confirm that system tray is activated.
  EXPECT_FALSE(shell_surface->GetWidget()->IsActive());
  EXPECT_TRUE(system_tray->IsBubbleActive());
}

TEST_P(ClientControlledShellSurfaceTest, Maximize) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
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

TEST_P(ClientControlledShellSurfaceTest, Restore) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
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

TEST_P(ClientControlledShellSurfaceTest, SetFullscreen) {
  auto shell_surface =
      exo::test::ShellSurfaceBuilder({256, 256})
          .SetWindowState(chromeos::WindowStateType::kFullscreen)
          .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
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
  EXPECT_NE(GetContext()->bounds().ToString(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().ToString());
}

TEST_P(ClientControlledShellSurfaceTest, ToggleFullscreen) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
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

TEST_P(ClientControlledShellSurfaceTest,
       DefaultDeviceScaleFactorForcedScaleFactor) {
  double scale = 1.5;
  display::Display::SetForceDeviceScaleFactor(scale);

  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::SetInternalDisplayIds({display_id});

  auto shell_surface = exo::test::ShellSurfaceBuilder({64, 64})
                           .EnableDefaultScaleCancellation()
                           .BuildClientControlledShellSurface();
  gfx::Transform transform;
  transform.Scale(1.0 / scale, 1.0 / scale);

  EXPECT_EQ(
      transform.ToString(),
      shell_surface->host_window()->layer()->GetTargetTransform().ToString());
}

TEST_P(ClientControlledShellSurfaceTest,
       DefaultDeviceScaleFactorFromDisplayManager) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::SetInternalDisplayIds({display_id});
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

  auto shell_surface = exo::test::ShellSurfaceBuilder({64, 64})
                           .BuildClientControlledShellSurface();

  gfx::Transform transform;
  transform.Scale(1.0 / scale, 1.0 / scale);

  EXPECT_EQ(
      transform.ToString(),
      shell_surface->host_window()->layer()->GetTargetTransform().ToString());
}

TEST_P(ClientControlledShellSurfaceTest, MouseAndTouchTarget) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .SetGeometry({0, 0, 256, 256})
                           .BuildClientControlledShellSurface();

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
TEST_P(ClientControlledShellSurfaceTest,
       ShellSurfaceInSystemModalIsUnresizable) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .SetUseSystemModalContainer()
                           .BuildClientControlledShellSurface();

  EXPECT_FALSE(shell_surface->GetWidget()->widget_delegate()->CanResize());
}

// The shell surface in SystemModal container should be a target
// at the edge.
TEST_P(ClientControlledShellSurfaceTest, ShellSurfaceInSystemModalHitTest) {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  auto shell_surface = exo::test::ShellSurfaceBuilder({640, 480})
                           .SetUseSystemModalContainer()
                           .SetGeometry(display.bounds())
                           .SetInputRegion(gfx::Rect(0, 0, 0, 0))
                           .BuildClientControlledShellSurface();
  EXPECT_FALSE(shell_surface->GetWidget()->widget_delegate()->CanResize());
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  aura::Window* root = window->GetRootWindow();

  ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(100, 0),
                       gfx::Point(100, 0), ui::EventTimeForNow(), 0, 0);
  aura::WindowTargeter targeter;
  aura::Window* found =
      static_cast<aura::Window*>(targeter.FindTargetForEvent(root, &event));
  EXPECT_TRUE(window->Contains(found));
}

// Test the snap functionalities in splitscreen in tablet mode.
TEST_P(ClientControlledShellSurfaceTest, SnapWindowInSplitViewModeTest) {
  UpdateDisplay("807x607");
  ash::Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  auto shell_surface1 =
      exo::test::ShellSurfaceBuilder({800, 600})
          .SetGeometry({0, 0, 800, 600})
          .SetWindowState(chromeos::WindowStateType::kMaximized)
          .BuildClientControlledShellSurface();
  aura::Window* window1 = shell_surface1->GetWidget()->GetNativeWindow();
  ash::WindowState* window_state1 = ash::WindowState::Get(window1);
  ash::ClientControlledState* state1 = static_cast<ash::ClientControlledState*>(
      ash::WindowState::TestApi::GetStateImpl(window_state1));
  EXPECT_EQ(window_state1->GetStateType(), WindowStateType::kMaximized);

  // Snap window to left.
  ash::SplitViewController* split_view_controller =
      ash::SplitViewController::Get(ash::Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(
      window1, ash::SplitViewController::SnapPosition::kPrimary);
  state1->set_bounds_locally(true);
  window1->SetBounds(split_view_controller->GetSnappedWindowBoundsInScreen(
      ash::SplitViewController::SnapPosition::kPrimary, window1));
  state1->set_bounds_locally(false);
  EXPECT_EQ(window_state1->GetStateType(), WindowStateType::kPrimarySnapped);
  EXPECT_EQ(shell_surface1->GetWidget()->GetWindowBoundsInScreen(),
            split_view_controller->GetSnappedWindowBoundsInScreen(
                ash::SplitViewController::SnapPosition::kPrimary,
                shell_surface1->GetWidget()->GetNativeWindow()));
  EXPECT_TRUE(HasBackdrop());
  split_view_controller->EndSplitView();

  // Snap window to right.
  split_view_controller->SnapWindow(
      window1, ash::SplitViewController::SnapPosition::kSecondary);
  state1->set_bounds_locally(true);
  window1->SetBounds(split_view_controller->GetSnappedWindowBoundsInScreen(
      ash::SplitViewController::SnapPosition::kSecondary, window1));
  state1->set_bounds_locally(false);
  EXPECT_EQ(window_state1->GetStateType(), WindowStateType::kSecondarySnapped);
  EXPECT_EQ(shell_surface1->GetWidget()->GetWindowBoundsInScreen(),
            split_view_controller->GetSnappedWindowBoundsInScreen(
                ash::SplitViewController::SnapPosition::kSecondary,
                shell_surface1->GetWidget()->GetNativeWindow()));
  EXPECT_TRUE(HasBackdrop());
}

// The shell surface in SystemModal container should not become target
// at the edge.
TEST_P(ClientControlledShellSurfaceTest, ClientIniatedResize) {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  auto shell_surface = exo::test::ShellSurfaceBuilder({100, 100})
                           .SetGeometry(gfx::Rect({0, 0, 100, 100}))
                           .BuildClientControlledShellSurface();
  EXPECT_TRUE(shell_surface->GetWidget()->widget_delegate()->CanResize());
  shell_surface->StartDrag(HTTOP, gfx::PointF(0, 0));

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  // Client cannot start drag if mouse isn't pressed.
  ash::WindowState* window_state = ash::WindowState::Get(window);
  ASSERT_FALSE(window_state->is_dragged());

  // Client can start drag only when the mouse is pressed on the widget.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window);
  event_generator->PressLeftButton();
  shell_surface->StartDrag(HTTOP, gfx::PointF(0, 0));
  ASSERT_TRUE(window_state->is_dragged());
  event_generator->ReleaseLeftButton();
  ASSERT_FALSE(window_state->is_dragged());

  // Press pressed outside of the window.
  event_generator->MoveMouseTo(gfx::Point(200, 50));
  event_generator->PressLeftButton();
  shell_surface->StartDrag(HTTOP, gfx::PointF(0, 0));
  ASSERT_FALSE(window_state->is_dragged());
}

TEST_P(ClientControlledShellSurfaceTest, ResizabilityAndSizeConstraints) {
  auto shell_surface = exo::test::ShellSurfaceBuilder()
                           .SetMinimumSize(gfx::Size(0, 0))
                           .SetMaximumSize(gfx::Size(0, 0))
                           .BuildClientControlledShellSurface();
  EXPECT_FALSE(shell_surface->GetWidget()->widget_delegate()->CanResize());

  shell_surface->SetMinimumSize(gfx::Size(400, 400));
  shell_surface->SetMaximumSize(gfx::Size(0, 0));
  auto* surface = shell_surface->root_surface();
  surface->Commit();
  EXPECT_TRUE(shell_surface->GetWidget()->widget_delegate()->CanResize());

  shell_surface->SetMinimumSize(gfx::Size(400, 400));
  shell_surface->SetMaximumSize(gfx::Size(400, 400));
  surface->Commit();
  EXPECT_FALSE(shell_surface->GetWidget()->widget_delegate()->CanResize());
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

  ShellSurfaceWindowObserver(const ShellSurfaceWindowObserver&) = delete;
  ShellSurfaceWindowObserver& operator=(const ShellSurfaceWindowObserver&) =
      delete;

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
  raw_ptr<aura::Window, ExperimentalAsh> window_;
  bool has_delegate_;
};

}  // namespace

// Test that when a shell surface is destroyed during its dragging, its window
// delegate should be reset properly.
TEST_P(ClientControlledShellSurfaceTest, CloseWindowWhenDraggingTest) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .SetGeometry({0, 0, 256, 256})
                           .BuildClientControlledShellSurface();

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

class TestClientControlledShellSurfaceDelegate
    : public test::ClientControlledShellSurfaceDelegate {
 public:
  explicit TestClientControlledShellSurfaceDelegate(
      ClientControlledShellSurface* shell_surface)
      : test::ClientControlledShellSurfaceDelegate(shell_surface) {}
  ~TestClientControlledShellSurfaceDelegate() override = default;
  TestClientControlledShellSurfaceDelegate(
      const TestClientControlledShellSurfaceDelegate&) = delete;
  TestClientControlledShellSurfaceDelegate& operator=(
      const TestClientControlledShellSurfaceDelegate&) = delete;

  int geometry_change_count() const { return geometry_change_count_; }
  std::vector<gfx::Rect> geometry_bounds() const { return geometry_bounds_; }
  int bounds_change_count() const { return bounds_change_count_; }
  std::vector<gfx::Rect> requested_bounds() const { return requested_bounds_; }
  std::vector<int64_t> requested_display_ids() const {
    return requested_display_ids_;
  }

  void Reset() {
    geometry_change_count_ = 0;
    geometry_bounds_.clear();
    bounds_change_count_ = 0;
    requested_bounds_.clear();
    requested_display_ids_.clear();
  }

  static TestClientControlledShellSurfaceDelegate* SetUp(
      ClientControlledShellSurface* shell_surface) {
    return static_cast<TestClientControlledShellSurfaceDelegate*>(
        shell_surface->set_delegate(
            std::make_unique<TestClientControlledShellSurfaceDelegate>(
                shell_surface)));
  }

 private:
  // ClientControlledShellSurface::Delegate:
  void OnGeometryChanged(const gfx::Rect& geometry) override {
    geometry_change_count_++;
    geometry_bounds_.push_back(geometry);
  }
  void OnBoundsChanged(chromeos::WindowStateType current_state,
                       chromeos::WindowStateType requested_state,
                       int64_t display_id,
                       const gfx::Rect& bounds_in_display,
                       bool is_resize,
                       int bounds_change) override {
    bounds_change_count_++;
    requested_bounds_.push_back(bounds_in_display);
    requested_display_ids_.push_back(display_id);
  }

  int geometry_change_count_ = 0;
  std::vector<gfx::Rect> geometry_bounds_;

  int bounds_change_count_ = 0;
  std::vector<gfx::Rect> requested_bounds_;
  std::vector<int64_t> requested_display_ids_;
};

class ClientControlledShellSurfaceDisplayTest : public test::ExoTestBase {
 public:
  ClientControlledShellSurfaceDisplayTest() = default;
  ~ClientControlledShellSurfaceDisplayTest() override = default;
  ClientControlledShellSurfaceDisplayTest(
      const ClientControlledShellSurfaceDisplayTest&) = delete;
  ClientControlledShellSurfaceDisplayTest& operator=(
      const ClientControlledShellSurfaceDisplayTest&) = delete;

  static ash::WindowResizer* CreateDragWindowResizer(
      aura::Window* window,
      const gfx::Point& point_in_parent,
      int window_component) {
    return ash::CreateWindowResizer(window, gfx::PointF(point_in_parent),
                                    window_component,
                                    ::wm::WINDOW_MOVE_SOURCE_MOUSE)
        .release();
  }

  gfx::PointF CalculateDragPoint(const ash::WindowResizer& resizer,
                                 int delta_x,
                                 int delta_y) {
    gfx::PointF location = resizer.GetInitialLocation();
    location.set_x(location.x() + delta_x);
    location.set_y(location.y() + delta_y);
    return location;
  }
};

}  // namespace

TEST_F(ClientControlledShellSurfaceDisplayTest, MoveToAnotherDisplayByDrag) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();

  auto shell_surface = exo::test::ShellSurfaceBuilder({200, 200})
                           .SetNoCommit()
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  gfx::Rect initial_bounds(-150, 10, 200, 200);
  shell_surface->SetBounds(primary_display.id(), initial_bounds);
  surface->Commit();

  EXPECT_EQ(initial_bounds,
            shell_surface->GetWidget()->GetWindowBoundsInScreen());

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  // Prevent snapping |window|. It only distracts from the purpose of the test.
  // TODO: Remove this code after adding functionality where the mouse has to
  // dwell in the snap region before the dragged window can get snapped.
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorNone);
  ASSERT_FALSE(ash::WindowState::Get(window)->CanSnap());

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

  auto* delegate =
      TestClientControlledShellSurfaceDelegate::SetUp(shell_surface.get());
  resizer->CompleteDrag();

  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  // TODO(oshima): We currently generate bounds change twice,
  // first when reparented, then set bounds. Chagne wm::SetBoundsInScreen
  // to simply request WM_EVENT_SET_BOUND with target display id.
  ASSERT_EQ(2, delegate->bounds_change_count());
  // Bounds is local to 2nd display.
  EXPECT_EQ(gfx::Rect(-150, 10, 200, 200), delegate->requested_bounds()[0]);
  EXPECT_EQ(gfx::Rect(-150, 10, 200, 200), delegate->requested_bounds()[1]);

  EXPECT_EQ(secondary_display.id(), delegate->requested_display_ids()[0]);
  EXPECT_EQ(secondary_display.id(), delegate->requested_display_ids()[1]);
}

TEST_F(ClientControlledShellSurfaceDisplayTest,
       MoveToAnotherDisplayByShortcut) {
  UpdateDisplay("400x600,800x600*2");
  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
  auto shell_surface = exo::test::ShellSurfaceBuilder({200, 200})
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();

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

  auto* delegate =
      TestClientControlledShellSurfaceDelegate::SetUp(shell_surface.get());

  display::Display secondary_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]);

  EXPECT_TRUE(
      ash::window_util::MoveWindowToDisplay(window, secondary_display.id()));

  ASSERT_EQ(1, delegate->bounds_change_count());
  // Should be scaled by 2x in pixels on 2x-density density.
  EXPECT_EQ(gfx::Rect(-348, 20, 400, 400), delegate->requested_bounds()[0]);
  EXPECT_EQ(secondary_display.id(), delegate->requested_display_ids()[0]);

  gfx::Rect secondary_position(700, 10, 200, 200);
  shell_surface->SetBounds(secondary_display.id(), secondary_position);
  surface->Commit();
  // Should be scaled by half when converted from pixels to DP.
  EXPECT_EQ(gfx::Rect(750, 5, 100, 100), window->GetBoundsInScreen());

  delegate->Reset();

  // Moving to the outside of another display.
  EXPECT_TRUE(
      ash::window_util::MoveWindowToDisplay(window, primary_display.id()));
  ASSERT_EQ(1, delegate->bounds_change_count());
  // Should fit in the primary display.
  EXPECT_EQ(gfx::Rect(350, 5, 100, 100), delegate->requested_bounds()[0]);
  EXPECT_EQ(primary_display.id(), delegate->requested_display_ids()[0]);
}

TEST_P(ClientControlledShellSurfaceTest, CaptionButtonModel) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({64, 64})
                           .SetGeometry(gfx::Rect(0, 0, 64, 64))
                           .BuildClientControlledShellSurface();

  constexpr views::CaptionButtonIcon kAllButtons[] = {
      views::CAPTION_BUTTON_ICON_MINIMIZE,
      views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
      views::CAPTION_BUTTON_ICON_CLOSE,
      views::CAPTION_BUTTON_ICON_BACK,
      views::CAPTION_BUTTON_ICON_MENU,
      views::CAPTION_BUTTON_ICON_FLOAT,
  };
  constexpr uint32_t kAllButtonMask =
      1 << views::CAPTION_BUTTON_ICON_MINIMIZE |
      1 << views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE |
      1 << views::CAPTION_BUTTON_ICON_CLOSE |
      1 << views::CAPTION_BUTTON_ICON_BACK |
      1 << views::CAPTION_BUTTON_ICON_MENU |
      1 << views::CAPTION_BUTTON_ICON_FLOAT;

  ash::NonClientFrameViewAsh* frame_view =
      static_cast<ash::NonClientFrameViewAsh*>(
          shell_surface->GetWidget()->non_client_view()->frame_view());
  chromeos::FrameCaptionButtonContainerView* container =
      static_cast<chromeos::HeaderView*>(frame_view->GetHeaderView())
          ->caption_button_container();

  // Visible
  for (auto visible : kAllButtons) {
    uint32_t visible_buttons = 1 << visible;
    shell_surface->SetFrameButtons(visible_buttons, 0);
    const chromeos::CaptionButtonModel* model = container->model();
    for (auto not_visible : kAllButtons) {
      if (not_visible == views::CAPTION_BUTTON_ICON_FLOAT) {
        // Float is dependent only on maximize/restore.
        EXPECT_EQ(
            !model->IsVisible(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE),
            model->IsVisible(views::CAPTION_BUTTON_ICON_FLOAT));
      } else if (not_visible != visible) {
        EXPECT_FALSE(model->IsVisible(not_visible));
      }
    }
    EXPECT_TRUE(model->IsVisible(visible));
    if (visible == views::CAPTION_BUTTON_ICON_FLOAT) {
      EXPECT_EQ(!model->IsEnabled(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE),
                model->IsEnabled(views::CAPTION_BUTTON_ICON_FLOAT));
    } else {
      EXPECT_FALSE(model->IsEnabled(visible));
    }
  }

  // Enable
  for (auto enabled : kAllButtons) {
    uint32_t enabled_buttons = 1 << enabled;
    shell_surface->SetFrameButtons(kAllButtonMask, enabled_buttons);
    const chromeos::CaptionButtonModel* model = container->model();
    for (auto not_enabled : kAllButtons) {
      if (not_enabled == views::CAPTION_BUTTON_ICON_FLOAT) {
        // Float is dependent only on maximize/restore.
        EXPECT_EQ(
            !model->IsEnabled(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE),
            model->IsEnabled(views::CAPTION_BUTTON_ICON_FLOAT));
      } else if (not_enabled != enabled) {
        EXPECT_FALSE(model->IsEnabled(not_enabled));
      }
    }
    EXPECT_TRUE(model->IsEnabled(enabled));
    if (enabled == views::CAPTION_BUTTON_ICON_FLOAT) {
      EXPECT_EQ(!model->IsVisible(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE),
                model->IsVisible(views::CAPTION_BUTTON_ICON_FLOAT));
    } else {
      EXPECT_TRUE(model->IsVisible(enabled));
    }
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
TEST_P(ClientControlledShellSurfaceTest, SetExtraTitle) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({640, 64})
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();

  const std::u16string window_title(u"title");
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
  shell_surface->SetExtraTitle(u"extra");
  surface->Commit();
  EXPECT_EQ(window_title, window->GetTitle());
  EXPECT_TRUE(paint_does_draw_text());
  EXPECT_FALSE(
      shell_surface->GetWidget()->widget_delegate()->ShouldShowWindowTitle());
}

TEST_P(ClientControlledShellSurfaceTest, WideFrame) {
  auto shell_surface =
      exo::test::ShellSurfaceBuilder({64, 64})
          .SetWindowState(chromeos::WindowStateType::kMaximized)
          .SetGeometry(gfx::Rect(100, 0, 64, 64))
          .SetInputRegion(gfx::Rect(0, 0, 64, 64))
          .SetFrame(SurfaceFrameType::NORMAL)
          .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  ash::Shelf* shelf = ash::Shelf::ForWindow(window);
  shelf->SetAlignment(ash::ShelfAlignment::kLeft);

  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  ASSERT_TRUE(work_area.x() != display_bounds.x());

  auto* wide_frame = shell_surface->wide_frame_for_test();
  ASSERT_TRUE(wide_frame);
  EXPECT_FALSE(wide_frame->header_view()->in_immersive_mode());
  EXPECT_EQ(work_area.x(), wide_frame->GetBoundsInScreen().x());
  EXPECT_EQ(work_area.width(), wide_frame->GetBoundsInScreen().width());

  auto another_window = ash::TestWidgetBuilder().BuildOwnsNativeWidget();
  another_window->SetFullscreen(true);

  // Make sure that the wide frame stays in maximzied size even if there is
  // active fullscreen window.
  EXPECT_EQ(work_area.x(), wide_frame->GetBoundsInScreen().x());
  EXPECT_EQ(work_area.width(), wide_frame->GetBoundsInScreen().width());

  shell_surface->Activate();

  EXPECT_EQ(work_area.x(), wide_frame->GetBoundsInScreen().x());
  EXPECT_EQ(work_area.width(), wide_frame->GetBoundsInScreen().width());

  // If the shelf is set to auto hide by a user, use the display bounds.
  ash::Shelf::ForWindow(window)->SetAutoHideBehavior(
      ash::ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(display_bounds.x(), wide_frame->GetBoundsInScreen().x());
  EXPECT_EQ(display_bounds.width(), wide_frame->GetBoundsInScreen().width());

  ash::Shelf::ForWindow(window)->SetAutoHideBehavior(
      ash::ShelfAutoHideBehavior::kNever);
  EXPECT_EQ(work_area.x(), wide_frame->GetBoundsInScreen().x());
  EXPECT_EQ(work_area.width(), wide_frame->GetBoundsInScreen().width());

  shell_surface->SetFullscreen(true);
  surface->Commit();
  EXPECT_EQ(display_bounds.x(), wide_frame->GetBoundsInScreen().x());
  EXPECT_EQ(display_bounds.width(), wide_frame->GetBoundsInScreen().width());
  EXPECT_EQ(display_bounds,
            display::Screen::GetScreen()->GetPrimaryDisplay().work_area());

  // Activating maximized window should not affect the fullscreen shell
  // surface's wide frame.
  another_window->Activate();
  another_window->SetFullscreen(false);
  EXPECT_EQ(work_area,
            display::Screen::GetScreen()->GetPrimaryDisplay().work_area());
  EXPECT_EQ(display_bounds.x(), wide_frame->GetBoundsInScreen().x());
  EXPECT_EQ(display_bounds.width(), wide_frame->GetBoundsInScreen().width());

  another_window->Close();

  // Check targeter is still CustomWindowTargeter.
  ASSERT_TRUE(window->parent());

  auto* custom_targeter = window->targeter();
  gfx::Point mouse_location(101, 50);

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

// Tests that a WideFrameView is created for an unparented ARC task and that the
TEST_P(ClientControlledShellSurfaceTest, NoFrameOnModalContainer) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({64, 64})
                           .SetUseSystemModalContainer()
                           .SetGeometry(gfx::Rect(100, 0, 64, 64))
                           .SetFrame(SurfaceFrameType::NORMAL)
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  EXPECT_FALSE(shell_surface->frame_enabled());
  surface->SetFrame(SurfaceFrameType::AUTOHIDE);
  surface->Commit();
  EXPECT_FALSE(shell_surface->frame_enabled());
}

TEST_P(ClientControlledShellSurfaceTest,
       SetGeometryReparentsToDisplayOnFirstCommit) {
  UpdateDisplay("100x200,100x200");
  const auto* screen = display::Screen::GetScreen();

  {
    gfx::Rect geometry(16, 16, 32, 32);
    auto shell_surface = exo::test::ShellSurfaceBuilder({64, 64})
                             .SetGeometry(geometry)
                             .BuildClientControlledShellSurface();
    EXPECT_EQ(geometry, shell_surface->GetWidget()->GetWindowBoundsInScreen());

    display::Display primary_display = screen->GetPrimaryDisplay();
    display::Display display = screen->GetDisplayNearestWindow(
        shell_surface->GetWidget()->GetNativeWindow());
    EXPECT_EQ(primary_display.id(), display.id());
  }

  {
    gfx::Rect geometry(116, 16, 32, 32);
    auto shell_surface = exo::test::ShellSurfaceBuilder({64, 64})
                             .SetGeometry(geometry)
                             .BuildClientControlledShellSurface();
    EXPECT_EQ(geometry, shell_surface->GetWidget()->GetWindowBoundsInScreen());

    auto root_windows = ash::Shell::GetAllRootWindows();
    display::Display secondary_display =
        screen->GetDisplayNearestWindow(root_windows[1]);
    display::Display display = screen->GetDisplayNearestWindow(
        shell_surface->GetWidget()->GetNativeWindow());
    EXPECT_EQ(secondary_display.id(), display.id());
  }
}

TEST_P(ClientControlledShellSurfaceTest, SetBoundsReparentsToDisplay) {
  UpdateDisplay("100x200,100x200");

  const auto* screen = display::Screen::GetScreen();
  gfx::Rect geometry(16, 16, 32, 32);
  auto shell_surface = exo::test::ShellSurfaceBuilder({64, 64})
                           .SetGeometry(geometry)
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();

  display::Display primary_display = screen->GetPrimaryDisplay();
  // Move to primary display with bounds inside display.
  shell_surface->SetBounds(primary_display.id(), geometry);
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

// Test if the surface bounds is correctly set when default scale cancellation
// is enabled or disabled.
TEST_P(ClientControlledShellSurfaceTest,
       SetBoundsWithAndWithoutDefaultScaleCancellation) {
  UpdateDisplay("800x600*2,800x600*2");

  const auto primary_display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  const auto secondary_display_id =
      display::Screen::GetScreen()->GetAllDisplays().back().id();

  const gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

  constexpr double kOriginalScale = 4.f;
  const gfx::Rect bounds_dp(64, 64, 128, 128);
  const gfx::Rect bounds_px_for_2x = gfx::ScaleToRoundedRect(bounds_dp, 2.f);
  const gfx::Rect bounds_px_for_4x =
      gfx::ScaleToRoundedRect(bounds_dp, kOriginalScale);

  for (const auto default_scale_cancellation : {true, false}) {
    SCOPED_TRACE(::testing::Message() << "default_scale_cancellation: "
                                      << default_scale_cancellation);
    {
      // Set display id, bounds origin, bounds size at the same time via
      // SetBounds method.
      auto builder = exo::test::ShellSurfaceBuilder();
      if (default_scale_cancellation) {
        builder.EnableDefaultScaleCancellation();
      }
      auto shell_surface =
          builder.SetNoCommit().BuildClientControlledShellSurface();
      auto* surface = shell_surface->root_surface();

      // When display doesn't change, scale stays the same
      shell_surface->SetScale(kOriginalScale);
      shell_surface->SetDisplay(primary_display_id);
      shell_surface->SetBounds(primary_display_id, default_scale_cancellation
                                                       ? bounds_dp
                                                       : bounds_px_for_4x);
      surface->Attach(buffer.get());
      surface->Commit();

      EXPECT_EQ(bounds_dp,
                shell_surface->GetWidget()->GetWindowBoundsInScreen());

      // When display changes, scale gets updated by the display dsf
      shell_surface->SetScale(kOriginalScale);
      shell_surface->SetBounds(secondary_display_id, default_scale_cancellation
                                                         ? bounds_dp
                                                         : bounds_px_for_2x);
      surface->Attach(buffer.get());
      surface->Commit();

      EXPECT_EQ(bounds_dp.width(),
                shell_surface->GetWidget()->GetWindowBoundsInScreen().width());
      EXPECT_EQ(bounds_dp.height(),
                shell_surface->GetWidget()->GetWindowBoundsInScreen().height());
    }
    {
      // Set display id and bounds origin at the same time via SetBoundsOrigin
      // method, and set bounds size separately.
      const auto bounds_to_set =
          default_scale_cancellation ? bounds_dp : bounds_px_for_4x;
      auto builder = exo::test::ShellSurfaceBuilder();
      if (default_scale_cancellation) {
        builder.EnableDefaultScaleCancellation();
      }
      auto shell_surface =
          builder.SetNoCommit().BuildClientControlledShellSurface();
      auto* surface = shell_surface->root_surface();

      shell_surface->SetScale(kOriginalScale);
      shell_surface->SetBoundsOrigin(primary_display_id,
                                     bounds_to_set.origin());
      shell_surface->SetBoundsSize(bounds_to_set.size());
      surface->Attach(buffer.get());
      surface->Commit();

      EXPECT_EQ(bounds_dp,
                shell_surface->GetWidget()->GetWindowBoundsInScreen());
    }
  }
}

// Set orientation lock to a window.
TEST_P(ClientControlledShellSurfaceTest, SetOrientationLock) {
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .SetFirstDisplayAsInternalDisplay();

  EnableTabletMode(true);
  ash::ScreenOrientationController* controller =
      ash::Shell::Get()->screen_orientation_controller();

  auto shell_surface =
      exo::test::ShellSurfaceBuilder({256, 256})
          .SetWindowState(chromeos::WindowStateType::kMaximized)
          .BuildClientControlledShellSurface();

  shell_surface->SetOrientationLock(
      chromeos::OrientationType::kLandscapePrimary);
  EXPECT_TRUE(controller->rotation_locked());
  display::Display display(display::Screen::GetScreen()->GetPrimaryDisplay());
  gfx::Size displaySize = display.size();
  EXPECT_GT(displaySize.width(), displaySize.height());

  shell_surface->SetOrientationLock(chromeos::OrientationType::kAny);
  EXPECT_FALSE(controller->rotation_locked());

  EnableTabletMode(false);
}

// Tests adjust bounds locally should also request remote client bounds update.
TEST_P(ClientControlledShellSurfaceTest, AdjustBoundsLocally) {
  UpdateDisplay("800x600");
  gfx::Rect client_bounds(900, 0, 200, 300);
  auto shell_surface = exo::test::ShellSurfaceBuilder({64, 64})
                           .SetGeometry(client_bounds)
                           .SetNoCommit()
                           .BuildClientControlledShellSurface();
  auto* delegate =
      TestClientControlledShellSurfaceDelegate::SetUp(shell_surface.get());
  auto* surface = shell_surface->root_surface();
  surface->Commit();

  views::Widget* widget = shell_surface->GetWidget();
  EXPECT_EQ(gfx::Rect(774, 0, 200, 300), widget->GetWindowBoundsInScreen());
  EXPECT_EQ(gfx::Rect(774, 0, 200, 300), delegate->requested_bounds().back());

  // Receiving the same bounds shouldn't try to update the bounds again.
  delegate->Reset();
  shell_surface->SetGeometry(client_bounds);
  surface->Commit();

  EXPECT_EQ(0, delegate->bounds_change_count());
}

TEST_P(ClientControlledShellSurfaceTest, SnappedInTabletMode) {
  gfx::Rect client_bounds(256, 256);
  auto shell_surface = exo::test::ShellSurfaceBuilder(client_bounds.size())
                           .SetGeometry(client_bounds)
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  auto* window = shell_surface->GetWidget()->GetNativeWindow();
  auto* window_state = ash::WindowState::Get(window);

  EnableTabletMode(true);

  ash::WMEvent event(ash::WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&event);
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kPrimarySnapped);

  ash::NonClientFrameViewAsh* frame_view =
      static_cast<ash::NonClientFrameViewAsh*>(
          shell_surface->GetWidget()->non_client_view()->frame_view());
  // Snapped window can also use auto hide.
  surface->SetFrame(SurfaceFrameType::AUTOHIDE);
  surface->Commit();
  EXPECT_TRUE(frame_view->GetFrameEnabled());
  EXPECT_TRUE(frame_view->GetHeaderView()->in_immersive_mode());
}

TEST_P(ClientControlledShellSurfaceTest, PipWindowCannotBeActivated) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();

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
  gfx::Rect client_bounds(100, 100);
  auto shell_surface = exo::test::ShellSurfaceBuilder(client_bounds.size())
                           .SetGeometry(client_bounds)
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();

  auto* delegate =
      TestClientControlledShellSurfaceDelegate::SetUp(shell_surface.get());
  ASSERT_EQ(0, delegate->bounds_change_count());
  auto* window_state =
      ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow());
  int64_t display_id = window_state->GetDisplay().id();

  shell_surface->OnBoundsChangeEvent(WindowStateType::kNormal,
                                     WindowStateType::kNormal, display_id,
                                     gfx::Rect(10, 10, 100, 100), 0);
  ASSERT_EQ(1, delegate->bounds_change_count());

  EXPECT_FALSE(shell_surface->GetWidget()->IsMinimized());

  shell_surface->SetMinimized();
  surface->Commit();

  EXPECT_TRUE(shell_surface->GetWidget()->IsMinimized());
  shell_surface->OnBoundsChangeEvent(WindowStateType::kMinimized,
                                     WindowStateType::kMinimized, display_id,
                                     gfx::Rect(0, 0, 100, 100), 0);
  ASSERT_EQ(1, delegate->bounds_change_count());

  // Send bounds change when exiting minmized.
  shell_surface->OnBoundsChangeEvent(WindowStateType::kMinimized,
                                     WindowStateType::kNormal, display_id,
                                     gfx::Rect(0, 0, 100, 100), 0);
  ASSERT_EQ(2, delegate->bounds_change_count());

  // Snapped, in clamshell mode.
  ash::NonClientFrameViewAsh* frame_view =
      static_cast<ash::NonClientFrameViewAsh*>(
          shell_surface->GetWidget()->non_client_view()->frame_view());
  surface->SetFrame(SurfaceFrameType::NORMAL);
  surface->Commit();
  shell_surface->OnBoundsChangeEvent(WindowStateType::kMinimized,
                                     WindowStateType::kSecondarySnapped,
                                     display_id, gfx::Rect(0, 0, 100, 100), 0);
  EXPECT_EQ(3, delegate->bounds_change_count());
  EXPECT_EQ(
      frame_view->GetClientBoundsForWindowBounds(gfx::Rect(0, 0, 100, 100)),
      delegate->requested_bounds().back());
  EXPECT_NE(gfx::Rect(0, 0, 100, 100), delegate->requested_bounds().back());

  // Snapped, in tablet mode.
  EnableTabletMode(true);
  shell_surface->OnBoundsChangeEvent(WindowStateType::kMinimized,
                                     WindowStateType::kSecondarySnapped,
                                     display_id, gfx::Rect(0, 0, 100, 100), 0);
  EXPECT_EQ(4, delegate->bounds_change_count());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), delegate->requested_bounds().back());
}

TEST_P(ClientControlledShellSurfaceTest, SetPipWindowBoundsAnimates) {
  gfx::Rect client_bounds(256, 256);
  auto shell_surface = exo::test::ShellSurfaceBuilder(client_bounds.size())
                           .SetWindowState(chromeos::WindowStateType::kPip)
                           .SetGeometry(client_bounds)
                           .BuildClientControlledShellSurface();
  shell_surface->GetWidget()->Show();
  auto* surface = shell_surface->root_surface();

  // Making an extra commit may set the next bounds change animation type
  // wrongly.
  surface->Commit();

  ui::ScopedAnimationDurationScaleMode animation_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(gfx::Rect(8, 8, 256, 256), window->layer()->GetTargetBounds());
  EXPECT_EQ(gfx::Rect(8, 8, 256, 256), window->layer()->bounds());
  window->SetBounds(gfx::Rect(10, 10, 256, 256));
  EXPECT_EQ(gfx::Rect(8, 10, 256, 256), window->layer()->GetTargetBounds());
  EXPECT_EQ(gfx::Rect(8, 8, 256, 256), window->layer()->bounds());
}

TEST_P(ClientControlledShellSurfaceTest, PipWindowDragDoesNotAnimate) {
  gfx::Rect client_bounds(256, 256);
  auto shell_surface = exo::test::ShellSurfaceBuilder(client_bounds.size())
                           .SetWindowState(chromeos::WindowStateType::kPip)
                           .SetGeometry(client_bounds)
                           .BuildClientControlledShellSurface();

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(gfx::Rect(8, 8, 256, 256), window->layer()->GetTargetBounds());
  EXPECT_EQ(gfx::Rect(8, 8, 256, 256), window->layer()->bounds());
  ui::ScopedAnimationDurationScaleMode animation_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  std::unique_ptr<ash::WindowResizer> resizer(ash::CreateWindowResizer(
      window, gfx::PointF(), HTCAPTION, ::wm::WINDOW_MOVE_SOURCE_MOUSE));
  resizer->Drag(gfx::PointF(10, 10), 0);
  EXPECT_EQ(gfx::Rect(18, 18, 256, 256), window->layer()->GetTargetBounds());
  EXPECT_EQ(gfx::Rect(18, 18, 256, 256), window->layer()->bounds());
  resizer->CompleteDrag();
}

TEST_P(ClientControlledShellSurfaceTest,
       PipWindowDragDoesNotAnimateWithExtraCommit) {
  gfx::Rect client_bounds(256, 256);
  auto shell_surface = exo::test::ShellSurfaceBuilder({client_bounds.size()})
                           .SetWindowState(chromeos::WindowStateType::kPip)
                           .SetGeometry(client_bounds)
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();

  // Making an extra commit may set the next bounds change animation type
  // wrongly.
  surface->Commit();

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(gfx::Rect(8, 8, 256, 256), window->layer()->GetTargetBounds());
  EXPECT_EQ(gfx::Rect(8, 8, 256, 256), window->layer()->bounds());
  ui::ScopedAnimationDurationScaleMode animation_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  std::unique_ptr<ash::WindowResizer> resizer(ash::CreateWindowResizer(
      window, gfx::PointF(), HTCAPTION, ::wm::WINDOW_MOVE_SOURCE_MOUSE));
  resizer->Drag(gfx::PointF(10, 10), 0);
  EXPECT_EQ(gfx::Rect(18, 18, 256, 256), window->layer()->GetTargetBounds());
  EXPECT_EQ(gfx::Rect(18, 18, 256, 256), window->layer()->bounds());
  EXPECT_FALSE(window->layer()->GetAnimator()->is_animating());
  resizer->CompleteDrag();
}

TEST_P(ClientControlledShellSurfaceTest,
       ExpandingPipInTabletModeEndsSplitView) {
  EnableTabletMode(true);

  ash::SplitViewController* split_view_controller =
      ash::SplitViewController::Get(ash::Shell::GetPrimaryRootWindow());
  EXPECT_FALSE(split_view_controller->InSplitViewMode());

  // Create a PIP window:
  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .SetWindowState(chromeos::WindowStateType::kPip)
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  auto window_left = CreateTestWindow();
  auto window_right = CreateTestWindow();

  split_view_controller->SnapWindow(
      window_left.get(), ash::SplitViewController::SnapPosition::kPrimary);
  split_view_controller->SnapWindow(
      window_right.get(), ash::SplitViewController::SnapPosition::kSecondary);
  EXPECT_TRUE(split_view_controller->InSplitViewMode());

  // Should end split view.
  shell_surface->SetRestored();
  surface->Commit();
  EXPECT_FALSE(split_view_controller->InSplitViewMode());
}

TEST_P(ClientControlledShellSurfaceTest,
       DismissingPipInTabletModeDoesNotEndSplitView) {
  EnableTabletMode(true);

  ash::SplitViewController* split_view_controller =
      ash::SplitViewController::Get(ash::Shell::GetPrimaryRootWindow());
  EXPECT_FALSE(split_view_controller->InSplitViewMode());

  // Create a PIP window:
  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .SetWindowState(chromeos::WindowStateType::kPip)
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  auto window_left = CreateTestWindow();
  auto window_right = CreateTestWindow();

  split_view_controller->SnapWindow(
      window_left.get(), ash::SplitViewController::SnapPosition::kPrimary);
  split_view_controller->SnapWindow(
      window_right.get(), ash::SplitViewController::SnapPosition::kSecondary);
  EXPECT_TRUE(split_view_controller->InSplitViewMode());

  // Should not end split-view.
  shell_surface->SetMinimized();
  surface->Commit();
  EXPECT_TRUE(split_view_controller->InSplitViewMode());
}

class StateChangeCounterDelegate
    : public test::ClientControlledShellSurfaceDelegate {
 public:
  explicit StateChangeCounterDelegate(
      ClientControlledShellSurface* shell_surface,
      int expected_state_change_count)
      : test::ClientControlledShellSurfaceDelegate(shell_surface),
        expected_state_change_count_(expected_state_change_count) {}
  ~StateChangeCounterDelegate() override {
    EXPECT_EQ(0, expected_state_change_count_);
  }
  StateChangeCounterDelegate(const StateChangeCounterDelegate&) = delete;
  StateChangeCounterDelegate& operator=(const StateChangeCounterDelegate&) =
      delete;

 private:
  int expected_state_change_count_ = 0;

  // ClientControlledShellSurface::Delegate:
  void OnStateChanged(chromeos::WindowStateType old_state_type,
                      chromeos::WindowStateType new_state_type) override {
    ClientControlledShellSurfaceDelegate::OnStateChanged(old_state_type,
                                                         new_state_type);
    expected_state_change_count_--;
  }
};

TEST_P(ClientControlledShellSurfaceTest, DoNotReplayWindowStateRequest) {
  auto shell_surface =
      exo::test::ShellSurfaceBuilder({64, 64})
          .SetWindowState(chromeos::WindowStateType::kMinimized)
          .SetNoCommit()
          .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  shell_surface->set_delegate(
      std::make_unique<StateChangeCounterDelegate>(shell_surface.get(), 0));
  surface->Commit();
}

TEST_P(ClientControlledShellSurfaceTest, UnPinTriggersStateChangeRequest) {
  // Only test in tablet mode. Because after restore from pin state, in tablet
  // mode the window will still be fullscreen.
  EnableTabletMode(true);
  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .BuildClientControlledShellSurface();

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  ash::WindowState* window_state = ash::WindowState::Get(window);

  ash::WMEvent pin_event(ash::WM_EVENT_PIN);
  ash::WindowState::Get(window)->OnWMEvent(&pin_event);
  EXPECT_TRUE(window_state->IsPinned());

  // Verify maximized->Pinned->Maximized triggers an unpin request to clients.
  shell_surface->set_delegate(
      std::make_unique<StateChangeCounterDelegate>(shell_surface.get(), 1));
  ash::WMEvent restore_event(ash::WM_EVENT_RESTORE);
  ash::WindowState::Get(window)->OnWMEvent(&restore_event);
  EXPECT_FALSE(window_state->IsPinned());
}

TEST_F(ClientControlledShellSurfaceDisplayTest,
       RequestBoundsChangeOnceWithStateTransition) {
  constexpr gfx::Size buffer_size(64, 64);
  constexpr gfx::Rect original_bounds({20, 20}, buffer_size);
  auto shell_surface = exo::test::ShellSurfaceBuilder(buffer_size)
                           .SetWindowState(chromeos::WindowStateType::kNormal)
                           .SetGeometry(original_bounds)
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();

  auto* delegate =
      TestClientControlledShellSurfaceDelegate::SetUp(shell_surface.get());
  shell_surface->SetPip();
  surface->Commit();

  ASSERT_EQ(1, delegate->bounds_change_count());
}

TEST_P(ClientControlledShellSurfaceTest,
       DoNotSavePipBoundsAcrossMultiplePipTransition) {
  // Create a PIP window:
  constexpr gfx::Size buffer_size(100, 100);
  constexpr gfx::Rect original_bounds({8, 10}, buffer_size);
  auto shell_surface = exo::test::ShellSurfaceBuilder(buffer_size)
                           .SetWindowState(chromeos::WindowStateType::kPip)
                           .SetGeometry(original_bounds)
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(gfx::Rect(8, 10, 100, 100), window->bounds());

  const gfx::Rect moved_bounds(gfx::Point(8, 20), buffer_size);
  shell_surface->SetGeometry(moved_bounds);
  surface->Commit();
  EXPECT_EQ(gfx::Rect(8, 20, 100, 100), window->bounds());

  shell_surface->SetRestored();
  surface->Commit();
  shell_surface->SetGeometry(original_bounds);
  shell_surface->SetPip();
  surface->Commit();
  EXPECT_EQ(gfx::Rect(8, 10, 100, 100), window->bounds());

  shell_surface->SetGeometry(moved_bounds);
  surface->Commit();
  EXPECT_EQ(gfx::Rect(8, 20, 100, 100), window->bounds());

  shell_surface->SetRestored();
  surface->Commit();
  shell_surface->SetGeometry(moved_bounds);
  shell_surface->SetPip();
  surface->Commit();
  EXPECT_EQ(gfx::Rect(8, 20, 100, 100), window->bounds());
}

TEST_P(ClientControlledShellSurfaceTest,
       DoNotApplyCollisionDetectionWhileDragged) {
  constexpr gfx::Size buffer_size(256, 256);
  constexpr gfx::Rect original_bounds({8, 50}, buffer_size);
  auto shell_surface = exo::test::ShellSurfaceBuilder(buffer_size)
                           .SetWindowState(chromeos::WindowStateType::kPip)
                           .SetGeometry(original_bounds)
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  ash::WindowState* window_state = ash::WindowState::Get(window);
  EXPECT_EQ(gfx::Rect(8, 50, 256, 256), window->bounds());

  // Ensure that the collision detection logic is not applied during drag move.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window);
  event_generator->PressLeftButton();
  shell_surface->StartDrag(HTTOP, gfx::PointF(0, 0));
  ASSERT_TRUE(window_state->is_dragged());
  shell_surface->SetGeometry(gfx::Rect(gfx::Point(20, 50), buffer_size));
  surface->Commit();
  EXPECT_EQ(gfx::Rect(20, 50, 256, 256), window->bounds());
}

TEST_P(ClientControlledShellSurfaceTest, EnteringPipSavesPipSnapFraction) {
  constexpr gfx::Size buffer_size(100, 100);
  constexpr gfx::Rect original_bounds({8, 50}, buffer_size);
  auto shell_surface = exo::test::ShellSurfaceBuilder(buffer_size)
                           .SetWindowState(chromeos::WindowStateType::kPip)
                           .SetGeometry(original_bounds)
                           .BuildClientControlledShellSurface();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  ash::WindowState* window_state = ash::WindowState::Get(window);
  EXPECT_EQ(gfx::Rect(8, 50, 100, 100), window->bounds());

  // Ensure the correct value is saved to pip snap fraction.
  EXPECT_TRUE(ash::PipPositioner::HasSnapFraction(window_state));
  EXPECT_EQ(ash::PipPositioner::GetSnapFractionAppliedBounds(window_state),
            window->bounds());
}

TEST_P(ClientControlledShellSurfaceTest,
       ShadowBoundsChangedIsResetAfterCommit) {
  auto shell_surface =
      exo::test::ShellSurfaceBuilder().BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  surface->SetFrame(SurfaceFrameType::SHADOW);
  shell_surface->SetShadowBounds(gfx::Rect(10, 10, 100, 100));
  EXPECT_TRUE(shell_surface->get_shadow_bounds_changed_for_testing());
  surface->Commit();
  EXPECT_FALSE(shell_surface->get_shadow_bounds_changed_for_testing());
}

namespace {

class ClientControlledShellSurfaceScaleTest : public test::ExoTestBase {
 public:
  ClientControlledShellSurfaceScaleTest() = default;
  ~ClientControlledShellSurfaceScaleTest() override = default;

 private:
  ClientControlledShellSurfaceScaleTest(
      const ClientControlledShellSurfaceScaleTest&) = delete;
  ClientControlledShellSurfaceScaleTest& operator=(
      const ClientControlledShellSurfaceScaleTest&) = delete;
};

}  // namespace

TEST_F(ClientControlledShellSurfaceScaleTest, ScaleSetOnInitialCommit) {
  UpdateDisplay("1200x800*2.0");

  auto shell_surface = exo::test::ShellSurfaceBuilder({20, 20})
                           .SetNoCommit()
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();

  auto* delegate =
      TestClientControlledShellSurfaceDelegate::SetUp(shell_surface.get());
  surface->Commit();

  EXPECT_EQ(2.f, 1.f / shell_surface->GetClientToDpScale());
  EXPECT_EQ(0, delegate->bounds_change_count());
  EXPECT_EQ(1, delegate->geometry_change_count());
}

TEST_F(ClientControlledShellSurfaceScaleTest,
       DeferScaleCommitForRestoredWindow) {
  UpdateDisplay("1200x800*2.0");

  gfx::Rect initial_native_bounds(100, 100, 100, 100);
  auto shell_surface = exo::test::ShellSurfaceBuilder({20, 20})
                           .SetWindowState(chromeos::WindowStateType::kNormal)
                           .SetNoCommit()
                           .BuildClientControlledShellSurface();
  auto* delegate =
      TestClientControlledShellSurfaceDelegate::SetUp(shell_surface.get());

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  shell_surface->SetBounds(primary_display.id(), initial_native_bounds);
  auto* surface = shell_surface->root_surface();
  surface->Commit();

  EXPECT_EQ(2.f, 1.f / shell_surface->GetClientToDpScale());
  EXPECT_EQ(0, delegate->bounds_change_count());
  EXPECT_EQ(1, delegate->geometry_change_count());
  EXPECT_EQ(gfx::ScaleToRoundedRect(initial_native_bounds,
                                    shell_surface->GetClientToDpScale()),
            delegate->geometry_bounds()[0]);

  UpdateDisplay("2400x1600*1.0");

  // The surface's scale should not be committed until the buffer size changes.
  EXPECT_EQ(2.f, 1.f / shell_surface->GetClientToDpScale());
  EXPECT_EQ(0, delegate->bounds_change_count());

  const gfx::Size new_buffer_size(10, 10);
  std::unique_ptr<Buffer> new_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(new_buffer_size)));
  surface->Attach(new_buffer.get());
  surface->Commit();

  EXPECT_EQ(1.f, shell_surface->GetClientToDpScale());
  EXPECT_EQ(0, delegate->bounds_change_count());
}

TEST_F(ClientControlledShellSurfaceScaleTest,
       CommitScaleChangeImmediatelyForMaximizedWindow) {
  UpdateDisplay("1200x800*2.0");

  gfx::Rect initial_native_bounds(100, 100, 100, 100);
  auto shell_surface =
      exo::test::ShellSurfaceBuilder({20, 20})
          .SetWindowState(chromeos::WindowStateType::kMaximized)
          .SetNoCommit()
          .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  auto* delegate =
      TestClientControlledShellSurfaceDelegate::SetUp(shell_surface.get());

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  shell_surface->SetBounds(primary_display.id(), initial_native_bounds);
  surface->Commit();

  EXPECT_EQ(2.f, 1.f / shell_surface->GetClientToDpScale());
  EXPECT_EQ(1, delegate->geometry_change_count());
  EXPECT_EQ(gfx::ScaleToRoundedRect(initial_native_bounds,
                                    shell_surface->GetClientToDpScale()),
            delegate->geometry_bounds()[0]);

  UpdateDisplay("2400x1600*1.0");

  EXPECT_EQ(1.f, shell_surface->GetClientToDpScale());
  EXPECT_EQ(0, delegate->bounds_change_count());
}

TEST_F(ClientControlledShellSurfaceScaleTest,
       CommitScaleChangeImmediatelyInTabletMode) {
  EnableTabletMode(true);
  UpdateDisplay("1200x800*2.0");

  gfx::Rect initial_native_bounds(100, 100, 100, 100);
  auto shell_surface =
      exo::test::ShellSurfaceBuilder({20, 20})
          .SetWindowState(chromeos::WindowStateType::kSecondarySnapped)
          .SetNoCommit()
          .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  auto* delegate =
      TestClientControlledShellSurfaceDelegate::SetUp(shell_surface.get());
  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  shell_surface->SetBounds(primary_display.id(), initial_native_bounds);
  surface->Commit();

  EXPECT_EQ(2.f, 1.f / shell_surface->GetClientToDpScale());
  EXPECT_EQ(1, delegate->geometry_change_count());
  EXPECT_EQ(gfx::ScaleToRoundedRect(initial_native_bounds,
                                    shell_surface->GetClientToDpScale()),
            delegate->geometry_bounds()[0]);

  // A bounds change is requested because the window is snapped.
  EXPECT_EQ(1, delegate->bounds_change_count());

  delegate->Reset();
  UpdateDisplay("2400x1600*1.0");

  EXPECT_EQ(1.f, shell_surface->GetClientToDpScale());

  // Updating the scale in tablet mode should request a bounds change, because
  // the window is snapped. Changing the scale will change its bounds in DP,
  // even if the position of the window as visible to the user does not change.
  EXPECT_GE(delegate->bounds_change_count(), 1);
}

TEST_P(ClientControlledShellSurfaceTest, SnappedClientBounds) {
  UpdateDisplay("800x600");

  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .SetNoCommit()
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();

  // Clear insets so that it won't affects the bounds.
  shell_surface->SetSystemUiVisibility(true);
  aura::Window* root = ash::Shell::GetPrimaryRootWindow();
  ash::WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
      root, gfx::Rect(), gfx::Insets(), gfx::Insets());

  auto* delegate =
      TestClientControlledShellSurfaceDelegate::SetUp(shell_surface.get());
  surface->Commit();
  views::Widget* widget = shell_surface->GetWidget();
  aura::Window* window = widget->GetNativeWindow();

  // Normal state -> Snap.
  shell_surface->SetGeometry(gfx::Rect(50, 100, 200, 300));
  surface->SetFrame(SurfaceFrameType::NORMAL);
  surface->Commit();
  EXPECT_EQ(gfx::Rect(50, 68, 200, 332), widget->GetWindowBoundsInScreen());

  ash::WMEvent event(ash::WM_EVENT_SNAP_PRIMARY);
  ash::WindowState::Get(window)->OnWMEvent(&event);
  EXPECT_EQ(gfx::Rect(0, 32, 400, 568), delegate->requested_bounds().back());

  // Maximized -> Snap.
  shell_surface->SetMaximized();
  shell_surface->SetGeometry(gfx::Rect(0, 0, 800, 568));
  surface->Commit();
  EXPECT_TRUE(widget->IsMaximized());

  ash::WindowState::Get(window)->OnWMEvent(&event);
  EXPECT_EQ(gfx::Rect(0, 32, 400, 568), delegate->requested_bounds().back());
  shell_surface->SetSnapPrimary(chromeos::kDefaultSnapRatio);
  shell_surface->SetGeometry(gfx::Rect(0, 0, 400, 568));
  surface->Commit();

  // Clamshell mode -> tablet mode. The bounds start from top-left corner.
  EnableTabletMode(true);
  EXPECT_EQ(gfx::Rect(0, 0, 396, 564), delegate->requested_bounds().back());
  shell_surface->SetGeometry(gfx::Rect(0, 0, 396, 568));
  surface->SetFrame(SurfaceFrameType::AUTOHIDE);
  surface->Commit();

  // Tablet mode -> clamshell mode. Top caption height should be reserved.
  EnableTabletMode(false);
  EXPECT_EQ(gfx::Rect(0, 32, 400, 568), delegate->requested_bounds().back());

  // Clean up state.
  shell_surface->SetSnapPrimary(chromeos::kDefaultSnapRatio);
  surface->Commit();
}

// The shell surface with resize lock on should be unresizable.
TEST_P(ClientControlledShellSurfaceTest,
       ShellSurfaceWithResizeLockOnIsUnresizable) {
  base::test::ScopedFeatureList scoped_feature_list(
      chromeos::wm::features::kWindowLayoutMenu);

  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();

  EXPECT_TRUE(shell_surface->CanResize());

  shell_surface->SetResizeLockType(
      ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);
  surface->Commit();
  EXPECT_FALSE(shell_surface->CanResize());

  // Test that the float caption button is visible on unresizable apps.
  EXPECT_TRUE(chromeos::wm::CanFloatWindow(
      shell_surface->GetWidget()->GetNativeWindow()));
  ash::NonClientFrameViewAsh* frame_view =
      static_cast<ash::NonClientFrameViewAsh*>(
          shell_surface->GetWidget()->non_client_view()->frame_view());
  const chromeos::CaptionButtonModel* model =
      static_cast<chromeos::HeaderView*>(frame_view->GetHeaderView())
          ->caption_button_container()
          ->model();
  EXPECT_TRUE(model->IsVisible(views::CAPTION_BUTTON_ICON_FLOAT));
  EXPECT_TRUE(model->IsEnabled(views::CAPTION_BUTTON_ICON_FLOAT));

  shell_surface->SetResizeLockType(
      ash::ArcResizeLockType::RESIZE_ENABLED_TOGGLABLE);
  surface->Commit();
  EXPECT_TRUE(shell_surface->CanResize());
}

TEST_P(ClientControlledShellSurfaceTest, OverlayShadowBounds) {
  gfx::Rect initial_bounds(150, 10, 200, 200);
  auto shell_surface = exo::test::ShellSurfaceBuilder({1, 1})
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  shell_surface->SetBounds(primary_display.id(), initial_bounds);
  shell_surface->OnSetFrame(SurfaceFrameType::NORMAL);
  surface->Commit();

  EXPECT_FALSE(shell_surface->HasOverlay());

  ShellSurfaceBase::OverlayParams params(std::make_unique<views::View>());
  params.overlaps_frame = false;
  shell_surface->AddOverlay(std::move(params));
  EXPECT_TRUE(shell_surface->HasOverlay());

  {
    gfx::Size overlay_size =
        shell_surface->GetWidget()->GetWindowBoundsInScreen().size();
    gfx::Size shadow_size = shell_surface->GetShadowBounds().size();
    EXPECT_EQ(shadow_size, overlay_size);
  }
}

// WideFrameView follows its respective surface when it is eventually parented.
// See crbug.com/1223135.
TEST_P(ClientControlledShellSurfaceTest, WideframeForUnparentedTasks) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({64, 64})
                           .SetGeometry(gfx::Rect(100, 0, 64, 64))
                           .SetInputRegion(gfx::Rect(0, 0, 64, 64))
                           .SetFrame(SurfaceFrameType::NORMAL)
                           .BuildClientControlledShellSurface();
  auto* surface = shell_surface->root_surface();
  auto* wide_frame = shell_surface->wide_frame_for_test();
  ASSERT_FALSE(wide_frame);

  // Set the |app_restore::kParentToHiddenContainerKey| for the surface and
  // reparent it, simulating the Full Restore process for an unparented ARC
  // task.
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  window->SetProperty(app_restore::kParentToHiddenContainerKey, true);
  aura::client::ParentWindowWithContext(window,
                                        /*context=*/window->GetRootWindow(),
                                        window->GetBoundsInScreen());

  // Maximize the surface. The WideFrameView should be created and a crash
  // should not occur.
  shell_surface->SetMaximized();
  surface->Commit();
  const auto* hidden_container_parent = window->parent();
  wide_frame = shell_surface->wide_frame_for_test();
  EXPECT_TRUE(wide_frame);
  EXPECT_EQ(hidden_container_parent,
            wide_frame->GetWidget()->GetNativeWindow()->parent());

  // Call the WindowRestoreController, simulating the ARC task becoming ready.
  // The surface should be reparented and the WideFrameView should follow it.
  ash::WindowRestoreController::Get()->OnParentWindowToValidContainer(window);
  EXPECT_NE(hidden_container_parent, window->parent());
  wide_frame = shell_surface->wide_frame_for_test();
  EXPECT_TRUE(wide_frame);
  EXPECT_EQ(window->parent(),
            wide_frame->GetWidget()->GetNativeWindow()->parent());
}

TEST_P(ClientControlledShellSurfaceTest,
       InitializeWindowStateGrantsPermissionToActivate) {
  auto shell_surface =
      exo::test::ShellSurfaceBuilder().BuildClientControlledShellSurface();

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  auto* permission = window->GetProperty(kPermissionKey);

  EXPECT_TRUE(permission->Check(Permission::Capability::kActivate));
}

TEST_P(ClientControlledShellSurfaceTest, SupportsFloatedState) {
  base::test::ScopedFeatureList scoped_feature_list(
      chromeos::wm::features::kWindowLayoutMenu);

  // Test disabling support.
  {
    auto shell_surface = exo::test::ShellSurfaceBuilder()
                             .DisableSupportsFloatedState()
                             .BuildClientControlledShellSurface();
    auto* const window = shell_surface->GetWidget()->GetNativeWindow();
    EXPECT_FALSE(chromeos::wm::CanFloatWindow(window));
  }
  // Test enabling (default) support.
  {
    auto shell_surface =
        exo::test::ShellSurfaceBuilder().BuildClientControlledShellSurface();
    auto* const window = shell_surface->GetWidget()->GetNativeWindow();
    EXPECT_TRUE(chromeos::wm::CanFloatWindow(window));
  }
}

}  // namespace exo
