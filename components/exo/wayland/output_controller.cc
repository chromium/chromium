// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/output_controller.h"

#include <secure-output-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <xdg-output-unstable-v1-server-protocol.h>

#include "ash/shell.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "components/exo/wayland/wl_output.h"
#include "components/exo/wayland/zaura_output_manager.h"
#include "components/exo/wayland/zcr_secure_output.h"
#include "components/exo/wayland/zxdg_output_manager.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace exo::wayland {

OutputController::OutputController(Delegate* delegate) : delegate_(delegate) {
  // aura_output_manager needs to be registered before the wl_output globals to
  // ensure clients can bind to the aura_output_manager before any wl_outputs.
  // This is necessary to ensure aura_output_manager can send relevant output
  // events immediately after an output is bound to the client and before the
  // data in these events might be needed by the client.
  wl_display* display = delegate_->GetWaylandDisplay();
  wl_global_create(display, &zaura_output_manager_interface,
                   kZAuraOutputManagerVersion, nullptr,
                   bind_aura_output_manager);

  // Populate the initial output state.
  OnDidProcessDisplayChanges(
      {ash::Shell::Get()->display_manager()->active_display_list(),
       Displays(),
       {}});

  wl_global_create(display, &zcr_secure_output_v1_interface,
                   /*version=*/1, nullptr, bind_secure_output);
  wl_global_create(display, &zxdg_output_manager_v1_interface,
                   /*version=*/3, nullptr, bind_zxdg_output_manager);

  display_manager_observation_.Observe(ash::Shell::Get()->display_manager());
  ash::Shell::Get()->AddShellObserver(this);
}

OutputController::~OutputController() {
  ash::Shell::Get()->RemoveShellObserver(this);
}

void OutputController::OnDidProcessDisplayChanges(
    const DisplayConfigurationChange& configuration_change) {
  // Process added displays before removed displays to ensure exo does not leave
  // clients in a temporary state where no outputs are present.
  for (const display::Display& added_display :
       configuration_change.added_displays) {
    auto output = std::make_unique<WaylandDisplayOutput>(added_display);
    output->set_global(wl_global_create(delegate_->GetWaylandDisplay(),
                                        &wl_output_interface, kWlOutputVersion,
                                        output.get(), bind_output));
    CHECK_EQ(outputs_.count(added_display.id()), 0u);
    outputs_.insert(std::make_pair(added_display.id(), std::move(output)));
  }

  for (const display::Display& removed_display :
       configuration_change.removed_displays) {
    // There should always be at least one display tracked by Exo.
    CHECK(outputs_.size() > 1);
    CHECK_EQ(outputs_.count(removed_display.id()), 1u);
    std::unique_ptr<WaylandDisplayOutput> output =
        std::move(outputs_[removed_display.id()]);
    outputs_.erase(removed_display.id());
    output.release()->OnDisplayRemoved();
  }

  for (const auto& change : configuration_change.display_metrics_changes) {
    if (auto* wayland_display_output =
            GetWaylandDisplayOutput(change.display->id())) {
      wayland_display_output->SendDisplayMetricsChanges(change.display.get(),
                                                        change.changed_metrics);
    }
  }

  UpdateActivatedDisplayIfNecessary();

  // Flush updated outputs to clients immediately.
  // TODO(crbug.com/1502682): Exo should be updated to automatically flush
  // buffers at the end of task processing if necessary.
  delegate_->Flush();
}

void OutputController::OnDisplayForNewWindowsChanged() {
  UpdateActivatedDisplayIfNecessary();
}

wl_resource* OutputController::GetOutputResource(wl_client* client,
                                                 int64_t display_id) {
  CHECK_NE(display_id, display::kInvalidDisplayId);
  WaylandDisplayOutput* wayland_display_output =
      GetWaylandDisplayOutput(display_id);
  return wayland_display_output
             ? wayland_display_output->GetOutputResourceForClient(client)
             : nullptr;
}

WaylandDisplayOutput* OutputController::GetWaylandDisplayOutput(
    int64_t display_id) {
  CHECK_NE(display_id, display::kInvalidDisplayId);
  auto iter = outputs_.find(display_id);
  return iter == outputs_.end() ? nullptr : iter->second.get();
}

void OutputController::UpdateActivatedDisplayIfNecessary() {
  const int64_t current_active_display_id =
      display::Screen::GetScreen()->GetDisplayForNewWindows().id();
  if (dispatched_activated_display_id_ == current_active_display_id) {
    return;
  }

  // Since the ordering of observers on the display manager is not well defined
  // there may be observers that respond to display changes before the
  // controller, resulting in attempted activations of outputs that have not yet
  // been created (see b/323403137).
  // TODO(tluk): Explore solutions that may improve ordering guarantees without
  // relying on state-tracking within the controller class.
  auto output_pair = outputs_.find(current_active_display_id);
  if (output_pair != outputs_.end()) {
    output_pair->second->SendOutputActivated();
    dispatched_activated_display_id_ = current_active_display_id;
  }
}

}  // namespace exo::wayland
