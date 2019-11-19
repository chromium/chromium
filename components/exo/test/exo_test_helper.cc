// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/exo_test_helper.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_positioning_utils.h"
#include "components/exo/buffer.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/display.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/exo/xdg_shell_surface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace exo {
namespace test {

namespace {

void HandleWindowStateRequest(ClientControlledShellSurface* shell_surface,
                              ash::WindowStateType old_state,
                              ash::WindowStateType new_state) {
  switch (new_state) {
    case ash::WindowStateType::kNormal:
    case ash::WindowStateType::kDefault:
      shell_surface->SetRestored();
      break;
    case ash::WindowStateType::kMinimized:
      shell_surface->SetMinimized();
      break;
    case ash::WindowStateType::kMaximized:
      shell_surface->SetMaximized();
      break;
    case ash::WindowStateType::kFullscreen:
      shell_surface->SetFullscreen(true);
      break;
    default:
      NOTIMPLEMENTED();
      break;
  }
  shell_surface->OnSurfaceCommit();
}

void HandleBoundsChangedRequest(ClientControlledShellSurface* shell_surface,
                                ash::WindowStateType current_state,
                                ash::WindowStateType requested_state,
                                int64_t display_id,
                                const gfx::Rect& bounds_in_screen,
                                bool is_resize,
                                int bounds_change) {
  ASSERT_TRUE(display_id != display::kInvalidDisplayId);

  auto* window_state =
      ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow());

  if (!shell_surface->host_window()->GetRootWindow())
    return;

  display::Display target_display;
  const display::Screen* screen = display::Screen::GetScreen();

  if (!screen->GetDisplayWithDisplayId(display_id, &target_display)) {
    return;
  }

  // Don't change the bounds in maximize/fullscreen/pinned state.
  if (window_state->IsMaximizedOrFullscreenOrPinned() &&
      requested_state == window_state->GetStateType()) {
    return;
  }

  gfx::Rect bounds_in_display(bounds_in_screen);
  bounds_in_display.Offset(-target_display.bounds().OffsetFromOrigin());
  shell_surface->SetBounds(display_id, bounds_in_display);

  if (requested_state != window_state->GetStateType()) {
    DCHECK(requested_state == ash::WindowStateType::kLeftSnapped ||
           requested_state == ash::WindowStateType::kRightSnapped);

    if (requested_state == ash::WindowStateType::kLeftSnapped)
      shell_surface->SetSnappedToLeft();
    else
      shell_surface->SetSnappedToRight();
  }

  shell_surface->OnSurfaceCommit();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ExoTestHelper, public:

ExoTestWindow::ExoTestWindow(std::unique_ptr<gfx::GpuMemoryBuffer> gpu_buffer,
                             bool is_modal) {
  surface_.reset(new Surface());
  int container = is_modal ? ash::kShellWindowId_SystemModalContainer
                           : ash::desks_util::GetActiveDeskContainerId();
  shell_surface_ = std::make_unique<ShellSurface>(surface_.get(), gfx::Point(),
                                                  true, false, container);

  buffer_.reset(new Buffer(std::move(gpu_buffer)));
  surface_->Attach(buffer_.get());
  surface_->Commit();

  ash::CenterWindow(shell_surface_->GetWidget()->GetNativeWindow());
}

ExoTestWindow::ExoTestWindow(ExoTestWindow&& other) {
  surface_ = std::move(other.surface_);
  buffer_ = std::move(other.buffer_);
  shell_surface_ = std::move(other.shell_surface_);
}

ExoTestWindow::~ExoTestWindow() {}

gfx::Point ExoTestWindow::origin() {
  return surface_->window()->GetBoundsInScreen().origin();
}

////////////////////////////////////////////////////////////////////////////////
// ExoTestHelper, public:

ExoTestHelper::ExoTestHelper() {
  ash::WindowPositioner::DisableAutoPositioning(true);
}

ExoTestHelper::~ExoTestHelper() {}

std::unique_ptr<gfx::GpuMemoryBuffer> ExoTestHelper::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format) {
  return aura::Env::GetInstance()
      ->context_factory()
      ->GetGpuMemoryBufferManager()
      ->CreateGpuMemoryBuffer(size, format, gfx::BufferUsage::GPU_READ,
                              gpu::kNullSurfaceHandle);
}

ExoTestWindow ExoTestHelper::CreateWindow(int width,
                                          int height,
                                          bool is_modal) {
  return ExoTestWindow(CreateGpuMemoryBuffer(gfx::Size(width, height)),
                       is_modal);
}

std::unique_ptr<ClientControlledShellSurface>
ExoTestHelper::CreateClientControlledShellSurface(Surface* surface,
                                                  bool is_modal) {
  int container = is_modal ? ash::kShellWindowId_SystemModalContainer
                           : ash::desks_util::GetActiveDeskContainerId();
  auto shell_surface = Display().CreateClientControlledShellSurface(
      surface, container,
      WMHelper::GetInstance()->GetDefaultDeviceScaleFactor());

  shell_surface->set_state_changed_callback(base::BindRepeating(
      &HandleWindowStateRequest, base::Unretained(shell_surface.get())));

  shell_surface->set_bounds_changed_callback(
      base::BindRepeating(&HandleBoundsChangedRequest, shell_surface.get()));

  return shell_surface;
}

}  // namespace test
}  // namespace exo
