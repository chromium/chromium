// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/fullscreen_shell_surface.h"

#include "base/bind.h"
#include "components/exo/buffer.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base_views.h"
#include "components/exo/wm_helper.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/buffer_types.h"
#include "ui/views/widget/widget.h"

namespace exo {
namespace {

using FullscreenShellSurfaceTest = test::ExoTestBaseViews;

std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format) {
  return aura::Env::GetInstance()
      ->context_factory()
      ->GetGpuMemoryBufferManager()
      ->CreateGpuMemoryBuffer(size, format, gfx::BufferUsage::GPU_READ,
                              gpu::kNullSurfaceHandle);
}

void DestroyFullscreenShellSurface(
    std::unique_ptr<FullscreenShellSurface>* fullscreen_surface) {
  fullscreen_surface->reset();
}

void Close(int* close_call_count) {
  (*close_call_count)++;
}

TEST_F(FullscreenShellSurfaceTest, SurfaceDestroyedCallback) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<FullscreenShellSurface> fullscreen_surface(
      new FullscreenShellSurface());
  fullscreen_surface->SetSurface(surface.get());

  fullscreen_surface->set_surface_destroyed_callback(base::BindOnce(
      &DestroyFullscreenShellSurface, base::Unretained(&fullscreen_surface)));

  // Change the surface so the commit has an actual change otherwise it triggers
  // a DCHECK during frame submission.
  surface->SetViewport(gfx::Size(64, 64));
  surface->Commit();

  EXPECT_TRUE(fullscreen_surface.get());
  surface.reset();
  EXPECT_FALSE(fullscreen_surface.get());
}

TEST_F(FullscreenShellSurfaceTest, CloseCallback) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(new Buffer(
      CreateGpuMemoryBuffer(buffer_size, gfx::BufferFormat::RGBA_8888)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<FullscreenShellSurface> fullscreen_surface(
      new FullscreenShellSurface());
  fullscreen_surface->SetSurface(surface.get());

  int close_call_count = 0;
  fullscreen_surface->set_close_callback(
      base::Bind(&Close, base::Unretained(&close_call_count)));

  surface->Attach(buffer.get());
  surface->Commit();

  EXPECT_EQ(0, close_call_count);
  fullscreen_surface->Close();
  EXPECT_EQ(1, close_call_count);
}

TEST_F(FullscreenShellSurfaceTest, ShouldShowWindowTitle) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<FullscreenShellSurface> fullscreen_surface(
      new FullscreenShellSurface());
  fullscreen_surface->SetSurface(surface.get());

  EXPECT_FALSE(fullscreen_surface->ShouldShowWindowTitle());
}

TEST_F(FullscreenShellSurfaceTest, SetApplicationId) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(new Buffer(
      CreateGpuMemoryBuffer(buffer_size, gfx::BufferFormat::RGBA_8888)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<FullscreenShellSurface> fullscreen_surface(
      new FullscreenShellSurface());
  fullscreen_surface->SetSurface(surface.get());

  EXPECT_TRUE(fullscreen_surface->GetWidget());
  fullscreen_surface->SetApplicationId("test-id");

  surface->Attach(buffer.get());
  surface->Commit();
  aura::Window* window = fullscreen_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ("test-id", *GetShellApplicationId(window));
  fullscreen_surface->SetApplicationId("test");
  EXPECT_EQ("test", *GetShellApplicationId(window));

  fullscreen_surface->SetApplicationId(nullptr);
  EXPECT_EQ(nullptr, GetShellApplicationId(window));
}

TEST_F(FullscreenShellSurfaceTest, SetStartupId) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(new Buffer(
      CreateGpuMemoryBuffer(buffer_size, gfx::BufferFormat::RGBA_8888)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<FullscreenShellSurface> fullscreen_surface(
      new FullscreenShellSurface());
  fullscreen_surface->SetSurface(surface.get());

  EXPECT_TRUE(fullscreen_surface->GetWidget());
  fullscreen_surface->SetStartupId("test-id");

  surface->Attach(buffer.get());
  surface->Commit();
  aura::Window* window = fullscreen_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ("test-id", *GetShellStartupId(window));
  fullscreen_surface->SetStartupId("test");
  EXPECT_EQ("test", *GetShellStartupId(window));

  fullscreen_surface->SetStartupId(nullptr);
  EXPECT_EQ(nullptr, GetShellStartupId(window));
}

TEST_F(FullscreenShellSurfaceTest, Maximize) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(new Buffer(
      CreateGpuMemoryBuffer(buffer_size, gfx::BufferFormat::RGBA_8888)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<FullscreenShellSurface> fullscreen_surface(
      new FullscreenShellSurface());
  fullscreen_surface->SetSurface(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();
  fullscreen_surface->Maximize();
  EXPECT_TRUE(fullscreen_surface->GetWidget()->IsMaximized());
}

TEST_F(FullscreenShellSurfaceTest, Minimize) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(new Buffer(
      CreateGpuMemoryBuffer(buffer_size, gfx::BufferFormat::RGBA_8888)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<FullscreenShellSurface> fullscreen_surface(
      new FullscreenShellSurface());
  fullscreen_surface->SetSurface(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();
  fullscreen_surface->Minimize();
  EXPECT_TRUE(fullscreen_surface->GetWidget()->IsMinimized());
}

TEST_F(FullscreenShellSurfaceTest, Bounds) {
  aura::Window* root_window =
      WMHelper::GetInstance()->GetRootWindowForNewWindows();
  gfx::Rect new_root_bounds(10, 10, 100, 100);
  root_window->SetBounds(new_root_bounds);

  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(new Buffer(
      CreateGpuMemoryBuffer(buffer_size, gfx::BufferFormat::RGBA_8888)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<FullscreenShellSurface> fullscreen_surface(
      new FullscreenShellSurface());
  fullscreen_surface->SetSurface(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();
  gfx::Rect fullscreen_bounds =
      fullscreen_surface->GetWidget()->GetWindowBoundsInScreen();
  gfx::Rect expected_bounds(new_root_bounds.size());
  EXPECT_EQ(fullscreen_bounds, expected_bounds);
}

TEST_F(FullscreenShellSurfaceTest, BoundsWithPartiallyOffscreenSubSurface) {
  aura::Window* root_window =
      WMHelper::GetInstance()->GetRootWindowForNewWindows();
  gfx::Rect new_root_bounds(10, 10, 100, 100);
  gfx::Rect expected_bounds(new_root_bounds.size());
  root_window->SetBounds(new_root_bounds);

  gfx::Size buffer_size(100, 100);
  auto buffer = std::make_unique<Buffer>(
      CreateGpuMemoryBuffer(buffer_size, gfx::BufferFormat::RGBA_8888));
  auto parent = std::make_unique<Surface>();
  auto fullscreen_surface = std::make_unique<FullscreenShellSurface>();
  fullscreen_surface->SetSurface(parent.get());

  parent->Attach(buffer.get());
  parent->Commit();
  EXPECT_EQ(fullscreen_surface->GetWidget()->GetWindowBoundsInScreen(),
            expected_bounds);
  EXPECT_EQ(parent->window()->bounds(), expected_bounds);

  auto surface = std::make_unique<Surface>();
  auto sub_surface = std::make_unique<SubSurface>(surface.get(), parent.get());
  surface->Attach(buffer.get());
  sub_surface->SetPosition(gfx::Point(-50, -50));

  parent->Commit();
  // Make sure the sub-surface doesn't affect the Fullscreen Shell's Window
  // size/position.
  EXPECT_EQ(fullscreen_surface->GetWidget()->GetWindowBoundsInScreen(),
            expected_bounds);
  // The root surface should also have the same position/size as before.
  EXPECT_EQ(parent->window()->bounds(), expected_bounds);
}

TEST_F(FullscreenShellSurfaceTest, SetAXChildTree) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<FullscreenShellSurface> fullscreen_surface(
      new FullscreenShellSurface());
  fullscreen_surface->SetSurface(surface.get());
  ui::AXNodeData node_data;
  fullscreen_surface->GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(
      node_data.HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId));

  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  fullscreen_surface->SetChildAxTreeId(tree_id);
  fullscreen_surface->GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(
      node_data.HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId));
}

TEST_F(FullscreenShellSurfaceTest, SwapSurface) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<FullscreenShellSurface> fullscreen_surface(
      new FullscreenShellSurface());
  fullscreen_surface->SetSurface(surface.get());
  fullscreen_surface->SetEnabled(true);
  fullscreen_surface->set_surface_destroyed_callback(base::BindOnce(
      &DestroyFullscreenShellSurface, base::Unretained(&fullscreen_surface)));
  EXPECT_EQ(surface.get(), fullscreen_surface->root_surface());
  std::unique_ptr<Surface> surface2(new Surface);
  fullscreen_surface->SetSurface(surface2.get());
  EXPECT_EQ(surface2.get(), fullscreen_surface->root_surface());
  EXPECT_NE(surface.get(), fullscreen_surface->root_surface());
  // RootWindow->FullscreenShellSurface->FullscreenShellSurfaceHost->ExoSurface
  EXPECT_EQ(1ul, WMHelper::GetInstance()
                     ->GetRootWindowForNewWindows()
                     ->children()[0]
                     ->children()[0]
                     ->children()
                     .size());
}

TEST_F(FullscreenShellSurfaceTest, RemoveSurface) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<FullscreenShellSurface> fullscreen_surface(
      new FullscreenShellSurface());
  fullscreen_surface->SetSurface(surface.get());
  fullscreen_surface->SetEnabled(true);
  fullscreen_surface->set_surface_destroyed_callback(base::BindOnce(
      &DestroyFullscreenShellSurface, base::Unretained(&fullscreen_surface)));
  EXPECT_EQ(surface.get(), fullscreen_surface->root_surface());
  fullscreen_surface->SetSurface(nullptr);
  EXPECT_FALSE(fullscreen_surface->root_surface());
  // RootWindow->FullscreenShellSurface->FullscreenShellSurfaceHost->null
  EXPECT_EQ(0ul, WMHelper::GetInstance()
                     ->GetRootWindowForNewWindows()
                     ->children()[0]
                     ->children()[0]
                     ->children()
                     .size());
}

}  // namespace
}  // namespace exo
