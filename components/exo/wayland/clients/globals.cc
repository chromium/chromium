// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/globals.h"

#include <algorithm>

#include "base/logging.h"

namespace exo::wayland::clients {
namespace {

uint32_t CalculateVersion(
    uint32_t server_version,
    const base::flat_map<std::string, uint32_t>& requested_versions,
    const std::string& interface) {
  auto iter = requested_versions.find(interface);
  if (iter == requested_versions.end())
    return server_version;
  return std::min(server_version, iter->second);
}

void RegistryHandler(void* data,
                     wl_registry* registry,
                     uint32_t id,
                     const char* interface,
                     uint32_t version) {
  Globals* globals = static_cast<Globals*>(data);

  if (globals->observer_for_testing_) {
    globals->observer_for_testing_->OnRegistryGlobal(id, interface, version);
  }

#define BIND(interface_type, global_member)                        \
  if (strcmp(interface, #interface_type) == 0) {                   \
    globals->global_member.reset(                                  \
        static_cast<interface_type*>(wl_registry_bind(             \
            registry, id, &interface_type##_interface,             \
            CalculateVersion(version, globals->requested_versions, \
                             #interface_type))));                  \
    globals->global_member.set_name(id);                           \
    return;                                                        \
  }

#define BIND_VECTOR(interface_type, global_member)                 \
  if (strcmp(interface, #interface_type) == 0) {                   \
    globals->global_member.emplace_back(                           \
        static_cast<interface_type*>(wl_registry_bind(             \
            registry, id, &interface_type##_interface,             \
            CalculateVersion(version, globals->requested_versions, \
                             #interface_type))));                  \
    globals->global_member.back().set_name(id);                    \
    return;                                                        \
  }

  BIND(wl_compositor, compositor)
  BIND(wl_shm, shm)
  BIND(wl_shell, shell)
  BIND(wl_seat, seat)
  BIND(wp_presentation, presentation)
  BIND(zaura_shell, aura_shell)
  BIND(zaura_output_manager, aura_output_manager)
  BIND(zaura_output_manager_v2, aura_output_manager_v2)
  BIND(zwp_linux_dmabuf_v1, linux_dmabuf)
  BIND(wl_subcompositor, subcompositor)
  BIND(zcr_color_manager_v1, color_manager)
  BIND(zwp_input_timestamps_manager_v1, input_timestamps_manager)
  BIND(zwp_fullscreen_shell_v1, fullscreen_shell)
  BIND_VECTOR(wl_output, outputs)
  BIND(zwp_linux_explicit_synchronization_v1, linux_explicit_synchronization)
  BIND(zcr_vsync_feedback_v1, vsync_feedback)
  BIND(xdg_wm_base, xdg_wm_base)
  BIND(zcr_stylus_v2, stylus)
  BIND(zcr_remote_shell_v1, cr_remote_shell_v1)
  BIND(zcr_remote_shell_v2, cr_remote_shell_v2)
  BIND(surface_augmenter, surface_augmenter)
  BIND(wp_single_pixel_buffer_manager_v1, wp_single_pixel_buffer_manager_v1)
  BIND(wp_viewporter, wp_viewporter)
  BIND(wp_fractional_scale_manager_v1, wp_fractional_scale_manager_v1)
  BIND(wl_data_device_manager, data_device_manager);

#undef BIND
#undef BIND_VECTOR
}

void RegistryRemover(void* data, wl_registry* registry, uint32_t id) {
  LOG(WARNING) << "Got a registry losing event for " << id;

  Globals* globals = static_cast<Globals*>(data);
  if (globals->observer_for_testing_) {
    globals->observer_for_testing_->OnRegistryGlobalRemove(id);
  }
}

wl_registry_listener registry_listener = {RegistryHandler, RegistryRemover};

}  // namespace

Globals::Globals() = default;

Globals::~Globals() = default;

void Globals::Init(
    wl_display* display,
    base::flat_map<std::string, uint32_t> in_requested_versions) {
  registry.reset(wl_display_get_registry(display));
  requested_versions = std::move(in_requested_versions);

  wl_registry_add_listener(registry.get(), &registry_listener, this);
  wl_display_roundtrip(display);
}

}  // namespace exo::wayland::clients
