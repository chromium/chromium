// Copyright 2015 The Chromium Authors
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
#include "components/exo/input_method_surface.h"
#include "components/exo/surface.h"
#include "components/exo/toast_surface.h"
#include "components/exo/wm_helper.h"
#include "components/exo/xdg_shell_surface.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace exo {
namespace test {

ClientControlledShellSurfaceDelegate::ClientControlledShellSurfaceDelegate(
    ClientControlledShellSurface* shell_surface,
    bool delay_commit)
    : shell_surface_(shell_surface), delay_commit_(delay_commit) {}
ClientControlledShellSurfaceDelegate::~ClientControlledShellSurfaceDelegate() =
    default;

void ClientControlledShellSurfaceDelegate::OnGeometryChanged(
    const gfx::Rect& geometry) {}
void ClientControlledShellSurfaceDelegate::OnStateChanged(
    chromeos::WindowStateType old_state,
    chromeos::WindowStateType new_state) {
  switch (new_state) {
    case chromeos::WindowStateType::kNormal:
    case chromeos::WindowStateType::kDefault:
      shell_surface_->SetRestored();
      break;
    case chromeos::WindowStateType::kMinimized:
      shell_surface_->SetMinimized();
      break;
    case chromeos::WindowStateType::kMaximized:
      shell_surface_->SetMaximized();
      break;
    case chromeos::WindowStateType::kFullscreen:
      shell_surface_->SetFullscreen(true, display::kInvalidDisplayId);
      break;
    case chromeos::WindowStateType::kPinned:
      shell_surface_->SetPinned(chromeos::WindowPinType::kPinned);
      break;
    case chromeos::WindowStateType::kTrustedPinned:
      shell_surface_->SetPinned(chromeos::WindowPinType::kTrustedPinned);
      break;
    default:
      NOTIMPLEMENTED();
      break;
  }
  Commit();
}
void ClientControlledShellSurfaceDelegate::OnBoundsChanged(
    chromeos::WindowStateType current_state,
    chromeos::WindowStateType requested_state,
    int64_t display_id,
    const gfx::Rect& bounds_in_screen,
    bool is_resize,
    int bounds_change,
    bool is_adjusted_bounds) {
  ASSERT_TRUE(display_id != display::kInvalidDisplayId);

  auto* window_state =
      ash::WindowState::Get(shell_surface_->GetWidget()->GetNativeWindow());

  if (!shell_surface_->host_window()->GetRootWindow())
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
  shell_surface_->SetBounds(display_id, bounds_in_display);

  if (requested_state != window_state->GetStateType()) {
    DCHECK(requested_state == chromeos::WindowStateType::kPrimarySnapped ||
           requested_state == chromeos::WindowStateType::kSecondarySnapped);

    if (requested_state == chromeos::WindowStateType::kPrimarySnapped)
      shell_surface_->SetSnapPrimary(chromeos::kDefaultSnapRatio);
    else
      shell_surface_->SetSnapSecondary(chromeos::kDefaultSnapRatio);
  }

  Commit();
}
void ClientControlledShellSurfaceDelegate::OnDragStarted(int component) {}
void ClientControlledShellSurfaceDelegate::OnDragFinished(int x,
                                                          int y,
                                                          bool canceled) {}
void ClientControlledShellSurfaceDelegate::OnZoomLevelChanged(
    ZoomChange zoom_change) {}

void ClientControlledShellSurfaceDelegate::Commit() {
  if (!delay_commit_)
    shell_surface_->OnSurfaceCommit();
}

////////////////////////////////////////////////////////////////////////////////
// ExoTestHelper, public:

ExoTestHelper::ExoTestHelper() {
  ash::window_positioner::DisableAutoPositioning(true);
}

ExoTestHelper::~ExoTestHelper() = default;

// static
std::unique_ptr<Buffer> ExoTestHelper::CreateBuffer(
    ShellSurfaceBase* shell_surface,
    gfx::BufferFormat format) {
  return CreateBuffer(
      shell_surface->GetWidget()->GetWindowBoundsInScreen().size(), format);
}

// static
std::unique_ptr<Buffer> ExoTestHelper::CreateBuffer(
    gfx::Size buffer_size,
    gfx::BufferFormat buffer_format,
    bool is_overlay_candidate) {
  return Buffer::CreateBuffer(buffer_size, buffer_format,
                              gfx::BufferUsage::GPU_READ, "ExoTestHelper",
                              gpu::kNullSurfaceHandle,
                              /*shutdown_event=*/nullptr, is_overlay_candidate);
}

// static
std::unique_ptr<Buffer> ExoTestHelper::CreateBufferFromGMBHandle(
    gfx::GpuMemoryBufferHandle handle,
    gfx::Size buffer_size,
    gfx::BufferFormat buffer_format) {
  return Buffer::CreateBufferFromGMBHandle(
      std::move(handle), buffer_size, buffer_format, gfx::BufferUsage::GPU_READ,
      /*query_type=*/GL_COMMANDS_COMPLETED_CHROMIUM, /*use_zero_copy=*/true,
      /*is_overlay_candidate=*/false, /*y_invert=*/false);
}

std::unique_ptr<InputMethodSurface> ExoTestHelper::CreateInputMethodSurface(
    Surface* surface,
    InputMethodSurfaceManager* surface_manager,
    bool default_scale_cancellation) {
  auto shell_surface = std::make_unique<InputMethodSurface>(
      surface_manager, surface, default_scale_cancellation);

  shell_surface->set_delegate(
      std::make_unique<ClientControlledShellSurfaceDelegate>(
          shell_surface.get()));

  return shell_surface;
}

std::unique_ptr<ToastSurface> ExoTestHelper::CreateToastSurface(
    Surface* surface,
    ToastSurfaceManager* surface_manager,
    bool default_scale_cancellation) {
  auto shell_surface = std::make_unique<ToastSurface>(
      surface_manager, surface, default_scale_cancellation);

  shell_surface->set_delegate(
      std::make_unique<ClientControlledShellSurfaceDelegate>(
          shell_surface.get()));

  return shell_surface;
}

}  // namespace test
}  // namespace exo
