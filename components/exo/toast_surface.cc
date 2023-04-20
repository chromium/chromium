// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/toast_surface.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "components/exo/toast_surface_manager.h"
#include "components/exo/wm_helper.h"
#include "ui/base/class_property.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace exo {

ToastSurface::ToastSurface(ToastSurfaceManager* manager,
                           Surface* surface,
                           bool default_scale_cancellation)
    : ClientControlledShellSurface(surface,
                                   false /* can_minimize */,
                                   ash::kShellWindowId_OverlayContainer,
                                   default_scale_cancellation,
                                   /*supports_floated_state=*/false),
      manager_(manager) {
  SetActivatable(false);
  DisableMovement();
  host_window()->SetName("ExoToastSurface");
}

ToastSurface::~ToastSurface() {
  if (added_to_manager_)
    manager_->RemoveSurface(this);
}

void ToastSurface::OnSurfaceCommit() {
  ClientControlledShellSurface::OnSurfaceCommit();

  if (!added_to_manager_) {
    added_to_manager_ = true;
    manager_->AddSurface(this);
  }
}

}  // namespace exo
