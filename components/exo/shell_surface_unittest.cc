// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface.h"

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/buffer.h"
#include "components/exo/permission.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/wm_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

namespace exo {

using ShellSurfaceTest = test::ExoTestBase;

bool HasBackdrop() {
  ash::WorkspaceController* wc = ash::ShellTestApi().workspace_controller();
  return !!ash::WorkspaceControllerTestApi(wc).GetBackdropWindow();
}

uint32_t ConfigureFullscreen(uint32_t serial,
                             const gfx::Size& size,
                             ash::WindowStateType state_type,
                             bool resizing,
                             bool activated,
                             const gfx::Vector2d& origin_offset) {
  EXPECT_EQ(ash::WindowStateType::kFullscreen, state_type);
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

TEST_F(ShellSurfaceTest, AcknowledgeConfigure) {
  gfx::Size buffer_size(32, 32);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();

  gfx::Point origin(100, 100);
  shell_surface->GetWidget()->SetBounds(gfx::Rect(origin, buffer_size));
  EXPECT_EQ(origin.ToString(),
            surface->window()->GetBoundsInRootWindow().origin().ToString());

  const uint32_t kSerial = 1;
  shell_surface->set_configure_callback(
      base::BindRepeating(&ConfigureFullscreen, kSerial));
  shell_surface->SetFullscreen(true);

  // Surface origin should not change until configure request is acknowledged.
  EXPECT_EQ(origin.ToString(),
            surface->window()->GetBoundsInRootWindow().origin().ToString());

  // Compositor should be locked until configure request is acknowledged.
  ui::Compositor* compositor =
      shell_surface->GetWidget()->GetNativeWindow()->layer()->GetCompositor();
  EXPECT_TRUE(compositor->IsLocked());

  shell_surface->AcknowledgeConfigure(kSerial);
  std::unique_ptr<Buffer> fullscreen_buffer(new Buffer(
      exo_test_helper()->CreateGpuMemoryBuffer(GetContext()->bounds().size())));
  surface->Attach(fullscreen_buffer.get());
  surface->Commit();

  EXPECT_EQ(gfx::Point().ToString(),
            surface->window()->GetBoundsInRootWindow().origin().ToString());
  EXPECT_FALSE(compositor->IsLocked());
}

TEST_F(ShellSurfaceTest, SetParent) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> parent_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> parent_surface(new Surface);
  std::unique_ptr<ShellSurface> parent_shell_surface(
      new ShellSurface(parent_surface.get()));

  parent_surface->Attach(parent_buffer.get());
  parent_surface->Commit();

  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  shell_surface->SetParent(parent_shell_surface.get());

  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_EQ(
      parent_shell_surface->GetWidget()->GetNativeWindow(),
      wm::GetTransientParent(shell_surface->GetWidget()->GetNativeWindow()));

  // Use OnSetParent to move shell surface to 10, 10.
  gfx::Point parent_origin =
      parent_shell_surface->GetWidget()->GetWindowBoundsInScreen().origin();
  shell_surface->OnSetParent(
      parent_surface.get(),
      gfx::PointAtOffsetFromOrigin(gfx::Point(10, 10) - parent_origin));
  EXPECT_EQ(gfx::Rect(10, 10, 256, 256),
            shell_surface->GetWidget()->GetWindowBoundsInScreen());
  EXPECT_FALSE(shell_surface->CanActivate());
}

TEST_F(ShellSurfaceTest, DeleteShellSurfaceWithTransientChildren) {
  gfx::Size buffer_size(256, 256);
  auto parent_info = CreateShellSurfaceHolder(buffer_size, nullptr);
  auto child1_info =
      CreateShellSurfaceHolder(buffer_size, parent_info->shell_surface());
  auto child2_info =
      CreateShellSurfaceHolder(buffer_size, parent_info->shell_surface());
  auto child3_info =
      CreateShellSurfaceHolder(buffer_size, parent_info->shell_surface());
  EXPECT_EQ(parent_info->shell_surface()->GetWidget()->GetNativeWindow(),
            wm::GetTransientParent(
                child1_info->shell_surface()->GetWidget()->GetNativeWindow()));
  EXPECT_EQ(parent_info->shell_surface()->GetWidget()->GetNativeWindow(),
            wm::GetTransientParent(
                child2_info->shell_surface()->GetWidget()->GetNativeWindow()));
  EXPECT_EQ(parent_info->shell_surface()->GetWidget()->GetNativeWindow(),
            wm::GetTransientParent(
                child3_info->shell_surface()->GetWidget()->GetNativeWindow()));

  parent_info.reset();
}

TEST_F(ShellSurfaceTest, Maximize) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();
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
  gfx::Size buffer_size(400, 300);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();

  // Make sure we've created a resizable window.
  EXPECT_TRUE(shell_surface->CanResize());

  // Assert: Resizable windows can be maximized.
  EXPECT_TRUE(shell_surface->CanMaximize());
}

TEST_F(ShellSurfaceTest, CannotMaximizeNonResizableWindow) {
  gfx::Size buffer_size(400, 300);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  shell_surface->SetMinimumSize(buffer_size);
  shell_surface->SetMaximumSize(buffer_size);
  surface->Commit();

  // Make sure we've created a non-resizable window.
  EXPECT_FALSE(shell_surface->CanResize());

  // Assert: Non-resizable windows cannot be maximized.
  EXPECT_FALSE(shell_surface->CanMaximize());
}

TEST_F(ShellSurfaceTest, Minimize) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  EXPECT_TRUE(shell_surface->CanMinimize());

  // Minimizing can be performed before the surface is committed, but
  // widget creation will be deferred.
  shell_surface->Minimize();
  EXPECT_FALSE(shell_surface->GetWidget());

  // Attaching the buffer will create a widget with minimized state.
  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_TRUE(shell_surface->GetWidget()->IsMinimized());

  shell_surface->Restore();
  EXPECT_FALSE(shell_surface->GetWidget()->IsMinimized());

  std::unique_ptr<Surface> child_surface(new Surface);
  std::unique_ptr<ShellSurface> child_shell_surface(
      new ShellSurface(child_surface.get()));

  // Transient shell surfaces cannot be minimized.
  child_surface->SetParent(surface.get(), gfx::Point());
  child_surface->Attach(buffer.get());
  child_surface->Commit();
  EXPECT_FALSE(child_shell_surface->CanMinimize());
}

TEST_F(ShellSurfaceTest, Restore) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
  // Note: Remove contents to avoid issues with maximize animations in tests.
  shell_surface->Maximize();
  EXPECT_FALSE(HasBackdrop());
  shell_surface->Restore();
  EXPECT_FALSE(HasBackdrop());
  EXPECT_EQ(
      buffer_size.ToString(),
      shell_surface->GetWidget()->GetWindowBoundsInScreen().size().ToString());
}

TEST_F(ShellSurfaceTest, HostWindowBoundsUpdatedAfterCommitWidget) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  shell_surface->SurfaceTreeHost::OnSurfaceCommit();
  shell_surface->root_surface()->SetSurfaceHierarchyContentBoundsForTest(
      gfx::Rect(0, 0, 50, 50));

  // Host Window Bounds size before committing.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), shell_surface->host_window()->bounds());
  EXPECT_TRUE(shell_surface->OnPreWidgetCommit());
  shell_surface->CommitWidget();
  // CommitWidget should update the Host Window Bounds.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), shell_surface->host_window()->bounds());
}

TEST_F(ShellSurfaceTest, SetFullscreen) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->SetFullscreen(true);
  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
  EXPECT_EQ(GetContext()->bounds().ToString(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().ToString());
  shell_surface->SetFullscreen(false);
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
  EXPECT_NE(GetContext()->bounds().ToString(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().ToString());
}

TEST_F(ShellSurfaceTest, SetTitle) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->SetTitle(base::string16(base::ASCIIToUTF16("test")));
  surface->Attach(buffer.get());
  surface->Commit();

  // NativeWindow's title is used within the overview mode, so it should
  // have the specified title.
  EXPECT_EQ(base::ASCIIToUTF16("test"),
            shell_surface->GetWidget()->GetNativeWindow()->GetTitle());
  // The titlebar shouldn't show the title.
  EXPECT_FALSE(
      shell_surface->GetWidget()->widget_delegate()->ShouldShowWindowTitle());
}

TEST_F(ShellSurfaceTest, SetApplicationId) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  EXPECT_FALSE(shell_surface->GetWidget());
  shell_surface->SetApplicationId("pre-widget-id");

  surface->Attach(buffer.get());
  surface->Commit();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ("pre-widget-id", *GetShellApplicationId(window));
  shell_surface->SetApplicationId("test");
  EXPECT_EQ("test", *GetShellApplicationId(window));
  EXPECT_FALSE(ash::WindowState::Get(window)->allow_set_bounds_direct());

  shell_surface->SetApplicationId(nullptr);
  EXPECT_EQ(nullptr, GetShellApplicationId(window));
}

TEST_F(ShellSurfaceTest, ActivationPermission) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  surface->Attach(buffer.get());
  surface->Commit();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  ASSERT_TRUE(window);

  // No permission granted so can't activate.
  EXPECT_FALSE(HasPermissionToActivate(window));

  // Can grant permission.
  std::unique_ptr<exo::Permission> permission =
      GrantPermissionToActivate(window, base::TimeDelta::FromDays(1));
  EXPECT_TRUE(permission->Check(Permission::Capability::kActivate));
  EXPECT_TRUE(HasPermissionToActivate(window));

  // Overriding the permission revokes the previous one.
  std::unique_ptr<exo::Permission> permission2 =
      GrantPermissionToActivate(window, base::TimeDelta::FromDays(2));
  EXPECT_FALSE(permission->Check(Permission::Capability::kActivate));
  EXPECT_TRUE(permission2->Check(Permission::Capability::kActivate));

  // The old permission no longer affects the window
  permission.reset();
  EXPECT_TRUE(HasPermissionToActivate(window));

  // Deleting the permission revokes.
  permission2.reset();
  EXPECT_FALSE(HasPermissionToActivate(window));
}

TEST_F(ShellSurfaceTest, EmulateOverrideRedirect) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
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
            child_window->parent()->id());

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
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  EXPECT_FALSE(shell_surface->GetWidget());
  shell_surface->SetStartupId("pre-widget-id");

  surface->Attach(buffer.get());
  surface->Commit();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ("pre-widget-id", *GetShellStartupId(window));
  shell_surface->SetStartupId("test");
  EXPECT_EQ("test", *GetShellStartupId(window));

  shell_surface->SetStartupId(nullptr);
  EXPECT_EQ(nullptr, GetShellStartupId(window));
}

TEST_F(ShellSurfaceTest, StartMove) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  // Map shell surface.
  surface->Commit();

  // The interactive move should end when surface is destroyed.
  shell_surface->StartMove();

  // Test that destroying the shell surface before move ends is OK.
  shell_surface.reset();
}

TEST_F(ShellSurfaceTest, StartResize) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  // Map shell surface.
  surface->Commit();

  // The interactive resize should end when surface is destroyed.
  shell_surface->StartResize(HTBOTTOMRIGHT);

  // Test that destroying the surface before resize ends is OK.
  surface.reset();
}

TEST_F(ShellSurfaceTest, SetGeometry) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  gfx::Rect geometry(16, 16, 32, 32);
  shell_surface->SetGeometry(geometry);
  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_EQ(
      geometry.size().ToString(),
      shell_surface->GetWidget()->GetWindowBoundsInScreen().size().ToString());
  EXPECT_EQ(gfx::Rect(gfx::Point() - geometry.OffsetFromOrigin(), buffer_size)
                .ToString(),
            shell_surface->host_window()->bounds().ToString());
}

TEST_F(ShellSurfaceTest, SetMinimumSize) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  gfx::Size size(50, 50);
  shell_surface->SetMinimumSize(size);
  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_EQ(size, shell_surface->GetMinimumSize());
  EXPECT_EQ(size, shell_surface->GetWidget()->GetMinimumSize());
  EXPECT_EQ(size, shell_surface->GetWidget()
                      ->GetNativeWindow()
                      ->delegate()
                      ->GetMinimumSize());

  gfx::Size size_with_frame(50, 82);
  surface->SetFrame(SurfaceFrameType::NORMAL);
  EXPECT_EQ(size, shell_surface->GetMinimumSize());
  EXPECT_EQ(size_with_frame, shell_surface->GetWidget()->GetMinimumSize());
  EXPECT_EQ(size_with_frame, shell_surface->GetWidget()
                                 ->GetNativeWindow()
                                 ->delegate()
                                 ->GetMinimumSize());
}

TEST_F(ShellSurfaceTest, SetMaximumSize) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  gfx::Size size(100, 100);
  shell_surface->SetMaximumSize(size);
  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_EQ(size, shell_surface->GetMaximumSize());
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
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  int pre_close_call_count = 0;
  int close_call_count = 0;
  shell_surface->set_pre_close_callback(
      base::BindRepeating(&PreClose, base::Unretained(&pre_close_call_count),
                          base::Unretained(&close_call_count)));
  shell_surface->set_close_callback(
      base::BindRepeating(&Close, base::Unretained(&pre_close_call_count),
                          base::Unretained(&close_call_count)));

  surface->Attach(buffer.get());
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

void DestroyedCallbackCounter(int* count) {
  *count += 1;
}

TEST_F(ShellSurfaceTest, ForceClose) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  surface->Attach(buffer.get());
  surface->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());

  int surface_destroyed_ctr = 0;
  shell_surface->set_surface_destroyed_callback(base::BindOnce(
      &DestroyedCallbackCounter, base::Unretained(&surface_destroyed_ctr)));

  // Since we did not set the close callback, closing this widget will have no
  // effect.
  shell_surface->GetWidget()->Close();
  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_EQ(surface_destroyed_ctr, 0);

  // CloseNow() will always destroy the widget.
  shell_surface->GetWidget()->CloseNow();
  EXPECT_FALSE(shell_surface->GetWidget());
  EXPECT_EQ(surface_destroyed_ctr, 1);
}

uint32_t Configure(gfx::Size* suggested_size,
                   ash::WindowStateType* has_state_type,
                   bool* is_resizing,
                   bool* is_active,
                   const gfx::Size& size,
                   ash::WindowStateType state_type,
                   bool resizing,
                   bool activated,
                   const gfx::Vector2d& origin_offset) {
  *suggested_size = size;
  *has_state_type = state_type;
  *is_resizing = resizing;
  *is_active = activated;
  return 0;
}

TEST_F(ShellSurfaceTest, ConfigureCallback) {
  // Must be before shell_surface so it outlives it, for shell_surface's
  // destructor calls Configure() referencing these 4 variables.
  gfx::Size suggested_size;
  ash::WindowStateType has_state_type = ash::WindowStateType::kNormal;
  bool is_resizing = false;
  bool is_active = false;

  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->set_configure_callback(base::BindRepeating(
      &Configure, base::Unretained(&suggested_size),
      base::Unretained(&has_state_type), base::Unretained(&is_resizing),
      base::Unretained(&is_active)));

  gfx::Rect geometry(16, 16, 32, 32);
  shell_surface->SetGeometry(geometry);

  // Commit without contents should result in a configure callback with empty
  // suggested size as a mechanims to ask the client size itself.
  surface->Commit();
  EXPECT_TRUE(suggested_size.IsEmpty());

  // Geometry should not be committed until surface has contents.
  EXPECT_TRUE(shell_surface->CalculatePreferredSize().IsEmpty());

  // Widget creation is deferred until the surface has contents.
  shell_surface->Maximize();
  shell_surface->AcknowledgeConfigure(0);

  EXPECT_FALSE(shell_surface->GetWidget());
  EXPECT_TRUE(suggested_size.IsEmpty());
  EXPECT_EQ(ash::WindowStateType::kNormal, has_state_type);

  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  gfx::Rect maximized_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_EQ(maximized_bounds.size(), suggested_size);
  EXPECT_EQ(ash::WindowStateType::kMaximized, has_state_type);
  shell_surface->Restore();
  shell_surface->AcknowledgeConfigure(0);
  // It should be restored to the original geometry size.
  EXPECT_EQ(geometry.size(), shell_surface->CalculatePreferredSize());

  shell_surface->SetFullscreen(true);
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_EQ(GetContext()->bounds().size().ToString(),
            suggested_size.ToString());
  EXPECT_EQ(ash::WindowStateType::kFullscreen, has_state_type);
  shell_surface->SetFullscreen(false);
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_EQ(geometry.size(), shell_surface->CalculatePreferredSize());

  shell_surface->GetWidget()->Activate();
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_TRUE(is_active);
  shell_surface->GetWidget()->Deactivate();
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_FALSE(is_active);

  EXPECT_FALSE(is_resizing);
  shell_surface->StartResize(HTBOTTOMRIGHT);
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_TRUE(is_resizing);
}

TEST_F(ShellSurfaceTest, ToggleFullscreen) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
  EXPECT_EQ(
      buffer_size.ToString(),
      shell_surface->GetWidget()->GetWindowBoundsInScreen().size().ToString());
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
  std::unique_ptr<Surface> surface(new Surface);
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
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
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_EQ(buffer_size,
            shell_surface->GetWidget()->GetWindowBoundsInScreen().size());

  ash::WMEvent event(ash::WM_EVENT_CYCLE_SNAP_LEFT);
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  // Enter snapped mode.
  ash::WindowState::Get(window)->OnWMEvent(&event);

  EXPECT_EQ(GetContext()->bounds().width() / 2,
            shell_surface->GetWidget()->GetWindowBoundsInScreen().width());

  surface->Attach(buffer.get());
  surface->Commit();

  // Commit shouldn't change widget bounds when snapped.
  EXPECT_EQ(GetContext()->bounds().width() / 2,
            shell_surface->GetWidget()->GetWindowBoundsInScreen().width());
}

TEST_F(ShellSurfaceTest, Transient) {
  gfx::Size buffer_size(256, 256);

  std::unique_ptr<Buffer> parent_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> parent_surface(new Surface);
  parent_surface->Attach(parent_buffer.get());
  std::unique_ptr<ShellSurface> parent_shell_surface(
      new ShellSurface(parent_surface.get()));
  parent_surface->Commit();

  std::unique_ptr<Buffer> child_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> child_surface(new Surface);
  child_surface->Attach(child_buffer.get());
  std::unique_ptr<ShellSurface> child_shell_surface(
      new ShellSurface(child_surface.get()));
  // Importantly, a transient window has an associated application.
  child_surface->SetApplicationId("fake_app_id");
  child_surface->SetParent(parent_surface.get(), gfx::Point(50, 50));
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

TEST_F(ShellSurfaceTest, Popup) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  std::unique_ptr<Buffer> popup_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
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
            popup_shell_surface->GetWidget()->GetNativeWindow()->type());
  EXPECT_EQ(WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow(),
            popup_shell_surface->GetWidget()->GetNativeWindow());

  // Setting frame type on popup should have no effect.
  popup_surface->SetFrame(SurfaceFrameType::NORMAL);
  EXPECT_FALSE(popup_shell_surface->frame_enabled());

  // ShellSurface can capture the event even after it is craeted.
  std::unique_ptr<Buffer> sub_popup_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> sub_popup_surface(new Surface);
  sub_popup_surface->Attach(popup_buffer.get());
  std::unique_ptr<ShellSurface> sub_popup_shell_surface(CreatePopupShellSurface(
      sub_popup_surface.get(), popup_shell_surface.get(), gfx::Point(100, 50)));
  sub_popup_shell_surface->Grab();
  sub_popup_surface->Commit();
  ASSERT_EQ(gfx::Rect(100, 50, 256, 256),
            sub_popup_shell_surface->GetWidget()->GetWindowBoundsInScreen());
  aura::Window* target =
      sub_popup_shell_surface->GetWidget()->GetNativeWindow();
  // The capture should be on sub_popup_shell_surface.
  EXPECT_EQ(WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow(),
            target);
  EXPECT_EQ(aura::client::WINDOW_TYPE_POPUP, target->type());

  {
    // Mouse is on the top most popup.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(0, 0),
                         gfx::Point(100, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(sub_popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
  {
    // Move the mouse to the parent popup.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(-25, 0),
                         gfx::Point(75, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
  {
    // Move the mouse to the main window.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(-25, -25),
                         gfx::Point(75, 25), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }

  // Removing top most popup moves the grab to parent popup.
  sub_popup_shell_surface.reset();
  target = popup_shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow(),
            target);
  {
    // Targetting should still work.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(0, 0),
                         gfx::Point(50, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
}

TEST_F(ShellSurfaceTest, PopupWithInputRegion) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->SetInputRegion(cc::Region());

  std::unique_ptr<Buffer> child_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> child_surface(new Surface);
  child_surface->Attach(child_buffer.get());

  auto subsurface =
      std::make_unique<SubSurface>(child_surface.get(), surface.get());
  subsurface->SetPosition(gfx::Point(10, 10));
  child_surface->SetInputRegion(cc::Region(gfx::Rect(0, 0, 256, 2560)));
  child_surface->Commit();
  surface->Commit();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  std::unique_ptr<Buffer> popup_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
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
            popup_shell_surface->GetWidget()->GetNativeWindow()->type());
  EXPECT_EQ(WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow(),
            popup_shell_surface->GetWidget()->GetNativeWindow());

  // Setting frame type on popup should have no effect.
  popup_surface->SetFrame(SurfaceFrameType::NORMAL);
  EXPECT_FALSE(popup_shell_surface->frame_enabled());

  aura::Window* target = popup_shell_surface->GetWidget()->GetNativeWindow();

  {
    // Mouse is on the popup.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(0, 0),
                         gfx::Point(50, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }

  {
    // If it matches the parent's sub surface, use it.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(-25, 0),
                         gfx::Point(25, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(child_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
  {
    // If it didnt't match any surface in the parent, fallback to
    // the popup's surface.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(-50, 0),
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
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(0, 0),
                         gfx::Point(50, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(window.get());
    EXPECT_EQ(nullptr, GetTargetSurfaceForLocatedEvent(&event));
  }
}

TEST_F(ShellSurfaceTest, Caption) {
  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  shell_surface->OnSetFrame(SurfaceFrameType::NORMAL);
  surface->Commit();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  aura::Window* target = shell_surface->GetWidget()->GetNativeWindow();
  target->SetCapture();
  EXPECT_EQ(WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow(),
            shell_surface->GetWidget()->GetNativeWindow());
  {
    // Move the mouse at the caption of the captured window.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(5, 5), gfx::Point(5, 5),
                         ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(nullptr, GetTargetSurfaceForLocatedEvent(&event));
  }

  {
    // Move the mouse at the center of the captured window.
    gfx::Rect bounds = shell_surface->GetWidget()->GetWindowBoundsInScreen();
    gfx::Point center = bounds.CenterPoint();
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, center - bounds.OffsetFromOrigin(),
                         center, ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
}

TEST_F(ShellSurfaceTest, CaptionWithPopup) {
  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));
  shell_surface->OnSetFrame(SurfaceFrameType::NORMAL);

  auto popup_buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
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
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(5, 5),
                         gfx::Point(55, 55), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }

  {
    // Move the mouse at the caption of the main window.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(-45, -45),
                         gfx::Point(5, 5), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(nullptr, GetTargetSurfaceForLocatedEvent(&event));
  }

  {
    // Move the mouse in the main window.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(-25, 0),
                         gfx::Point(25, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
}

TEST_F(ShellSurfaceTest, SkipImeProcessingPropagateToSurface) {
  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));
  shell_surface->OnSetFrame(SurfaceFrameType::NORMAL);

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  ASSERT_FALSE(window->GetProperty(aura::client::kSkipImeProcessing));
  ASSERT_FALSE(
      surface->window()->GetProperty(aura::client::kSkipImeProcessing));

  window->SetProperty(aura::client::kSkipImeProcessing, true);
  EXPECT_TRUE(window->GetProperty(aura::client::kSkipImeProcessing));
  EXPECT_TRUE(surface->window()->GetProperty(aura::client::kSkipImeProcessing));
}

TEST_F(ShellSurfaceTest, NotifyLeaveEnter) {
  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  auto func = [](int64_t* old_display_id, int64_t* new_display_id,
                 int64_t old_id, int64_t new_id) {
    DCHECK_EQ(0, *old_display_id);
    DCHECK_EQ(0, *new_display_id);
    *old_display_id = old_id;
    *new_display_id = new_id;
  };

  int64_t old_display_id = 0, new_display_id = 0;

  surface->set_leave_enter_callback(
      base::BindRepeating(func, &old_display_id, &new_display_id));
  ;
  // Creating a new shell surface should notify on which display
  // it is created.
  surface->Attach(buffer.get());
  surface->Commit();
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

}  // namespace exo
