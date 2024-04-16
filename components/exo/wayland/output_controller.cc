// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/output_controller.h"

#include <aura-output-management-server-protocol.h>
#include <secure-output-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <xdg-output-unstable-v1-server-protocol.h>

#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "components/exo/wayland/output_configuration_change.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "components/exo/wayland/wl_output.h"
#include "components/exo/wayland/zaura_output_manager.h"
#include "components/exo/wayland/zaura_output_manager_v2.h"
#include "components/exo/wayland/zcr_secure_output.h"
#include "components/exo/wayland/zxdg_output_manager.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace exo::wayland {

OutputController::OutputController(Delegate* delegate) : delegate_(delegate) {
  // Clients need to bind aura output manager globals before binding any of the
  // wl_output globals. This is necessary to ensure clients won't receive events
  // for outputs before receiving events for the aura output manager.
  wl_display* display = delegate_->GetWaylandDisplay();
  aura_output_manager_v2_ =
      std::make_unique<AuraOutputManagerV2>(base::BindRepeating(
          &OutputController::GetActiveOutputs, base::Unretained(this)));
  wl_global_create(display, &zaura_output_manager_v2_interface,
                   kZAuraOutputManagerV2Version, aura_output_manager_v2_.get(),
                   bind_aura_output_manager_v2);
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

  for (const auto& change : configuration_change.display_metrics_changes) {
    if (auto* wayland_display_output =
            GetWaylandDisplayOutput(change.display->id())) {
      wayland_display_output->SendDisplayMetricsChanges(change.display.get(),
                                                        change.changed_metrics);
    }
  }

  // Propagate display configuration changes to aura output manager clients.
  const bool needs_done =
      ProcessDisplayChangesForAuraOutputManager(configuration_change);

  // Remove outputs after propagating config changes as the WaylandDisplayOutput
  // object may be needed during change notification propagation.
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

  if (needs_done) {
    aura_output_manager_v2_->SendDone();
  }

  UpdateActivatedDisplayIfNecessary();

  // Flush updated outputs to clients immediately.
  // TODO(crbug.com/40943061): Exo should be updated to automatically flush
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

WaylandOutputList OutputController::GetActiveOutputs() const {
  WaylandOutputList output_list;
  base::ranges::transform(outputs_, std::back_inserter(output_list),
                          [](auto& pair) { return pair.second.get(); });
  return output_list;
}

bool OutputController::ProcessDisplayChangesForAuraOutputManager(
    const DisplayConfigurationChange& configuration_change) {
  OutputConfigurationChange output_config_change;

  base::ranges::transform(
      configuration_change.added_displays,
      std::back_inserter(output_config_change.added_outputs),
      [this](const display::Display& display) {
        return GetWaylandDisplayOutput(display.id());
      });
  base::ranges::transform(
      configuration_change.removed_displays,
      std::back_inserter(output_config_change.removed_outputs),
      [this](const display::Display& display) {
        return GetWaylandDisplayOutput(display.id());
      });
  for (const DisplayMetricsChange& change :
       configuration_change.display_metrics_changes) {
    // Added displays may appear in both added and changed lists, ensure added
    // outputs are not represented in both added and changed lists.
    if (!base::Contains(configuration_change.added_displays,
                        change.display.get())) {
      output_config_change.changed_outputs.emplace_back(
          GetWaylandDisplayOutput(change.display->id()),
          change.changed_metrics);
    }
  }

  return aura_output_manager_v2_->OnDidProcessDisplayChanges(
      output_config_change);
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
    aura_output_manager_v2_->SendOutputActivated(*output_pair->second);
    dispatched_activated_display_id_ = current_active_display_id;
  }
}

}  // namespace exo::wayland
