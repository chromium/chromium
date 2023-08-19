// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/custom_window_state_delegate.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/window_state.h"
#include "components/exo/shell_surface_presentation_time_recorder.h"
#include "ui/base/hit_test.h"

namespace exo {

CustomWindowStateDelegate::CustomWindowStateDelegate()
    : CustomWindowStateDelegate(nullptr) {}

CustomWindowStateDelegate::CustomWindowStateDelegate(
    ShellSurface* shell_surface)
    : shell_surface_(shell_surface) {}

CustomWindowStateDelegate::~CustomWindowStateDelegate() = default;

bool CustomWindowStateDelegate::ToggleFullscreen(
    ash::WindowState* window_state) {
  return false;
}

void CustomWindowStateDelegate::ToggleLockedFullscreen(
    ash::WindowState* window_state) {
  // Sets up the shell environment as appropriate for locked Lacros or Ash
  // chrome sessions including disabling ARC.
  ash::Shell::Get()->shell_delegate()->SetUpEnvironmentForLockedFullscreen(
      *window_state);
}

std::unique_ptr<ash::PresentationTimeRecorder>
CustomWindowStateDelegate::OnDragStarted(int component) {
  if (!shell_surface_ || !ui::IsResizingComponent(component))
    return nullptr;

  return std::make_unique<ShellSurfacePresentationTimeRecorder>(
      shell_surface_,
      ShellSurfacePresentationTimeRecorder::CreateHistogramReporter(
          "Ash.InteractiveWindowResize.Lacros.TimeToPresent",
          "Ash.InteractiveWindowResize.Lacros.TimeToPresent.MaxLatency"));
}

void CustomWindowStateDelegate::OnDragFinished(bool cancel,
                                               const gfx::PointF& location) {
  shell_surface_->EndDrag();
}

}  // namespace exo
