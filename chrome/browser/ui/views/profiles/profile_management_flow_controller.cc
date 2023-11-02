// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"

#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"

ProfileManagementFlowController::ProfileManagementFlowController(
    ProfilePickerWebContentsHost* host,
    Step initial_step)
    : initial_step_(initial_step), host_(host) {}

ProfileManagementFlowController::~ProfileManagementFlowController() = default;

void ProfileManagementFlowController::Init() {
  SwitchToStep(initial_step());
}

void ProfileManagementFlowController::SwitchToStep(
    Step step,
    bool reset_state,
    base::OnceClosure pop_step_callback,
    base::OnceCallback<void(bool)> step_switch_finished_callback) {
  DCHECK_NE(Step::kUnknown, step);
  DCHECK_NE(current_step_, step);

  auto* new_step_controller = initialized_steps_.at(step).get();
  DCHECK(new_step_controller);
  new_step_controller->set_pop_step_callback(std::move(pop_step_callback));
  new_step_controller->Show(std::move(step_switch_finished_callback),
                            reset_state);

  if (initialized_steps_.contains(current_step_)) {
    initialized_steps_.at(current_step_)->OnHidden();
  }

  current_step_ = step;
}

void ProfileManagementFlowController::OnNavigateBackRequested() {
  DCHECK(initialized_steps_.contains(current_step_));
  initialized_steps_.at(current_step_)->OnNavigateBackRequested();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void ProfileManagementFlowController::OnReloadRequested() {
  DCHECK(initialized_steps_.contains(current_step_));
  initialized_steps_.at(current_step_)->OnReloadRequested();
}
#endif

void ProfileManagementFlowController::RegisterStep(
    Step step,
    std::unique_ptr<ProfileManagementStepController> step_controller) {
  initialized_steps_[step] = std::move(step_controller);
}

void ProfileManagementFlowController::UnregisterStep(Step step) {
  initialized_steps_.erase(step);
}

bool ProfileManagementFlowController::IsStepInitialized(Step step) const {
  return initialized_steps_.contains(step) && initialized_steps_.at(step);
}
