// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zaura_output_manager_v2.h"

#include <aura-output-management-server-protocol.h>
#include <wayland-server-core.h>

#include <memory>

#include "base/bit_cast.h"
#include "components/exo/wayland/output_metrics.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_display_observer.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"

namespace exo::wayland {

void bind_aura_output_manager_v2(wl_client* client,
                                 void* data,
                                 uint32_t version,
                                 uint32_t id) {
  wl_resource* outout_manager_resouce =
      wl_resource_create(client, &zaura_output_manager_v2_interface,
                         std::min(version, kZAuraOutputManagerV2Version), id);

  auto user_data = std::make_unique<AuraOutputManagerV2::UserData>(
      static_cast<AuraOutputManagerV2*>(data), outout_manager_resouce);
  SetImplementation(outout_manager_resouce, nullptr, std::move(user_data));
}

AuraOutputManagerV2::UserData::UserData(AuraOutputManagerV2* output_manager,
                                        wl_resource* outout_manager_resouce)
    : output_manager_(output_manager->GetWeakPtr()),
      outout_manager_resouce_(outout_manager_resouce) {
  output_manager_->Register(outout_manager_resouce);
}

AuraOutputManagerV2::UserData::~UserData() {
  if (output_manager_) {
    output_manager_->Unregister(outout_manager_resouce_.get());
  }
}

AuraOutputManagerV2::AuraOutputManagerV2(
    ActiveOutputGetter active_output_getter)
    : active_output_getter_(std::move(active_output_getter)) {}

AuraOutputManagerV2::~AuraOutputManagerV2() = default;

bool AuraOutputManagerV2::OnDidProcessDisplayChanges(
    const OutputConfigurationChange& configuration_change) {
  // A done event is required if any outputs have been added or removed.
  bool needs_done = !configuration_change.added_outputs.empty() ||
                    !configuration_change.removed_outputs.empty();

  // Send metrics for any newly-added displays.
  for (const WaylandDisplayOutput* added_output :
       configuration_change.added_outputs) {
    static constexpr uint32_t kAllDisplayChanges = 0xFFFFFFFF;
    SendOutputMetrics(*added_output, kAllDisplayChanges);
  }

  // Send metrics for any existing updated displays.
  for (const auto& change_pair : configuration_change.changed_outputs) {
    needs_done |= SendOutputMetrics(*change_pair.first, change_pair.second);
  }

  return needs_done;
}

void AuraOutputManagerV2::SendOutputActivated(
    const WaylandDisplayOutput& output) {
  for (wl_resource* manager_resource : manager_resources_) {
    const uint32_t output_name = wl_global_get_name(
        output.global(), wl_resource_get_client(manager_resource));
    zaura_output_manager_v2_send_activated(manager_resource, output_name);
  }
}

void AuraOutputManagerV2::SendDone() {
  for (wl_resource* manager_resource : manager_resources_) {
    zaura_output_manager_v2_send_done(manager_resource);
  }
}

void AuraOutputManagerV2::Register(wl_resource* manager_resource) {
  // When a client first binds to the global the current active global outputs
  // and their metrics make up the first configuration change.
  manager_resources_.insert(manager_resource);
  for (const WaylandDisplayOutput* output : active_output_getter_.Run()) {
    SendOutputMetricsForClient(*output, manager_resource);
  }
  SendDone();
}

void AuraOutputManagerV2::Unregister(wl_resource* manager_resource) {
  manager_resources_.erase(manager_resource);
}

base::WeakPtr<AuraOutputManagerV2> AuraOutputManagerV2::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool AuraOutputManagerV2::SendOutputMetrics(const WaylandDisplayOutput& output,
                                            uint32_t changed_metrics) {
  if (!(changed_metrics &
        (display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
         display::DisplayObserver::DISPLAY_METRIC_WORK_AREA |
         display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
         display::DisplayObserver::DISPLAY_METRIC_ROTATION))) {
    return false;
  }

  for (wl_resource* manager_resource : manager_resources_) {
    SendOutputMetricsForClient(output, manager_resource);
  }
  return true;
}

void AuraOutputManagerV2::SendOutputMetricsForClient(
    const WaylandDisplayOutput& output,
    wl_resource* manager_resource) {
  const uint32_t output_name = wl_global_get_name(
      output.global(), wl_resource_get_client(manager_resource));
  const OutputMetrics& output_metrics = output.metrics();

  const ui::wayland::WaylandDisplayIdPair& display_id =
      output_metrics.display_id;
  zaura_output_manager_v2_send_display_id(manager_resource, output_name,
                                          display_id.high, display_id.low);

  const auto& logical_origin = output_metrics.logical_origin;
  zaura_output_manager_v2_send_logical_position(
      manager_resource, output_name, logical_origin.x(), logical_origin.y());

  const auto& logical_size = output_metrics.logical_size;
  zaura_output_manager_v2_send_logical_size(manager_resource, output_name,
                                            logical_size.width(),
                                            logical_size.height());

  const auto& physical_size = output_metrics.physical_size_px;
  if (output_metrics.mode_flags & WL_OUTPUT_MODE_CURRENT) {
    zaura_output_manager_v2_send_physical_size(manager_resource, output_name,
                                               physical_size.width(),
                                               physical_size.height());
  }

  const auto& insets = output_metrics.logical_insets;
  zaura_output_manager_v2_send_work_area_insets(
      manager_resource, output_name, insets.top(), insets.left(),
      insets.bottom(), insets.right());

  const auto& overscan = output_metrics.physical_overscan_insets;
  zaura_output_manager_v2_send_overscan_insets(
      manager_resource, output_name, overscan.top(), overscan.left(),
      overscan.bottom(), overscan.right());

  // The float value is bit_cast<> into a uint32_t. It must later be cast back
  // into a float. This is because wayland does not support native transport of
  // floats. As different CPU architectures may use different endian
  // representations for IEEE 754 floats, this implicitly assumes that the
  // caller and receiver are the same machine.
  zaura_output_manager_v2_send_device_scale_factor(
      manager_resource, output_name,
      base::bit_cast<uint32_t>(output_metrics.device_scale_factor));

  zaura_output_manager_v2_send_logical_transform(
      manager_resource, output_name, output_metrics.logical_transform);

  zaura_output_manager_v2_send_panel_transform(manager_resource, output_name,
                                               output_metrics.panel_transform);

  zaura_output_manager_v2_send_description(manager_resource, output_name,
                                           output_metrics.description.c_str());
}

}  // namespace exo::wayland
