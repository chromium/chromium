// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/output_controller_test_api.h"
#include "ash/shell.h"

namespace exo::wayland {

OutputControllerTestApi::OutputControllerTestApi(
    OutputController& output_controller)
    : output_controller_(output_controller) {}

WaylandDisplayOutput* OutputControllerTestApi::GetWaylandDisplayOutput(
    int64_t display_id) {
  return output_controller_->GetWaylandDisplayOutput(display_id);
}

const OutputController::DisplayOutputMap&
OutputControllerTestApi::GetOutputMap() const {
  return output_controller_->outputs_;
}

int64_t OutputControllerTestApi::GetDispatchedActivatedDisplayId() const {
  return output_controller_->dispatched_activated_display_id_;
}

void OutputControllerTestApi::ResetDisplayManagerObservation() {
  output_controller_->display_manager_observation_.Reset();
  output_controller_->display_manager_observation_.Observe(
      ash::Shell::Get()->display_manager());
}

}  // namespace exo::wayland
