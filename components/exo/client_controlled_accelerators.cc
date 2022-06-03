// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/client_controlled_accelerators.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"

namespace exo {

ClientControlledAcceleratorTarget::ClientControlledAcceleratorTarget(
    ClientControlledShellSurface* surface)
    : surface_(surface) {}

ClientControlledAcceleratorTarget::~ClientControlledAcceleratorTarget() =
    default;

void ClientControlledAcceleratorTarget::RegisterAccelerator(
    const ui::Accelerator& accelerator,
    ClientControlledAcceleratorAction action) {
  accelerators_.insert(std::make_pair(ui::Accelerator{accelerator}, action));
}

void ClientControlledAcceleratorTarget::RegisterAccelerator(
    ui::Accelerator&& accelerator,
    ClientControlledAcceleratorAction action) {
  accelerators_.insert(std::make_pair(std::move(accelerator), action));
}

bool ClientControlledAcceleratorTarget::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  auto it = accelerators_.find(accelerator);
  DCHECK(it != accelerators_.end());
  ClientControlledAcceleratorAction action = it->second;

  switch (action) {
    case ClientControlledAcceleratorAction::ZOOM_IN:
      surface_->ChangeZoomLevel(ZoomChange::IN);
      base::RecordAction(base::UserMetricsAction("Accel_ARC_Zoom_Ui_In"));
      break;
    case ClientControlledAcceleratorAction::ZOOM_OUT:
      surface_->ChangeZoomLevel(ZoomChange::OUT);
      base::RecordAction(base::UserMetricsAction("Accel_ARC_Zoom_Ui_Out"));
      break;
    case ClientControlledAcceleratorAction::ZOOM_RESET:
      surface_->ChangeZoomLevel(ZoomChange::RESET);
      base::RecordAction(base::UserMetricsAction("Accel_ARC_Zoom_Ui_Reset"));
      break;
  }
  return true;
}

bool ClientControlledAcceleratorTarget::CanHandleAccelerators() const {
  return true;
}

}  // namespace exo
