// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zaura_output_manager.h"

#include <aura-shell-server-protocol.h>
#include <wayland-server-core.h>

#include <memory>

#include "components/exo/wayland/output_metrics.h"
#include "components/exo/wayland/server_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace exo::wayland {

AuraOutputManager::AuraOutputManager(wl_resource* manager_resource)
    : client_(wl_resource_get_client(manager_resource)),
      manager_resource_(manager_resource) {}

// static.
AuraOutputManager* AuraOutputManager::Get(wl_client* client) {
  AuraOutputManager* metrics_manager = nullptr;
  wl_client_for_each_resource(
      client,
      [](wl_resource* resource, void* user_data) {
        constexpr char kAuraOutputManagerClass[] = "zaura_output_manager";
        const char* class_name = wl_resource_get_class(resource);
        if (std::strcmp(kAuraOutputManagerClass, class_name) != 0) {
          return WL_ITERATOR_CONTINUE;
        }

        DCHECK_NE(nullptr, user_data);
        AuraOutputManager** metrics_manager_ref =
            static_cast<AuraOutputManager**>(user_data);

        DCHECK_EQ(nullptr, *metrics_manager_ref);
        *metrics_manager_ref = GetUserDataAs<AuraOutputManager>(resource);
        return WL_ITERATOR_STOP;
      },
      &metrics_manager);
  return metrics_manager;
}

bool AuraOutputManager::SendOutputMetrics(wl_resource* output_resource,
                                          const display::Display& display,
                                          uint32_t changed_metrics) {
  DCHECK_EQ(client_, wl_resource_get_client(output_resource));

  if (!(changed_metrics &
        (display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
         display::DisplayObserver::DISPLAY_METRIC_WORK_AREA |
         display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
         display::DisplayObserver::DISPLAY_METRIC_ROTATION))) {
    return false;
  }

  const OutputMetrics output_metrics(display);

  const ui::wayland::WaylandDisplayIdPair& display_id =
      output_metrics.display_id;
  zaura_output_manager_send_display_id(manager_resource_, output_resource,
                                       display_id.high, display_id.low);

  const auto& logical_origin = output_metrics.logical_origin;
  zaura_output_manager_send_logical_position(manager_resource_, output_resource,
                                             logical_origin.x(),
                                             logical_origin.y());

  const auto& logical_size = output_metrics.logical_size;
  zaura_output_manager_send_logical_size(manager_resource_, output_resource,
                                         logical_size.width(),
                                         logical_size.height());

  const auto& physical_size = output_metrics.physical_size_px;
  if (output_metrics.mode_flags & WL_OUTPUT_MODE_CURRENT) {
    zaura_output_manager_send_physical_size(manager_resource_, output_resource,
                                            physical_size.width(),
                                            physical_size.height());
  }

  const auto& insets = output_metrics.logical_insets;
  zaura_output_manager_send_insets(manager_resource_, output_resource,
                                   insets.top(), insets.left(), insets.bottom(),
                                   insets.right());

  // The float value is bit_cast<> into a uint32_t. It must later be cast back
  // into a float. This is because wayland does not support native transport of
  // floats. As different CPU architectures may use different endian
  // representations for IEEE 754 floats, this implicitly assumes that the
  // caller and receiver are the same machine.
  zaura_output_manager_send_device_scale_factor(
      manager_resource_, output_resource,
      base::bit_cast<uint32_t>(output_metrics.device_scale_factor));

  zaura_output_manager_send_logical_transform(
      manager_resource_, output_resource, output_metrics.logical_transform);

  zaura_output_manager_send_panel_transform(manager_resource_, output_resource,
                                            output_metrics.panel_transform);

  zaura_output_manager_send_description(manager_resource_, output_resource,
                                        output_metrics.description.c_str());

  zaura_output_manager_send_done(manager_resource_, output_resource);

  return true;
}

void AuraOutputManager::SendOutputActivated(wl_resource* output_resource) {
  if (wl_resource_get_version(manager_resource_) >=
      ZAURA_OUTPUT_MANAGER_ACTIVATED_SINCE_VERSION) {
    CHECK_EQ(client_, wl_resource_get_client(output_resource));
    zaura_output_manager_send_activated(manager_resource_, output_resource);
  }
}

void bind_aura_output_manager(wl_client* client,
                              void* data,
                              uint32_t version,
                              uint32_t id) {
  wl_resource* manager_resource =
      wl_resource_create(client, &zaura_output_manager_interface,
                         std::min(version, kZAuraOutputManagerVersion), id);

  auto handler = std::make_unique<AuraOutputManager>(manager_resource);
  SetImplementation(manager_resource, nullptr, std::move(handler));
}

}  // namespace exo::wayland
